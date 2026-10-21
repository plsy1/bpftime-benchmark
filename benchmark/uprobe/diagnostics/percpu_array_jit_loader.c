#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "percpu_array_jit.skel.h"
static volatile sig_atomic_t exiting;
static void stop(int sig) { (void)sig; exiting=1; }
struct outcome { unsigned long long seed,checksum,hits,misses; };
int main(void)
{
    signal(SIGTERM,stop); signal(SIGINT,stop);
    int cpus=libbpf_num_possible_cpus();
    if(cpus<=0 || cpus!=sysconf(_SC_NPROCESSORS_ONLN)) return 1;
    struct percpu_array_jit_bpf *skel=percpu_array_jit_bpf__open();
    if(!skel) return 1;
    int err=percpu_array_jit_bpf__load(skel);
    unsigned long long *values=calloc(cpus,sizeof(*values));
    struct outcome *out=calloc(cpus,sizeof(*out));
    if(!values || !out) { err=1; goto done; }
    if(err) goto done;
    for(unsigned key=0;key<1024;++key) {
        for(int cpu=0;cpu<cpus;++cpu) values[cpu]=key*17u+5u;
        err=bpf_map_update_elem(bpf_map__fd(skel->maps.data_map),&key,values,BPF_ANY);
        if(err) goto done;
    }
    err=percpu_array_jit_bpf__attach(skel);
    if(err) goto done;
    puts("Successfully started!"); fflush(stdout);
    while(!exiting) sleep(1);
    for(unsigned key=0;key<4;++key) {
        err=bpf_map_lookup_elem(bpf_map__fd(skel->maps.result_map),&key,out);
        if(err) goto done;
        int written=0;
        for(int cpu=0;cpu<cpus;++cpu) {
            if(!out[cpu].hits && !out[cpu].misses) continue;
            ++written;
            printf("RESULT slot=%u seed=%llu checksum=%llu hits=%llu misses=%llu\n",
                key,out[cpu].seed,out[cpu].checksum,out[cpu].hits,out[cpu].misses);
            printf("RESULT_CPU slot=%u cpu=%d\n",key,cpu);
        }
        if(written!=1) { err=1; goto done; }
    }
done:
    free(values); free(out);
    percpu_array_jit_bpf__destroy(skel);
    return err?1:0;
}
