// malloc/free workload for memleak: N outstanding blocks of SIZE bytes, each iteration frees
// the oldest and allocates a new one, so allocations and frees are balanced. Prints ops/s
// (one op = one malloc + one free) every second for DURATION seconds, then the median.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static int cmp(const void *a, const void *b) { double x = *(double *)a, y = *(double *)b; return (x > y) - (x < y); }
int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 10, size = argc > 2 ? atoi(argv[2]) : 64, n = argc > 3 ? atoi(argv[3]) : 1024;
    void **slots = calloc(n, sizeof(void *));
    for (int i = 0; i < n; i++) slots[i] = malloc(size);
    double rates[1000]; int k = 0; unsigned long ops = 0, total_ops = 0, idx = 0;
    double start = now(), mark = start;
    while (k < duration) {
        for (int j = 0; j < 1000; j++) {
            free(slots[idx]); slots[idx] = malloc(size); memset(slots[idx], 0, 8);
            idx = (idx + 1) % n; ops++; total_ops++;
        }
        double t = now();
        if (t - mark >= 1.0) { rates[k++] = ops / (t - mark); ops = 0; mark = t; }
    }
    qsort(rates, k, sizeof(double), cmp);
    printf("ops_per_sec_median %.1f\n", rates[k / 2]);
    printf("total_ops %lu\n", total_ops);
    return 0;
}
