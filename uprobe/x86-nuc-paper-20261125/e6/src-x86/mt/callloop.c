// N2 multithreaded callloop: each worker calls the noinline target_func (same body as the single-thread
// callloop) in batches of 1000 and adds them to its own counter. See mtcommon.h.
#include "mtcommon.h"
volatile unsigned long sink[MAXT * 8];
__attribute__((noinline)) unsigned long target_func(unsigned long x) {
    unsigned long s = 0;
    for (int i = 0; i < 16; i++) s += (x ^ i) * 2654435761u;
    return s;
}
static void *worker(void *arg) {
    int id = ((struct warg *)arg)->id; unsigned long i = 0, acc = 0;
    while (!stop_flag) {
        for (int j = 0; j < 1000; j++) acc += target_func(i++);
        counters[id].ops += 1000;
    }
    sink[id * 8] = acc;
    return NULL;
}
int main(int argc, char **argv) { return run_threads(argc > 1 ? atoi(argv[1]) : 10); }
