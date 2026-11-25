#include <stdio.h>
#include <stdlib.h>
#include <time.h>
void C_dir(long),C_ind(long),J_dir(long),J_ind(long);
static double now(void){struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW,&t); return t.tv_sec*1e9+t.tv_nsec;}
int main(int c,char**v){ long N=atol(v[1]); void(*f[])(long)={C_dir,C_ind,J_dir,J_ind}; const char*nm[]={"call-ret direct","call-ret indirect","jmp direct","jmp indirect"};
 for(int r=0;r<3;r++) for(int k=0;k<4;k++){ f[k](1000); double t0=now(); f[k](N); double t1=now(); printf("%-18s %.3f ns\n",nm[k],(t1-t0)/N);} }
