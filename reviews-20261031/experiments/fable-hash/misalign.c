#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
int main(int argc,char**argv){ char *buf = aligned_alloc(64,128); uint64_t *p=(uint64_t*)(buf+4); *p=1; 
 uint64_t old=__atomic_fetch_add(p,1,__ATOMIC_SEQ_CST); printf("ok old=%lu new=%lu\n",old,*p); return 0;}
