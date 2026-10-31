#define _GNU_SOURCE
#include <errno.h>
#include <linux/bpf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static long bpf(int cmd, union bpf_attr *a){ return syscall(__NR_bpf, cmd, a, sizeof(*a)); }
int main(int argc, char **argv){
  int type = atoi(argv[1]); int nullval = argc > 2;
  union bpf_attr a; memset(&a,0,sizeof a); a.map_type=type; a.key_size=4; a.value_size=8; a.max_entries=4;
  long fd = bpf(BPF_MAP_CREATE,&a); printf("create type %d -> %ld\n", type, fd); fflush(stdout);
  uint32_t k=1; uint64_t v=5;
  memset(&a,0,sizeof a); a.map_fd=fd; a.key=(uintptr_t)&k; a.value=(uintptr_t)&v; bpf(BPF_MAP_UPDATE_ELEM,&a);
  memset(&a,0,sizeof a); a.map_fd=fd;
  if (nullval) { a.key=(uintptr_t)&k; a.value=0; } else { a.key=0; a.value=(uintptr_t)&v; }
  errno=0; long r = bpf(BPF_MAP_LOOKUP_AND_DELETE_ELEM,&a);
  printf("lookup_and_delete %s=NULL -> %ld errno=%d\n", nullval?"value":"key", r, errno);
  return 0; }
