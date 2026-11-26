# J6 v2 working-set pinbench (Orin), exploratory; NUC request 11-25/26
Source: handoff-J6v2-20261125.tar.gz (sha256 f4e04b8878d10703...), pinbench-ws.c v2 (677c5274...), adds pin_direct_ns and
getpid_direct_ns (per-call timing minus one clock pair), because v1's (touch+call) - touch is swamped by touch variance above L2.
Build: gcc -O2 -Wall -o pinbench-ws pinbench-ws.c (aarch64 native, same as v1).
Run on the sniff core cpu3 under the usual guard (waitload, timers paused, nothing else measuring):
  ./pinbench-ws 3 5 100000 0,128,256,512,1024,2048,4096 single=3 pair=2,3 all=0-5 > j6v2.jsonl
Masks per NUC: single/pair/all (v1's cross dropped; pair = 2,3 as in v1). ws list per NUC: 0,128,256,512,1024,2048,4096 KB.
Orin caches: 256 KB L2 per core, 2 MB L3 shared (cluster cpu0-3), so knees are expected near 256 KB and 2 MB.
Output: L2 knee for NUC (x86: pin flat to 1 MB, +45-70 % at >= 2 MB, after its 1.25 MB L2).
