// A short-lived traced process (more-dims-20261106/n2-startup): calls count_event once when HIT
// is set in its arguments ("hit"), else not at all, and exits. With FUNCS=M (argv[2]) it is
// built with M probe-able functions f0..f(M-1) by many.c instead (see there).
#include <stdlib.h>
#include <string.h>
#include <time.h>

__attribute__((noinline)) unsigned long count_event(unsigned long key)
{
	unsigned long h = key;
	for (int i = 0; i < 4; i++)
		h = h * 2654435761u + i;
	__asm__ volatile("" : : "r"(h) : "memory");
	return h;
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "hit")) {
		struct timespec t;
		clock_gettime(CLOCK_MONOTONIC, &t);
		return (int)(count_event(t.tv_nsec) & 0);
	}
	return 0;
}
