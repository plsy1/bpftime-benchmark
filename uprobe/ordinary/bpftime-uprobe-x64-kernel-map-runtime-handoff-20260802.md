# x64 普通 map kernel runtime A/B 执行交接（2026-08-02）

## 任务目标

在 x64 物理机上复现 Jetson 已完成的第二阶段实验，只测 kernel eBPF 中普通
array/hash map 的四条路径：

- array lookup-hit；
- array update-existing；
- hash lookup-hit；
- hash update-existing。

使用 matched control/real BPF program 的 `run_time_ns/run_cnt` 差值，回答 x64
kernel map 路径是否比 ARM64 更重。本阶段不跑完整 uprobe benchmark，不测
per-CPU map、delete、ssl-nginx，也不使用 Docker。

## 代码与基线

使用仓库和分支：

```text
https://github.com/plsy1/bpftime-benchmark.git
branch: codex/official-no-btf
minimum commit: 8ed291e
```

诊断代码位于：

```text
benchmark/uprobe/diagnostics/kernel_map_runtime.bpf.c
benchmark/uprobe/diagnostics/kernel_map_runtime.c
benchmark/uprobe/diagnostics/kernel_map_runtime_victim.c
```

先确认当前提交包含 `8ed291e`，并记录工作树状态。不要在存在不明本地修改的目录中
强制清理；必要时新建干净 clone。

## 从空 Ubuntu x64 主机准备环境

本交接不假设机器上已经存在源码或构建依赖。先安装这个独立 kernel 诊断目标所需
的最小依赖：

```bash
sudo apt-get update
sudo apt-get install -y \
  ca-certificates git build-essential make pkg-config \
  clang-15 llvm-15 \
  libelf-dev zlib1g-dev libzstd-dev flex bison python3
```

这里不需要构建完整 BPFtime runtime，因此不需要先安装 Boost、CMake，也不需要
下载预构建 Docker 镜像。Makefile 会从仓库子模块构建本实验使用的 libbpf 和
bpftool。

如果系统软件源没有 `clang-15`/`llvm-15`，不要静默改用其他版本。先记录发行版，
再安装 LLVM 15，或者明确记录实际替代版本。跨架构比较应优先保持 BPF Clang 15
一致。

拉取包含诊断代码的分支：

```bash
mkdir -p "$HOME/src"
cd "$HOME/src"

git clone --branch codex/official-no-btf \
  --single-branch --recurse-submodules \
  https://github.com/plsy1/bpftime-benchmark.git \
  bpftime-official-no-btf

cd "$HOME/src/bpftime-official-no-btf"
git submodule update --init --recursive
git rev-parse HEAD
git merge-base --is-ancestor 8ed291e HEAD
git status --short
```

`git merge-base --is-ancestor` 返回 0 且工作树为空，才继续构建。当前分支可以比
`8ed291e` 更新，但必须仍包含该诊断提交；将最终实际 commit 写入结果。

运行权限和 kernel 前置检查：

```bash
test "$(uname -m)" = x86_64
test -e /proc/sys/kernel/bpf_stats_enabled
sudo true
```

如果第二项不存在，说明当前 kernel 没有提供本实验依赖的 BPF runtime stats，不能
用普通 wall time 替代后继续声称测得相同指标。

## 环境记录

执行前保存以下信息：

```bash
git rev-parse HEAD
git status --short
uname -a
lscpu
clang --version
gcc --version
bpftool version
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true
cat /sys/devices/system/cpu/intel_pstate/no_turbo 2>/dev/null || true
```

选择一个空闲物理核心，避免使用正在处理系统任务的 CPU，并尽量避开同一核心的
SMT sibling。记录所选逻辑 CPU。测试期间保持 governor、turbo 状态不变，不要同时
运行其他 benchmark 或编译任务。

## 构建

初始化子模块并使用仓库原有 Makefile 构建独立诊断目标：

```bash
git submodule update --init --recursive
make -C benchmark/uprobe \
  CLANG=clang-15 \
  kernel-map-runtime-diagnostic \
  -j"$(nproc)"
```

应生成：

```text
benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime
benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim
```

构建失败时先保留完整输出并定位依赖或工具链问题，不要改动实验语义、循环次数或
BPF 程序来绕过错误。

## 正式执行

以下参数必须与 Jetson 保持一致：

```text
20000 BPF invocations/round
5 rounds
1000 helpers/invocation
1000 warm-up invocations
lookup-hit
update-existing
```

设置一个选定的 CPU，例如：

```bash
CPU=<空闲且避开 SMT sibling 的逻辑 CPU>
LOADER=benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime
VICTIM=benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim
```

记录原来的 BPF runtime stats 状态，执行后必须恢复：

```bash
OLD_BPF_STATS=$(cat /proc/sys/kernel/bpf_stats_enabled)
restore_bpf_stats() {
  sudo sysctl -w kernel.bpf_stats_enabled="$OLD_BPF_STATS"
}
trap restore_bpf_stats EXIT INT TERM

sudo sysctl -w kernel.bpf_stats_enabled=1

sudo "$LOADER" "$VICTIM" 20000 5 "$CPU" 1000 \
  | tee kernel-map-runtime-x64-raw.csv

restore_bpf_stats
trap - EXIT INT TERM
```

如果执行中断，也要恢复 `kernel.bpf_stats_enabled`。loader 会让 victim 固定到指定
CPU，并自动完成八个程序的 attach、warm-up、奇偶轮 control/real 顺序交换以及
map 前置状态恢复。

## 计算口径

原始 CSV 每项包含 control、real 和 `real-minus-control`。每次 BPF program 内执行
1000 次 helper，因此：

```text
avg_program_ns = delta(run_time_ns) / delta(run_cnt)

kernel net ns/helper
  = (real avg ns/invocation - control avg ns/invocation) / 1000
```

对每个操作的 5 个 `real-minus-control` 结果报告：

- mean；
- standard deviation；
- CV；
- min/max。

不得把 victim wall time、完整 uprobe benchmark 时间或进程级 `perf stat` 时间混入
这张表。

## ARM64 对照基线

Jetson 使用同一 harness 得到：

| 操作 | ARM64 kernel net ns/helper |
|---|---:|
| array lookup-hit | 1.384079 |
| array update-existing | 10.951741 |
| hash lookup-hit | 26.768945 |
| hash update-existing | 90.822847 |

x64 完成后逐项计算：

```text
x64 / ARM64 = x64 kernel net ns/helper / ARM64 kernel net ns/helper
```

这项比率只能说明两个受控平台上的 kernel BPF map 路径差异；由于 CPU、kernel 和
配置不同，不应把全部差异简单归结为 ISA。

## 结果归档

至少保存：

```text
benchmark-results/uprobe/kernel-map-runtime-x64-<date>/
├── README.md
├── environment.txt
├── raw.csv
└── summary.csv
```

`README.md` 需要记录 commit、kernel、CPU、固定 CPU、工具链、运行参数、是否启用
SMT/turbo，以及与 ARM64 的逐项比率。

## 验收标准

- 四项操作均有 5 个有效的 `real-minus-control` 样本；
- 每轮 `run_cnt` 与 20000 次 invocation 一致；
- lookup 全部为 hit，update 全部为 existing；
- `kernel.bpf_stats_enabled` 已恢复原值；
- 原始数据与汇总计算可相互复核；
- 没有使用 Docker，也没有改动诊断程序语义；
- 最终明确报告各项 x64/ARM64 比率，而不是只比较顶层 BPFtime/kernel 百分比。

## 后续判断

如果 x64 的 array update、hash lookup、hash update 明显高于 ARM64，这将直接支持
“x64 顶层结果更有利于 BPFtime，至少部分来自 x64 kernel map 分母更重”。如果
x64 kernel 数据并未更重，再执行跨架构计划中的 x64 BPFtime JIT helper/no-op A/B
和 direct L0–L3，检查 BPFtime 分子是否发生平台差异。
