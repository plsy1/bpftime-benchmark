FROM ubuntu:24.04

WORKDIR /bpftime

RUN apt-get update && apt-get install -y --no-install-recommends \
        autoconf automake binutils-dev build-essential ca-certificates \
        clang-16 cmake curl git libboost1.74-all-dev libelf1 libelf-dev \
        libssl-dev libtool libyaml-cpp-dev llvm-16 llvm-16-dev \
        nginx pkg-config python-is-python3 python3 python3-matplotlib \
        python3-numpy sudo systemtap-sdt-dev wrk zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . .

ENV BPFTIME_VM_NAME=llvm
ENV LLVM_DIR=/usr/lib/llvm-16/lib/cmake/llvm
ENV PATH="${PATH}:/usr/lib/llvm-16/bin:/root/.bpftime/"

# Keep the source revision's normal runtime configuration. In particular, do
# not use the root "make benchmark" target here because it forces probe
# read/write checks off. Build the four Jetson suites with their own Makefiles.
RUN rm -rf build \
    && cmake -Bbuild \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DBUILD_BPFTIME_DAEMON=1 \
        -DCMAKE_C_COMPILER=/usr/lib/llvm-16/bin/clang \
        -DCMAKE_CXX_COMPILER=/usr/lib/llvm-16/bin/clang++ \
        -DLLVM_CONFIG=/usr/lib/llvm-16/bin/llvm-config \
        -DLLVM_DIR=/usr/lib/llvm-16/lib/cmake/llvm \
    && cmake --build build -j"$(nproc)" \
    && cmake --install build

RUN make -C benchmark test \
    && make -C benchmark/uprobe -j"$(nproc)" \
    && make -C benchmark/syscall -j"$(nproc)" \
    && make -C benchmark/syscount-nginx -j"$(nproc)" \
    && make -C benchmark/ssl-nginx -j"$(nproc)"

# Older benchmark drivers use compatibility paths that predate the tracing/
# layout. Keep both forms available in every image.
RUN if [ ! -e example/sslsniff ] && [ -d example/tracing/sslsniff ]; then \
        ln -s tracing/sslsniff example/sslsniff; \
    fi \
    && if [ ! -e example/libbpf-tools/syscount/syscount ] \
          && [ -x example/tracing/syscount/syscount ]; then \
        mkdir -p example/libbpf-tools/syscount; \
        ln -s ../../tracing/syscount/syscount \
          example/libbpf-tools/syscount/syscount; \
    fi \
    && chmod a+rx benchmark benchmark/ssl-nginx benchmark/syscount-nginx \
    && chmod a+r benchmark/ssl-nginx/index.html \
                 benchmark/syscount-nginx/index.html

# Fail the image build instead of publishing an image that needs compilation
# after startup.
RUN test -x build/tools/bpftimetool/bpftimetool \
    && test -x benchmark/test \
    && test -x benchmark/uprobe/uprobe \
    && test -x benchmark/syscall/syscall \
    && test -x example/tracing/sslsniff/sslsniff \
    && test -x example/tracing/syscount/syscount \
    && test -x example/sslsniff/sslsniff \
    && test -x example/libbpf-tools/syscount/syscount \
    && test -r benchmark/ssl-nginx/index.html \
    && test -r benchmark/syscount-nginx/index.html

CMD ["sleep", "infinity"]
