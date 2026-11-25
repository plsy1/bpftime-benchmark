#include <stdio.h>
#include <stdlib.h>
#include <time.h>
void CP_dir(long),CP_ind(long),CM_ind(long);
static double now(void){struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW,&t); return t.tv_sec*1e9+t.tv_nsec;}
int main(int c,char**v){ long N=atol(v[1]); void(*f[])(long)={CP_dir,CP_ind,CM_ind}; const char*nm[]={"call(no ret) direct","call(no ret) indirect","call *mem + ret"};
 for(int r=0;r<3;r++) for(int k=0;k<3;k++){ f[k](1000); double t0=now(); f[k](N); double t1=now(); printf("%-22s %.3f ns\n",nm[k],(t1-t0)/N);} }
