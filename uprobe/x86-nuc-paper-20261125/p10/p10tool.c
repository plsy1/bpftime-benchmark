// P10 loader: loads p10.bpf.o, attaches program PROG (rbprog | empty) as a uprobe on FUNC in BINARY (all
// processes). For rbprog it consumes the ring buffer (ring_buffer__poll, 100 ms) and counts records. Prints
// "attached"; on SIGINT/SIGTERM drains once more and prints "records R drops D". The same binary is the kernel tool
// (plain) and the bpftime tool (LD_PRELOAD=libbpftime-syscall-server.so). Usage: p10tool OBJ PROG BINARY FUNC
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t exiting;
static unsigned long long records;
static void on_signal(int sig)
{
	(void)sig;
	exiting = 1;
}
static int on_rec(void *ctx, void *data, size_t len)
{
	(void)ctx;
	(void)data;
	(void)len;
	records++;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 5) {
		fprintf(stderr, "usage: p10tool OBJ PROG BINARY FUNC\n");
		return 1;
	}
	struct bpf_object *obj = bpf_object__open_file(argv[1], NULL);
	if (!obj) {
		fprintf(stderr, "open failed\n");
		return 1;
	}
	struct bpf_program *p = bpf_object__find_program_by_name(obj, argv[2]), *q;
	if (!p) {
		fprintf(stderr, "no program %s\n", argv[2]);
		return 1;
	}
	bpf_object__for_each_program(q, obj)
		bpf_program__set_autoload(q, q == p);
	if (bpf_object__load(obj)) {
		fprintf(stderr, "load failed\n");
		return 1;
	}
	LIBBPF_OPTS(bpf_uprobe_opts, opts, .func_name = argv[4]);
	struct bpf_link *link = bpf_program__attach_uprobe_opts(p, -1, argv[3], 0, &opts);
	if (!link) {
		fprintf(stderr, "attach failed: %s\n", strerror(errno));
		return 1;
	}
	struct ring_buffer *rb = NULL;
	int busy = strcmp(argv[2], "rbnw") == 0;  // P10b: no wakeups, so poll with timeout 0
	if (strcmp(argv[2], "rbprog") == 0 || busy) {
		rb = ring_buffer__new(bpf_map__fd(bpf_object__find_map_by_name(obj, "rb")), on_rec, NULL, NULL);
		if (!rb) {
			fprintf(stderr, "ring_buffer__new failed\n");
			return 1;
		}
	}
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	printf("attached\n");
	fflush(stdout);
	while (!exiting) {
		if (rb) {
			// busy (rbnw): no wakeup ever makes the ring fd ready for epoll, so read the ring directly
			int r = busy ? ring_buffer__consume(rb) : ring_buffer__poll(rb, 100);
			if (r < 0 && r != -EINTR) {
				fprintf(stderr, "poll error %d\n", r);
				break;
			}
		} else {
			struct timespec ts = {0, 100000000};
			nanosleep(&ts, NULL);
		}
	}
	if (rb)
		ring_buffer__consume(rb);
	unsigned long long d = 0;
	__u32 z = 0;
	bpf_map_lookup_elem(bpf_map__fd(bpf_object__find_map_by_name(obj, "drops")), &z, &d);
	printf("records %llu drops %llu\n", records, d);
	fflush(stdout);
	ring_buffer__free(rb);
	bpf_link__destroy(link);
	bpf_object__close(obj);
	return 0;
}
