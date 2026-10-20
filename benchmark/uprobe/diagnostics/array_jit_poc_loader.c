#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "array_jit_poc.skel.h"
static volatile sig_atomic_t exiting;
static void stop(int sig){(void)sig;exiting=1;}
struct outcome {unsigned long long seed,checksum,hits,misses;};
int main(void)
{
    signal(SIGTERM,stop);signal(SIGINT,stop);
    struct array_jit_poc_bpf *skel=array_jit_poc_bpf__open();
    if(!skel)return 1;
    int err=array_jit_poc_bpf__load(skel);
    if(err)goto done;
    for(unsigned key=0;key<1024;++key){
        unsigned long long value=key*17u+5u;
        err=bpf_map_update_elem(bpf_map__fd(skel->maps.data_map),&key,&value,BPF_ANY);
        if(err)goto done;
    }
    err=array_jit_poc_bpf__attach(skel);
    if(err)goto done;
    puts("Successfully started!");fflush(stdout);
    while(!exiting)sleep(1);
    for(unsigned key=0;key<4;++key){
        struct outcome value={0};
        err=bpf_map_lookup_elem(bpf_map__fd(skel->maps.result_map),&key,&value);
        if(err)goto done;
        printf("RESULT slot=%u seed=%llu checksum=%llu hits=%llu misses=%llu\n",
            key,value.seed,value.checksum,value.hits,value.misses);
    }
done:
    array_jit_poc_bpf__destroy(skel);
    return err?1:0;
}
