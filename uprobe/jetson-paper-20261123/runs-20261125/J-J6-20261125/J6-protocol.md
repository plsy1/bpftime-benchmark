# J6 working-set pinbench (Orin), exploratory; supports Q6 (pin premium 2.1-2.4x in situ on Orin vs 1.08x on x86)
Build:  gcc -O2 -Wall -o pinbench-ws pinbench-ws.c      (aarch64 native)
Run (under the usual guard: frequency pinned, waitload, nothing else running), on the sniff core of policy M (cpu3):
  ./pinbench-ws 3 5 100000 0,64,128,256,512,1024,2048 single=3 pair=2,3 cross=3,4 all=0-5 > j6.jsonl
Fields per (ws_kb, mask, run): touch_ns (touch only), pin_ns = (touch+pin) - touch, getpid_ns = (touch+getpid) - touch, per iteration.
Masks: single = the traced thread already pinned (pin is cheap path); pair = same cluster; cross = two clusters (cpu3 cluster 0, cpu4 cluster 1); all = the default affinity.
Prediction (hypothesis under test, written before the run): pin_ns rises with ws_kb once ws exceeds the 256 KB per-core L2 and
reaches about 2x its ws=0 value by 512-1024 KB on the Orin; getpid_ns rises much less. If pin_ns is flat in ws, the cache-capacity
explanation is not supported and the paper drops it.
x86 counterpart (NUC, same program): ./pinbench-ws 4 5 100000 0,64,128,256,512,1024,2048,4096 single=4 pair=4,6 all=0-15
