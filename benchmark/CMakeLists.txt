add_executable(simple-benchmark-with-embed-ebpf-calling test_embed.c)

add_dependencies(simple-benchmark-with-embed-ebpf-calling bpftime_vm libbpf)
target_compile_definitions(simple-benchmark-with-embed-ebpf-calling
    PRIVATE
)
target_link_libraries(simple-benchmark-with-embed-ebpf-calling bpftime_vm ${LIBBPF_LIBRARIES} elf z dl)
target_include_directories(simple-benchmark-with-embed-ebpf-calling
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}../vm/include
    ${LIBBPF_INCLUDE_DIRS})

# Diagnostic-only microbenchmark for decomposing the userspace array-map path.
# It does not alter the runtime implementation or any existing benchmark.
add_executable(array-map-path-layers
  uprobe/diagnostics/array_map_path_layers.cpp)
target_link_libraries(array-map-path-layers PRIVATE runtime)
set_target_properties(array-map-path-layers PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION FALSE
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/uprobe/diagnostics)

# Diagnostic-only microbenchmark for decomposing the userspace per-CPU
# array-map lookup/update path.  Synthetic layers mirror the implementation;
# l0-l3 add the production class, handler, shared-memory, and helper layers.
add_executable(per-cpu-array-path-layers
  uprobe/diagnostics/per_cpu_array_path_layers.cpp)
target_link_libraries(per-cpu-array-path-layers PRIVATE runtime)
set_target_properties(per-cpu-array-path-layers PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION FALSE
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/uprobe/diagnostics)

add_executable(hash-map-path-layers
  uprobe/diagnostics/hash_map_path_layers.cpp)
target_link_libraries(hash-map-path-layers PRIVATE runtime)
set_target_properties(hash-map-path-layers PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION FALSE
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/uprobe/diagnostics)

# Diagnostic-only microbenchmark for decomposing the userspace per-CPU
# hash-map lookup/update/delete path.
add_executable(per-cpu-hash-path-layers
  uprobe/diagnostics/per_cpu_hash_path_layers.cpp)
target_link_libraries(per-cpu-hash-path-layers PRIVATE runtime)
set_target_properties(per-cpu-hash-path-layers PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION FALSE
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/uprobe/diagnostics)

find_program(DIAGNOSTIC_BPF_CLANG NAMES clang-16 clang-15 clang REQUIRED)
set(ARRAY_HELPER_JIT_BPF_OBJECT
  ${CMAKE_BINARY_DIR}/benchmark/uprobe/diagnostics/array_helper_jit.bpf.o)
add_custom_command(
  OUTPUT ${ARRAY_HELPER_JIT_BPF_OBJECT}
  COMMAND ${CMAKE_COMMAND} -E make_directory
          ${CMAKE_BINARY_DIR}/benchmark/uprobe/diagnostics
  COMMAND ${DIAGNOSTIC_BPF_CLANG} -O2 -g -target bpf
          -c ${CMAKE_CURRENT_SOURCE_DIR}/uprobe/diagnostics/array_helper_jit.bpf.c
          -o ${ARRAY_HELPER_JIT_BPF_OBJECT}
  DEPENDS uprobe/diagnostics/array_helper_jit.bpf.c
  VERBATIM)
add_custom_target(array-helper-jit-bpf DEPENDS ${ARRAY_HELPER_JIT_BPF_OBJECT})
add_executable(array-helper-jit-layers
  uprobe/diagnostics/array_helper_jit_layers.cpp)
add_dependencies(array-helper-jit-layers array-helper-jit-bpf)
target_link_libraries(array-helper-jit-layers PRIVATE runtime)
target_compile_definitions(array-helper-jit-layers PRIVATE
  ARRAY_HELPER_JIT_BPF_OBJECT="${ARRAY_HELPER_JIT_BPF_OBJECT}")
set_target_properties(array-helper-jit-layers PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION FALSE
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/uprobe/diagnostics)

add_executable(hash-helper-jit-layers
  uprobe/diagnostics/hash_helper_jit_layers.cpp)
add_dependencies(hash-helper-jit-layers array-helper-jit-bpf)
target_link_libraries(hash-helper-jit-layers PRIVATE runtime)
target_compile_definitions(hash-helper-jit-layers PRIVATE
  ARRAY_HELPER_JIT_BPF_OBJECT="${ARRAY_HELPER_JIT_BPF_OBJECT}")
set_target_properties(hash-helper-jit-layers PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION FALSE
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/uprobe/diagnostics)

if(BPFTIME_ENABLE_CUDA_ATTACH)
  add_executable(gpu_array_map_host_perf
    gpu/host/gpu_array_map_host_perf.cpp)
  add_dependencies(gpu_array_map_host_perf runtime)
  target_link_libraries(gpu_array_map_host_perf
    PRIVATE
    runtime
    spdlog::spdlog
  )
  set_target_properties(gpu_array_map_host_perf
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/gpu/host
  )
  add_executable(gpu_per_thread_array_map_host_perf
    gpu/host/gpu_per_thread_array_map_host_perf.cpp)
  add_dependencies(gpu_per_thread_array_map_host_perf runtime)
  target_link_libraries(gpu_per_thread_array_map_host_perf
    PRIVATE
    runtime
    spdlog::spdlog
  )
  set_target_properties(gpu_per_thread_array_map_host_perf
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmark/gpu/host
  )
endif()
