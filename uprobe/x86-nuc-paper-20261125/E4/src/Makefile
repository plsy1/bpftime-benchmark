# E4 build: standalone, against the libbpf/bpftool already built in the official 4708f409 tree's
# example/tracing/sslsniff-paper-s4/.output (the same libbpf the sslsniff loaders link; read only).
# BPF object compiled with clang-19 exactly like that Makefile's %.bpf.o rule.
OFFICIAL ?= /home/y/src/bpftime-paper/official-4708f409
LIBOUT   ?= $(OFFICIAL)/example/tracing/sslsniff-paper-s4/.output
CLANG    ?= clang-19
BPFTOOL  ?= $(LIBOUT)/bpftool/bootstrap/bpftool
CC       ?= cc
OUTPUT   := .output
INCLUDES := -I$(OUTPUT) -I$(LIBOUT) -I$(OFFICIAL)/third_party/libbpf/include/uapi
CLANG_BPF_SYS_INCLUDES := $(shell $(CLANG) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

all: pcpu_lost tickloop

$(OUTPUT):
	mkdir -p $@

$(OUTPUT)/pcpu_lost.bpf.o: pcpu_lost.bpf.c | $(OUTPUT)
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_x86 $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		-c $< -o $(OUTPUT)/pcpu_lost.tmp.bpf.o
	$(BPFTOOL) gen object $@ $(OUTPUT)/pcpu_lost.tmp.bpf.o

$(OUTPUT)/pcpu_lost.skel.h: $(OUTPUT)/pcpu_lost.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

pcpu_lost: pcpu_lost.c $(OUTPUT)/pcpu_lost.skel.h
	$(CC) -g -O2 -Wall $(INCLUDES) $< $(LIBOUT)/libbpf.a -lelf -lz -o $@

# -O2, no -fcf-protection: tick() keeps a plain prologue for the agent's inline hook
tickloop: tickloop.c
	$(CC) -g -O2 -Wall -pthread $< -o $@

clean:
	rm -rf $(OUTPUT) pcpu_lost tickloop

.PHONY: all clean
.DELETE_ON_ERROR:
