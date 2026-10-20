#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define TARGET(name) __attribute__((noinline,noclone)) uint64_t name(uint64_t seed){return seed+1;}
TARGET(__poc_control_hit)
TARGET(__poc_hit)
TARGET(__poc_control_mixed)
TARGET(__poc_mixed)
TARGET(__poc_unused)
typedef uint64_t(*target)(uint64_t);
static uint32_t seed_at(uint64_t i){return (uint32_t)(0x12345678u+i*0x9e3779b9u);}
static uint64_t invoke(target fn,uint64_t count)
{
    volatile uint64_t sink=0;
    for(uint64_t i=0;i<count;++i) sink+=fn(seed_at(i));
    return sink;
}
int main(int argc,char**argv)
{
    uint64_t count=argc>1?strtoull(argv[1],0,10):100000;
    unsigned order=argc>2?strtoul(argv[2],0,10):0;
    struct {const char*name;target fn;} cases[]={
        {"control_hit",__poc_control_hit},{"hit",__poc_hit},
        {"control_mixed",__poc_control_mixed},{"mixed",__poc_mixed},
        {"unused",__poc_unused}};
    if(!count)return 2;
    for(unsigned i=0;i<5;++i)invoke(cases[i].fn,3000);
    puts("case,iterations,total_ns,raw_ns_per_element,last_seed");
    for(unsigned i=0;i<5;++i){
        unsigned j=(i+order)%5;
        struct timespec begin,end;
        clock_gettime(CLOCK_MONOTONIC_RAW,&begin);
        uint64_t sink=invoke(cases[j].fn,count);
        clock_gettime(CLOCK_MONOTONIC_RAW,&end);
        uint64_t elapsed=(uint64_t)(end.tv_sec-begin.tv_sec)*1000000000ull+end.tv_nsec-begin.tv_nsec;
        printf("%s,%llu,%llu,%.9f,%u\n",cases[j].name,(unsigned long long)count,
            (unsigned long long)elapsed,(double)elapsed/count/1000,seed_at(count-1));
        if(!sink)return 3;
    }
    return 0;
}
