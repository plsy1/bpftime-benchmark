#!/usr/bin/env bash
# Compile the survey corpus with BPF clang 15 (same as the benchmark build).
# Sources are read in place; nothing is built or run inside the source trees.
set -uo pipefail
OUT=/home/jetson/src/results/uprobe/jit-eligibility-survey-20261021
BT=/home/jetson/src/bpftime-baseline-177a1dc-20261021
BCC=/home/jetson/src/survey-sources/bcc-v0.37.0/libbpf-tools
LIBBPF="$BT/benchmark/uprobe/.output"
UAPI="$BT/third_party/libbpf/include/uapi"
SYS=$(/usr/bin/clang-15 -v -E - </dev/null 2>&1 | sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)| -idirafter \1|p }' | tr -d '\n')
mkdir -p "$OUT/objs/bpftime" "$OUT/objs/libbpf-tools"
: >"$OUT/compile-status.tsv"
compile() { # corpus name src vmlinux-dir extra-includes...
    local corpus=$1 name=$2 src=$3 vml=$4; shift 4
    local obj="$OUT/objs/$corpus/$name.bpf.o"
    if /usr/bin/clang-15 -g -O2 -Wall -target bpf -D__TARGET_ARCH_arm64 \
        -I"$(dirname "$src")" "$@" -I"$LIBBPF" -I"$UAPI" -I"$vml" $SYS \
        -c "$src" -o "$obj" >"$OUT/objs/$corpus/$name.log" 2>&1; then
        printf '%s\t%s\tok\t%s\n' "$corpus" "$name" "$src" >>"$OUT/compile-status.tsv"
    else
        printf '%s\t%s\tfailed\t%s\n' "$corpus" "$name" "$src" >>"$OUT/compile-status.tsv"
        rm -f "$obj"
    fi
}
while read -r src; do
    rel=${src#$BT/example/}
    compile bpftime "$(echo "${rel%.bpf.c}" | tr / _)" "$src" "$BT/third_party/vmlinux/arm64"
done < <(find "$BT/example" -name '*.bpf.c' | sort)
for src in "$BCC"/*.bpf.c; do
    name=$(basename "$src" .bpf.c)
    compile libbpf-tools "$name" "$src" "$BCC/arm64" -I"$BCC"
done
awk -F'\t' '{n[$1" "$3]++} END {for (k in n) print k, n[k]}' "$OUT/compile-status.tsv" | sort
