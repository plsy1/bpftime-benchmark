// Two producers in different pid namespaces with the same (pid, tid).
#include "handler/perf_event_handler.cpp"
#include "fork_handlers.cpp"
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>
#include <chrono>
using namespace boost::interprocess;
struct payload { uint64_t tag; uint64_t seq; uint64_t pad[6]; };
int main(int argc, char **argv)
{
	const char *name = "fable2_ns_collide";
	if (argc == 3) { // producer: name tag
		managed_shared_memory shm(open_only, name);
		auto *perf = shm.find<bpftime::software_perf_event_data>("perf").first;
		payload p{ (uint64_t)atoi(argv[2]), 0, {} };
		for (int i = 0; i < 6; i++) p.pad[i] = p.tag * 0x0101010101010101ull;
		auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		uint64_t sent = 0, dropped = 0;
		while (std::chrono::steady_clock::now() < end) {
			p.seq = sent;
			if (perf->output_data(&p, sizeof(p)) == 0) sent++; else dropped++;
		}
		fprintf(stderr, "producer tag %lu pid %d tid %ld: sent %lu dropped %lu\n",
			(unsigned long)p.tag, getpid(), (long)syscall(SYS_gettid),
			(unsigned long)sent, (unsigned long)dropped);
		return 0;
	}
	shared_memory_object::remove(name);
	managed_shared_memory shm(create_only, name, 64 << 20);
	auto *perf = shm.construct<bpftime::software_perf_event_data>("perf")(0, 0, 0, shm);
	perf->ensure_mmap_buffer(getpagesize() + 256 * 1024);
	pid_t kids[2];
	for (int k = 0; k < 2; k++) {
		kids[k] = fork();
		if (kids[k] == 0) {
			std::string tag = std::to_string(k + 1);
			execlp("unshare", "unshare", "-Urpf", argv[0], name, tag.c_str(), (char *)nullptr);
			_exit(127);
		}
	}
	// consumer: drain and validate records from the consumer ring
	uint64_t good = 0, torn = 0, lost_records = 0, lost_count = 0;
	auto end = std::chrono::steady_clock::now() + std::chrono::seconds(4);
	auto &hdr = perf->get_header_ref();
	uint8_t *base = (uint8_t *)perf->consumer_buffer.mmap_buffer.data() + getpagesize();
	size_t size = perf->mmap_size();
	while (std::chrono::steady_clock::now() < end) {
		if (!perf->has_data()) { usleep(1000); continue; }
		uint64_t head = smp_load_acquire_u64(&hdr.data_head), tail = hdr.data_tail;
		while (tail < head) {
			uint8_t rec[4096];
			for (size_t i = 0; i < sizeof(perf_event_header); i++) rec[i] = base[(tail + i) & (size - 1)];
			auto *h = (perf_event_header *)rec;
			for (size_t i = 0; i < h->size; i++) rec[i] = base[(tail + i) & (size - 1)];
			if (h->type == PERF_RECORD_LOST) { lost_records++; lost_count += ((bpftime::perf_sample_lost *)rec)->lost; }
			else {
				auto *p = (payload *)(((bpftime::perf_sample_raw *)rec)->data);
				bool ok = (p->tag == 1 || p->tag == 2);
				for (int i = 0; i < 6 && ok; i++) ok = p->pad[i] == p->tag * 0x0101010101010101ull;
				ok ? good++ : torn++;
			}
			tail += h->size;
		}
		smp_store_release_u64(&hdr.data_tail, tail);
	}
	for (auto k : kids) { int st; waitpid(k, &st, 0); }
	printf("shards: %zu\n", perf->producer_shards.size());
	for (auto &s : perf->producer_shards)
		printf("  shard pid=%d tid=%ld ns=%lu\n", s.pid, (long)s.tid, (unsigned long)s.pid_namespace);
	printf("consumer: good %lu torn %lu, LOST records %lu (count %lu)\n",
	       (unsigned long)good, (unsigned long)torn, (unsigned long)lost_records, (unsigned long)lost_count);
	shared_memory_object::remove(name);
	return 0;
}
