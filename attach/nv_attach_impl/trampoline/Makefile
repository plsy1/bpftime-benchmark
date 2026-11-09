CLANG ?= clang++-17
CUDA_LIBS ?= -L/usr/local/cuda/lib64/ -lcudart -pthread
PTX_TARGET_ARCH ?= sm_61
CLANG_FLAGS ?= -U__FLOAT128__ -U__SIZEOF_FLOAT128__
# The PTX bpftime ships is the device code alone, built without CUDA's or the
# host's headers (BPFTIME_TRAMPOLINE_DEVICE_ONLY), so clang 15 or later builds
# it on any host: clang 16 cannot parse CUDA 13's headers, nor glibc's aarch64
# vector math declarations in a device pass. The header keeps .version 8.1
# (clang 16 emits 7.8 for the same code).
DEVICE_ONLY_FLAGS ?= -x cuda --cuda-device-only -nocudainc -nocudalib \
	-DBPFTIME_TRAMPOLINE_DEVICE_ONLY
PTX_VERSION ?= 8.1

.PHONY: all clean

all: ../trampoline_ptx.h test

../trampoline_ptx.h: default_trampoline-device-$(PTX_TARGET_ARCH).ptx
	@echo "Generating trampoline_ptx.h..."
	@echo 'static const char TRAMPOLINE_PTX[] = R"(' > $@
	@sed 's/^\.version .*/.version $(PTX_VERSION)/' $< >> $@
	@echo ')";' >> $@
	@echo "Generated $@"

default_trampoline-device-$(PTX_TARGET_ARCH).ptx: default_trampoline.cu
	$(CLANG) $(DEVICE_ONLY_FLAGS) --cuda-gpu-arch=$(PTX_TARGET_ARCH) -O2 -S $< -o $@

# compile it to exec for test
test: default_trampoline.cu
	$(CLANG) -o test --cuda-gpu-arch=$(PTX_TARGET_ARCH) default_trampoline.cu $(CLANG_FLAGS) $(CUDA_LIBS)

clean:
	rm -f default_trampoline-device-$(PTX_TARGET_ARCH).ptx
	rm -f ../trampoline_ptx.h 
