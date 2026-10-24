#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
RESULTS=/home/jetson/src/results/uprobe/array-jit-default-20261021
BUILD="$ROOT/build-jit"
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/build-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/build-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/build-started-at.txt"
cmake -S "$ROOT" -B "$BUILD" >"$RESULTS/reconfigure.log" 2>&1
make_target() {
	# Refresh compiler-generated header dependencies before target rules.
	gmake --no-print-directory -C "$BUILD" -f "$1" "${1%/build.make}/depend" >>"$RESULTS/build.log" 2>&1
    gmake --no-print-directory -C "$BUILD" -f "$1" "$2" -j2 >>"$RESULTS/build.log" 2>&1
}
make_target vm/compat/llvm-vm/llvm-jit/CMakeFiles/llvmbpf_vm.dir/build.make vm/compat/llvm-vm/libllvmbpf_vm.a
make_target vm/compat/llvm-vm/CMakeFiles/bpftime_llvm_vm_obj.dir/build.make vm/compat/llvm-vm/CMakeFiles/bpftime_llvm_vm_obj.dir/compat_llvm.cpp.o
make_target vm/compat/llvm-vm/CMakeFiles/bpftime_llvm_vm.dir/build.make vm/compat/llvm-vm/libbpftime_llvm_vm.a
make_target vm/vm-core/CMakeFiles/bpftime_vm.dir/build.make vm/vm-core/libbpftime_vm.a
make_target runtime/object/CMakeFiles/bpftime-object.dir/build.make runtime/object/CMakeFiles/bpftime-object.dir/bpf_object.cpp.o
make_target runtime/CMakeFiles/runtime.dir/build.make runtime/libruntime.a
make_target runtime/agent/CMakeFiles/bpftime-agent.dir/build.make runtime/agent/libbpftime-agent.so
make_target runtime/syscall-server/CMakeFiles/bpftime-syscall-server.dir/build.make runtime/syscall-server/libbpftime-syscall-server.so
make_target runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/build.make runtime/unit-test/bpftime_runtime_tests
make_target tools/bpftimetool/CMakeFiles/bpftimetool.dir/build.make tools/bpftimetool/bpftimetool
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-default]' >"$RESULTS/default-tests.log" 2>&1
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    >"$RESULTS/tests.log" 2>&1
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' \
    >"$RESULTS/host-contract.log" 2>&1
printf 'Default array JIT build and correctness checks completed.\n'
