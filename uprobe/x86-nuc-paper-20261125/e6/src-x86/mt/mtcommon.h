// N2 (x86, 2026-11-26): multithreaded versions of the E6 workloads. The thread count and placement come from the
// process's CPU affinity: one worker per CPU of the mask it starts with (audit.py runs workloads under
// `taskset -c $E6_WL_CPUS`, memleak's command via the static wrapper's SW_CPUS), each worker pinned to its CPU in
// mask order. Same output as the single-thread workloads: the median of the per-second aggregate rates over
// DURATION seconds ("ops_per_sec_median", all threads together) and the exact "total_ops"; plus "threads N cpus ...".
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define MAXT 64
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static int cmp(const void *a, const void *b) { double x = *(double *)a, y = *(double *)b; return (x > y) - (x < y); }
struct slot { volatile unsigned long ops; char pad[56]; } __attribute__((aligned(64)));
static struct slot counters[MAXT];
static volatile int stop_flag;
static int cpus[MAXT], nthreads;
static void *worker(void *arg);
struct warg { int id; };
static void pin(int cpu) { cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu, &s); pthread_setaffinity_np(pthread_self(), sizeof s, &s); }
static void *trampoline(void *a) { struct warg *w = a; pin(cpus[w->id]); return worker(a); }
static int run_threads(int duration) {
    cpu_set_t s; sched_getaffinity(0, sizeof s, &s);
    for (int c = 0; c < CPU_SETSIZE && nthreads < MAXT; c++) if (CPU_ISSET(c, &s)) cpus[nthreads++] = c;
    pthread_t th[MAXT]; struct warg wa[MAXT];
    for (int i = 0; i < nthreads; i++) { wa[i].id = i; pthread_create(&th[i], NULL, trampoline, &wa[i]); }
    double rates[1000]; int k = 0; unsigned long last = 0;
    double mark = now();
    while (k < duration) {
        usleep(10000);
        double t = now();
        if (t - mark >= 1.0) {
            unsigned long sum = 0; for (int i = 0; i < nthreads; i++) sum += counters[i].ops;
            rates[k++] = (sum - last) / (t - mark); last = sum; mark = t;
        }
    }
    stop_flag = 1;
    for (int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    unsigned long total = 0; for (int i = 0; i < nthreads; i++) total += counters[i].ops;
    qsort(rates, k, sizeof(double), cmp);
    printf("threads %d cpus", nthreads); for (int i = 0; i < nthreads; i++) printf(" %d", cpus[i]); printf("\n");
    printf("ops_per_sec_median %.1f\n", rates[k / 2]);
    printf("total_ops %lu\n", total);
    return 0;
}
