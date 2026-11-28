#!/bin/bash
# Builds the hash-map variants of pt_hash.bpf.c (2026-11-26). Same flags as
# the Makefile's BPFFLAGS. Names: bpf/hs_<op>_<rate>_<key>_<set>.bpf.o
set -eu
cd "$(dirname "$0")"
W=/home/jetson/src/bpftime-gpu-work
FLAGS="-g -O2 -target bpf -D__TARGET_ARCH_arm64 -I$W/step0/e7-llama/.libbpf -I$W/third_party/libbpf/include/uapi -I$W/third_party/vmlinux/arm64 -I. -idirafter /usr/lib/llvm-15/lib/clang/15.0.7/include -idirafter /usr/local/include -idirafter /usr/include/aarch64-linux-gnu -idirafter /usr/include"
mk() { # op rate key set [extra flags]
	local o=bpf/hs_$(echo $1 | tr A-Z a-z)_$(echo $2 | tr A-Z a-z)_$(echo $3 | tr A-Z a-z)_$(echo $4 | tr A-Z a-z).bpf.o
	clang-15 $FLAGS -DPT_SET_$4 -DHS_OP=HS_OP_$1 -DHS_RATE=HS_RATE_$2 -DHS_KEY=HS_KEY_$3 -c pt_hash.bpf.c -o $o
	echo $o
}
# the device hash map (1533, -DHS_DEV): bpf/hd_<op>_<rate>_<key>_<set>.bpf.o
mkd() { # op rate key set
	local o=bpf/hd_$(echo $1 | tr A-Z a-z)_$(echo $2 | tr A-Z a-z)_$(echo $3 | tr A-Z a-z)_$(echo $4 | tr A-Z a-z).bpf.o
	clang-15 $FLAGS -DHS_DEV -DPT_SET_$4 -DHS_OP=HS_OP_$1 -DHS_RATE=HS_RATE_$2 -DHS_KEY=HS_KEY_$3 -c pt_hash.bpf.c -o $o
	echo $o
}
mkdir -p bpf
for op in PRE MISS UPD DEL TID TIDND CNT RMW; do mk $op THREAD KC TEST; done
mk CNT THREAD KSM TEST
mk CNT BLOCK KBLK TEST
mk CNT BLOCK KC TEST
mk CNT LEADER KC TEST
mk PRE THREAD KC RMS
mk CNT BLOCK KC RMS
mk CNT LEADER KC LLAMA4
mk NONE LEADER KC LLAMA4
# 1533 (2026-11-28): the victim matrix's CNT/PRE/MISS, and llama.cpp's
for op in PRE MISS CNT; do mkd $op THREAD KC TEST; done
mkd CNT THREAD KSM TEST
mkd CNT BLOCK KBLK TEST
mkd CNT LEADER KC TEST
mkd PRE THREAD KC RMS
mkd CNT BLOCK KC RMS
mkd CNT LEADER KC LLAMA4
# S8 (2026-11-28): the per-block counter on one key, and all rows live for the delivery cost
mkd CNT BLOCK KC TEST
for n in 512 1024; do
	o=bpf/hd_cnt_thread_kmod${n}_test.bpf.o
	clang-15 $FLAGS -DHS_DEV -DHS_MAX=$n -DPT_SET_TEST -DHS_OP=HS_OP_CNT -DHS_RATE=HS_RATE_THREAD -DHS_KEY=HS_KEY_KMOD -c pt_hash.bpf.c -o $o
	echo $o
done
# S8 addendum: no side counts (-DHS_NOCHK): bpf/hdn_* (1533), bpf/hsn_* (host)
mkn() { # prefix devflag op rate key set [extra]
	local o=bpf/$1_$(echo $3 | tr A-Z a-z)_$(echo $4 | tr A-Z a-z)_$(echo $5 | tr A-Z a-z)_$(echo $6 | tr A-Z a-z)${7:-}.bpf.o
	clang-15 $FLAGS -DHS_NOCHK $2 -DPT_SET_$6 -DHS_OP=HS_OP_$3 -DHS_RATE=HS_RATE_$4 -DHS_KEY=HS_KEY_$5 ${8:-} -c pt_hash.bpf.c -o $o
	echo $o
}
for op in PRE MISS CNT; do mkn hdn -DHS_DEV $op THREAD KC TEST; done
mkn hsn "" PRE THREAD KC TEST
mkn hdn -DHS_DEV CNT THREAD KMOD TEST 512 "-DHS_MAX=512"
mkn hdn -DHS_DEV CNT THREAD KMOD TEST 1024 "-DHS_MAX=1024"
# S8 addendum 2: rms_norm without side counts; the per-launch filter with a 1533 map declared, unused
mkn hdn -DHS_DEV PRE THREAD KC RMS
mkn hsn "" PRE THREAD KC RMS
mkd NONE LEADER KC LLAMA4
