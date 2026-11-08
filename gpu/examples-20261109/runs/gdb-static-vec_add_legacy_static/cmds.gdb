set startup-with-shell off
set pagination off
set breakpoint pending on
set environment LD_PRELOAD=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-cuda/runtime/agent/libbpftime-agent.so
set environment BPFTIME_GLOBAL_SHM_NAME=bpftime_gpu_gdbstatic
set environment BPFTIME_LOG_OUTPUT=console
set environment BPFTIME_SM_ARCH=sm_87
set environment SPDLOG_LEVEL=info
break cuda_driver_function__cuLaunchKernel
commands
silent
printf "HIT bpftime driver hook cuLaunchKernel f=%p\n", $x0
set $buf = (const char **)((void *(*)(unsigned long))malloc)(16)
set $r1 = ((int (*)(const char **, void *))cuFuncGetName)($buf, $x0)
printf "  cuFuncGetName -> %d\n", $r1
if $r1 == 0
printf "  name=%s\n", *$buf
end
set $r2 = ((int (*)(const char **, void *))cuKernelGetName)($buf, $x0)
printf "  cuKernelGetName -> %d\n", $r2
if $r2 == 0
printf "  name=%s\n", *$buf
end
bt 8
continue
end
break cuLaunchKernel_ptsz
commands
silent
printf "HIT cuLaunchKernel_ptsz f=%p\n", $x0
bt 6
continue
end
break cuLaunchKernelEx
commands
silent
printf "HIT cuLaunchKernelEx config=%p f=%p\n", $x0, *(void **)$x0
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
