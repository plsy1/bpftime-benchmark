# step0 e7-llama: eBPF objects (one per kernel, plus all4), the loader, the
# positive-control victim and the host unit test. No GPU needed to build.
# libbpf (static) and its headers come from the threadhist-llama example's
# build (copied into .libbpf/ once), vmlinux.h and the uapi headers from the
# dev tree's third_party (read only).
W := /home/jetson/src/bpftime-gpu-work
TP := $(W)/third_party
EXOUT := $(W)/example/gpu/threadhist-llama/.output
CLANG ?= clang-15
LLVM_OBJDUMP ?= llvm-objdump-15
NVCC ?= /usr/local/cuda-13.2/bin/nvcc
LIBBPF_DIR := .libbpf
INCLUDES := -I$(LIBBPF_DIR) -I$(TP)/libbpf/include/uapi -I$(TP)/vmlinux/arm64 -I.
CLANG_BPF_SYS_INCLUDES := $(shell $(CLANG) -v -E - </dev/null 2>&1 | sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')
BPFFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_arm64 $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES)

OBJS := bpf/e7_rms.bpf.o bpf/e7_mmq.bpf.o bpf/e7_mmvq.bpf.o bpf/e7_ropek.bpf.o bpf/e7_ctrl.bpf.o bpf/e7_all4.bpf.o

all: $(OBJS) e7_loader e7_ctrl popc_test disasm

$(LIBBPF_DIR)/libbpf.a:
	mkdir -p $(LIBBPF_DIR)
	cp -a $(EXOUT)/libbpf.a $(LIBBPF_DIR)/
	cp -a $(EXOUT)/bpf $(LIBBPF_DIR)/

bpf/e7_%.bpf.o: e7.bpf.c e7_common.h $(LIBBPF_DIR)/libbpf.a
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(BPF_DEFS_$*) -c e7.bpf.c -o $@

BPF_DEFS_rms := -DE7_KIDX=0
BPF_DEFS_mmq := -DE7_KIDX=1
BPF_DEFS_mmvq := -DE7_KIDX=2
BPF_DEFS_ropek := -DE7_KIDX=3
BPF_DEFS_ctrl := -DE7_KIDX=4
BPF_DEFS_all4 := -DE7_ALL4

e7_loader: e7_loader.c e7_common.h $(LIBBPF_DIR)/libbpf.a
	cc -g -O2 -Wall $(INCLUDES) e7_loader.c $(LIBBPF_DIR)/libbpf.a -lelf -lz -o $@

# PTX for compute_87 must be in the fatbin: bpftime patches PTX.
e7_ctrl: e7_ctrl.cu
	$(NVCC) -cudart shared -O2 -gencode arch=compute_87,code=[sm_87,compute_87] -keep -keep-dir build-ctrl -o $@ e7_ctrl.cu

build-ctrl:
	mkdir -p build-ctrl
e7_ctrl: | build-ctrl

popc_test: popc_test.c e7_common.h
	cc -O2 -Wall -o $@ popc_test.c

# BPF disassembly of every object (to check the calls and the atomic add)
disasm: $(OBJS)
	@mkdir -p bpf
	for o in $(OBJS); do $(LLVM_OBJDUMP) -d $$o > $${o%.o}.dis; done

clean:
	rm -rf bpf e7_loader e7_ctrl popc_test build-ctrl

.PHONY: all clean disasm
