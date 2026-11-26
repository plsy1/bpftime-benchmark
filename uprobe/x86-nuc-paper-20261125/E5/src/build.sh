#!/usr/bin/env bash
# Build E5: BPF object, loader (static libbpf from the official tree's sslsniff build output,
# read only), target. Run under the measurement lock:
#   flock -n /home/y/src/bpftime-x86-runs/paper-20261124/.measure.lock ./build.sh
# "./build.sh target" rebuilds only e5target and replaces only its line in sha256.txt, so the
# kernel loader and the BPF object (and their recorded hashes) stay byte-identical across the
# target change that added W7t/W5r (P5a). The previous target is kept as e5target.prev.
set -euo pipefail
cd "$(dirname "$0")"
LIBBPF_OUT=/home/y/src/bpftime-paper/official-4708f409/example/tracing/sslsniff-paper-s4/.output
VMLINUX_DIR=/home/y/src/bpftime-paper/official-4708f409/third_party/vmlinux/x86
CLANG=${CLANG:-clang-19}
if [ "${1:-}" = target ]; then
	[ -e e5target ] && cp -p e5target e5target.prev
	gcc -O2 -g -Wall -Wextra -pthread e5target.c -o e5target
	new=$(sha256sum e5target)
	grep -v '  e5target$' sha256.txt >sha256.txt.tmp || true
	echo "$new" >>sha256.txt.tmp
	mv sha256.txt.tmp sha256.txt
	{ $CLANG --version | head -1; gcc --version | head -1; ldd --version | head -1; } >toolchain.txt
	cat sha256.txt
	exit 0
fi
$CLANG -g -O2 -target bpf -D__TARGET_ARCH_x86 -I"$LIBBPF_OUT" -I"$VMLINUX_DIR" -c e5audit.bpf.c -o e5audit.bpf.o
gcc -O2 -g -Wall -I"$LIBBPF_OUT" e5tool.c "$LIBBPF_OUT/libbpf.a" -lelf -lz -o e5tool
gcc -O2 -g -Wall -Wextra -pthread e5target.c -o e5target
sha256sum e5audit.bpf.o e5tool e5target >sha256.txt
{ $CLANG --version | head -1; gcc --version | head -1; ldd --version | head -1; } >toolchain.txt
cat sha256.txt
