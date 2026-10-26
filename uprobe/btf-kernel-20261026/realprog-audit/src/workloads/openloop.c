// opensnoop target: opens and closes PATH (default /etc/hostname) in a loop for DURATION seconds,
// with an optional pause after every 100 opens, and prints the median opens/s and the exact count.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static int cmp(const void *a, const void *b) { double x = *(double *)a, y = *(double *)b; return (x > y) - (x < y); }
int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 10;
    int pause_us = argc > 2 ? atoi(argv[2]) : 0;
    const char *path = argc > 3 ? argv[3] : "/etc/hostname";
    double rates[1000]; int k = 0; unsigned long ops = 0, total_ops = 0;
    double mark = now();
    while (k < duration) {
        for (int j = 0; j < 100; j++) {
            int fd = open(path, O_RDONLY);
            if (fd < 0) { perror("open"); return 1; }
            close(fd);
        }
        ops += 100; total_ops += 100;
        if (pause_us) usleep(pause_us);
        double t = now();
        if (t - mark >= 1.0) { rates[k++] = ops / (t - mark); ops = 0; mark = t; }
    }
    qsort(rates, k, sizeof(double), cmp);
    printf("ops_per_sec_median %.1f\n", rates[k / 2]);
    printf("total_ops %lu\n", total_ops);
    return 0;
}
