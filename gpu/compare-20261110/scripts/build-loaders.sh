#!/usr/bin/env bash
# Victims and loaders for the official GPU benchmark comparison (official 4708f409 vs dev
# c115b33d). Everything is built outside the worktrees except the two example loaders that
# differ between the trees (cuda-counter, kernelretsnoop: their .output is gitignored).
set -u
V=/home/jetson/src/bpftime-gpu-work/compare
T=/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e
O=$T/official; D=$T/candidate
R=/home/jetson/src/results/gpu/compare-20261110/build
N=/usr/local/cuda-13.2/bin/nvcc
J=2
date +%T

# Victims: vec_add.cu as the framework's Makefile builds it (default nvcc 13.2 stubs, CUDA 13
# kernel handles) and with the legacy launch stubs; both -cudart shared, no -arch.
mkdir -p $V/victims
F="-cudart shared -g -Wno-deprecated-gpu-targets -O3"
L="-D__NV_LEGACY_LAUNCH -Xcompiler -D__NV_LEGACY_LAUNCH"
(cd $O/benchmark/gpu/workload && timeout 600 $N $F vec_add.cu -o $V/victims/vec_add && timeout 600 $N $F $L vec_add.cu -o $V/victims/vec_add_legacy) || { echo victim build failed; exit 1; }
{
  for v in vec_add vec_add_legacy; do
    echo "== $v"; sha256sum $V/victims/$v
    ldd $V/victims/$v | grep -E 'cudart'
    readelf -Ws --dyn-syms $V/victims/$v | grep -E -o '(__cudaLaunchKernel|cudaLaunchKernel)@[^ ]*' | sort -u
    /usr/local/cuda-13.2/bin/cuobjdump --list-ptx --list-elf $V/victims/$v
  done
} > $R/victims.txt 2>&1
cat $R/victims.txt

# Micro loader: stock (1502/1503 as shipped) and fix (1512/1513, the host-memory variants both
# builds create on this GPU). Copies at the tree's depth: the Makefile uses ../../../third_party.
for k in stock fix; do
  mkdir -p $V/$k/benchmark/gpu
  rm -rf -- "${V:?}/${k:?}/benchmark/gpu/micro"
  cp -a $O/benchmark/gpu/micro $V/$k/benchmark/gpu/
  ln -sfn $O/third_party $V/$k/third_party
done
sed -i '6s/1502/1512/; 8s/1503/1513/' $V/fix/benchmark/gpu/micro/cuda_probe.bpf.c
grep -n -E '^#define BPF_MAP_TYPE_(PERGPUTD|GPU_ARRAY)' $V/stock/benchmark/gpu/micro/cuda_probe.bpf.c $V/fix/benchmark/gpu/micro/cuda_probe.bpf.c
for k in stock fix; do
  (cd $V/$k/benchmark/gpu/micro && timeout 1200 make CLANG=clang-15 -j$J cuda_probe > $R/loader-micro-$k.log 2>&1) || { echo "micro $k failed"; tail -n 20 $R/loader-micro-$k.log; exit 1; }
done

# Example loaders that differ between the trees, built in each tree.
for n in official candidate; do
  timeout 1200 make -C $T/$n/example/gpu/cuda-counter CLANG=clang-15 -j$J cuda_probe > $R/loader-$n-cuda-counter.log 2>&1 || { echo "$n cuda-counter failed"; tail -n 20 $R/loader-$n-cuda-counter.log; exit 1; }
  timeout 1200 make -C $T/$n/example/gpu/kernelretsnoop CLANG=clang-15 -j$J kernelretsnoop > $R/loader-$n-kernelretsnoop.log 2>&1 || { echo "$n kernelretsnoop failed"; tail -n 20 $R/loader-$n-kernelretsnoop.log; exit 1; }
done

# mem_trace and threadhist as shipped (1503/1502), per tree, in copies; and the gate copies
# asking for 1513/1512.
for n in official candidate; do
  if [ $n = official ]; then E=$V/offex; else E=$V/devex; fi
  for dst in $E $V/gate/$n; do
    mkdir -p $dst/example/gpu
    for e in mem_trace threadhist; do rm -rf -- "${dst:?}/example/gpu/${e:?}"; cp -a "$T/$n/example/gpu/$e" "$dst/example/gpu/"; rm -rf -- "${dst:?}/example/gpu/${e:?}/.output"; done
    ln -sfn $T/$n/third_party $dst/third_party
  done
  sed -i '6s/1502/1512/' $V/gate/$n/example/gpu/threadhist/threadhist.bpf.c
  sed -i '11s/1503/1513/' $V/gate/$n/example/gpu/mem_trace/mem_trace.bpf.c
  grep -n -E '^#define BPF_MAP_TYPE_(PERGPUTD|GPU_ARRAY)' $V/gate/$n/example/gpu/threadhist/threadhist.bpf.c $V/gate/$n/example/gpu/mem_trace/mem_trace.bpf.c
  for dst in $E $V/gate/$n; do
    for e in mem_trace threadhist; do
      timeout 1200 make -C $dst/example/gpu/$e CLANG=clang-15 -j$J $e > $R/loader-$(basename $dst)-$n-$e.log 2>&1 || { echo "$dst $e failed"; tail -n 20 $R/loader-$(basename $dst)-$n-$e.log; exit 1; }
    done
  done
done

# Runner views: the framework resolves build/... from its cwd.
for n in official candidate; do
  mkdir -p $V/view/$n
  ln -sfn $T/$n/build-cuda $V/view/$n/build
  ln -sfn $T/$n/benchmark $V/view/$n/benchmark
  ln -sfn $T/$n/example $V/view/$n/example
done
ls -la $V/view/*/
find $V -maxdepth 6 -type f \( -name cuda_probe -o -name kernelretsnoop -o -name mem_trace -o -name threadhist \) -newer $V/build-loaders.sh | sort
for n in official candidate; do ls -la $T/$n/example/gpu/cuda-counter/cuda_probe $T/$n/example/gpu/kernelretsnoop/kernelretsnoop; done
git -C $O status --short | head; git -C $D status --short | head
date +%T
