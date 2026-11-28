# Prototype tools (proto/map-model). libbpf and headers as step0/e7-llama.
W := /home/jetson/src/bpftime-gpu-work
TP := $(W)/third_party
LIBBPF_DIR := $(W)/step0/e7-llama/.libbpf
CLANG ?= clang-15
NVCC ?= /usr/local/cuda-13.2/bin/nvcc
INCLUDES := -I$(LIBBPF_DIR) -I$(TP)/libbpf/include/uapi -I$(TP)/vmlinux/arm64 -I.
CLANG_BPF_SYS_INCLUDES := $(shell $(CLANG) -v -E - </dev/null 2>&1 | sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')
BPFFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_arm64 $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES)
SETS := test llama4 rms mmq mmvq ropek
SETDEF_test := -DPT_SET_TEST
SETDEF_exitfast := -DPT_SET_EXITFAST
SETDEF_llama4 := -DPT_SET_LLAMA4
SETDEF_rms := -DPT_SET_RMS
SETDEF_mmq := -DPT_SET_MMQ
SETDEF_mmvq := -DPT_SET_MMVQ
SETDEF_ropek := -DPT_SET_ROPEK
OBJS := $(foreach s,$(SETS),bpf/carry_$(s).bpf.o bpf/lat_$(s).bpf.o bpf/latold_$(s).bpf.o bpf/latmix_$(s).bpf.o bpf/hist_$(s).bpf.o bpf/histold_$(s).bpf.o)

all: $(OBJS) bpf/noop_test.bpf.o bpf/empty2_llama4.bpf.o bpf/empty1_llama4.bpf.o bpf/carryonly_llama4.bpf.o bpf/lat_exitfast.bpf.o bpf/tlatomic_test.bpf.o bpf/carry1_test.bpf.o pt_loader pt_victim pt_exitfast

bpf/empty2_%.bpf.o: pt_empty.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@
bpf/empty1_%.bpf.o: pt_empty.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -DPT_EXIT_ONLY -c $< -o $@
bpf/carryonly_%.bpf.o: pt_lat.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -DPT_NOMERGE -c $< -o $@
bpf/tlatomic_%.bpf.o: pt_tlatomic.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -mcpu=v3 -c $< -o $@
bpf/noop_%.bpf.o: pt_noop.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@
bpf/carry_%.bpf.o: pt_carry.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@
bpf/lat_%.bpf.o: pt_lat.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@
bpf/latd_%.bpf.o: pt_lat.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -DPT_DEVHASH -c $< -o $@
bpf/latold_%.bpf.o: pt_lat.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -DPT_OLD -c $< -o $@
bpf/latmix_%.bpf.o: pt_lat.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -DPT_MIX -c $< -o $@
bpf/hist_%.bpf.o: pt_hist.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@
bpf/histold_%.bpf.o: pt_hist.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -DPT_OLD -c $< -o $@

bpf/timer_%.bpf.o: pt_timer.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@
bpf/count_%.bpf.o: pt_count.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@

bpf/sreg_%.bpf.o: pt_sreg.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@
bpf/carrynolog_%.bpf.o: pt_carrynolog.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@

pt_loader: pt_loader.c
	cc -g -O2 -Wall $(INCLUDES) $< $(LIBBPF_DIR)/libbpf.a -lelf -lz -o $@

pt_exitfast: pt_exitfast.cu
	$(NVCC) -cudart shared -gencode arch=compute_87,code=compute_87 -gencode arch=compute_87,code=sm_87 -O2 -o $@ $<

pt_victim: pt_victim.cu
	$(NVCC) -cudart shared -gencode arch=compute_87,code=compute_87 -gencode arch=compute_87,code=sm_87 -O2 -o $@ $<

clean:
	rm -rf bpf pt_loader pt_victim

bpf/carry1_%.bpf.o: pt_carry1.bpf.c pt_common.h
	@mkdir -p bpf
	$(CLANG) $(BPFFLAGS) $(SETDEF_$*) -c $< -o $@
