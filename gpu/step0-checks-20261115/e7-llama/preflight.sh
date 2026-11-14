#!/bin/bash
# step0 preflight for the instrumentation build (e7-llama, e9-hooks).
# Exit 0 only when every required check passes; prints one line per check.
# Read-only: no GPU, no build, no clocks changed. Wall: ~5 s.
set -u
X=/home/jetson/src/step0-bpftime-exp
B=$X/build-cuda
fail=0
ok() { echo "ok   $*"; }
bad() { echo "FAIL $*"; fail=1; }
info() { echo "info $*"; }

# 1. the background build is over and succeeded
if [ -f $X/build.pid ] && kill -0 "$(cat $X/build.pid)" 2>/dev/null; then
  bad "build still running (pid $(cat $X/build.pid)); tail $X/build.log"
else
  ok "no build running"
fi
if grep -qE "error:|Error [0-9]|\*\*\*" $X/build.log 2>/dev/null; then
  bad "build.log has errors: $(grep -m1 -E 'error:|Error [0-9]|\*\*\*' $X/build.log)"
else
  ok "build.log has no errors"
fi
for t in bpftime-agent bpftime-syscall-server ptxpass_kprobe_entry ptxpass_kretprobe ptxpass_kprobe_memcapture nv_attach_impl_ptx_compiler; do
  grep -q "Built target $t\$" $X/build.log 2>/dev/null && ok "built target $t" || bad "target $t not reported built in build.log"
done
# 2. the six libraries exist and are newer than the instrumentation commit
ct=$(git -C $X log -1 --format=%ct 2>/dev/null || echo 0)
for f in runtime/agent/libbpftime-agent.so runtime/syscall-server/libbpftime-syscall-server.so \
         attach/nv_attach_impl/pass/ptxpass_kprobe_entry/libptxpass_kprobe_entry.so \
         attach/nv_attach_impl/pass/ptxpass_kretprobe/libptxpass_kretprobe.so \
         attach/nv_attach_impl/pass/ptxpass_kprobe_memcapture/libptxpass_kprobe_memcapture.so \
         attach/nv_attach_impl/ptx_compiler/libnv_attach_impl_ptx_compiler.so; do
  if [ -f $B/$f ] && [ "$(stat -c %Y $B/$f)" -ge "$ct" ]; then ok "$f"; else bad "$f missing or older than the commit"; fi
done
# 3. the instrumentation is in the binaries, and the agent loads this build's passes
A=$B/runtime/agent/libbpftime-agent.so
if [ -f $A ]; then
  strings $A | grep -q "_bpf_helper_ext_0520" && ok "agent trampoline has _bpf_helper_ext_0520" || bad "agent trampoline lacks helper 520"
  strings $A | grep -q "BPFTIME_EXP_GEOMETRY_LOG" && ok "agent has the geometry log" || bad "agent lacks BPFTIME_EXP_GEOMETRY_LOG"
  p=$(strings $A | grep -m1 "libptxpass_kretprobe.so")
  case "$p" in *"$B/attach/nv_attach_impl/pass/ptxpass_kretprobe/libptxpass_kretprobe.so"*) ok "agent pass paths point into $B";;
    *) bad "agent pass paths do not point into $B: $p";; esac
  p=$(strings $A | grep -m1 "libnv_attach_impl_ptx_compiler.so")
  case "$p" in "$B/"*) ok "agent PTX compiler path points into $B";; *) bad "agent PTX compiler path: $p";; esac
fi
K=$B/attach/nv_attach_impl/pass/ptxpass_kretprobe/libptxpass_kretprobe.so
[ -f $K ] && { strings $K | grep -q "get_active_mask" && ok "kretprobe pass registers helper 520" || bad "kretprobe pass lacks get_active_mask (520 unregistered: the application would exit)"; }
# 4. the source state
h=$(git -C $X log -1 --format='%h %s' 2>/dev/null)
case "$h" in *"step0: instrumentation helpers 520-523"*) ok "HEAD $h";; *) bad "unexpected HEAD: $h";; esac
[ -z "$(git -C $X status --porcelain --untracked-files=no 2>/dev/null)" ] && ok "worktree clean (tracked files)" || bad "worktree has tracked changes"
# 5. inputs
for f in /home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench /home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf \
         /home/jetson/src/bpftime-gpu-work/run.sh /home/jetson/src/bpftime-gpu-work/llama/census/pp512-graph.sqlite /home/jetson/src/bpftime-gpu-work/llama/census/tg128-graph.sqlite; do
  [ -e $f ] && ok "exists $f" || bad "missing $f"
done
# 6. nothing left over from earlier runs
pgrep -a -x 'llama-bench|e7_loader|e7_ctrl|threadhist|cuda_probe|vec_add_graph|vec_add_graph_l' && bad "leftover processes above" || ok "no leftover llama-bench/e7/threadhist/cudagraph processes"
l=$(ls /dev/shm | grep -E '^(step0_|bpftime_gpu_step0-e7|bpftime_gpu_step0-e9)' || true)
[ -z "$l" ] && ok "no e7/e9 shm segments" || bad "leftover shm: $l"
o=$(ls /dev/shm | grep -E '^bpftime_gpu_' | grep -vE '^bpftime_gpu_step0-e[79]' | tr '\n' ' ')
[ -n "$o" ] && info "other bpftime run segments present (not ours, 128 MiB each by default): $o"
[ -n "${GGML_CUDA_DISABLE_GRAPHS+x}" ] && bad "GGML_CUDA_DISABLE_GRAPHS is set (graphs would be off)" || ok "GGML_CUDA_DISABLE_GRAPHS unset (graphs on)"
# 7. memory (llama-bench + agent need ~2 GiB)
av=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)
[ "$av" -ge 2500 ] && ok "MemAvailable ${av} MiB" || bad "MemAvailable ${av} MiB < 2500"
# informational: clocks (count runs, not timed)
info "cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null) $(date '+%F %T')"
[ $fail = 0 ] && echo "PREFLIGHT PASS" || echo "PREFLIGHT FAIL"
exit $fail
