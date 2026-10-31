// Perf output: parent threads output sequenced, checksummed samples while
// many fork children (nginx shape) output once and exit; a consumer thread
// drains like the tool (has_data) and parses the consumer ring. Mode "drop":
// several threads start outputting with the segment almost full (shards'
// data areas cannot be allocated), the memory is freed later.
#include <sstream>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/list.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <spdlog/spdlog.h>
#include <bpftime_shm.hpp>
#define private public
#include <handler/perf_event_handler.hpp>
#undef private
using bpftime::perf_sample_lost; using bpftime::perf_sample_raw;
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <random>
#include <map>

struct payload {
	uint32_t who;
	uint32_t len;
	uint64_t seq;
	uint64_t sum;
	uint8_t data[200];
};
static uint64_t checksum(const uint8_t *p, size_t n)
{
	uint64_t h = 1469598103934665603ull;
	for (size_t i = 0; i < n; i++)
		h = (h ^ p[i]) * 1099511628211ull;
	return h;
}
static std::atomic<bool> stop{ false };
static std::atomic<long> produced{ 0 }, dropped{ 0 }, delivered{ 0 }, lost_reported{ 0 },
	torn{ 0 }, gaps{ 0 }, child_records{ 0 }, children{ 0 }, enospc_noshard{ 0 };
static std::map<uint32_t, uint64_t> next_seq;
static std::map<uint32_t, int> child_seen;

static void make(payload &p, uint32_t who, uint64_t seq, std::mt19937 &rng)
{
	p.who = who;
	p.len = rng() % 200;
	p.seq = seq;
	for (uint32_t i = 0; i < p.len; i++)
		p.data[i] = (uint8_t)rng();
	p.sum = checksum(p.data, p.len);
}
static size_t psize(const payload &p)
{
	return offsetof(payload, data) + p.len;
}

// Parse the consumer ring like a tool would.
static void consume(bpftime::software_perf_event_data &perf)
{
	auto &buf = perf.consumer_buffer;
	auto &hdr = buf.get_header_ref();
	uint64_t tail = __atomic_load_n(&hdr.data_tail, __ATOMIC_ACQUIRE);
	uint64_t head = __atomic_load_n(&hdr.data_head, __ATOMIC_ACQUIRE);
	const size_t size = buf.mmap_size();
	const uint8_t *base = buf.mmap_buffer.data() + buf.pagesize;
	while (tail != head) {
		perf_event_header eh;
		buf.read_wrapped(tail, &eh, sizeof(eh));
		std::vector<uint8_t> rec(eh.size);
		buf.read_wrapped(tail, rec.data(), eh.size);
		if (eh.type == PERF_RECORD_LOST) {
			auto *l = (perf_sample_lost *)rec.data();
			lost_reported += l->lost;
		} else if (eh.type == PERF_RECORD_SAMPLE) {
			auto *s = (perf_sample_raw *)rec.data();
			auto *p = (payload *)(rec.data() + sizeof(perf_sample_raw));
			if (s->size < offsetof(payload, data) || p->len > 200 ||
			    s->size < psize(*p) || p->sum != checksum(p->data, p->len)) {
				torn++;
			} else if (p->who < 1000) {
				// parent thread: contiguous unless a drop was reported
				if (next_seq.count(p->who) && p->seq != next_seq[p->who]) {
					if (p->seq < next_seq[p->who]) { gaps++; fprintf(stderr, "dup/backwards who=%u seq=%lu expected %lu\n", p->who, p->seq, next_seq[p->who]); }
					else gaps += 0; // forward gap: must be covered by LOST
				}
				next_seq[p->who] = p->seq + 1;
			} else {
				if (child_seen[p->who]++ > 0) { gaps++; fprintf(stderr, "child %u delivered twice\n", p->who); }
				child_records++;
			}
			delivered++;
		} else {
			torn++;
		}
		tail += eh.size;
		__atomic_store_n(&hdr.data_tail, tail, __ATOMIC_RELEASE);
	}
	(void)size; (void)base;
}

int main(int argc, char **argv)
{
	const std::string mode = argc > 1 ? argv[1] : "storm";
	const int seconds = argc > 2 ? atoi(argv[2]) : 6;
	const std::string name = "fable4_perf_storm_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	const size_t shm_size = mode == "drop" ? (1 << 20) : (64 << 20);
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, name.c_str(), shm_size);
	auto *perf = shm.construct<bpftime::software_perf_event_data>("perf")(0, 0, 0, shm);
	if (perf->ensure_mmap_buffer(getpagesize() + 256 * 1024) == nullptr && mode != "drop") {
		fprintf(stderr, "consumer mmap failed\n"); return 2;
	}
	if (mode == "drop") {
		// 64 KiB consumer ring; fill the segment so shard data (64 KiB) does not fit
		if (perf->ensure_mmap_buffer(getpagesize() + 64 * 1024) == nullptr) { fprintf(stderr, "mmap failed\n"); return 2; }
	}
	std::vector<void *> filler;
	if (mode == "drop") {
		while (shm.get_free_memory() > 24 * 1024) {
			size_t want = shm.get_free_memory() - 24 * 1024;
			if (want > 64 * 1024) want = 64 * 1024;
			try { filler.push_back(shm.allocate(want)); } catch (...) { break; }
		}
	}
	const int NPROD = mode == "drop" ? 4 : 2;
	std::vector<std::thread> prods;
	std::vector<uint64_t> prod_seq(NPROD + 1);
	for (int t = 1; t <= NPROD; t++) {
		prods.emplace_back([&, t] {
			std::mt19937 rng(t);
			uint64_t seq = 0;
			payload p;
			while (!stop) {
				make(p, t, seq, rng);
				errno = 0;
				int r = perf->output_data(&p, psize(p));
				if (r == 0) { produced++; seq++; }
				else {
					dropped++;
					if (perf->producer_shards.empty() || errno == ENOSPC) enospc_noshard++;
					sched_yield();
				}
				prod_seq[t] = seq;
			}
		});
	}
	std::thread consumer([&] {
		while (!stop) {
			if (perf->has_data()) consume(*perf);
			usleep(500);
		}
		for (int i = 0; i < 200; i++) { perf->has_data(); consume(*perf); usleep(1000); }
	});
	auto t0 = std::chrono::steady_clock::now();
	std::vector<pid_t> zombies;
	uint32_t child_id = 1000;
	int round = 0;
	size_t max_shards = 0;
	while (std::chrono::steady_clock::now() - t0 < std::chrono::seconds(seconds)) {
		round++;
		if (mode == "drop") {
			if (round < 100) { size_t n = perf->producer_shards.size(); if (n > max_shards) max_shards = n; }
			if (round == 100) { printf("filler freed; shards kept while full (max, expect 0): %zu, outputs failed so far: %ld\n", max_shards, dropped.load()); for (void *f : filler) shm.deallocate(f); filler.clear(); max_shards = 0; }
			usleep(5000);
			continue;
		}
		pid_t c = fork();
		if (c == 0) {
			std::mt19937 rng(child_id);
			payload p;
			make(p, child_id, 0, rng);
			int r = perf->output_data(&p, psize(p));
			if (r != 0) _exit(3);
			if (round % 5 == 0) {
				// grandchild outputs too
				pid_t g = fork();
				if (g == 0) { make(p, child_id + 500000, 0, rng); _exit(perf->output_data(&p, psize(p)) == 0 ? 0 : 3); }
				waitpid(g, nullptr, 0);
			}
			if (round % 3 == 0) raise(SIGKILL);
			_exit(0);
		}
		children++;
		child_id++;
		if (round % 2 == 0) {
			zombies.push_back(c);
			if (zombies.size() > 8) { waitpid(zombies.front(), nullptr, 0); zombies.erase(zombies.begin()); }
		} else {
			int st = 0; waitpid(c, &st, 0);
			if (WIFEXITED(st) && WEXITSTATUS(st) == 3) fprintf(stderr, "child output failed\n");
		}
		{
			// shard count under the lock is the consumer's business; a racy read is fine for a maximum
			size_t n = perf->producer_shards.size();
			if (n > max_shards) max_shards = n;
		}
		usleep(3000);
	}
	stop = true;
	for (auto &p : prods) p.join();
	for (pid_t z : zombies) waitpid(z, nullptr, 0);
	consumer.join();
	// after every child is dead: many drains => reclaim
	for (int i = 0; i < 70; i++) { perf->has_data(); consume(*perf); }
	long expected_children = 0;
	for (auto &kv : child_seen) (void)kv, expected_children++;
	long parent_lost = 0;
	for (int t = 1; t <= NPROD; t++) parent_lost += (long)(prod_seq[t] - (next_seq.count(t) ? next_seq[t] : 0));
	printf("mode=%s produced=%ld dropped(ENOSPC)=%ld delivered=%ld lost_reported=%ld parent_missing=%ld "
	       "torn=%ld dup=%ld children=%ld child_records=%ld distinct_children=%ld max_shards=%zu shards_left=%zu free_shm=%zu\n",
	       mode.c_str(), produced.load(), dropped.load(), delivered.load(), lost_reported.load(), parent_lost,
	       torn.load(), gaps.load(), children.load(), child_records.load(), expected_children, max_shards,
	       perf->producer_shards.size(), shm.get_free_memory());
	for (auto &s : perf->producer_shards)
		printf("  shard pid=%d tid=%ld ns=%lx gen=%lu data=%zu has_data=%d\n", s.pid, (long)s.tid, s.pid_namespace, s.generation, s.buffer.mmap_size(), s.buffer.has_data());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	// parent records missing must equal the lost counts reported (drops are counted, never silent)
	bool ok = torn == 0 && gaps == 0 && parent_lost == lost_reported.load();
	return ok ? 0 : 1;
}
