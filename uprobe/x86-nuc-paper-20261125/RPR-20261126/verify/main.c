#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "loops.h"
/* helper mirrors dnew's bpftime_probe_read: frame pointer, sign test, memcpy via PLT */
__attribute__((noinline,used,optimize("no-omit-frame-pointer"))) int64_t helper(uint64_t dst, int64_t size, uint64_t ptr, uint64_t a, uint64_t b)
{ if (__builtin_expect(size<0,0)) return -14; memcpy((void*)dst,(void*)ptr,(size_t)size); return 0; }
static double now(void){struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW,&t); return t.tv_sec*1e9+t.tv_nsec;}
int main(int argc,char**argv){
  static char src[64]="hello world"; static char dst[64];
  long N=argc>1?atol(argv[1]):100000000; int rounds=argc>2?atoi(argv[2]):3;
  int which=argc>3?atoi(argv[3]):-1; /* -1 = all; else index: 0..3 dir, 4..7 ind */
  int offs[]=OFFS; int n=sizeof offs/sizeof offs[0];
  for(int r=0;r<rounds;r++){
    for(int k=0;k<2*n;k++){ if(which>=0&&which!=k) continue;
      lf f = k<n?DIR[k]:IND[k-n];
      f(1000,dst,src);
      double t0=now(); f(N,dst,src); double t1=now();
      printf("%s off=%2d %.3f ns\n",k<n?"direct  ":"indirect",offs[k%n],(t1-t0)/N);
    }
  }
}
