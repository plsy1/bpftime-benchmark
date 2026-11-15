#!/bin/bash
# build_bpf.sh OUTDIR: the eBPF objects of C1 (clang-15, as the bpftime
# examples: -g -O2 -target bpf, no -mcpu). CPU only.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
OUT=$1
mkdir -p "$OUT"
W=/home/jetson/src/bpftime-gpu-work
INC=(-I"$W/example/gpu/threadhist-llama/.output" -I"$W/third_party/libbpf/include/uapi" -I"$W/third_party/vmlinux/arm64")
mapfile -t SYS < <(clang-15 -v -E - </dev/null 2>&1 | sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter\n\1|p }')
CF=(-g -O2 -Wall -Wno-unused-variable -Wno-unused-function -target bpf -D__TARGET_ARCH_arm64 "${INC[@]}" "${SYS[@]}")
# one SALT per kernel family (bit 63 set, so id ^ SALT is never 0)
declare -A SALTS=([kmr]=0x8000c1c1a0000000 [kdiv]=0x8000c1c1b0000000 [kspin]=0x8000c1c1c0000000 \
	[ks2a]=0x8000c1c1d0000000 [ks2b]=0x8000c1c1e0000000)
for f in "${!SALTS[@]}"; do
	clang-15 "${CF[@]}" -DPROG_CHK -DSALT="${SALTS[$f]}ull" -c "$D/src/probes.bpf.c" -o "$OUT/chk_$f.bpf.o"
done
clang-15 "${CF[@]}" -DPROG_COST -c "$D/src/probes.bpf.c" -o "$OUT/cost.bpf.o"
clang-15 "${CF[@]}" -DPROG_COST -DCOST_BAD -c "$D/src/probes.bpf.c" -o "$OUT/cost_bad.bpf.o"
clang-15 "${CF[@]}" -DPROG_VALUE -c "$D/src/probes.bpf.c" -o "$OUT/value.bpf.o"
clang-15 "${CF[@]}" -DPROG_EMPTY -c "$D/src/probes.bpf.c" -o "$OUT/empty.bpf.o"
for o in "$OUT"/*.bpf.o; do llvm-objdump-15 -dr "$o" > "${o%.o}.dis.txt"; done
echo "bpf objects: $(ls "$OUT"/*.bpf.o | wc -l)"
