#!/bin/bash
# step0 e8, part 2 (no bpftime, no agent): load the patched rms_norm module with
# an appended .global of each size, find it, write and read it; then the
# constant-bank budget. One process per case (e8_load), each under a 90 s timeout.
# usage: run_all.sh [PATCHED_PTX]   (default: the newest ptx/rmsnorm-module-*.patched.ptx;
#        get_llama_ptx.sh makes one)
# Wall: ~40-80 s (each case compiles the module once, ~1 s; 1 GiB memset+readback a few s).
set -u
D=/home/jetson/src/bpftime-gpu-work/step0/e8-package
CUDA=/usr/local/cuda-13.2/bin
cd "$D" || exit 2
P=${1:-$(ls -t ptx/rmsnorm-module-*.patched.ptx 2>/dev/null | head -1)}
[ -f "$P" ] || { echo "no patched PTX: run get_llama_ptx.sh first"; exit 2; }
timeout 300 make > /dev/null 2>&1 || { echo "build failed"; exit 2; }   # incremental; never run a stale binary
TS=${TS:-$(date +%Y%m%d-%H%M%S)}
O=$D/results/run-$TS
mkdir -p "$O"
J=$O/results.jsonl
: > "$J"
avail_mib() { awk '/^MemAvailable:/{print int($2/1024)}' /proc/meminfo; }
one() { # label, args...
  local label=$1; shift
  echo "== $label: $* (MemAvailable $(avail_mib) MiB)" | tee -a "$O/log.txt"
  timeout -k 5 90 ./e8_load "$@" > "$O/$label.out" 2>&1
  local rc=$?
  local line=$(grep '^E8RESULT ' "$O/$label.out" | sed 's/^E8RESULT //')
  [ -z "$line" ] && line="{\"error\":\"no result\",\"rc\":$rc}"
  echo "{\"label\":\"$label\",\"rc\":$rc,\"avail_mib_before\":$(avail_mib),\"r\":$line}" >> "$J"
  tail -c 400 "$O/$label.out" | tee -a "$O/log.txt"; echo | tee -a "$O/log.txt"
}
echo "ptx=$P start $(date '+%F %T')" > "$O/meta.txt"
free -m >> "$O/meta.txt"
# baseline: the module as bpftime loads it, nothing appended
one base "$P" base 0 --info-log "$O/base.info.txt"
# appended .global at the sizes D and B need (288 KiB per module, 2.8 MiB for 10
# modules, then 32 MiB, 256 MiB, 1 GiB)
for N in 294912 2936016 33554432 268435456 1073741824; do
  need=$(( N / 1048576 + 1024 ))
  if [ "$(avail_mib)" -lt "$need" ]; then
    echo "skip N=$N: MemAvailable $(avail_mib) MiB < $need MiB" | tee -a "$O/log.txt"
    echo "{\"label\":\"skip-$N\",\"skipped\":true,\"N\":$N,\"avail_mib_before\":$(avail_mib)}" >> "$J"
    continue
  fi
  for m in plain visible kernel; do one "$m-$N" "$P" $m $N; done
done
# the driver-JIT path once (PTX text to cuLibraryLoadData)
one "jit-kernel-33554432" "$P" kernel 33554432 --jit
# constant bank 3: what the module uses, then a .const of exactly the rest and 8 B more
C3=$(grep -ao '[0-9]* bytes cmem\[3\]' "$O/base.info.txt" | head -1 | awk '{print $1}')
if [ -n "$C3" ]; then
  R=$(( 65536 - C3 ))
  one "constpad-$R" "$P" constpad $R
  one "constpad-$((R+8))" "$P" constpad $((R+8))
fi
# resource usage of the base cubin as bpftime would load it
timeout -k 5 60 $CUDA/cuobjdump -res-usage "$O/base.info.txt.cubin" > "$O/base.res-usage.txt" 2>&1
timeout -k 5 60 $CUDA/cuobjdump -elf "$O/base.info.txt.cubin" 2>/dev/null | grep -E '(PROGBITS|NOBITS|REL) .*\.nv\.(constant[2-4]|global)|\.rel\.nv\.constant4' | sort -u > "$O/base.elf-sections.txt"
echo "end $(date '+%F %T')" >> "$O/meta.txt"
python3 -I "$D/summarize_e8.py" "$O" | tee "$O/summary.txt"
