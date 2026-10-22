// Inserts keys 0..511 (value key*17+5), pre-creates the outcome slots, and
// on exit prints them and checks that the data map still holds keys 0..511.
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "hash_mutate.skel.h"
static volatile sig_atomic_t exiting;
static void stop(int sig) { (void)sig; exiting = 1; }
struct outcome { unsigned long long seed, checksum, ok, failed; };
int main(void)
{
    signal(SIGTERM, stop); signal(SIGINT, stop);
    struct hash_mutate_bpf *skel = hash_mutate_bpf__open();
    if (!skel) return 1;
    int err = hash_mutate_bpf__load(skel);
    if (err) goto done;
    int data = bpf_map__fd(skel->maps.data_map);
    for (unsigned key = 0; key < 512; ++key) {
        unsigned long long value = key * 17u + 5u;
        if ((err = bpf_map_update_elem(data, &key, &value, BPF_NOEXIST))) goto done;
    }
    for (unsigned key = 0; key < 3; ++key) {
        struct outcome empty = {0};
        if ((err = bpf_map_update_elem(bpf_map__fd(skel->maps.result_map), &key, &empty, BPF_ANY))) goto done;
    }
    if ((err = hash_mutate_bpf__attach(skel))) goto done;
    puts("Successfully started!"); fflush(stdout);
    while (!exiting) sleep(1);
    for (unsigned key = 0; key < 3; ++key) {
        struct outcome value = {0};
        if ((err = bpf_map_lookup_elem(bpf_map__fd(skel->maps.result_map), &key, &value))) goto done;
        printf("RESULT slot=%u seed=%llu checksum=%llu hits=%llu misses=%llu\n",
               key, value.seed, value.checksum, value.ok, value.failed);
    }
    unsigned intact = 0;
    for (unsigned key = 0; key < 1024; ++key) {
        unsigned long long value = 0;
        const int found = !bpf_map_lookup_elem(data, &key, &value);
        if (key < 512) {
            if (found && value == key * 17u + 5u) ++intact;
        } else if (found) {
            err = -1;
            fprintf(stderr, "unexpected key %u\n", key);
        }
    }
    printf("INTACT %u\n", intact);
done:
    if (err) fprintf(stderr, "loader error %d\n", err);
    hash_mutate_bpf__destroy(skel);
    return err ? 1 : 0;
}
