#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
static void bus(int s) { (void)s; write(1, "  SIGBUS\n", 9); _exit(1); }
int main(int argc, char **argv) {
	signal(SIGBUS, bus);
	static uint8_t buf[64] __attribute__((aligned(64)));
	int off = atoi(argv[1]);
	uint64_t *p = (uint64_t *)(buf + off);
	*p = 1; /* plain access */
	__atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST);
	printf("  off %d ok (%llu)\n", off, (unsigned long long)*p);
	return 0;
}
