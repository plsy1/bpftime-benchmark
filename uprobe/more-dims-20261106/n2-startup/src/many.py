#!/usr/bin/env python3
"""Writes many.c: M functions f0..f(M-1) (noinline, long enough for Gum's redirect) and a main
that either exits at once ("start") or calls them round robin for SECS seconds on CPU 5 and
prints total_ops ("run SECS")."""
import sys
m = int(sys.argv[1])
out = ['#define _GNU_SOURCE', '#include <sched.h>', '#include <stdio.h>', '#include <stdlib.h>',
       '#include <string.h>', '#include <time.h>', '']
for i in range(m):
    out.append(f'__attribute__((noinline)) unsigned long f{i}(unsigned long k) '
               f'{{ unsigned long h = k; for (int i = 0; i < 4; i++) h = h * 2654435761u + {i}; '
               f'__asm__ volatile("" : : "r"(h) : "memory"); return h; }}')
out.append('typedef unsigned long (*fn)(unsigned long);')
out.append('static fn fns[] = {' + ', '.join(f'f{i}' for i in range(m)) + '};')
out.append('''static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
int main(int argc, char **argv)
{
	if (argc < 2 || strcmp(argv[1], "run"))
		return 0;
	int secs = argc > 2 ? atoi(argv[2]) : 3;
	cpu_set_t set; CPU_ZERO(&set); CPU_SET(5, &set); sched_setaffinity(0, sizeof(set), &set);
	unsigned long calls = 0, acc = 0, n = sizeof(fns) / sizeof(fns[0]);
	double end = now() + secs;
	do {
		for (unsigned long i = 0; i < 1024; i++)
			acc += fns[(calls + i) % n](i);
		calls += 1024;
	} while (now() < end);
	printf("total_ops %lu\\nops_per_sec %.1f\\nacc %lu\\n", calls, calls / (double)secs, acc & 1);
	FILE *f = fopen("/proc/self/status", "r");
	char line[256];
	while (f && fgets(line, sizeof(line), f))
		if (!strncmp(line, "VmRSS:", 6))
			printf("vmrss_kb %ld\\n", atol(line + 6));
	if (f)
		fclose(f);
	return 0;
}''')
open(f'many-{m}.c', 'w').write('\n'.join(out) + '\n')
