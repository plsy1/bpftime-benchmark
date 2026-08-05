# SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
OUTPUT := .output
CLANG ?= clang
LIBBPF_SRC := $(abspath ../../third_party/libbpf/src)
BPFTOOL_SRC := $(abspath ../../third_party/bpftool/src)
LIBBPF_OBJ := $(abspath $(OUTPUT)/libbpf.a)
BPFTOOL_OUTPUT ?= $(abspath $(OUTPUT)/bpftool)
BPFTOOL ?= $(BPFTOOL_OUTPUT)/bootstrap/bpftool
ARCH ?= $(shell uname -m | sed 's/x86_64/x86/' \
			 | sed 's/arm.*/arm/' \
			 | sed 's/aarch64/arm64/' \
			 | sed 's/ppc64le/powerpc/' \
			 | sed 's/mips.*/mips/' \
			 | sed 's/riscv64/riscv/' \
			 | sed 's/loongarch64/loongarch/')
VMLINUX := ../../third_party/vmlinux/$(ARCH)/vmlinux.h
# Use our own libbpf API headers and Linux UAPI headers distributed with
# libbpf to avoid dependency on system-wide headers, which could be missing or
# outdated
INCLUDES := -I$(OUTPUT) -I../../third_party/libbpf/include/ -I../../third_party/libbpf/include/uapi -I$(dir $(VMLINUX))
CFLAGS := -g -Wall
ALL_LDFLAGS := $(LDFLAGS) $(EXTRA_LDFLAGS)

APPS = uprobe uprobe-override # minimal minimal_legacy  kprobe fentry usdt sockfilter tc ksyscall

CARGO ?= $(shell which cargo)
ifeq ($(strip $(CARGO)),)
BZS_APPS :=
else
BZS_APPS := # profile
APPS += $(BZS_APPS)
# Required by libblazesym
ALL_LDFLAGS += -lrt -ldl -lpthread -lm
endif

# Get Clang's default includes on this system. We'll explicitly add these dirs
# to the includes list when compiling with `-target bpf` because otherwise some
# architecture-specific dirs will be "missing" on some architectures/distros -
# headers such as asm/types.h, asm/byteorder.h, asm/socket.h, asm/sockios.h,
# sys/cdefs.h etc. might be missing.
#
# Use '-idirafter': Don't interfere with include mechanics except where the
# build would have failed anyways.
CLANG_BPF_SYS_INCLUDES ?= $(shell $(CLANG) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

ifeq ($(V),1)
	Q =
	msg =
else
	Q = @
	msg = @printf '  %-8s %s%s\n'					\
		      "$(1)"						\
		      "$(patsubst $(abspath $(OUTPUT))/%,%,$(2))"	\
		      "$(if $(3), $(3))";
	MAKEFLAGS += --no-print-directory
endif

define allow-override
  $(if $(or $(findstring environment,$(origin $(1))),\
            $(findstring command line,$(origin $(1)))),,\
    $(eval $(1) = $(2)))
endef

$(call allow-override,CC,$(CROSS_COMPILE)cc)
$(call allow-override,LD,$(CROSS_COMPILE)ld)

.PHONY: all
all: $(APPS)

.PHONY: clean
clean:
	$(call msg,CLEAN)
	$(Q)rm -rf $(OUTPUT) $(APPS)
	rm benchmark-*.txt benchmark-*.json results.md

$(OUTPUT) $(OUTPUT)/libbpf $(BPFTOOL_OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

# Build libbpf
$(LIBBPF_OBJ): $(wildcard $(LIBBPF_SRC)/*.[ch] $(LIBBPF_SRC)/Makefile) | $(OUTPUT)/libbpf
	$(call msg,LIB,$@)
	$(Q)$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1		      \
		    OBJDIR=$(dir $@)/libbpf DESTDIR=$(dir $@)		      \
		    INCLUDEDIR= LIBDIR= UAPIDIR=			      \
		    install

# Build bpftool
$(BPFTOOL): | $(BPFTOOL_OUTPUT)
	$(call msg,BPFTOOL,$@)
	$(Q)$(MAKE) ARCH= CROSS_COMPILE= OUTPUT=$(BPFTOOL_OUTPUT)/ -C $(BPFTOOL_SRC) bootstrap


$(LIBBLAZESYM_SRC)/target/release/libblazesym.a::
	$(Q)cd $(LIBBLAZESYM_SRC) && $(CARGO) build --features=cheader,dont-generate-test-files --release

$(LIBBLAZESYM_OBJ): $(LIBBLAZESYM_SRC)/target/release/libblazesym.a | $(OUTPUT)
	$(call msg,LIB, $@)
	$(Q)cp $(LIBBLAZESYM_SRC)/target/release/libblazesym.a $@

$(LIBBLAZESYM_HEADER): $(LIBBLAZESYM_SRC)/target/release/libblazesym.a | $(OUTPUT)
	$(call msg,LIB,$@)
	$(Q)cp $(LIBBLAZESYM_SRC)/target/release/blazesym.h $@

# Build BPF code
$(OUTPUT)/%.bpf.o: %.bpf.c $(LIBBPF_OBJ) $(wildcard %.h) $(VMLINUX) | $(OUTPUT) $(BPFTOOL)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH)		      \
		     $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES)		      \
		     -c $(filter %.c,$^) -o $(patsubst %.bpf.o,%.tmp.bpf.o,$@)
	$(Q)$(BPFTOOL) gen object $@ $(patsubst %.bpf.o,%.tmp.bpf.o,$@)

# Generate BPF skeletons
$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT) $(BPFTOOL)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

# Build user-space code
$(patsubst %,$(OUTPUT)/%.o,$(APPS)): %.o: %.skel.h

$(OUTPUT)/%.o: %.c $(wildcard %.h) | $(OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c $(filter %.c,$^) -o $@

$(patsubst %,$(OUTPUT)/%.o,$(BZS_APPS)): $(LIBBLAZESYM_HEADER)

$(BZS_APPS): $(LIBBLAZESYM_OBJ)

# Build application binary
$(APPS): %: $(OUTPUT)/%.o $(LIBBPF_OBJ) | $(OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz -o $@

# delete failed targets
.DELETE_ON_ERROR:

# keep intermediate (.skel.h, .bpf.o, etc) targets
.SECONDARY:

# Matched kernel-BPF runtime A/B for ordinary array/hash lookup and update.
KMR_OUTPUT := $(OUTPUT)/kernel-map-runtime
KMR_BPF_OBJ := $(KMR_OUTPUT)/kernel_map_runtime.bpf.o
KMR_SKEL := $(KMR_OUTPUT)/kernel_map_runtime.skel.h
KMR_LOADER_OBJ := $(KMR_OUTPUT)/kernel_map_runtime.o
KMR_LOADER := $(KMR_OUTPUT)/kernel-map-runtime
KMR_VICTIM := $(KMR_OUTPUT)/kernel-map-runtime-victim

# Matched kernel-BPF ordinary/per-CPU array/hash runtime diagnostic.
KPMR_OUTPUT := $(OUTPUT)/kernel-percpu-map-runtime
KPMR_BPF_OBJ := $(KPMR_OUTPUT)/kernel_percpu_map_runtime.bpf.o
KPMR_SKEL := $(KPMR_OUTPUT)/kernel_percpu_map_runtime.skel.h
KPMR_LOADER_OBJ := $(KPMR_OUTPUT)/kernel_percpu_map_runtime.o
KPMR_LOADER := $(KPMR_OUTPUT)/kernel-percpu-map-runtime
KPMR_VICTIM := $(KPMR_OUTPUT)/kernel-percpu-map-runtime-victim

# Matched kernel-BPF delete-hit diagnostic for ordinary/per-CPU hash maps,
# with and without the kernel's BPF_F_NO_PREALLOC flag.
KHD_OUTPUT := $(OUTPUT)/kernel-percpu-hash-delete-runtime
KHD_BPF_OBJ := $(KHD_OUTPUT)/kernel_percpu_hash_delete_runtime.bpf.o
KHD_SKEL := $(KHD_OUTPUT)/kernel_percpu_hash_delete_runtime.skel.h
KHD_LOADER_OBJ := $(KHD_OUTPUT)/kernel_percpu_hash_delete_runtime.o
KHD_LOADER := $(KHD_OUTPUT)/kernel-percpu-hash-delete-runtime
KHD_VICTIM := $(KHD_OUTPUT)/kernel-percpu-hash-delete-runtime-victim

# Top-level matched empty/loop/full hash-lookup diagnostic.  It attaches three
# programs to byte-equivalent victim functions so the complete uprobe path can
# be split without modifying the official benchmark or the runtime.
THR_OUTPUT := $(OUTPUT)/top-hash-residual
THR_BPF_OBJ := $(THR_OUTPUT)/top_hash_residual.bpf.o
THR_SKEL := $(THR_OUTPUT)/top_hash_residual.skel.h
THR_LOADER_OBJ := $(THR_OUTPUT)/top_hash_residual.o
THR_LOADER := $(THR_OUTPUT)/top-hash-residual
THR_VICTIM := $(THR_OUTPUT)/top-hash-residual-victim

# Matched helper/array/hash lookup ladder.  This is separate from the
# top-hash-residual diagnostic so its raw data remain independently auditable.
HML_OUTPUT := $(OUTPUT)/helper-map-ladder
HML_BPF_OBJ := $(HML_OUTPUT)/helper_map_ladder.bpf.o
HML_SKEL := $(HML_OUTPUT)/helper_map_ladder.skel.h
HML_LOADER_OBJ := $(HML_OUTPUT)/helper_map_ladder.o
HML_LOADER := $(HML_OUTPUT)/helper-map-ladder
HML_VICTIM := $(HML_OUTPUT)/helper-map-ladder-victim

# Matched kernel-BPF array-update value-size sweep.  This is separate from the
# official uprobe benchmark and the general kernel-map-runtime diagnostic so
# changing the diagnostic matrix cannot change either benchmark's semantics.
KAUS_OUTPUT := $(OUTPUT)/kernel-array-update-sizes
KAUS_BPF_OBJ := $(KAUS_OUTPUT)/kernel_array_update_sizes.bpf.o
KAUS_SKEL := $(KAUS_OUTPUT)/kernel_array_update_sizes.skel.h
KAUS_LOADER_OBJ := $(KAUS_OUTPUT)/kernel_array_update_sizes.o
KAUS_LOADER := $(KAUS_OUTPUT)/kernel-array-update-sizes
KAUS_VICTIM := $(KAUS_OUTPUT)/kernel-array-update-sizes-victim

.PHONY: kernel-map-runtime-diagnostic
kernel-map-runtime-diagnostic: $(KMR_LOADER) $(KMR_VICTIM)

.PHONY: kernel-percpu-map-runtime-diagnostic
kernel-percpu-map-runtime-diagnostic: $(KPMR_LOADER) $(KPMR_VICTIM)

.PHONY: kernel-percpu-hash-delete-diagnostic
kernel-percpu-hash-delete-diagnostic: $(KHD_LOADER) $(KHD_VICTIM)

.PHONY: top-hash-residual-diagnostic
top-hash-residual-diagnostic: $(THR_LOADER) $(THR_VICTIM)

.PHONY: helper-map-ladder-diagnostic
helper-map-ladder-diagnostic: $(HML_LOADER) $(HML_VICTIM)

.PHONY: kernel-array-update-sizes-diagnostic
kernel-array-update-sizes-diagnostic: $(KAUS_LOADER) $(KAUS_VICTIM)

$(KMR_OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

$(KMR_BPF_OBJ): diagnostics/kernel_map_runtime.bpf.c $(VMLINUX) | $(KMR_OUTPUT) $(BPFTOOL)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		     $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		     -c $< -o $(KMR_OUTPUT)/kernel_map_runtime.tmp.bpf.o
	$(Q)$(BPFTOOL) gen object $@ $(KMR_OUTPUT)/kernel_map_runtime.tmp.bpf.o

$(KMR_SKEL): $(KMR_BPF_OBJ) | $(KMR_OUTPUT) $(BPFTOOL)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

$(KMR_LOADER_OBJ): diagnostics/kernel_map_runtime.c $(KMR_SKEL) | $(KMR_OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -I$(KMR_OUTPUT) -c $< -o $@

$(KMR_LOADER): $(KMR_LOADER_OBJ) $(LIBBPF_OBJ) | $(KMR_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz -o $@

$(KMR_VICTIM): diagnostics/kernel_map_runtime_victim.c | $(KMR_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $< -o $@

$(KPMR_OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

$(KPMR_BPF_OBJ): diagnostics/kernel_percpu_map_runtime.bpf.c $(VMLINUX) | $(KPMR_OUTPUT) $(BPFTOOL)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		     $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		     -c $< -o $(KPMR_OUTPUT)/kernel_percpu_map_runtime.tmp.bpf.o
	$(Q)$(BPFTOOL) gen object $@ $(KPMR_OUTPUT)/kernel_percpu_map_runtime.tmp.bpf.o

$(KPMR_SKEL): $(KPMR_BPF_OBJ) | $(KPMR_OUTPUT) $(BPFTOOL)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

$(KPMR_LOADER_OBJ): diagnostics/kernel_percpu_map_runtime.c $(KPMR_SKEL) | $(KPMR_OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -I$(KPMR_OUTPUT) -c $< -o $@

$(KPMR_LOADER): $(KPMR_LOADER_OBJ) $(LIBBPF_OBJ) | $(KPMR_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz -o $@

$(KPMR_VICTIM): diagnostics/kernel_percpu_map_runtime_victim.c | $(KPMR_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $< -o $@

$(KHD_OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

$(KHD_BPF_OBJ): diagnostics/kernel_percpu_hash_delete_runtime.bpf.c $(VMLINUX) | $(KHD_OUTPUT) $(BPFTOOL)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		     $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		     -c $< -o $(KHD_OUTPUT)/kernel_percpu_hash_delete_runtime.tmp.bpf.o
	$(Q)$(BPFTOOL) gen object $@ $(KHD_OUTPUT)/kernel_percpu_hash_delete_runtime.tmp.bpf.o

$(KHD_SKEL): $(KHD_BPF_OBJ) | $(KHD_OUTPUT) $(BPFTOOL)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

$(KHD_LOADER_OBJ): diagnostics/kernel_percpu_hash_delete_runtime.c $(KHD_SKEL) | $(KHD_OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -I$(KHD_OUTPUT) -c $< -o $@

$(KHD_LOADER): $(KHD_LOADER_OBJ) $(LIBBPF_OBJ) | $(KHD_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz -o $@

$(KHD_VICTIM): diagnostics/kernel_percpu_hash_delete_runtime_victim.c | $(KHD_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $< -o $@

$(THR_OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

$(THR_BPF_OBJ): diagnostics/top_hash_residual.bpf.c $(VMLINUX) | $(THR_OUTPUT) $(BPFTOOL)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		     $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		     -c $< -o $(THR_OUTPUT)/top_hash_residual.tmp.bpf.o
	$(Q)$(BPFTOOL) gen object $@ $(THR_OUTPUT)/top_hash_residual.tmp.bpf.o

$(THR_SKEL): $(THR_BPF_OBJ) | $(THR_OUTPUT) $(BPFTOOL)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

$(THR_LOADER_OBJ): diagnostics/top_hash_residual.c $(THR_SKEL) | $(THR_OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -I$(THR_OUTPUT) -c $< -o $@

$(THR_LOADER): $(THR_LOADER_OBJ) $(LIBBPF_OBJ) | $(THR_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz -o $@

$(THR_VICTIM): diagnostics/top_hash_residual_victim.c | $(THR_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) -g -O3 -Wall $< -o $@

$(HML_OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

$(HML_BPF_OBJ): diagnostics/helper_map_ladder.bpf.c $(VMLINUX) | $(HML_OUTPUT) $(BPFTOOL)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		     $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		     -c $< -o $(HML_OUTPUT)/helper_map_ladder.tmp.bpf.o
	$(Q)$(BPFTOOL) gen object $@ $(HML_OUTPUT)/helper_map_ladder.tmp.bpf.o

$(HML_SKEL): $(HML_BPF_OBJ) | $(HML_OUTPUT) $(BPFTOOL)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

$(HML_LOADER_OBJ): diagnostics/helper_map_ladder.c $(HML_SKEL) | $(HML_OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -I$(HML_OUTPUT) -c $< -o $@

$(HML_LOADER): $(HML_LOADER_OBJ) $(LIBBPF_OBJ) | $(HML_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz -o $@

$(HML_VICTIM): diagnostics/helper_map_ladder_victim.c | $(HML_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) -g -O3 -Wall $< -o $@

$(KAUS_OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

$(KAUS_BPF_OBJ): diagnostics/kernel_array_update_sizes.bpf.c $(VMLINUX) | $(KAUS_OUTPUT) $(BPFTOOL)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		     $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) \
		     -c $< -o $(KAUS_OUTPUT)/kernel_array_update_sizes.tmp.bpf.o
	$(Q)$(BPFTOOL) gen object $@ $(KAUS_OUTPUT)/kernel_array_update_sizes.tmp.bpf.o

$(KAUS_SKEL): $(KAUS_BPF_OBJ) | $(KAUS_OUTPUT) $(BPFTOOL)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

$(KAUS_LOADER_OBJ): diagnostics/kernel_array_update_sizes.c $(KAUS_SKEL) | $(KAUS_OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -I$(KAUS_OUTPUT) -c $< -o $@

$(KAUS_LOADER): $(KAUS_LOADER_OBJ) $(LIBBPF_OBJ) | $(KAUS_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -lelf -lz -o $@

$(KAUS_VICTIM): diagnostics/kernel_array_update_sizes_victim.c | $(KAUS_OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $< -o $@
