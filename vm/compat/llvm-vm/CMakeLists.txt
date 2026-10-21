add_library(bpftime_llvm_vm_obj OBJECT compat_llvm.cpp)

add_dependencies(bpftime_llvm_vm_obj llvmbpf_vm spdlog::spdlog bpftime_vm_compat)

add_library(bpftime_llvm_vm STATIC $<TARGET_OBJECTS:bpftime_llvm_vm_obj>)
target_link_libraries(bpftime_llvm_vm PUBLIC bpftime_vm_compat spdlog::spdlog llvmbpf_vm)

target_include_directories(bpftime_llvm_vm_obj PRIVATE ../../llvm-jit/include ${CMAKE_CURRENT_SOURCE_DIR} ${SPDLOG_INCLUDE} ../include)

if(BPFTIME_BUILD_WITH_LIBBPF)
  add_subdirectory(../../llvm-jit ${CMAKE_CURRENT_BINARY_DIR}/llvm-jit)
else()
  add_subdirectory(../../llvm-jit ${CMAKE_CURRENT_BINARY_DIR}/llvm-jit EXCLUDE_FROM_ALL)
endif()

# Keep the upstream submodule clean. Generate one patched translation unit in
# the build tree, with a checked patch owned/replayed by the parent repository.
# Upstream conflicts fail configuration rather than silently losing errno or
# unaligned-key correctness. The remaining llvmbpf sources are unmodified.
find_program(BPFTIME_PATCH_EXECUTABLE NAMES patch REQUIRED)
get_filename_component(BPFTIME_LLVM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../llvm-jit" ABSOLUTE)
set(BPFTIME_ARRAY_JIT_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/array-map-inline.patch")
set(BPFTIME_ARRAY_JIT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/llvm_jit_context_array_inline.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${BPFTIME_ARRAY_JIT_PATCH}" "${BPFTIME_LLVM_SOURCE_DIR}/src/llvm_jit_context.cpp")
execute_process(
  COMMAND "${BPFTIME_PATCH_EXECUTABLE}" --batch --fuzz=0
    "--output=${BPFTIME_ARRAY_JIT_SOURCE}.tmp"
    "${BPFTIME_LLVM_SOURCE_DIR}/src/llvm_jit_context.cpp" "${BPFTIME_ARRAY_JIT_PATCH}"
  RESULT_VARIABLE BPFTIME_ARRAY_JIT_PATCH_RESULT
  OUTPUT_VARIABLE BPFTIME_ARRAY_JIT_PATCH_OUTPUT
  ERROR_VARIABLE BPFTIME_ARRAY_JIT_PATCH_ERROR)
if(NOT BPFTIME_ARRAY_JIT_PATCH_RESULT EQUAL 0)
  message(FATAL_ERROR "Array JIT patch conflicts with upstream llvmbpf; rebase array-map-inline.patch.\n${BPFTIME_ARRAY_JIT_PATCH_OUTPUT}\n${BPFTIME_ARRAY_JIT_PATCH_ERROR}")
endif()
configure_file("${BPFTIME_ARRAY_JIT_SOURCE}.tmp" "${BPFTIME_ARRAY_JIT_SOURCE}" COPYONLY)
file(REMOVE "${BPFTIME_ARRAY_JIT_SOURCE}.tmp")
get_target_property(BPFTIME_LLVM_SOURCES llvmbpf_vm SOURCES)
list(FIND BPFTIME_LLVM_SOURCES "src/llvm_jit_context.cpp" BPFTIME_LLVM_SOURCE_INDEX)
if(BPFTIME_LLVM_SOURCE_INDEX LESS 0)
  message(FATAL_ERROR "Upstream llvmbpf source layout changed; audit the array JIT overlay.")
endif()
list(REMOVE_AT BPFTIME_LLVM_SOURCES ${BPFTIME_LLVM_SOURCE_INDEX})
list(APPEND BPFTIME_LLVM_SOURCES "${BPFTIME_ARRAY_JIT_SOURCE}")
set_property(TARGET llvmbpf_vm PROPERTY SOURCES "${BPFTIME_LLVM_SOURCES}")
target_include_directories(llvmbpf_vm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")

# Add explicit per-map kind without editing the upstream submodule. All
# parent-linked native VM consumers must see the same descriptor definition.
set(BPFTIME_ARRAY_DESCRIPTOR_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/array-map-descriptor.patch")
set(BPFTIME_ARRAY_DESCRIPTOR_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/array-inline-include")
file(MAKE_DIRECTORY "${BPFTIME_ARRAY_DESCRIPTOR_INCLUDE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${BPFTIME_ARRAY_DESCRIPTOR_PATCH}" "${BPFTIME_LLVM_SOURCE_DIR}/include/llvmbpf.hpp")
execute_process(
  COMMAND "${BPFTIME_PATCH_EXECUTABLE}" --batch --fuzz=0
    "--output=${BPFTIME_ARRAY_DESCRIPTOR_INCLUDE}/llvmbpf.hpp.tmp"
    "${BPFTIME_LLVM_SOURCE_DIR}/include/llvmbpf.hpp" "${BPFTIME_ARRAY_DESCRIPTOR_PATCH}"
  RESULT_VARIABLE BPFTIME_ARRAY_DESCRIPTOR_RESULT
  OUTPUT_VARIABLE BPFTIME_ARRAY_DESCRIPTOR_OUTPUT
  ERROR_VARIABLE BPFTIME_ARRAY_DESCRIPTOR_ERROR)
if(NOT BPFTIME_ARRAY_DESCRIPTOR_RESULT EQUAL 0)
  message(FATAL_ERROR "Array descriptor patch conflicts with upstream llvmbpf.\n${BPFTIME_ARRAY_DESCRIPTOR_OUTPUT}\n${BPFTIME_ARRAY_DESCRIPTOR_ERROR}")
endif()
configure_file("${BPFTIME_ARRAY_DESCRIPTOR_INCLUDE}/llvmbpf.hpp.tmp"
  "${BPFTIME_ARRAY_DESCRIPTOR_INCLUDE}/llvmbpf.hpp" COPYONLY)
file(REMOVE "${BPFTIME_ARRAY_DESCRIPTOR_INCLUDE}/llvmbpf.hpp.tmp")
target_include_directories(llvmbpf_vm BEFORE PUBLIC "${BPFTIME_ARRAY_DESCRIPTOR_INCLUDE}")
target_include_directories(bpftime_llvm_vm_obj BEFORE PRIVATE "${BPFTIME_ARRAY_DESCRIPTOR_INCLUDE}")

# Native CLI/examples/tests can specify the upstream include directory before
# transitive includes. Prepend explicitly for every native consumer as well;
# otherwise an aggregate descriptor would have inconsistent sizes across TUs.
function(bpftime_array_descriptor_includes directory)
  get_property(array_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(array_target IN LISTS array_targets)
    get_target_property(array_target_type "${array_target}" TYPE)
    if(NOT array_target_type STREQUAL "UTILITY" AND
       NOT array_target_type STREQUAL "INTERFACE_LIBRARY")
      target_include_directories("${array_target}" BEFORE PRIVATE "${BPFTIME_ARRAY_DESCRIPTOR_INCLUDE}")
    endif()
  endforeach()
  get_property(array_subdirs DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(array_subdir IN LISTS array_subdirs)
    bpftime_array_descriptor_includes("${array_subdir}")
  endforeach()
endfunction()
bpftime_array_descriptor_includes("${BPFTIME_LLVM_SOURCE_DIR}")
