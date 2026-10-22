// Fills keys 0..HASH_SHAPE_FILL-1 (value key*17+5). With HASH_SHAPE_CHURN,
// first runs that many insert/delete cycles of other keys to leave deleted
// buckets behind, then reports the outcome slots on exit.
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "hash_shape.skel.h"
#ifndef HASH_SHAPE_FILL
#define HASH_SHAPE_FILL 1024
#endif
#ifndef HASH_SHAPE_CHURN
#define HASH_SHAPE_CHURN 0
#endif
static volatile sig_atomic_t exiting;
static void stop(int sig) { (void)sig; exiting = 1; }
struct outcome { unsigned long long seed, checksum, hits, misses; };
int main(void)
{
    signal(SIGTERM, stop); signal(SIGINT, stop);
    struct hash_shape_bpf *skel = hash_shape_bpf__open();
    if (!skel) return 1;
    int err = hash_shape_bpf__load(skel);
    if (err) goto done;
    int data = bpf_map__fd(skel->maps.data_map);
    for (unsigned key = 0; key < HASH_SHAPE_FILL; ++key) {
        unsigned long long value = key * 17u + 5u;
        if ((err = bpf_map_update_elem(data, &key, &value, BPF_NOEXIST))) goto done;
    }
    const unsigned spare = 1024 - HASH_SHAPE_FILL;
    for (unsigned cycle = 0; cycle < HASH_SHAPE_CHURN; ++cycle) {
        for (unsigned i = 0; i < spare; ++i) {
            unsigned key = 0x40000000u + cycle * 1024u + i;
            unsigned long long value = 0;
            if ((err = bpf_map_update_elem(data, &key, &value, BPF_NOEXIST))) goto done;
        }
        for (unsigned i = 0; i < spare; ++i) {
            unsigned key = 0x40000000u + cycle * 1024u + i;
            if ((err = bpf_map_delete_elem(data, &key))) goto done;
        }
    }
    for (unsigned key = 0; key < 5; ++key) {
        struct outcome empty = {0};
        if ((err = bpf_map_update_elem(bpf_map__fd(skel->maps.result_map), &key, &empty, BPF_ANY))) goto done;
    }
    if ((err = hash_shape_bpf__attach(skel))) goto done;
    printf("fill=%u churn_cycles=%u\n", HASH_SHAPE_FILL, HASH_SHAPE_CHURN);
    puts("Successfully started!"); fflush(stdout);
    while (!exiting) sleep(1);
    for (unsigned key = 0; key < 5; ++key) {
        struct outcome value = {0};
        if ((err = bpf_map_lookup_elem(bpf_map__fd(skel->maps.result_map), &key, &value))) goto done;
        printf("RESULT slot=%u seed=%llu checksum=%llu hits=%llu misses=%llu\n",
               key, value.seed, value.checksum, value.hits, value.misses);
    }
done:
    if (err) fprintf(stderr, "loader error %d\n", err);
    hash_shape_bpf__destroy(skel);
    return err ? 1 : 0;
}
