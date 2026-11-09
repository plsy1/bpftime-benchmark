#!/bin/bash
# event counts on the late path: fixed build, then with the two event changes reverted, then fixed again
S=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/liu-ev
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
W=/home/jetson/src/bpftime-gpu-work
cd $C || exit 1
restore() { cp $S/backup/nv_attach_impl.cpp $S/backup/nv_attach_impl_frida_setup.cpp attach/nv_attach_impl/; }
trap restore EXIT
build() { timeout 2500 cmake --build build-cuda -j4 --target bpftime-agent bpftime-syscall-server > $S/build-$1.log 2>&1 || { echo "build $1 failed"; exit 1; }; }
run() {
  ( cd $W && export SPDLOG_LEVEL=info SRV_ENV="BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16" NO_AGENT=1 && \
    timeout 300 ./run.sh $1 $W/example/gpu/kernelretsnoop ./kernelretsnoop \
      "env BPFTIME_CUDA_LOAD_SUBSTITUTION=0 LD_PRELOAD=$W/shim/libevent_count.so:$C/build-cuda/runtime/agent/libbpftime-agent.so $W/benchmark/gpu/workload/vec_add_legacy 5 3 5 1" | grep -E "client_rc|server_rc" )
  echo "$1: $(grep -o 'event-shim:.*' $W/runs/$1/client.log) events=$(grep -o 'Total events collected: [0-9]*' $W/runs/$1/server.log | tail -1) $(grep -o 'C\[1\] = [0-9.]*' $W/runs/$1/client.log | tail -1)"
}
build fixed; run liu-ev-fixed
restore
python3 - <<'PY'
root='/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/attach/nv_attach_impl/'
import subprocess
# the two files as in HEAD (45743763) for the event code only: take HEAD's record_patched_launch_event and cu_launch_kernel_common body
head_impl=subprocess.run(['git','-C',root,'show','HEAD:attach/nv_attach_impl/nv_attach_impl.cpp'],capture_output=True,text=True).stdout
head_frida=subprocess.run(['git','-C',root,'show','HEAD:attach/nv_attach_impl/nv_attach_impl_frida_setup.cpp'],capture_output=True,text=True).stdout
def section(s,start,end):
    a=s.index(start); b=s.index(end,a); return s[a:b]
for f,head,start,end in [
  ('nv_attach_impl.cpp',head_impl,'void nv_attach_impl::record_patched_launch_event(CUstream stream)','void nv_attach_impl::wait_for_patched_launch_events('),
  ('nv_attach_impl_frida_setup.cpp',head_frida,'	auto kernel_name = cuda_graph_maybe_get_kernel_name_from_cufunction(\n		*impl, func);\n	if (kernel_name) {','	} else {\n		BPFTIME_LOG_WARN_RATELIMITED(\n			"nv_attach_impl: late path: a cuLaunchKernel')]:
    p=root+f; cur=open(p).read()
    cur=cur.replace(section(cur,start,end),section(head,start,end),1)
    open(p,'w').write(cur)
PY
build reverted; run liu-ev-reverted
restore
build final; echo "restored, agent rebuilt"
