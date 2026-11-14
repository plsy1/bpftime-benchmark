# Build for check 1 (ids). Outputs go to build/; -keep keeps the PTX and cubins in build/keep.
NVCC ?= /usr/local/cuda-13.2/bin/nvcc
CUOBJDUMP ?= /usr/local/cuda-13.2/bin/cuobjdump
ARCH ?= sm_87
NVFLAGS = -O3 -arch=$(ARCH) -std=c++17 -lineinfo

all: build/ids build/hog build/idmod.ptx build/idmod.cubin build/ids.sass build/hog.sass build/idmod.sass

build:
	mkdir -p build/keep

build/ids: ids.cu rec.cuh | build
	$(NVCC) $(NVFLAGS) -keep -keep-dir build/keep -o $@ ids.cu -lcuda

build/hog: hog.cu | build
	$(NVCC) $(NVFLAGS) -keep -keep-dir build/keep -o $@ hog.cu

build/idmod.ptx: idmod.cu rec.cuh | build
	$(NVCC) -O3 -arch=$(ARCH) -std=c++17 --ptx -o $@ idmod.cu

build/idmod.cubin: idmod.cu rec.cuh | build
	$(NVCC) -O3 -arch=$(ARCH) -std=c++17 -cubin -o $@ idmod.cu

build/ids.sass: build/ids
	$(CUOBJDUMP) -sass $< > $@

build/hog.sass: build/hog
	$(CUOBJDUMP) -sass $< > $@

build/idmod.sass: build/idmod.cubin
	$(CUOBJDUMP) -sass $< > $@

clean:
	rm -rf build

.PHONY: all clean
