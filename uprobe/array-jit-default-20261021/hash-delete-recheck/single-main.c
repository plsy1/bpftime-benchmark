#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint64_t (*benchmark_test_function_t)(char *,int,uint64_t);
extern uint64_t __setup_hash_map_delete(char *,int,uint64_t);
extern uint64_t __bench_hash_map_delete(char *,int,uint64_t);
extern void do_benchmark_userspace_with_setup(benchmark_test_function_t,
    benchmark_test_function_t,const char *,int,int);

int main(int argc,char **argv)
{
    int iterations=argc>1?atoi(argv[1]):10000;
    int warmup=argc>2?atoi(argv[2]):1000;
    if(iterations<=0||warmup<0)return 2;
    char buffer[20]="hello world";
    for(int i=0;i<warmup;++i){
        __setup_hash_map_delete(buffer,i%4,i);
        __bench_hash_map_delete(buffer,i%4,i);
    }
    printf("Single ordinary hash delete; warmup=%d; 1000 keys per invocation\n",warmup);
    do_benchmark_userspace_with_setup(__setup_hash_map_delete,
        __bench_hash_map_delete,"__bench_hash_map_delete",iterations,1);
    return 0;
}
