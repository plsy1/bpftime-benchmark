# BPFtime uprobe 与 per-CPU map 跨架构调查（2026-07-28）

## 技术摘要

2026-07-28 的调查完成了两项确认：

1. Jetson 上原版与修复版 `benchmark/uprobe` 的显著差异只集中在
   `bpf_probe_read_user` / `bpf_probe_write_user` 路径。`ead56c9` 将 SIGSEGV
   handler 从“每次 helper 调用重新安装”改为“每线程只安装一次”后，
   `__bench_read` 和 `__bench_write` 分别加速约 **34.83×** 和 **33.38×**；
   基础 uprobe、普通 map 和 per-CPU map 基本不变。
2. GitHub Actions 同一次 10 轮测试中，ARM64 的 6 个 `__bench_per_cpu*` 项，
   `BPFtime / Kernel` 倍率都大于 x64。但这主要是**分母效应**：
   ARM64 kernel per-CPU map 比 x64 kernel 快约 **1.68–5.80×**，而 BPFtime
   的 ARM64 绝对耗时在 6 项中的 5 项反而低于 x64。

因此，当前证据支持：

> BPFtime 的 userspace per-CPU map 路径比 kernel 原生实现重；ARM64 上更大的
> 相对差距主要来自 ARM64 kernel 基线更快，不能解释为“BPFtime 的 per-CPU
> 实现在 ARM 上绝对执行得更慢”。

本次 per-CPU map 路径只读取当前 CPU（`sched_getcpu()`），不执行
`sched_setaffinity()`，与 `bpf_perf_event_output()` 中已删除的临时绑核序列
没有直接关系。

## 调查范围与证据

### Jetson 原版与修复版

- 平台：Jetson Orin Nano，Linux `6.8.12-1021-tegra`
- benchmark：`benchmark/uprobe/benchmark.py`
- 每组：默认 10 轮
- 原版结果：
  `benchmark-results/uprobe/local-original-6bd5319-20260728_135328/`
- workflow 修复版结果：
  `benchmark-results/uprobe/workflow-image-ead56c9-20260728_133118/`
- 原版 GHCR 镜像复核：
  `benchmark-results/uprobe/original-52ddee-uprobe-20260728_165049/`

`52ddee1` 是官方基线加 Jetson no-BTF/Docker 构建适配，不包含后续的 software
perf record 对齐修复、event-output affinity 删除和 SIGSEGV handler 修复。
它与本地原版 `6bd5319` 的关键结果差异小于 0.1%，说明原版镜像基线复现正确。

### GitHub Actions ARM64 与 x64

- Workflow run：
  <https://github.com/plsy1/bpftime-benchmark/actions/runs/30340475614>
- 代码版本：`codex/official-no-btf`，包含 `ead56c9`
- ARM64：Linux `6.17.0-1020-azure`，4 threads
- x64：Linux `6.17.0-1020-azure`，AMD EPYC 7763，2 cores / 4 threads
- 每个平台：kernel uprobe 10 轮 + BPFtime userspace uprobe 10 轮

两边在线 CPU 数相同，所以本次差异不能直接归因于 per-CPU map 为更多 CPU
分配了更多 value slot。但两边仍是不同指令集、CPU 微架构和云 runner，绝对
时间不应当直接当作架构优劣结论。

## 测量定义

`uprobe.bpf.c` 中每个 map benchmark 的 BPF 程序都执行：

```c
for (int i = 0; i < 1000; i++) {
    bpf_map_lookup_elem(...);
    // 或 update/delete
}
```

结果 JSON 中的 `avg` 是**一次 benchmark 函数触发的总时间**，其中包含 1000 次
map helper 调用；它不是单次 helper 的时间。本文保留 benchmark 原始口径，
表中用 `µs / 1000 helpers` 表示，倍率不受是否除以 1000 的影响。

`__bench_uprobe` 没有上述 1000 次循环，不能把它的绝对时间与 map 项直接相减。

## SIGSEGV handler 修复只改变 read/write helper

以下为 Jetson 10 轮平均值，单位 ns，越低越好：

| BPFtime userspace 操作 | 原版 `6bd5319` | 修复版 `ead56c9` | 变化 |
|---|---:|---:|---:|
| `__bench_uprobe` | 367.17 | 366.56 | -0.17% |
| `__bench_uretprobe` | 364.29 | 364.58 | +0.08% |
| `__bench_uprobe_uretprobe` | 420.24 | 420.54 | +0.07% |
| `__bench_read` | 346,149.46 | 9,938.56 | **-97.13%，34.83× 加速** |
| `__bench_write` | 345,001.05 | 10,336.14 | **-97.00%，33.38× 加速** |
| `__bench_per_cpu_array_map_lookup` | 18,307.80 | 18,342.90 | +0.19% |
| `__bench_per_cpu_hash_map_lookup` | 136,637.86 | 136,262.07 | -0.28% |

除高波动的 `__bench_per_cpu_hash_map_delete` 外，其余 map 项变化约在
`-0.84%` 到 `+1.29%` 内。因此：

- SIGSEGV handler 修复对 probe read/write helper 有决定性影响；
- 它不构成 per-CPU map 性能优化；
- 不能用该修复解释 `__bench_per_cpu*` userspace 偏慢现象。

## ARM64 的六项相对差距都大于 x64

以下为 Actions run `30340475614` 的 10 轮平均值。
单位为 `µs / 1000 helpers`，`BPFtime / Kernel` 大于 1 表示 BPFtime 更慢。

| per-CPU 操作 | ARM Kernel | ARM BPFtime | ARM B/K | x64 Kernel | x64 BPFtime | x64 B/K |
|---|---:|---:|---:|---:|---:|---:|
| hash update | 36.75 | 109.10 | **2.97×** | 101.79 | 116.93 | **1.15×** |
| hash lookup | 14.44 | 61.89 | **4.29×** | 42.00 | 73.88 | **1.76×** |
| hash delete | 23.51 | 84.40 | **3.59×** | 39.60 | 102.46 | **2.59×** |
| array update | 7.71 | 35.18 | **4.56×** | 44.03 | 31.40 | **0.71×** |
| array lookup | 1.74 | 11.31 | **6.51×** | 3.11 | 13.72 | **4.41×** |
| array delete | 1.61 | 6.64 | **4.13×** | 9.33 | 7.29 | **0.78×** |

表面结论是：

- ARM64 上 6 项全部为 BPFtime 更慢，约为 kernel 的 **2.97–6.51×**；
- x64 上 hash 三项和 array lookup 较慢；
- x64 的 array update/delete 在这次 run 中反而比 kernel 快。

但这个表是平台内部的相对倍率，不能单独说明 BPFtime 的 ARM64 实现更慢。

## 更大的 ARM 倍率主要是 kernel 分母更快

观察绝对时间：

- BPFtime ARM64 在 6 个 per-CPU 项中的 5 项比 x64 更快；
- 唯一例外是 array update：ARM 35.18 µs，x64 31.40 µs；
- kernel ARM64 在 6 项中全部比 x64 快，幅度约 **1.68–5.80×**。

两个代表性例子：

### per-CPU hash lookup

```text
BPFtime: ARM 61.89 µs，x64 73.88 µs
Kernel:  ARM 14.44 µs，x64 42.00 µs
比值:    ARM 4.29×，x64 1.76×
```

BPFtime ARM 的绝对时间更低，但 kernel ARM 快得更多，导致 B/K 倍率更大。

### per-CPU array update

```text
BPFtime: ARM 35.18 µs，x64 31.40 µs
Kernel:  ARM  7.71 µs，x64 44.03 µs
比值:    ARM 4.56×，x64 0.71×
```

这里 BPFtime ARM 确实比 x64 慢约 12%，但真正让结论翻转的是 kernel：
ARM kernel 比 x64 kernel 快约 5.7×。

所以跨架构数据最稳妥的解释是：

> ARM64 上 kernel 原生 per-CPU map helper 的基线非常快，放大了 BPFtime
> userspace 共享内存实现的相对劣势。

## per-CPU 相对普通 map 的额外成本并非普遍在 ARM 更大

为了减少 kernel 分母影响，下面只比较 BPFtime userspace 内部：
`per-CPU map / 同类型普通 map`。

| BPFtime 操作 | ARM per-CPU / 普通 | x64 per-CPU / 普通 |
|---|---:|---:|
| hash update | 3.83× | 4.25× |
| hash lookup | 2.75× | 3.16× |
| hash delete | 6.50× | 8.06× |
| array update | 3.69× | 2.81× |
| array lookup | 1.71× | 1.86× |
| array delete | 1.00× | 1.00× |

结果说明：

- hash 三项的 per-CPU 额外倍率在 x64 反而更大；
- array lookup 的额外倍率两边接近，ARM 略低；
- 只有 array update 显示 ARM 的 per-CPU 额外倍率更大；
- 因而当前数据不支持“BPFtime per-CPU 实现在 ARM 上整体更低效”的概括。

## BPFtime per-CPU 路径为什么比普通 map 重

### per-CPU array

`per_cpu_array_map_impl::elem_lookup/update()` 每次执行：

```text
my_sched_getcpu()
  -> 读取当前 CPU
  -> 检查 key
  -> data_at(key, cpu)
  -> update 时复制 value
```

底层数据位于 Boost.Interprocess 管理的共享内存 vector 中。

### per-CPU hash

`per_cpu_hash_map_impl::elem_lookup/update/delete()` 每次执行：

```text
my_sched_getcpu()
  -> key_templates[cpu]
  -> 将 key 复制到共享内存 vector
  -> Boost shared-memory hash lookup
  -> 选择 value_size * cpu 对应区域
```

update 还会复制 value；插入新 key 时会创建覆盖所有在线 CPU 的完整 value
vector。相比 kernel 原生 per-CPU map，BPFtime 多了 CPU 查询、通用 helper
dispatch、共享内存容器和 vector/hash 操作。

这些结构性成本可以解释 BPFtime per-CPU map 为什么通常比 kernel 重，但当前
汇总吞吐数据还不能把成本进一步精确分配到 `sched_getcpu()`、helper dispatch、
Boost.Interprocess、hash 或内存复制中的某一项。

## 与已删除 affinity 路径无关

per-CPU map 的 `ensure_on_current_cpu()` 名称容易造成误解。当前实现只是：

```cpp
return func(my_sched_getcpu());
```

它没有执行：

```text
sched_getaffinity()
sched_setaffinity()
```

因此：

- `__bench_per_cpu*` 测到的是 CPU 编号查询和 per-CPU map 数据访问；
- 不是 `bpf_perf_event_output()` 中曾经存在的临时 pin/restore；
- 删除 event-output affinity 不会直接改善 `benchmark/uprobe` 的 per-CPU 项。

## 测量限制与新发现

### 1. array delete 不是有效的删除性能

`per_cpu_array_map_impl::elem_delete()` 不支持删除，直接设置 `EINVAL` 并返回
`-1`。因此 `__bench_per_cpu_array_map_delete` 测的是 helper 调度和错误返回路径，
不能用于评价真实 per-CPU array 删除。

### 2. per-CPU hash delete 波动较大

- ARM BPFtime：CV **13.71%**
- x64 BPFtime：CV **9.71%**

其它大部分 per-CPU 项 CV 在 3% 内；x64 array update 是另一个例外，
CV 为 **18.16%**。涉及这些项的细粒度排序不应过度解释。

### 3. per-CPU hash delete 的实现语义需要单独确认

当前 `elem_delete()` 找到 key 后执行：

```cpp
std::fill(itr->second.begin(),
          itr->second.begin() + cpu * value_size, 0);
```

它没有从 hash map 中删除 key，而且填零范围随当前 CPU 编号变化；CPU 0 时甚至
是空区间。这与通常理解的 per-CPU hash `bpf_map_delete_elem()` 语义不一致，
也可能参与造成该项波动。

本文只将其记录为**待确认的正确性问题**，尚未通过专门单元测试验证，因此不把它
作为本次跨架构性能差异的既定根因。

### 4. 跨架构仍缺少硬件归一化

Actions 的 ARM64 与 x64 runner 虽然都是 Azure Linux `6.17.0-1020-azure`、
4 threads，但 CPU 微架构、频率、虚拟化宿主和 kernel BPF JIT 后端不同。
本次结果适合说明当前 runner 上的数量级与相对关系，不足以形成普适的 ISA 因果
结论。

## 已确认与尚未确认

### 已确认

1. 原版 GHCR 镜像正确复现了本地原版 uprobe 基线。
2. `ead56c9` 只显著优化 probe read/write helper，不优化 per-CPU map。
3. ARM64 的 6 个 per-CPU 项，BPFtime/kernel 倍率都大于 x64。
4. 这个现象主要由 ARM64 kernel per-CPU map 基线更快造成。
5. per-CPU map benchmark 与已删除的 event-output 临时绑核路径无关。

### 尚未确认

1. ARM64 kernel per-CPU helper 为什么在该 Azure runner 上明显快于 x64；
2. BPFtime per-CPU 成本中 `sched_getcpu()`、JIT helper dispatch、共享内存容器、
   vector/hash 和复制各自占多少；
3. per-CPU hash delete 是否存在正确性 bug，以及它对波动的贡献；
4. 不同物理 ARM/x64 平台上是否稳定复现相同倍率。

## 后续建议

当前结论足以确认：**per-CPU userspace map 偏慢与 event-output affinity 删除
无关，两者属于不同的实现路径。**

如果继续定位，优先级建议如下：

1. 为 per-CPU hash delete 增加最小正确性测试，先确认 delete 语义；
2. 分别测量普通 map 与 per-CPU map 的 `sched_getcpu()`、lookup、vector copy
   和 shared-memory hash 子路径；
3. 在同一平台对 BPFtime map 实现做消融，避免直接用不同硬件的绝对时间归因；
4. kernel/BPFtime 两侧同时使用 `perf stat/record`，记录 cycles、instructions
   和具体热点；
5. 不再使用 array delete 错误返回项评价真实 map 删除性能。

## 相关提交与文档

- `ead56c9`：每线程只安装一次 probe read/write SIGSEGV handler
- `076e3e4`：删除 `bpf_perf_event_output()` 每事件临时 affinity
- `0fcdb0e`：software perf record 8 字节对齐修复
- `bpftime-perf-event-output-cpu-affinity-explanation-20260728.md`
- `archive/bpftime-perf-event-output-affinity-redundancy-20260727.md`
- `bpftime-arm-performance-root-cause-20260727.md`
