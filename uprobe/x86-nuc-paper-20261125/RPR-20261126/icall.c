#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
__attribute__((noinline)) int64_t helper(uint64_t dst, int64_t size, uint64_t ptr, uint64_t a, uint64_t b)
{ memcpy((void*)dst, (void*)ptr, (size_t)size); return 0; }
static double now(void){struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW,&t); return t.tv_sec*1e9+t.tv_nsec;}
int main(void){
  char src[16]="hello world", dst[16];
  const long N=200000000;
  for (int round=0; round<5; round++){
    double t0=now();
    for (long i=0;i<N;i++){ asm volatile("" ::: "memory"); helper((uint64_t)dst,5,(uint64_t)src,0,0);}
    double t1=now();
    for (long i=0;i<N;i++){
      int64_t (*fp)(uint64_t,int64_t,uint64_t,uint64_t,uint64_t)=helper;
      asm volatile("" : "+r"(fp) :: "memory");   /* force an indirect call through a register */
      fp((uint64_t)dst,5,(uint64_t)src,0,0);}
    double t2=now();
    printf("direct %.3f ns  indirect %.3f ns  diff %.3f\n",(t1-t0)/N,(t2-t1)/N,((t2-t1)-(t1-t0))/N);
  }
}
