# step0 e5: build the three BPF programs, the loader and the victim.
# libbpf.a and its headers are copied from example/gpu/threadhist-llama/.output
# (built there by build-examples.sh); nothing in the dev tree is touched.
W := /home/jetson/src/bpftime-gpu-work
OUTPUT := .output
SRC_OUT := $(W)/example/gpu/threadhist-llama/.output
CLANG ?= clang-15
NVCC ?= /usr/local/cuda-13.2/bin/nvcc
INCLUDES := -I$(OUTPUT) -I$(W)/third_party/libbpf/include/uapi -I$(W)/third_party/vmlinux/arm64
CLANG_BPF_SYS_INCLUDES ?= $(shell $(CLANG) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')
# As the bpftime examples: no -mcpu (clang-15 default = generic = v1).
BPF_CFLAGS := -g -O2 -Wall -target bpf -D__TARGET_ARCH_arm64

all: e5_fadd.bpf.o e5_plain.bpf.o e5_cas.bpf.o e5_loader e5_victim

$(OUTPUT)/libbpf.a:
	mkdir -p $(OUTPUT)
	cp -a $(SRC_OUT)/libbpf.a $(SRC_OUT)/bpf $(OUTPUT)/

e5_fadd.bpf.o: e5.bpf.c | $(OUTPUT)/libbpf.a
	$(CLANG) $(BPF_CFLAGS) $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) -DE5_MODE_FADD -c $< -o $@
e5_plain.bpf.o: e5.bpf.c | $(OUTPUT)/libbpf.a
	$(CLANG) $(BPF_CFLAGS) $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) -DE5_MODE_PLAIN -c $< -o $@
e5_cas.bpf.o: e5.bpf.c | $(OUTPUT)/libbpf.a
	$(CLANG) $(BPF_CFLAGS) $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) -DE5_MODE_CAS -c $< -o $@

e5_loader: e5_loader.c | $(OUTPUT)/libbpf.a
	$(CC) -g -O2 -Wall $(INCLUDES) $< $(OUTPUT)/libbpf.a -lelf -lz -o $@

# Default nvcc 13.2 target (compute_75 PTX + sm_75 SASS), as the examples:
# the agent patches the PTX and compiles it for sm_87.
e5_victim: e5_victim.cu
	$(NVCC) -cudart shared -O2 -g $< -o $@

clean:
	rm -rf $(OUTPUT) e5_*.bpf.o e5_loader e5_victim

.PHONY: all clean
