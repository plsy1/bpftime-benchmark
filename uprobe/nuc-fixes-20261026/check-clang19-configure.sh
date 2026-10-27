#!/bin/bash
# Reproduce "only clang-19 on the machine" for benchmark/CMakeLists.txt: programs
# are found only under a root that links everything in /usr/bin except clang*,
# plus clang-19 (-> clang-16); BPFTIME_BPF_CLANG is passed as on the NUC.
# Usage: check-clang19-configure.sh SOURCE_DIR WORK_DIR NAME
set -u
SRC=$1; S=$2; NAME=$3
mkdir -p $S/root/usr/bin
for f in /usr/bin/*; do b=$(basename $f); case $b in clang|clang-[0-9]*|clang++*|clang-cpp*) ;; *) ln -sf $f $S/root/usr/bin/$b;; esac; done
ln -sf /usr/bin/clang-16 $S/root/usr/bin/clang-19
rm -rf "${S:?}/build-$NAME"
cmake -S $SRC -B $S/build-$NAME -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/gcc-13 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 -DLLVM_DIR=/usr/lib/llvm-16/cmake -DBPFTIME_LLVM_JIT=ON \
  -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBPFTIME_BPF_CLANG=$S/root/usr/bin/clang-19 \
  -DCMAKE_FIND_ROOT_PATH=$S/root -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY > $S/configure-$NAME.log 2>&1
echo "configure exit=$?"
