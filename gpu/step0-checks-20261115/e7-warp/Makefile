# Build for check 7 (standalone half). -keep keeps the PTX and cubin in build/keep.
NVCC ?= /usr/local/cuda-13.2/bin/nvcc
CUOBJDUMP ?= /usr/local/cuda-13.2/bin/cuobjdump
ARCH ?= sm_87
NVFLAGS = -O3 -arch=$(ARCH) -std=c++17 -lineinfo

all: build/warp build/warp.sass

build:
	mkdir -p build/keep

build/warp: warp.cu | build
	$(NVCC) $(NVFLAGS) -keep -keep-dir build/keep -o $@ warp.cu -lcuda

build/warp.sass: build/warp
	$(CUOBJDUMP) -sass $< > $@

clean:
	rm -rf build

.PHONY: all clean

# PTX for the driver-JIT runs: as nvcc emitted it, and with every call.uni turned into a plain
# call (bpftime inserts plain "call" instructions into the module PTX)
all: build/warp.ptx build/warp_plaincall.ptx
build/warp.ptx: build/warp
	cp build/keep/warp.ptx $@
build/warp_plaincall.ptx: build/warp.ptx
	sed 's/\bcall\.uni\b/call/g' $< > $@

# the plain-call PTX assembled offline, to read its SASS (the driver JIT is not dumpable)
PTXAS ?= /usr/local/cuda-13.2/bin/ptxas
all: build/warp_plaincall.sass
build/warp_plaincall.cubin: build/warp_plaincall.ptx
	$(PTXAS) -O3 -arch=$(ARCH) -o $@ $<
build/warp_plaincall.sass: build/warp_plaincall.cubin
	$(CUOBJDUMP) -sass $< > $@
