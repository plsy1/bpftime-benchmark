# find_cuda() (cmake/cuda.cmake) on fake CUDA roots, no CUDA needed:
#   cmake -P cmake/test/find_cuda_layouts.cmake   (also a CTest test)
# CUDA 12's targets/aarch64-linux and CUDA 13's targets/sbsa-linux (the only
# layout CUDA 13 installs, Jetson included), x86_64, and a root without
# cuda.h, which must fail at configure time. Each case runs in a child cmake,
# so that a FATAL_ERROR ends only that case.
cmake_minimum_required(VERSION 3.16)
get_filename_component(_src "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(DEFINED CASE_ROOT)
  include("${_src}/cmake/cuda.cmake")
  set(CMAKE_SYSTEM_PROCESSOR "${CASE_PROC}")
  set(BPFTIME_CUDA_ROOT "${CASE_ROOT}")
  find_cuda()
  message(STATUS "INCLUDE=${CUDA_INCLUDE_PATH}")
  message(STATUS "LIBRARY=${CUDA_LIBRARY_PATH}")
  return()
endif()

set(_work "${CMAKE_CURRENT_BINARY_DIR}/find_cuda_layouts_test")
file(REMOVE_RECURSE "${_work}")
set(_failed 0)

# make_root(name file...): a fake root holding the given files.
function(make_root name)
  foreach(f IN LISTS ARGN)
    file(WRITE "${_work}/${name}/${f}" "")
  endforeach()
  file(MAKE_DIRECTORY "${_work}/${name}")
endfunction()

# run_case(name proc expect_rc must_contain must_not_contain)
function(run_case name proc expect_rc must must_not)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DCASE_ROOT=${_work}/${name}"
            "-DCASE_PROC=${proc}" -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(all "${out}${err}")
  set(ok TRUE)
  if(expect_rc EQUAL 0 AND NOT rc EQUAL 0)
    set(ok FALSE)
  elseif(NOT expect_rc EQUAL 0 AND rc EQUAL 0)
    set(ok FALSE)
  endif()
  string(FIND "${all}" "${must}" pos)
  if(pos EQUAL -1)
    set(ok FALSE)
  endif()
  if(NOT must_not STREQUAL "")
    string(FIND "${all}" "${must_not}" pos)
    if(NOT pos EQUAL -1)
      set(ok FALSE)
    endif()
  endif()
  if(ok)
    message(STATUS "PASS ${name}")
  else()
    message(STATUS "FAIL ${name} (rc ${rc}, expected ${expect_rc}; wanted "
      "\"${must}\", not \"${must_not}\"):\n${all}")
    set(_failed 1 PARENT_SCOPE)
  endif()
endfunction()

make_root(cuda12-jetson targets/aarch64-linux/include/cuda.h)
run_case(cuda12-jetson aarch64 0 "INCLUDE=${_work}/cuda12-jetson/targets/aarch64-linux/include" "")

make_root(cuda13-arm targets/sbsa-linux/include/cuda.h targets/sbsa-linux/include/cupti.h)
run_case(cuda13-arm aarch64 0
  "INCLUDE=${_work}/cuda13-arm/targets/sbsa-linux/include;${_work}/cuda13-arm/targets/sbsa-linux/include"
  "extras/CUPTI/include")
run_case(cuda13-arm aarch64 0 "LIBRARY=${_work}/cuda13-arm/targets/sbsa-linux/lib/" "")

make_root(both-arm targets/aarch64-linux/include/cuda.h targets/sbsa-linux/include/cuda.h)
run_case(both-arm aarch64 0 "INCLUDE=${_work}/both-arm/targets/aarch64-linux/include" "")

make_root(cuda12-x86 targets/x86_64-linux/include/cuda.h extras/CUPTI/include/cupti.h)
run_case(cuda12-x86 x86_64 0
  "INCLUDE=${_work}/cuda12-x86/targets/x86_64-linux/include;${_work}/cuda12-x86/extras/CUPTI/include" "")

make_root(no-cuda-h targets/sbsa-linux/include/cupti.h)
# (CMake wraps the message at spaces: check its parts.)
run_case(no-cuda-h aarch64 1 "cuda.h not found in" "")
run_case(no-cuda-h aarch64 1 "/no-cuda-h/targets/sbsa-linux/include:" "")

file(REMOVE_RECURSE "${_work}")
if(_failed)
  message(FATAL_ERROR "find_cuda layout cases failed")
endif()
