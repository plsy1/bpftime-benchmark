// N2 multithreaded mallocloop: each worker keeps N/T outstanding blocks of SIZE bytes and replaces the oldest
// (free + malloc) in batches of 1000, so the process holds ~N blocks in total, as the single-thread mallocloop.
#include "mtcommon.h"
static int size = 64, n = 1024;
static void *worker(void *arg) {
    int id = ((struct warg *)arg)->id, m = n / nthreads > 0 ? n / nthreads : 1;
    void **slots = calloc(m, sizeof(void *));
    for (int i = 0; i < m; i++) slots[i] = malloc(size);
    unsigned long idx = 0;
    while (!stop_flag) {
        for (int j = 0; j < 1000; j++) {
            free(slots[idx]); slots[idx] = malloc(size); memset(slots[idx], 0, 8);
            idx = (idx + 1) % m;
        }
        counters[id].ops += 1000;
    }
    return NULL;  // blocks stay outstanding, as in the single-thread workload
}
int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 10;
    if (argc > 2) size = atoi(argv[2]);
    if (argc > 3) n = atoi(argv[3]);
    return run_threads(duration);
}
