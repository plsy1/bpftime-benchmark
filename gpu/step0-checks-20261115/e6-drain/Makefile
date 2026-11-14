# step0 e6: the drain loader (libbpf, under the syscall server) and the GPU workload.
# libbpf.a and its headers come from example/gpu/threadhist-llama/.output.
W := /home/jetson/src/bpftime-gpu-work
OUTPUT := .output
SRC_OUT := $(W)/example/gpu/threadhist-llama/.output
NVCC ?= /usr/local/cuda-13.2/bin/nvcc
INCLUDES := -I$(OUTPUT) -I$(W)/third_party/libbpf/include/uapi

all: e6_drain e6_gpubw

$(OUTPUT)/libbpf.a:
	mkdir -p $(OUTPUT)
	cp -a $(SRC_OUT)/libbpf.a $(SRC_OUT)/bpf $(OUTPUT)/

e6_drain: e6_drain.c | $(OUTPUT)/libbpf.a
	$(CC) -g -O2 -Wall $(INCLUDES) $< $(OUTPUT)/libbpf.a -lelf -lz -lpthread -o $@

# plain CUDA program (never run under the agent)
e6_gpubw: e6_gpubw.cu
	$(NVCC) -O3 -arch=sm_87 -keep -keep-dir .keep $< -o $@

.keep:
	mkdir -p .keep
e6_gpubw: | .keep

clean:
	rm -rf $(OUTPUT) .keep e6_drain e6_gpubw
.PHONY: all clean
