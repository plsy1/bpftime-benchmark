// Multi-thread versions of callloop and mallocloop (THREADS at compile time, thread i pinned to
// CPU 5 - i): every thread runs the single-thread loop on its own data; the main thread samples
// the summed ops every second for DURATION seconds and prints the median of those rates and the
// exact total, as the single-thread workloads do (one op = one target_func call, or one malloc +
// one free). Built as <dir>/t<N>/callloop and mallocloop so the audit harness's tool runners
// (which name these paths) work unchanged.
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef THREADS
#define THREADS 1
#endif
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static int cmp(const void *a, const void *b) { double x = *(double *)a, y = *(double *)b; return (x > y) - (x < y); }
static atomic_ulong ops[THREADS];
static atomic_int stop;
static int size = 64, n = 1024;
volatile unsigned long sink;
__attribute__((noinline)) unsigned long target_func(unsigned long x) {
    unsigned long s = 0;
    for (int i = 0; i < 16; i++) s += (x ^ i) * 2654435761u;
    return s;
}
static void pin(int cpu) { cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu, &s); pthread_setaffinity_np(pthread_self(), sizeof(s), &s); }
static void *worker(void *arg) {
    const int t = (int)(long)arg;
    pin(5 - t);
#ifdef MALLOC
    void **slots = calloc(n, sizeof(void *));
    for (int i = 0; i < n; i++) slots[i] = malloc(size);
    unsigned long idx = 0;
    while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
        for (int j = 0; j < 1000; j++) {
            free(slots[idx]); slots[idx] = malloc(size); memset(slots[idx], 0, 8);
            idx = (idx + 1) % n;
        }
        atomic_fetch_add_explicit(&ops[t], 1000, memory_order_relaxed);
    }
    for (int i = 0; i < n; i++) free(slots[i]);
    free(slots);
#else
    unsigned long i = (unsigned long)t << 40, local = 0;
    while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
        for (int j = 0; j < 1000; j++) local += target_func(i++);
        atomic_fetch_add_explicit(&ops[t], 1000, memory_order_relaxed);
    }
    sink += local;
#endif
    return NULL;
}
static unsigned long sum(void) { unsigned long s = 0; for (int t = 0; t < THREADS; t++) s += atomic_load(&ops[t]); return s; }
int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 10;
    if (argc > 2) size = atoi(argv[2]);
    if (argc > 3) n = atoi(argv[3]);
    pthread_t th[THREADS];
    for (long t = 0; t < THREADS; t++) pthread_create(&th[t], NULL, worker, (void *)t);
    double rates[1000]; int k = 0;
    double mark = now(); unsigned long last = sum();
    while (k < duration) {
        struct timespec ts = { 0, 10 * 1000 * 1000 };
        nanosleep(&ts, NULL);
        double t = now();
        if (t - mark >= 1.0) { unsigned long s = sum(); rates[k++] = (s - last) / (t - mark); last = s; mark = t; }
    }
    atomic_store(&stop, 1);
    for (int t = 0; t < THREADS; t++) pthread_join(th[t], NULL);
    qsort(rates, k, sizeof(double), cmp);
    printf("ops_per_sec_median %.1f\n", rates[k / 2]);
    printf("total_ops %lu\n", sum());
    printf("threads %d\n", THREADS);
    return 0;
}
