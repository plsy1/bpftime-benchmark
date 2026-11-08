#!/bin/bash
# Which launch entry does a static-cudart victim reach under the agent, and can its handle be named?
# Loader kernelretsnoop under the syscall server; victim vec_add_legacy_static 5 3 5 1 inside gdb.
set -u
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-cuda
W=/home/jetson/src/bpftime-gpu-work
V=$W/benchmark/gpu/workload
VIC=${1:-vec_add_legacy_static}
O=$W/runs/gdb-static-$VIC; mkdir -p $O
S=bpftime_gpu_gdbstatic; rm -f /dev/shm/$S
cd $W/example/gpu/kernelretsnoop
export BPFTIME_GLOBAL_SHM_NAME=$S BPFTIME_LOG_OUTPUT=console
timeout -k 5 -s INT 200 env BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16 \
  LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so ./kernelretsnoop > $O/server.log 2>&1 &
SP=$!
for i in $(seq 1 100); do [ -e /dev/shm/$S ] && break; sleep 0.2; done; sleep 3
cat > $O/cmds.gdb <<GDB
set startup-with-shell off
set pagination off
set breakpoint pending on
set environment LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so
set environment BPFTIME_GLOBAL_SHM_NAME=$S
set environment BPFTIME_LOG_OUTPUT=console
set environment BPFTIME_SM_ARCH=sm_87
set environment SPDLOG_LEVEL=info
break cuda_driver_function__cuLaunchKernel
commands
silent
printf "HIT bpftime driver hook cuLaunchKernel f=%p\n", \$x0
set \$buf = (const char **)((void *(*)(unsigned long))malloc)(16)
set \$r1 = ((int (*)(const char **, void *))cuFuncGetName)(\$buf, \$x0)
printf "  cuFuncGetName -> %d\n", \$r1
if \$r1 == 0
printf "  name=%s\n", *\$buf
end
set \$r2 = ((int (*)(const char **, void *))cuKernelGetName)(\$buf, \$x0)
printf "  cuKernelGetName -> %d\n", \$r2
if \$r2 == 0
printf "  name=%s\n", *\$buf
end
bt 8
continue
end
break cuLaunchKernel_ptsz
commands
silent
printf "HIT cuLaunchKernel_ptsz f=%p\n", \$x0
bt 6
continue
end
break cuLaunchKernelEx
commands
silent
printf "HIT cuLaunchKernelEx config=%p f=%p\n", \$x0, *(void **)\$x0
bt 6
continue
end
break cuLaunchKernelEx_ptsz
commands
silent
printf "HIT cuLaunchKernelEx_ptsz\n"
bt 6
continue
end
run
GDB
timeout -k 5 150 gdb -batch -nx -x $O/cmds.gdb --args $V/$VIC 5 3 5 1 > $O/gdb.log 2>&1
echo "gdb_rc=$?"
kill -INT $SP 2>/dev/null; wait $SP; echo "server_rc=$?"
rm -f /dev/shm/$S
pgrep -a -x "kernelretsnoop|$VIC" || echo no-leftovers
grep -E "HIT|cuFuncGetName|cuKernelGetName|name=|Validation|Median" $O/gdb.log | head -40
