# x64 per-CPU hash delete 修复后单轮验证交接

日期：2026-08-04

## 任务目标

在此前正式 x64 uprobe 实验使用的同一台物理机上，使用已修正
`per_cpu_hash_map_impl::elem_delete()` 语义的代码，分别运行 1 个 kernel victim
和 1 个 BPFtime victim，得到修复后的 `__bench_per_cpu_hash_map_delete` 单轮结果。

本轮只用于确认 x64 上修复后的方向和量级，不把单轮数值写成正式均值。不要运行
Docker，不修改 benchmark 循环、setup、BPF 程序或 map 参数。

## 必须使用的代码

```text
repository: https://github.com/plsy1/bpftime-benchmark.git
branch: codex/official-no-btf
commit: c6e2737aabddb23a9915f5eec2fe890f2c3fba07
```

该提交包含两部分：

1. `per_cpu_hash_map_impl::elem_delete()` 找到 key 后执行 `impl.erase(itr)`，删除
   整条 per-CPU hash entry；
2. key 不存在时返回 `-1` 并设置 `errno=ENOENT`，同时增加对应单元测试。

`8ed291e` 已经为普通 hash 和 per-CPU hash 的 delete benchmark 同时增加计时区外
setup。当前每一个计时样本的顺序都是：

```text
setup：重新插入 key 0..999（不计时）
delete：删除 key 0..999（计时）
```

因此本轮测量的是 1000 次 delete-hit，不是第一遍 hit、后续 miss。

## 现有 x64 基线环境

复用 `uprobe-top-x64-20260803-fixedfreq` 的正式环境：

```text
CPU: Intel Core i7-8750H
kernel: Linux 7.0.0-27-generic x86_64
GCC/G++: 13.4.0
Clang: 15.0.7
Boost: 1.83.0
build type: RelWithDebInfo
BPFTIME_LLVM_JIT: ON
BPFTIME_ENABLE_LTO: ON
ENABLE_PROBE_READ_CHECK: OFF
ENABLE_PROBE_WRITE_CHECK: OFF
logical CPU: 5
SMT sibling: CPU 11，测试期间 offline
frequency: 2.2 GHz，performance governor，turbo disabled
loader/victim privilege: root
```

如果实际机器、kernel、CPU topology 或 build 配置与上述记录不一致，先报告差异，
不要静默替换后继续声称是原 x64 基线的直接延续。

## 第一步：更新代码

在 x64 机器上执行：

```bash
ROOT="$HOME/src/bpftime-offical-no-btf"
cd "$ROOT"

git status --short --branch
git fetch benchmark codex/official-no-btf || \
  git fetch origin codex/official-no-btf
git checkout codex/official-no-btf
git pull --ff-only benchmark codex/official-no-btf || \
  git pull --ff-only origin codex/official-no-btf

git rev-parse HEAD
git status --short
```

验收：

```bash
test "$(git rev-parse HEAD)" = \
  c6e2737aabddb23a9915f5eec2fe890f2c3fba07
git diff --quiet
git diff --cached --quiet
```

如果已有不明本地修改，不要清理或覆盖；改用干净 worktree/clone。

## 第二步：复核并重建正式产物

沿用此前正式实验的 build 目录：

```bash
BUILD="$ROOT/build-map-path-x64-20260802"

grep -E \
  '^(CMAKE_BUILD_TYPE|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|BPFTIME_LLVM_JIT|BPFTIME_ENABLE_LTO|ENABLE_PROBE_READ_CHECK|ENABLE_PROBE_WRITE_CHECK|Boost_INCLUDE_DIR):' \
  "$BUILD/CMakeCache.txt"
```

必须确认仍是上节所列配置。然后只重建受 runtime 修改影响的正式产物：

```bash
cmake --build "$BUILD" \
  --target bpftime-agent bpftime-syscall-server bpftimetool \
  -j2
```

重新构建官方 victim、loader 和 BPF object：

```bash
make -C benchmark test CC=/usr/bin/gcc-13
make -C benchmark/uprobe \
  CC=/usr/bin/gcc-13 \
  CLANG=/usr/lib/llvm-15/bin/clang \
  -j"$(nproc)"
```

确认产物存在：

```bash
test -x benchmark/test
test -x benchmark/uprobe/uprobe
test -r "$BUILD/runtime/agent/libbpftime-agent.so"
test -r "$BUILD/runtime/syscall-server/libbpftime-syscall-server.so"
test -x "$BUILD/tools/bpftimetool/bpftimetool"
```

## 第三步：单轮正式运行

以此前的
`benchmark-results/uprobe/uprobe-top-x64-20260803-fixedfreq/run-official-uprobe-fixedfreq.sh`
为基础执行，不重新发明运行流程，只做以下改动：

```text
RUNS=1
ITER=100000
OUT=$HOME/src/benchmark-results/uprobe/percpu-hash-delete-fix-x64-20260804
EXPECTED_COMMIT=c6e2737aabddb23a9915f5eec2fe890f2c3fba07
```

运行脚本前增加精确 commit 检查：

```bash
if [[ $(git -C "$ROOT" rev-parse HEAD) != "$EXPECTED_COMMIT" ]]; then
  echo "unexpected source commit" >&2
  exit 1
fi
```

其余条件保持原正式实验不变：

- CPU5 固定到 2.2 GHz；
- CPU11 在实验期间 offline，结束后恢复；
- performance governor；
- turbo disabled；
- kernel 与 BPFtime loader 启动后各等待 5 秒；
- loader 和 victim 都固定 CPU5、都以 root 运行；
- kernel loader 必须创建 BPF links；
- BPFtime loader 必须输出 `Starting syscall server`，且 kernel link 数为 0；
- BPFtime victim 必须以 `shm_open_type 1` 打开 shared memory；
- 结束后必须恢复 CPU11、governor、turbo/pstate，并清除 links 和 shared memory。

虽然只需要 per-CPU hash delete 的结果，本轮仍运行未经裁剪的完整官方 victim，
以保持与此前顶层数据相同的 attach 和执行环境。修复后会真正重复分配、插入和删除
共享内存 hash entry，预计运行时间明显长于旧的错误清零路径，不要因此降低
iterations。

## 第四步：解析单轮结果

分别从 `raw/kernel-run-1.txt` 和 `raw/bpftime-run-1.txt` 读取：

```text
__bench_uprobe
__bench_per_cpu_hash_map_delete
```

统一计算：

```text
net ns/helper
  = (__bench_per_cpu_hash_map_delete ns/invocation
     - __bench_uprobe ns/invocation) / 1000
```

报告：

| 项目 | kernel | BPFtime |
|---|---:|---:|
| raw ns/invocation |  |  |
| empty-uprobe ns/invocation |  |  |
| net ns/helper |  |  |

另外单独给出 `BPFtime net ns/helper / kernel net ns/helper`。

不要用 setup 的执行时间减入或加到上述 net metric。setup 只保证每个计时样本开始
前 key 存在，本身位于计时区外。

## Jetson 单轮参照

Jetson Orin Nano、CPU5 1.728 GHz、相同提交和参数得到：

| 路径 | net ns/helper |
|---|---:|
| kernel per-CPU hash delete-hit | 150.214513 |
| BPFtime per-CPU hash delete-hit | 981.381322 |
| BPFtime/kernel | 6.533× |

这是单轮参照，只用于检查 x64 量级和方向，不用于正式跨架构均值结论。

## 结果归档与推送

结果目录：

```text
benchmark-results/uprobe/percpu-hash-delete-fix-x64-20260804/
```

至少包含：

```text
README.md
environment.txt
summary.csv
run.sh
raw/kernel-loader.txt
raw/bpftime-loader.txt
raw/kernel-run-1.txt
raw/bpftime-run-1.txt
raw/kernel-run-1.time.txt
raw/bpftime-run-1.time.txt
```

`README.md` 必须注明这是修复后的单轮 smoke，不是 5 轮正式统计。完成残留状态检查
后，只提交该新目录并推送到 `benchmark-results/jetson`；不要顺带提交其他未跟踪
文件。

## 验收标准

- 源码精确位于 `c6e2737aabddb23a9915f5eec2fe890f2c3fba07`；
- agent、syscall-server、bpftimetool 已从该源码重建；
- kernel、BPFtime 各有 1 个完整官方 victim 输出；
- `__bench_per_cpu_hash_map_delete` 使用计时区外 setup；
- BPFtime 使用 userspace shared-memory runtime，未误挂 kernel BPF links；
- 得到双方 raw 和 empty-subtracted `ns/helper`；
- CPU/SMT/turbo/governor 状态已经恢复；
- kernel links 为 0，`/dev/shm/bpftime_maps_shm` 不存在；
- 新结果目录已独立提交并推送。
