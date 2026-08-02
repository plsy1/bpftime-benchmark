#!/usr/bin/env bash
set -euo pipefail

OUT=/home/jetson/src/benchmark-results/uprobe/kernel-map-runtime-arm64-20260803-profile
RAW="$OUT/raw-kernel-disassembly"
IMAGE=/boot/Image
OBJDUMP=/usr/bin/aarch64-linux-gnu-objdump
KALLSYMS=$(mktemp)
trap 'rm -f "$KALLSYMS"' EXIT INT TERM

mkdir -p "$RAW"
sudo -n true
set +o pipefail
strings "$IMAGE" | grep -m1 '^Linux version ' >"$RAW/image-version.txt"
set -o pipefail
sudo -n cp /proc/kallsyms "$KALLSYMS"
sudo -n chown "$(id -u):$(id -g)" "$KALLSYMS"

base=$(awk '$3 == "_text" {print "0x" $1; exit}' "$KALLSYMS")
if [[ -z "$base" ]]; then
    echo "_text was not found in kallsyms" >&2
    exit 1
fi

disassemble_symbol() {
    local symbol=$1
    local occurrence=$2
    local output=$3
    local start
    local stop
    start=$(awk -v symbol="$symbol" -v occurrence="$occurrence" \
        '$3 == symbol && ++seen == occurrence {print $1; exit}' "$KALLSYMS")
    if [[ -z "$start" ]]; then
        echo "symbol was not found: $symbol occurrence $occurrence" >&2
        exit 1
    fi
    stop=$(awk -v start="$start" \
        '$1 == start {found=1; next} found && $1 != start {print $1; exit}' \
        "$KALLSYMS")
    if [[ -z "$stop" ]]; then
        echo "next symbol was not found after $symbol" >&2
        exit 1
    fi
    {
        echo "symbol=$symbol"
        echo "occurrence=$occurrence"
        echo "start=0x$start"
        echo "stop=0x$stop"
        "$OBJDUMP" -D -b binary -m aarch64 --adjust-vma="$base" \
            --start-address="0x$start" --stop-address="0x$stop" "$IMAGE"
    } >"$RAW/$output"
}

disassemble_symbol array_map_update_elem 1 array-map-update-elem.txt
disassemble_symbol bpf_obj_free_fields 1 bpf-obj-free-fields.txt
disassemble_symbol bpf_obj_memcpy 1 bpf-obj-memcpy-1.txt
disassemble_symbol bpf_obj_memcpy 2 bpf-obj-memcpy-2.txt
disassemble_symbol __memcpy 1 memcpy.txt

{
    echo "image=$IMAGE"
    echo "image_sha256=$(sha256sum "$IMAGE" | awk '{print $1}')"
    echo "text_base=$base"
    grep -E ' (array_map_update_elem|bpf_obj_free_fields|bpf_obj_memcpy|__memcpy|memcpy)$' "$KALLSYMS"
} >"$RAW/kernel-symbols.txt"
