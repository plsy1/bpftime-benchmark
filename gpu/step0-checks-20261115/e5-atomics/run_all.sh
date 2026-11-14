#!/bin/bash
# step0 e5 run: the three probe programs (fadd, plain, cas), one run.sh run each,
# on e5_victim (48 x 256 threads, 10 launches), with BPFTIME_CUDA_KEEP_PTX=1 so
# the JIT's PTX for each program is kept; then PTX/SASS extraction and a summary.
# usage: run_all.sh [modes...]   (default: fadd plain cas)
# env:   E5_BLOCKS E5_TPB E5_LAUNCHES (default 48 256 10), TS (results tag)
# Wall: about 30-60 s per mode (agent start-up and the module compile dominate).
set -u
H=/home/jetson/src/bpftime-gpu-work
D=$H/step0/e5-atomics
CUDA=/usr/local/cuda-13.2/bin
MODES=${*:-fadd plain cas}
BL=${E5_BLOCKS:-48}; TPB=${E5_TPB:-256}; LN=${E5_LAUNCHES:-10}
TS=${TS:-$(date +%Y%m%d-%H%M%S)}
O=$D/results/run-$TS
mkdir -p "$O/ptx"
export SPDLOG_LEVEL=info   # run.sh defaults the victim to debug
NAMES=()
cleanup() { for n in "${NAMES[@]}"; do rm -f "/dev/shm/bpftime_gpu_$n"; done; }
trap cleanup EXIT
cd "$D" || exit 2
# always make (incremental): a binary older than its source must not be run
timeout 600 make > "$O/make.log" 2>&1 || { echo "build failed: $O/make.log"; exit 2; }
{ echo "start $(date '+%F %T')"; ls /dev/shm | grep -c . | sed 's/^/dev_shm_entries_before=/'; } > "$O/meta.txt"
for m in $MODES; do
  NAME=step0-e5-$m-$TS; NAMES+=("$NAME")
  echo "== $m ($NAME)"
  SRV_ENV="E5_OUT=$O/$m.json E5_LABEL=$m E5_THREADS=$((BL*TPB)) E5_LAUNCHES=$LN" \
  VIC_ENV="BPFTIME_CUDA_KEEP_PTX=1" \
    timeout -k 5 170 "$H/run.sh" "$NAME" "$D" "./e5_loader e5_$m.bpf.o" "./e5_victim $BL $TPB $LN" 120 60 3
  # server timeout 120 > ready wait (<= 24 s) + victim timeout 60 + drain (3 + <= 10 s):
  # the loader must never be stopped by its own timeout while the victim still runs
  echo "run.sh rc=$? $m" >> "$O/meta.txt"
  R=$H/runs/$NAME
  cp "$R/meta.txt" "$O/$m.runmeta.txt" 2>/dev/null   # client_rc / server_rc for summarize_e5.py
  grep -h '^E5VICTIM' "$R/client.log" | tail -1 | sed 's/^E5VICTIM //' > "$O/$m.victim.json"
  grep -h '^E5RESULT\|^e5 ' "$R/server.log" | tail -2
  grep -h 'E5VICTIM' "$R/client.log" | tail -1
  # kept PTX dirs named in this run's agent log (never anyone else's)
  KEPT=$(grep -ho 'Writing patched PTX to /tmp/bpftime-fatbin-work\.[A-Za-z0-9]*\|Extracted PTX kept in /tmp/bpftime-fatbin-work\.[A-Za-z0-9]*' "$R/client.log" | awk '{print $NF}' | sort -u)
  echo "kept_dirs $m: $KEPT" >> "$O/meta.txt"
  P=""
  for k in $KEPT; do
    for f in "$k"/patched.*.ptx; do
      [ -f "$f" ] && grep -q 'e5step0_atomics_kernel' "$f" && grep -q 'map_info' "$f" && P=$f
    done
  done
  if [ -n "$P" ]; then
    cp "$P" "$O/ptx/$m.patched.ptx"
    python3 -I "$D/extract_probe_ptx.py" "$O/ptx/$m.patched.ptx" __retprobe_func__e5step0_atomics_kernel "$O/ptx/$m" > "$O/ptx/$m.atoms.txt" 2>&1
    # SASS as bpftime builds it: .target rewritten to sm_87, ptxas -O3 (nvPTXCompiler uses the same back end)
    sed '0,/^\.target .*/s//.target sm_87/' "$O/ptx/$m.patched.ptx" > "$O/ptx/$m.sm87.ptx"
    timeout -k 5 120 $CUDA/ptxas -arch=sm_87 -O3 -v "$O/ptx/$m.sm87.ptx" -o "$O/ptx/$m.cubin" > "$O/ptx/$m.ptxas.txt" 2>&1
    timeout -k 5 60 $CUDA/cuobjdump -sass "$O/ptx/$m.cubin" > "$O/ptx/$m.sass" 2>&1
    # ptxas inlines the probe and the helpers into the kernel (one SASS function),
    # so the kernel's section is cut out; the probe's own atomics are those that
    # differ between the three builds (summarize_e5.py), the trampoline's are common
    awk '/Function : /{p = ($0 ~ /Function : e5step0_atomics_kernel$/)} p' "$O/ptx/$m.sass" > "$O/ptx/$m.kernel.sass"
    grep -E 'ATOM|RED\.|MEMBAR|FENCE|CCTL|ERRBAR' "$O/ptx/$m.kernel.sass" > "$O/ptx/$m.kernel.atoms.sass.txt"
  else
    echo "no kept patched PTX with the probe for $m" | tee -a "$O/meta.txt"
  fi
  [ -n "$P" ] && for k in $KEPT; do rm -rf "$k"; done   # kept for inspection when nothing was found
done
echo "end $(date '+%F %T')" >> "$O/meta.txt"
python3 -I "$D/summarize_e5.py" "$O" | tee "$O/summary.txt"
ls /dev/shm | grep '^bpftime_gpu_step0-e5' || echo shm-clean
