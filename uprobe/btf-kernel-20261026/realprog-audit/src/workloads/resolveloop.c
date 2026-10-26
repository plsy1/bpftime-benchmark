// gethostlatency target: getaddrinfo("localhost") in a loop for DURATION seconds; prints the
// median lookups/s. /etc/hosts answers it, so no network traffic is involved.
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static int cmp(const void *a, const void *b) { double x = *(double *)a, y = *(double *)b; return (x > y) - (x < y); }
int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 10;
    double rates[1000]; int k = 0; unsigned long ops = 0, total_ops = 0;
    struct addrinfo hints; memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET;
    double mark = now();
    while (k < duration) {
        for (int j = 0; j < 100; j++) {
            struct addrinfo *res = NULL;
            if (getaddrinfo("localhost", NULL, &hints, &res) == 0) freeaddrinfo(res);
        }
        ops += 100; total_ops += 100;
        double t = now();
        if (t - mark >= 1.0) { rates[k++] = ops / (t - mark); ops = 0; mark = t; }
    }
    qsort(rates, k, sizeof(double), cmp);
    printf("ops_per_sec_median %.1f\n", rates[k / 2]);
    printf("total_ops %lu\n", total_ops);
    return 0;
}
