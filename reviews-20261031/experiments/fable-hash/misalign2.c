#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
int main(int argc,char**argv){ char *buf = aligned_alloc(64,128); int off=atoi(argv[1]); uint64_t *p=(uint64_t*)(buf+off); *p=1;
 uint64_t old=__atomic_fetch_add(p,1,__ATOMIC_SEQ_CST); printf("off=%d ok old=%lu new=%lu\n",off,old,*p); return 0;}
