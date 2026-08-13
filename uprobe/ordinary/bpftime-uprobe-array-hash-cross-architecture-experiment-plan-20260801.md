# BPFtime uprobe 普通 array/hash 跨架构路径对照实验设计（2026-08-01）

## 1. 目标

现有 GitHub Actions 结果出现明显的平台趋势差异：

- x64：普通 array/hash 的四项中，除 array lookup 外，BPFtime 都快于 kernel；
- ARM64：只有 hash update 快于 kernel，array lookup、array update、hash lookup
  都慢于 kernel。

本实验不再只比较顶层 `BPFtime / kernel` 倍率，而是把分子和分母拆开，回答：

1. BPFtime userspace map helper 在 ARM64 上是否执行了更多工作；
2. 相同 BPFtime helper 在 ARM64/x64 上的指令、周期和时间是否接近；
3. kernel map BPF 程序在 x64 上是否确实比 ARM64 更重；
4. 趋势翻转来自 BPFtime 分子、kernel 分母，还是两者共同作用。

## 2. 范围

第一轮只看普通 map 的四个项目：

```text
array lookup
array update
hash lookup
hash update
```

暂不包含：

- per-CPU array/hash；
- delete-hit/delete-miss；
- `bpf_probe_read_user`、`bpf_probe_write_user`；
- ssl-nginx、perf buffer 和 reader；
- 优化实现。

本地刚完成的 delete 修正不影响上述四项。本实验无需先重跑完整 uprobe
benchmark。

## 3. 总体方法

实验分成两条互相独立的路径：

```text
BPFtime 分子：
  相同 LLVM JIT BPF 字节码
    ├─ 注册 no-op helper
    └─ 注册真实 array/hash helper
  两者相减得到 BPFtime helper 净成本

kernel 分母：
  相同 uprobe 和 1000 次循环结构
    ├─ loop/control BPF 程序
    └─ 真实 kernel map helper BPF 程序
  两者相减得到 kernel map 操作净成本
```

BPFtime 侧以 LLVM JIT helper A/B 为主结果，L0–L3 direct 分层用于解释成本落在哪
个 C++ runtime 层。kernel 侧以 BPF runtime stats 为主，`perf` kernel counters
作为受控机器上的补充。

## 4. 平台与可比性要求

### 4.1 平台

- ARM64：Jetson Orin Nano；
- x64：可控的物理 x64 机器或 self-hosted runner；
- 两端都直接在 host 运行，不使用 QEMU；
- PMU/perf 必须在两端实际可用。

### 4.2 软件配置

两端必须记录并尽量对齐：

```text
git commit / git diff
kernel version and config
GCC version
LLVM/Clang version
Boost version
CMake cache
LTO on/off
LLVM JIT backend
probe read/write check options
CPU governor / frequency / turbo
NUMA node and pinned CPU
```

建议固定为当前 Jetson 诊断环境：

```text
RelWithDebInfo
LLVM 15 JIT
GCC 13
Boost 1.83
BPFTIME_ENABLE_LTO=ON
ENABLE_PROBE_WRITE_CHECK=OFF
ENABLE_PROBE_READ_CHECK=OFF
```

如果 x64 无法完全使用相同 kernel/config，结果应描述为“两个受控平台的路径
差异”，不能只归因为 ISA。BPFtime userspace A/B 不依赖 kernel map 实现，因此
它比顶层 kernel/BPFtime 倍率更适合判断 BPFtime 分子是否存在 ARM 特有退化。

### 4.3 运行状态

- 固定被测进程到一个物理核心；
- 避免使用 SMT sibling；
- governor 使用 performance；
- 尽可能固定频率；
- x64 turbo 要么关闭，要么保持一致并记录；
- Jetson 使用固定 nvpmodel 和 `jetson_clocks`；
- 测试期间不运行其他 benchmark、容器和后台编译；
- 每个平台选择固定 CPU，所有测试都使用同一个 CPU。

## 5. 构建

使用同一份源码和诊断 target：

```bash
cmake -S . -B build-map-path \
  -DLLVM_DIR=/usr/lib/llvm-15/cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBPFTIME_LLVM_JIT=1 \
  -DBPFTIME_ENABLE_LTO=1 \
  -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO \
  -DENABLE_PROBE_WRITE_CHECK=0 \
  -DENABLE_PROBE_READ_CHECK=0

cmake --build build-map-path --config RelWithDebInfo \
  --target \
    array-map-path-layers \
    array-helper-jit-layers \
    hash-map-path-layers \
    hash-helper-jit-layers \
  -j"$(nproc)"
```

诊断调用者关闭 IPO，runtime 本身保持 LTO。这一设置不能在单个平台上单独改变。

构建后保存：

```bash
git rev-parse HEAD
git status --short
gcc --version
/usr/lib/llvm-15/bin/clang --version
cmake -LA -N build-map-path
ldd build-map-path/benchmark/uprobe/diagnostics/array-helper-jit-layers
```

## 6. 阶段 A：BPFtime LLVM JIT helper/no-op A/B

### 6.1 原理

no-op 和真实 helper 使用字节级相同的 BPF 指令流、相同循环、相同 helper ID 和
相同 JIT→C ABI，只替换 VM 注册的 C 函数地址：

```text
real-minus-noop
  ≈ helper wrapper + shm/fd/handler + map implementation
```

这比从完整 uprobe 总时间中减去 `__bench_uprobe` 更严格，因为 BPF 栈上的
key/value 准备、JIT 循环和 helper call 指令完全一致。

### 6.2 执行矩阵

| map | operation | no-op | real |
|---|---|---|---|
| array | lookup | `noop lookup` | `array lookup` |
| array | update | `noop update` | `array update` |
| hash | lookup-hit | `noop lookup` | `hash lookup` |
| hash | update-existing | `noop update` | `hash update` |

array 每轮使用 100000 次 JIT invocation，每次 BPF 程序调用 1000 次 helper；hash
每轮使用 20000 次 invocation，避免单项耗时过长。两端参数必须相同。

命令模板：

```bash
CPU=<pinned-cpu>
BIN=build-map-path/benchmark/uprobe/diagnostics

"$BIN/array-helper-jit-layers" noop  lookup 100000 3 "$CPU"
"$BIN/array-helper-jit-layers" array lookup 100000 3 "$CPU"
"$BIN/array-helper-jit-layers" noop  update 100000 3 "$CPU"
"$BIN/array-helper-jit-layers" array update 100000 3 "$CPU"

"$BIN/hash-helper-jit-layers" noop lookup 20000 3 "$CPU"
"$BIN/hash-helper-jit-layers" hash lookup 20000 3 "$CPU"
"$BIN/hash-helper-jit-layers" noop update 20000 3 "$CPU"
"$BIN/hash-helper-jit-layers" hash update 20000 3 "$CPU"
```

每个命令独立重复 5 次。不要把 no-op 五次全部跑完后再跑 real；应按
`noop → real → noop → real` 交替，减小温度和频率漂移。

### 6.3 perf 采集

```bash
PERF=<real-perf-path>

"$PERF" stat -x, \
  -e task-clock,cycles,instructions,branches,branch-misses,cache-references,cache-misses \
  -- "$BIN/array-helper-jit-layers" array lookup 100000 3 "$CPU"
```

其余组合使用相同事件。保存完整 stderr，不只保存汇总数字。

perf 包含程序初始化、JIT 编译、1M helper warm-up 和正式轮次。因此硬件计数按
同一操作的 real/no-op raw counter 相减，再按总 helper 数归一化：

```text
array perf normalization = 1M warm-up + 3 × 100M = 301M helpers
hash  perf normalization = 1M warm-up + 3 × 20M  =  61M helpers

delta_instructions/helper
  = (real_instructions - noop_instructions) / total_helpers

delta_cycles/helper
  = (real_cycles - noop_cycles) / total_helpers
```

wall time 使用程序输出的三个正式 round，不包含 warm-up：

```text
delta_ns/helper
  = mean(real ns/helper) - mean(noop ns/helper)
```

每个平台最终使用 5 个独立进程结果的中位数，同时报告 min/max 或 MAD。

### 6.4 必须输出的指标

| 指标 | 用途 |
|---|---|
| net ns/helper | 实际时间成本 |
| net cycles/helper | 与 CPU 频率相对独立的执行成本 |
| net instructions/helper | 动态路径长度 |
| cycles/instruction | 粗略执行效率 |
| branch misses/helper | hash/probe 控制流代价 |
| cache misses/helper | 是否存在明显 cache 问题 |

ARM64 与 x64 是不同 ISA，不能把“动态指令条数相同”作为必要条件，也不能直接说
某条 x86 指令与某条 ARM 指令等价。instructions 用于判断路径是否出现数量级
变化；跨 ISA 的核心判断以净时间、净周期、层级占比和源码路径为主。

## 7. 阶段 B：BPFtime direct L0–L3 分层

### 7.1 分层定义

| 层 | array/hash 入口 | 主要内容 |
|---|---|---|
| control | 相同循环和间接调用 | 不访问 map |
| lock | hash 独有 | uncontended spin lock/unlock |
| L0 | 具体 `elem_lookup/update` | array 索引/copy 或 hash/probe/copy |
| L1 | `bpf_map_handler::map_*` | map type switch、锁策略、`offset_ptr` |
| L2 | `bpftime_shm::bpf_map_*` | fd、handler variant、共享内存分发 |
| L3 | `bpftime_map_*_helper` | 生产 helper 入口 |

### 7.2 命令模板

array：

```bash
for op in lookup update; do
  for layer in control l0 l1 l2 l3; do
    "$PERF" stat -x, \
      -e task-clock,cycles,instructions,branches,branch-misses,cache-references,cache-misses \
      -- "$BIN/array-map-path-layers" "$layer" "$op" 100000000 3 "$CPU"
  done
done
```

hash：

```bash
for op in lookup update; do
  for layer in control lock l0 l1 l2 l3; do
    "$PERF" stat -x, \
      -e task-clock,cycles,instructions,branches,branch-misses,cache-references,cache-misses \
      -- "$BIN/hash-map-path-layers" "$layer" "$op" 10000000 3 "$CPU"
  done
done
```

归一化次数：

```text
array direct = 1M warm-up + 3 × 100M = 301M operations
hash direct  = 1M warm-up + 3 × 10M  =  31M operations
```

逐层计算：

```text
L0 - control  ≈ 具体数据结构本体
L1 - L0       ≈ handler/type/lock 层
L2 - L1       ≈ fd/variant/shared-memory 层
L3            ≈ 最终生产 helper 入口
```

runtime 开启 LTO 后，L3 可能因跨函数内联而短于独立导出的 L2。不能机械地把
`L3-L2` 解释成 wrapper 成本；L0–L2 用于分层，L3/JIT A/B 用于表示生产路径。

## 8. 阶段 C：kernel map BPF runtime A/B

### 8.1 为什么需要单独设计

现有顶层数据包含：

```text
victim 函数 + uprobe 触发 + kernel BPF 程序 + 1000 次 map helper
```

直接减 `__bench_uprobe` 只能近似排除基础 hook，因为两个 BPF 程序的循环和
key/value 准备并不相同。kernel 侧应增加与 real 程序匹配的 loop-control：

```text
array_lookup_control：1000 次循环 + key 准备 + barrier，不调用 helper
array_lookup_real：   1000 次循环 + key 准备 + map lookup

array_update_control：1000 次循环 + key/value 准备 + barrier
array_update_real：   1000 次循环 + key/value 准备 + map update

hash lookup/update 使用相同结构和确定的 map 前置状态。
```

control 中需要使用 BPF inline barrier 或可验证的结果消费，防止 Clang 删除循环。
真实 hash 状态固定为 lookup-hit、update-existing；每轮开始前验证 key `0..999`
均存在。

### 8.2 BPF runtime stats

在受控机器上启用：

```bash
sudo sysctl -w kernel.bpf_stats_enabled=1
sudo bpftool -j prog show
```

victim 必须支持一次只运行一个目标函数，不能像当前完整 benchmark 一样把所有
操作连续跑完后只得到进程级总计数。对每个 control/real 程序分别记录运行前后：

```text
run_cnt
run_time_ns
```

计算：

```text
avg_program_ns = delta(run_time_ns) / delta(run_cnt)

kernel_map_ns/helper
  = (real_avg_program_ns - control_avg_program_ns) / 1000
```

`run_time_ns` 是 kernel 的软件运行时间统计，不依赖 PMU。它用于回答 kernel BPF
程序本体是否在 x64 上更重。

### 8.3 kernel perf counters

物理机支持 PMU 时，再对一次只运行一个程序的 victim 使用：

```bash
sudo "$PERF" stat -x, \
  -e cycles:k,instructions:k,branches:k,branch-misses:k,cache-references:k,cache-misses:k \
  -- <filtered-victim-command>
```

real-control 相减后按 helper 数归一化。该计数还可能包含 uprobe kernel hook 和
统计框架本身，因此必须使用相同 attach、相同调用次数的 control，不能拿未 attach
的 baseline 直接相减。

## 9. 数据表

### 9.1 BPFtime JIT A/B

| 操作 | ARM ns | x64 ns | ARM cycles | x64 cycles | ARM instructions | x64 instructions |
|---|---:|---:|---:|---:|---:|---:|
| array lookup | | | | | | |
| array update | | | | | | |
| hash lookup-hit | | | | | | |
| hash update-existing | | | | | | |

### 9.2 BPFtime 层级占比

| 操作/平台 | L0 | lock | handler | fd/variant | final JIT helper |
|---|---:|---:|---:|---:|---:|
| array lookup ARM | | — | | | |
| array lookup x64 | | — | | | |
| array update ARM | | — | | | |
| array update x64 | | — | | | |
| hash lookup ARM | | | | | |
| hash lookup x64 | | | | | |
| hash update ARM | | | | | |
| hash update x64 | | | | | |

### 9.3 Kernel BPF A/B

| 操作 | ARM ns/helper | x64 ns/helper | ARM cycles/helper | x64 cycles/helper |
|---|---:|---:|---:|---:|
| array lookup | | | | |
| array update | | | | |
| hash lookup-hit | | | | |
| hash update-existing | | | | |

## 10. 结论判据

### 情况 A：BPFtime 两端接近，kernel x64 明显更重

```text
BPFtime ARM ≈ BPFtime x64
kernel x64 >> kernel ARM
```

结论：趋势翻转主要来自 kernel 分母。BPFtime 在 ARM 上不是突然退化，而是 ARM
kernel 路径更轻，BPFtime 的固定 userspace map 成本无法被省掉的 uprobe/kernel
成本覆盖。

### 情况 B：BPFtime ARM cycles 明显更多，但 instructions 接近

结论：BPFtime 执行同一条逻辑路径，但 ARM 上 IPC、分支、load/store 或共享内存
访问效率更差，需要继续看 branch miss、cache、生成汇编和数据布局。

### 情况 C：BPFtime ARM instructions 也明显更多

结论：存在架构相关代码生成、条件分支或运行时路径差异。下一步对照 LLVM JIT
汇编和 L0–L3 每层的动态指令增量。

### 情况 D：BPFtime 和 kernel 两侧都发生明显变化

结论：跨平台倍率由分子和分母共同决定，不能用单一的“kernel 分母效应”或
“ARM BPFtime 路径重”概括，需要按操作分别报告。

## 11. 推荐执行顺序

为尽快得到决定性证据，按以下顺序执行：

1. x64 运行四项 LLVM JIT helper/no-op A/B wall time；
2. x64 运行同四项 JIT A/B perf counters；
3. x64 运行 array/hash direct L0–L3；
4. 将 x64 数据与现有 Jetson 同 harness 数据对齐；
5. 实现 kernel loop-control 和 filtered victim；
6. 两平台采集 BPF runtime stats；
7. 两台物理机都支持 PMU 时补 kernel `cycles:k/instructions:k`；
8. 只有出现无法解释的层级差异时，再看 JIT/内核 BPF 汇编。

前三步已经足以回答 BPFtime 分子是否存在 ARM 特有退化；第五至第七步用于严格
解释 kernel 分母为什么在 x64/ARM 上不同。无需先跑完整 10 轮 uprobe benchmark。

## 12. 结果归档

建议目录：

```text
benchmark-results/uprobe/map-path-cross-arch-20260801/
├── arm64/
│   ├── environment.txt
│   ├── jit-helper-ab.csv
│   ├── direct-layers.csv
│   └── kernel-bpf-ab.csv
├── x64/
│   ├── environment.txt
│   ├── jit-helper-ab.csv
│   ├── direct-layers.csv
│   └── kernel-bpf-ab.csv
└── README.md
```

原始 perf 输出必须保留，CSV 只作为派生汇总。每个结果文件记录 commit、dirty
diff hash、build options、CPU、kernel、运行次数和归一化分母。
