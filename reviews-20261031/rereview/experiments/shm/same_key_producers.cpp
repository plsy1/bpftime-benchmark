// Two producers whose (pid, tid) collide (as two containers' pid 1 do) share
// one shard: measure what the consumer sees.
#include "handler/perf_event_handler.cpp"
#include "fork_handlers.cpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdio>
using namespace boost::interprocess;
struct payload { uint64_t tag; uint64_t seq; uint64_t pad[6]; };
int main()
{
	const char *name = "fable2_same_key";
	shared_memory_object::remove(name);
	managed_shared_memory shm(create_only, name, 64 << 20);
	auto *perf = shm.construct<bpftime::software_perf_event_data>("perf")(0, 0, 0, shm);
	perf->ensure_mmap_buffer(getpagesize() + 256 * 1024);
	int pid; int64_t tid; bpftime::current_producer_identity(pid, tid);
	std::atomic<bool> stop{false};
	std::atomic<uint64_t> sent[3] = {}, dropped[3] = {};
	auto producer = [&](int tag) {
		// the second container's pid 1 / tid 1: same key as ours
		bpftime::cached_producer_pid = pid; bpftime::cached_producer_tid = tid;
		payload p{ (uint64_t)tag, 0, {} };
		for (auto &x : p.pad) x = tag * 0x0101010101010101ull;
		while (!stop.load(std::memory_order_relaxed)) {
			p.seq = sent[tag];
			if (perf->output_data(&p, sizeof(p)) == 0) sent[tag]++; else dropped[tag]++;
		}
	};
	std::thread t1(producer, 1), t2(producer, 2);
	uint64_t good = 0, torn = 0, lost_records = 0, lost_count = 0, bad_size = 0;
	auto &hdr = perf->get_header_ref();
	uint8_t *base = (uint8_t *)perf->consumer_buffer.mmap_buffer.data() + getpagesize();
	size_t size = perf->mmap_size();
	auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (std::chrono::steady_clock::now() < end) {
		if (!perf->has_data()) { usleep(500); continue; }
		uint64_t head = smp_load_acquire_u64(&hdr.data_head), tail = hdr.data_tail;
		while (tail < head) {
			uint8_t rec[4096];
			for (size_t i = 0; i < sizeof(perf_event_header); i++) rec[i] = base[(tail + i) & (size - 1)];
			auto *h = (perf_event_header *)rec;
			if (h->size < 8 || h->size > 4096) { bad_size++; tail = head; break; }
			for (size_t i = 0; i < h->size; i++) rec[i] = base[(tail + i) & (size - 1)];
			if (h->type == PERF_RECORD_LOST) { lost_records++; lost_count += ((bpftime::perf_sample_lost *)rec)->lost; }
			else {
				auto *p = (payload *)(((bpftime::perf_sample_raw *)rec)->data);
				bool ok = (p->tag == 1 || p->tag == 2) && h->size == 80;
				for (int i = 0; i < 6 && ok; i++) ok = p->pad[i] == p->tag * 0x0101010101010101ull;
				ok ? good++ : torn++;
			}
			tail += h->size;
		}
		smp_store_release_u64(&hdr.data_tail, tail);
	}
	stop = true; t1.join(); t2.join();
	printf("shards: %zu (two producers)\n", perf->producer_shards.size());
	printf("sent %lu+%lu dropped(ENOSPC) %lu+%lu\n", (unsigned long)sent[1].load(), (unsigned long)sent[2].load(),
	       (unsigned long)dropped[1].load(), (unsigned long)dropped[2].load());
	printf("consumer: good %lu torn %lu bad_size %lu, LOST records %lu (count %lu)\n",
	       (unsigned long)good, (unsigned long)torn, (unsigned long)bad_size, (unsigned long)lost_records, (unsigned long)lost_count);
	printf("records unaccounted (sent - good - lost_count): %ld\n",
	       (long)(sent[1] + sent[2] - good - lost_count));
	shared_memory_object::remove(name);
	return 0;
}
