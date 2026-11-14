# step0 e8: the standalone loader (driver API + nvPTXCompiler static library, CUDA 13.2)
CUDA := /usr/local/cuda-13.2
CXX := g++-13
e8_load: e8_load.cpp
	$(CXX) -O2 -g -Wall -std=c++17 -I$(CUDA)/include $< -o $@ \
	  -L$(CUDA)/lib64 $(CUDA)/lib64/libnvptxcompiler_static.a \
	  -L$(CUDA)/targets/sbsa-linux/lib/stubs -lcuda -lpthread -lm -ldl
clean:
	rm -f e8_load
.PHONY: clean
