#include <stdio.h>
__attribute__((noinline)) int traced_fn(int x){ asm volatile(""); return x+1; }
int main(void){ int s=0; for(int i=0;i<1000;i++) s=traced_fn(s); printf("%d\n",s); return 0; }
