// funclatency target: calls the noinline function target_func in a loop for DURATION seconds and
// prints the median calls/s. target_func does a little work so it is not optimized away.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static int cmp(const void *a, const void *b) { double x = *(double *)a, y = *(double *)b; return (x > y) - (x < y); }
volatile unsigned long sink;
__attribute__((noinline)) unsigned long target_func(unsigned long x) {
    unsigned long s = 0;
    for (int i = 0; i < 16; i++) s += (x ^ i) * 2654435761u;
    return s;
}
int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 10;
    double rates[1000]; int k = 0; unsigned long ops = 0, total_ops = 0, i = 0;
    double mark = now();
    while (k < duration) {
        for (int j = 0; j < 1000; j++) sink += target_func(i++);
        ops += 1000; total_ops += 1000;
        double t = now();
        if (t - mark >= 1.0) { rates[k++] = ops / (t - mark); ops = 0; mark = t; }
    }
    qsort(rates, k, sizeof(double), cmp);
    printf("ops_per_sec_median %.1f\n", rates[k / 2]);
    printf("total_ops %lu\n", total_ops);
    return 0;
}
