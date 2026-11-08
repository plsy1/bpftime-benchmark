#!/bin/bash
# Parse (g++ -fsyntax-only, writes nothing) the CUDA-attach translation units
# of the candidate tree against the CUDA 13.2 headers on this board, with the
# include paths cuda.cmake WOULD give them if it found the headers, to see
# which TUs fail to compile against CUDA 13.2 for reasons other than paths.
R=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=$R/build-jetson-daily
S=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$S/gen"
cat > "$S/gen/ptx_pass_config.h" <<'EOF'
#pragma once
static constexpr const char *DEFAULT_PTX_PASS_EXECUTABLE = "";static constexpr const char* DEFAULT_PTX_COMPILER_SHARED_LIB="";
EOF
CUDA_INC=${CUDA_INC:-/usr/local/cuda-13.2/include}
FLAGS=(-std=gnu++20 -fsyntax-only -w -DBPFTIME_ENABLE_CUDA_ATTACH=1
  -DBPFTIME_BUILD_WITH_LIBBPF=1 -DBPFTIME_LLVM_JIT=1 -DBPFTIME_UBPF_JIT=1
  -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1
  -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO -DSPDLOG_COMPILED_LIB
  -I"$S/gen" -I"$CUDA_INC"
  -I$R/vm/include -I$R/runtime/include -I$R/runtime -I$B/runtime
  -I$R/third_party -I$R/third_party/spdlog/include -I$R/runtime/src
  -I$B/libbpf/uapi -I$B/libbpf -I$B/FridaGum-prefix/src/FridaGum
  -I$R/vm/vm-core/include -I$R/vm/compat/include
  -I$B/vm/compat/llvm-vm/array-inline-include -I$R/vm/llvm-jit/src
  -I$R/vm/llvm-jit/include -I$R/attach/base_attach_impl
  -I$R/attach/nv_attach_impl -I$R/attach/nv_attach_impl/pass/ptxpass_core/include
  -I/usr/lib/llvm-15/include)
TUS=(
  attach/nv_attach_impl/nv_attach_impl.cpp
  attach/nv_attach_impl/nv_attach_impl_frida_setup.cpp
  attach/nv_attach_impl/nv_attach_fatbin_record.cpp
  attach/nv_attach_impl/nv_attach_utils.cpp
  attach/nv_attach_impl/nv_attach_impl_patcher.cpp
  attach/nv_attach_impl/nv_attach_private_data.cpp
  attach/nv_attach_impl/nv_elf_introspect.cpp
  attach/nv_attach_impl/ptx_compiler/ptx_compiler.cpp
  attach/nv_attach_impl/pass/ptxpass_core/src/core.cpp
  attach/nv_attach_impl/pass/ptxpass_core/src/nv_attach_impl_register_guard_patcher.cpp
  attach/nv_attach_impl/pass/ptxpass_kprobe_entry/main.cpp
  runtime/src/attach/bpf_attach_ctx_cuda.cpp
  runtime/src/attach/bpf_attach_ctx.cpp
  runtime/src/bpf_map/gpu/nv_gpu_gdrcopy.cpp
  runtime/src/bpf_map/gpu/nv_gpu_per_thread_array_map.cpp
  runtime/src/bpf_map/gpu/nv_gpu_shared_array_map.cpp
  runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp
  runtime/src/bpf_map/gpu/nv_gpu_shared_hash_map.cpp
  runtime/src/bpf_map/gpu/nv_gpu_array_host_map.cpp
  runtime/src/bpf_map/gpu/nv_gpu_shared_array_host_map.cpp
  runtime/src/bpf_map/gpu_kernel_shared/array_map_kernel_gpu.cpp
  runtime/src/handler/map_handler.cpp
  runtime/src/bpftime_shm_internal.cpp
  runtime/src/bpftime_shm.cpp
  runtime/src/bpftime_prog.cpp
)
for tu in "${TUS[@]}"; do
  out=$(timeout 180 nice g++-13 "${FLAGS[@]}" "$R/$tu" 2>&1)
  rc=$?
  nerr=$(printf '%s\n' "$out" | grep -c ' error: ')
  printf '%-75s rc=%d errors=%d\n' "$tu" "$rc" "$nerr"
  if [ "$rc" -ne 0 ]; then
    printf '%s\n' "$out" | grep -E ' error: |fatal error' | head -6 | sed 's/^/    /'
  fi
done
