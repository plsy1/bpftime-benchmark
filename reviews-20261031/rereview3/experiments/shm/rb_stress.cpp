// Ringbuf stress: live producer threads with sequenced, checksummed records;
// dead producers (forked children reserving then exiting, left as zombies or
// killed; in-process threads exiting mid-reservation); one consumer running
// recover_dead_reservation() + fetch_data() like the tool's epoll wait.
#include <atomic>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/smart_ptr/deleter.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>
#include <boost/interprocess/smart_ptr/weak_ptr.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <cstddef>
#include <functional>
#include <optional>
#include <pthread.h>
#include <vector>
#define class struct
#include <bpf_map/userspace/ringbuf_map.hpp>
#undef class
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <random>
#include "producer_identity.hpp"

struct rec {
	uint32_t thread;
	uint32_t len;
	uint64_t seq;
	uint64_t sum;
	uint8_t payload[];
};
static std::atomic<bool> stop{ false };
static std::atomic<long> enospc{ 0 }, noslot{ 0 }, produced{ 0 }, delivered{ 0 },
	torn{ 0 }, dup_or_gap{ 0 }, recovered{ 0 }, dead_children{ 0 },
	dead_threads{ 0 };
static constexpr int NPROD = 3;
static uint64_t next_seq[NPROD + 1];
static uint64_t produced_seq[NPROD + 1];
static std::atomic<long> prod_tid[NPROD + 1];
static std::atomic<long> live_discards{ 0 }, recovered_inproc{ 0 };
#include <sys/syscall.h>

static uint64_t checksum(const uint8_t *p, size_t n)
{
	uint64_t h = 1469598103934665603ull;
	for (size_t i = 0; i < n; i++)
		h = (h ^ p[i]) * 1099511628211ull;
	return h;
}

static void fill(void *p, uint32_t thread, uint32_t len, uint64_t seq,
		 std::mt19937 &rng)
{
	auto *r = (rec *)p;
	r->thread = thread;
	r->len = len;
	r->seq = seq;
	for (uint32_t i = 0; i < len; i++)
		r->payload[i] = (uint8_t)rng();
	r->sum = checksum(r->payload, len);
}

static bool recover_checked(bpftime::ringbuf &rb)
{
	// Peek at the head record's owner before recovery, as recovery does.
	unsigned long cons = *(volatile unsigned long *)rb.consumer_pos.get();
	unsigned long prod = *(volatile unsigned long *)rb.producer_pos.get();
	int owner_pid = 0, owner_tid = 0; bool busy = false;
	if (cons < prod) {
		auto *hdr = (uint32_t *)(rb.data.get() + (cons & rb.mask()));
		uint32_t len = __atomic_load_n(hdr, __ATOMIC_ACQUIRE);
		busy = len & 0x80000000u;
		int32_t slot = (int32_t)hdr[1];
		if (busy && slot >= 0 && slot < 256 && rb.owners[slot].position_plus_one.load() == cons + 1) {
			owner_pid = rb.owners[slot].pid; owner_tid = rb.owners[slot].tid;
		}
	}
	bool r = rb.recover_dead_reservation();
	if (r && owner_pid == getpid()) {
		recovered_inproc++;
		for (int t = 1; t <= NPROD; t++)
			if (prod_tid[t] == owner_tid) {
				live_discards++;
				fprintf(stderr, "recovery discarded live producer %d's record (tid %d) at %lu\n", t, owner_tid, cons);
			}
	}
	return r;
}

static int on_record(void *, void *data, size_t len)
{
	auto *r = (rec *)data;
	if (len < sizeof(rec) || r->len != len - sizeof(rec) ||
	    r->sum != checksum(r->payload, r->len)) {
		torn++;
		return 0;
	}
	if (r->thread >= 1 && r->thread <= NPROD) {
		if (r->seq != next_seq[r->thread]) {
			dup_or_gap++;
			fprintf(stderr, "thread %u: expected seq %lu got %lu\n",
				r->thread, next_seq[r->thread], r->seq);
		}
		next_seq[r->thread] = r->seq + 1;
	} else if (r->thread == 999) {
		// a dead producer's record must never be delivered
		fprintf(stderr, "delivered a record of a dead producer\n");
		torn++;
	}
	delivered++;
	return 0;
}

extern "C" long fable4_stale_release, fable4_lap_free;
int main(int argc, char **argv)
{
	const int seconds = argc > 1 ? atoi(argv[1]) : 8;
	const uint32_t max_ent = argc > 2 ? atoi(argv[2]) : 64 * 1024;
	const bool killer = argc > 3 ? atoi(argv[3]) != 0 : true;
	const std::string name = "fable4_rb_stress_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, name.c_str(), (size_t)max_ent * 2 + (16 << 20));
	auto *map = shm.construct<bpftime::ringbuf_map_impl>("ringbuf")(max_ent,
									 shm);
	auto rb = map->create_impl_shared_ptr();
	auto *premature = shm.construct<std::atomic<long>>("premature")(0);
	auto *child_checks = shm.construct<std::atomic<long>>("child_checks")(0);

	// A multi-threaded child whose main thread exits (pthread_exit) while
	// another thread keeps the process alive: the leader is a zombie, tgkill
	// still finds it, and its reservation must be recovered before the
	// process ends.
	{
		pid_t c = fork();
		if (c == 0) {
			void *p = map->reserve(sizeof(rec) + 16, 7);
			if (p) { ((rec *)p)->thread = 999; ((rec *)p)->len = 16; }
			std::thread([] { usleep(400000); }).detach();
			pthread_exit(nullptr);
		}
		bool rec_ok = false; char st = 0;
		for (int i = 0; i < 300 && !rec_ok; i++) {
			usleep(1000);
			char path[64]; snprintf(path, sizeof path, "/proc/%d/task/%d/stat", c, c);
			FILE *f = fopen(path, "r"); char buf[512] = {};
			if (f) { fread(buf, 1, 511, f); fclose(f); const char *e = strrchr(buf, ')'); if (e) st = e[2]; }
			rec_ok = rb->recover_dead_reservation();
		}
		int status = 0; bool still_alive = waitpid(c, &status, WNOHANG) == 0;
		printf("leader pthread_exit: leader state '%c', recovered=%d while process alive=%d\n", st, rec_ok, still_alive);
		waitpid(c, &status, 0);
		rb->fetch_data(on_record, nullptr);
	}

	std::vector<std::thread> prods;
	for (int t = 1; t <= NPROD; t++) {
		prods.emplace_back([&, t] {
			std::mt19937 rng(t);
			uint64_t seq = 0;
			prod_tid[t] = syscall(SYS_gettid);
			while (!stop) {
				uint32_t len = rng() % 200;
				void *p = map->reserve(sizeof(rec) + len, 7);
				if (!p) {
					enospc++;
					sched_yield();
					continue;
				}
				if (rb->owner_slot_for_test(p) < 0)
					noslot++;
				bool discard = (rng() % 50) == 0;
				fill(p, t, len, discard ? ~0ull : seq, rng);
				map->submit(p, discard);
				if (!discard) {
					seq++;
					produced++;
				}
				produced_seq[t] = seq;
			}
		});
	}
	// Consumer: tool shape.
	std::thread consumer([&] {
		auto last_progress = std::chrono::steady_clock::now();
		long last_delivered = 0;
		while (!stop) {
			if (recover_checked(*rb))
				recovered++;
			rb->fetch_data(on_record, nullptr);
			if (delivered != last_delivered) {
				last_delivered = delivered;
				last_progress = std::chrono::steady_clock::now();
			} else if (std::chrono::steady_clock::now() - last_progress >
				   std::chrono::seconds(2)) {
				fprintf(stderr, "consumer stalled for 2 s\n");
				torn += 1000000;
				stop = true;
			}
			usleep(200);
		}
		// drain: until the ring is empty (dead children's records are
		// recoverable only once they have died), at most 3 s
		auto t_end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
		while (std::chrono::steady_clock::now() < t_end) {
			recover_checked(*rb);
			rb->fetch_data(on_record, nullptr);
			unsigned long cons = *(volatile unsigned long *)rb->consumer_pos.get();
			unsigned long prod = *(volatile unsigned long *)rb->producer_pos.get();
			if (cons == prod)
				break;
			usleep(1000);
		}
	});
	// Killer: dead producers of three kinds.
	std::vector<pid_t> zombies;
	auto t0 = std::chrono::steady_clock::now();
	int round = 0;
	while (std::chrono::steady_clock::now() - t0 <
	       std::chrono::seconds(seconds)) {
		round++;
		if (!killer) { usleep(5000); continue; }
		pid_t c = fork();
		if (c == 0) {
			void *p = map->reserve(sizeof(rec) + 16, 7);
			if (p) {
				auto *r = (rec *)p;
				r->thread = 999;
				r->len = 16;
				r->sum = 0;
			}
			if (round % 3 == 0) {
				// killed with the reservation (no exit path)
				raise(SIGKILL);
			}
			if (round % 3 == 1) {
				// a non-leader thread dies mid-reservation, process lives a bit
				std::thread([&] {
					map->reserve(sizeof(rec) + 16, 7);
				}).join();
				usleep(20000);
				// the live leader's own record must still be busy
				if (p) {
					auto *h = (uint32_t *)((uint8_t *)p - 8);
					child_checks->fetch_add(1);
					if ((__atomic_load_n(h, __ATOMIC_ACQUIRE) & 0x80000000u) == 0)
						premature->fetch_add(1);
				}
			}
			_exit(0);
		}
		dead_children++;
		if (round % 2 == 0) {
			// leave a zombie for a while
			zombies.push_back(c);
			if (zombies.size() > 8) {
				waitpid(zombies.front(), nullptr, 0);
				zombies.erase(zombies.begin());
			}
		} else {
			waitpid(c, nullptr, 0);
		}
		// in-process thread exiting mid-reservation
		if (round % 4 == 0) {
			std::thread([&] {
				void *p = map->reserve(sizeof(rec) + 8, 7);
				if (p) {
					((rec *)p)->thread = 999;
					((rec *)p)->len = 8;
				}
			}).join();
			dead_threads++;
		}
		usleep(5000);
	}
	stop = true;
	for (auto &p : prods)
		p.join();
	consumer.join();
	for (pid_t z : zombies)
		waitpid(z, nullptr, 0);
	long lost = 0;
	for (int t = 1; t <= NPROD; t++) {
		lost += (long)(produced_seq[t] - next_seq[t]);
		if (produced_seq[t] != next_seq[t])
			printf("thread %d: produced %lu delivered up to %lu\n", t, produced_seq[t], next_seq[t]);
	}
	printf("recovered in-process (expected <= dead_threads): %ld\n", recovered_inproc.load());
	printf("live children checked: %ld, records discarded while the child lived: %ld\n", child_checks->load(), premature->load());
	if (premature->load()) torn += 1000;
	printf("produced=%ld delivered=%ld lost(live)=%ld torn=%ld dup_or_gap=%ld "
	       "recovered=%ld dead_children=%ld dead_threads=%ld enospc=%ld noslot=%ld live_discards=%ld\n",
	       produced.load(), delivered.load(), lost, torn.load(),
	       dup_or_gap.load(), recovered.load(), dead_children.load(),
	       dead_threads.load(), enospc.load(), noslot.load(), live_discards.load());
	// every ring that stalls for the whole run would have held records back:
	// also check the ring is empty at the end
	bool empty = !rb->has_data();
	printf("ring has_data false at end: %s\n", empty ? "yes" : "NO");
	{
		unsigned long cons = *(volatile unsigned long *)rb->consumer_pos.get();
		unsigned long prod = *(volatile unsigned long *)rb->producer_pos.get();
		printf("cons=%lu prod=%lu backlog=%lu stale_release=%ld lap_free=%ld\n", cons, prod, prod - cons, fable4_stale_release, fable4_lap_free);
		if (cons < prod) {
			auto *hdr = (uint32_t *)(rb->data.get() + (cons & rb->mask()));
			uint32_t len = hdr[0]; int32_t slot = (int32_t)hdr[1];
			printf("head: len=0x%x busy=%d slot=%d\n", len, (len >> 31) & 1, slot);
			if (slot >= 0 && slot < 256) {
				auto &o = rb->owners[slot];
				printf("slot %d: position_plus_one=%lu (head wants %lu) pid=%d tid=%d alive(tgkill)=%d gone=%d\n", slot,
				       (unsigned long)o.position_plus_one.load(), cons + 1, o.pid, o.tid,
				       bpftime::is_thread_alive(o.pid, o.tid), bpftime::is_thread_gone(o.pid, o.tid));
			}
			empty = false;
		}
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
	return (torn == 0 && dup_or_gap == 0 && lost == 0 && empty && noslot == 0) ? 0 : 1;
}
