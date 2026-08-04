# Jetson BPFtime benchmark 结果索引

本分支保存 Jetson ARM64 与固定频率 x64 上的 `ssl-nginx`、`benchmark/uprobe`
原始结果、统计汇总、复现脚本和环境记录。分析结论位于
[`summry/jetson`](https://github.com/plsy1/bpftime-benchmark/tree/summry/jetson) 分支。

## 快速入口

| 主题 | 结果入口 | 对应结论 |
|---|---|---|
| uprobe 普通 map 系统调查 | [ARM64/x64 顶层对照](uprobe/uprobe-top-cross-arch-20260803/) | [系统报告](https://github.com/plsy1/bpftime-benchmark/blob/summry/jetson/bpftime-uprobe-ordinary-map-systematic-report-20260804.md) |
| uprobe per-CPU map | [ARM64 路径诊断](uprobe/arm64-bpftime-vs-kernel-paths-20260803/) | [跨架构分析](https://github.com/plsy1/bpftime-benchmark/blob/summry/jetson/bpftime-uprobe-percpu-cross-architecture-analysis-20260728.md) |
| ssl-nginx V1–V4 消融 | [Empty Probe](empty-probe/run01/) · [No-copy/No-output](no-copy_no-output/run01/) · [No-output](no-output/run01/) · [Full aligned](full-aligned/) | [ARM 根因总览](https://github.com/plsy1/bpftime-benchmark/blob/summry/jetson/bpftime-arm-performance-root-cause-20260727.md) |
| software perf record 对齐 | [Full unaligned](full-unaligned/) 与 [Full aligned](full-aligned/) | [对齐修复报告](https://github.com/plsy1/bpftime-benchmark/blob/summry/jetson/archive/bpftime-software-perf-record-alignment-fix-20260722.md) |

## uprobe 数据索引

### 顶层官方 benchmark

| 目录 | 内容 |
|---|---|
| [uprobe-top-arm64-20260803](uprobe/uprobe-top-arm64-20260803/) | Jetson CPU5、5 个独立 victim 的完整官方 uprobe 顶层结果 |
| [uprobe-top-x64-20260803-fixedfreq](uprobe/uprobe-top-x64-20260803-fixedfreq/) | 固定频率 x64 的对应顶层结果 |
| [uprobe-top-cross-arch-20260803](uprobe/uprobe-top-cross-arch-20260803/) | 两个平台统一口径的 raw、net helper 与比较表 |
| [uprobe-top-x64-20260802](uprobe/uprobe-top-x64-20260802/) | x64 早期顶层实验，保留用于审计 |

### BPFtime helper 与 userspace map 路径

| 目录 | 内容 |
|---|---|
| [helper-map-ladder-arm64-20260804](uprobe/helper-map-ladder-arm64-20260804/) | Jetson matched helper ladder、direct L0–L3 与 PMU |
| [array-path-arm-diagnosis-20260801](uprobe/array-path-arm-diagnosis-20260801/) | Jetson array map userspace 路径分解 |
| [hash-path-arm-diagnosis-20260801](uprobe/hash-path-arm-diagnosis-20260801/) | Jetson hash map userspace 路径分解 |
| [map-path-x64-20260802](uprobe/map-path-x64-20260802/) | x64 对应 map 路径数据 |
| [hash-l0-perf-x64-20260802](uprobe/hash-l0-perf-x64-20260802/) | x64 hash L0 microbenchmark 与 perf 数据 |
| [top-hash-residual-arm64-20260803](uprobe/top-hash-residual-arm64-20260803/) | Jetson hash lookup 顶层 matched 边界与 PMU |

### Kernel map runtime 与跨架构诊断

| 目录 | 内容 |
|---|---|
| [kernel-map-runtime-arm64-20260803-profile](uprobe/kernel-map-runtime-arm64-20260803-profile/) | Jetson kernel helper wall、PMU、perf report 和反汇编 |
| [kernel-map-runtime-x64-20260803-fixedfreq](uprobe/kernel-map-runtime-x64-20260803-fixedfreq/) | 固定频率 x64 的对应 kernel runtime 数据 |
| [kernel-array-update-sizes-arm64-20260803](uprobe/kernel-array-update-sizes-arm64-20260803/) | Jetson array update 8B–256B value-size sweep |
| [kernel-array-update-sizes-x64-20260803-fixedfreq](uprobe/kernel-array-update-sizes-x64-20260803-fixedfreq/) | x64 的对应 value-size sweep |
| [kernel-array-update-internal-x64-20260803-fixedfreq](uprobe/kernel-array-update-internal-x64-20260803-fixedfreq/) | x64 kernel array update 内部路径诊断 |
| [kernel-map-runtime-arm64-20260801](uprobe/kernel-map-runtime-arm64-20260801/) | Jetson 第一阶段 kernel runtime A/B |
| [kernel-map-runtime-x64-20260802](uprobe/kernel-map-runtime-x64-20260802/) | x64 第一阶段 kernel runtime A/B |

### 其他 ARM64 诊断

| 目录 | 内容 |
|---|---|
| [arm64-bpftime-vs-kernel-paths-20260803](uprobe/arm64-bpftime-vs-kernel-paths-20260803/) | ARM64 BPFtime/kernel 路径与 PMU 对照 |

任务交接要求保存在
[ARM64-BPFTIME-KERNEL-PATH-TASK-20260803.md](uprobe/ARM64-BPFTIME-KERNEL-PATH-TASK-20260803.md)。

## ssl-nginx 数据索引

| 目录 | 变体或用途 |
|---|---|
| [empty-probe/run01](empty-probe/run01/) | V1：probe 立即返回，只保留 hook/runtime/JIT 基础路径 |
| [no-copy_no-output](no-copy_no-output/) | V2：metadata helper、map、事件准备；无 copy、无 output |
| [no-output/run01](no-output/run01/) | V3：增加 `bpf_probe_read_user`；无 perf-event output |
| [full-aligned](full-aligned/) | V4 正式结果：完整路径并应用 software perf record 8 字节对齐 |
| [full-unaligned](full-unaligned/) | V4 异常对照：未修复对齐问题 |
| [v4-ablation](v4-ablation/) | Full 内部子路径短测和 fair-scope 对照 |
| [old](old/) | 历史、失败和 v0.2.0 结果；不用于当前正式结论 |

## 归档约定

- 每个正式结果目录应包含 `README.md` 或环境说明、统计表、解析脚本和原始输出。
- 顶层吞吐量汇总使用 `size_benchmark_*.txt/.json`；单 payload 原始样本使用
  `benchmark_results_*.json`。
- 临时构建产物、staging 二进制、`__pycache__` 和超过结果复核所需范围的大型 perf
  数据不应提交。
- `old/` 只用于历史审计；引用当前结论时优先使用上表列出的正式目录。

---

## `ssl-nginx` 路径消融实验说明

## 技术摘要

本目录保存 Jetson 上 `ssl-nginx` benchmark 的路径消融实验。实验逐步恢复 `sslsniff.bpf.c` 的执行路径，通过端到端吞吐量（Requests/sec）变化，比较 kernel eBPF 与 BPFtime，并近似判断不同路径对吞吐量的影响。

当前正式对比使用 V1、V2、V3 和修复 software perf record 8 字节对齐问题后的 V4 Full：

- V1 空 Probe 下，BPFtime 在全部 payload 上比 kernel eBPF 快 `6.76%～14.90%`。
- V2 恢复 metadata helper、map 和事件准备后，BPFtime 仍比 kernel 快 `5.65%～15.18%`。
- V3 恢复 `bpf_probe_read_user` 后，BPFtime 优势收窄到 `2.92%～4.95%`。
- V4 恢复完整 event-output 路径后，BPFtime 在全部 payload 上转为比 kernel 慢 `4.86%～12.28%`。
- V4 相对 V3 增加了约 `15.56～19.70` 个百分点的 BPFtime 吞吐量损失，是当前四级消融中影响最大的阶段。
- 对齐修复使 V4 BPFtime 的七种 payload 平均 CV 从 `13.54%` 降至 `3.47%`，此前的明显双峰消失。

因此，这组吞吐量消融实验支持：

> BPFtime 的基础 probe/runtime 路径本身具有优势；helper、map 和明文复制逐步消耗这项优势；完整 perf-event 输出、传输和消费路径进一步造成最大的吞吐量下降，并使最终 Full 结果低于 kernel eBPF。

本文只分析当前目录中的新实验，不包含 `old/`。

## 一个 `run` 具体执行了什么

每个 `runXX/` 都表示完整执行一次：

```bash
python3 benchmark/ssl-nginx/draw_figture.py
```

`draw_figture.py` 依次生成并测试七种 payload：

```text
16B、1KB、2KB、4KB、16KB、128KB、256KB
```

对于每种 payload，脚本调用一次：

```bash
python3 benchmark/ssl-nginx/benchmark.py
```

`benchmark.py` 中 `NUM_RUNS = 10`，因此每种 payload 包含：

- Baseline：10 次 `wrk`
- Kernel sslsniff：10 次 `wrk`
- BPFtime sslsniff：10 次 `wrk`

每次 `wrk` 使用 100 个连接并持续 10 秒：

```bash
wrk https://127.0.0.1:4043/index.html -c 100 -d 10
```

因此：

- 一次完整 `draw_figture.py` 包含 `7 × 3 × 10 = 210` 个吞吐量样本。
- V1、V2、V3 各有一次完整 run，每种 payload、每种模式各有 10 个样本。
- V4 Full aligned 有两次完整 run，每种 payload、每种模式合计各有 20 个样本。
- V4 Full unaligned 也保留两次完整 run，但只作为未对齐 bug 的异常对照。

这里的“一次 run”不是单次 `wrk`，而是包含七种 payload 和三种运行模式的完整 benchmark。

## 四个正式消融版本

### V1：Empty Probe

目录：

```text
empty-probe/run01/
```

`SSL_read`、`SSL_write` 和 handshake 的 entry/return probe 均直接返回 `0`。

该版本只保留：

- uprobe/uretprobe 触发
- 上下文保存与恢复
- BPF 程序分发
- kernel BPF 或 BPFtime JIT 执行
- 返回 nginx

它排除 helper、map、metadata 准备、明文复制、perf-event 输出、数据传输和消费者处理。因此，V1 反映基础 probe/runtime 路径对 benchmark 吞吐量的影响。

### V2：No-copy + No-output

目录：

```text
no-copy_no-output/run01/
```

该版本恢复：

- `bpf_get_current_pid_tgid`
- `bpf_get_current_uid_gid`
- `bpf_ktime_get_ns`
- `bpf_get_current_comm`
- map update、lookup 和 delete
- per-CPU array lookup
- event metadata 初始化

仍禁用：

```c
bpf_probe_read_user(...);
bpf_perf_event_output(...);
```

V2 相对 V1 的吞吐量变化，近似反映 metadata helper、map 操作和 event preparation 的影响。

### V3：No-output

目录：

```text
no-output/run01/
```

该版本恢复：

```c
bpf_probe_read_user(...);
```

但仍禁用：

```c
bpf_perf_event_output(...);
```

V3 相对 V2 的吞吐量变化，近似反映 SSL 明文复制路径的影响。

### V4：Full aligned

目录：

```text
full-aligned/run01/
full-aligned/run02/
```

该版本恢复完整 `sslsniff.bpf.c`，包括：

- probe/runtime 执行
- metadata helper
- map 操作
- `bpf_probe_read_user`
- `bpf_perf_event_output`
- perf-buffer 数据传输
- userspace `sslsniff` 消费者

容器中的 BPFtime runtime 使用修复后的 agent、syscall-server 和 `bpftimetool`，software perf record 按 8 字节对齐。V4 相对 V3 的吞吐量变化，近似反映完整 event-output、传输和消费路径的端到端影响。

## V4 aligned 的完整吞吐量结果

下表汇总 `full-aligned/run01` 和 `run02`，每项为 20 个样本的平均值。CV 为 BPFtime Requests/sec 的变异系数。

| Payload | Baseline RPS | Kernel RPS | BPFtime RPS | BPFtime 相对 Kernel | BPFtime CV |
|---|---:|---:|---:|---:|---:|
| 16B | 16541.78 | 11259.38 | 10315.27 | −8.39% | 2.11% |
| 1KB | 15896.07 | 10883.89 | 10079.05 | −7.39% | 3.46% |
| 2KB | 15131.01 | 10457.03 | 9812.67 | −6.16% | 2.13% |
| 4KB | 14330.22 | 9924.30 | 9441.82 | −4.86% | 3.76% |
| 16KB | 9916.62 | 7059.99 | 6193.26 | −12.28% | 3.96% |
| 128KB | 2966.59 | 2028.68 | 1929.99 | −4.86% | 4.37% |
| 256KB | 1644.17 | 1164.48 | 1061.72 | −8.82% | 4.47% |

两次 aligned run 分开计算时，七种 payload 中 BPFtime 也都低于 kernel，说明方向在两轮之间一致：

- `run01`：BPFtime 相对 kernel 为 `−4.97%～−12.56%`
- `run02`：BPFtime 相对 kernel 为 `−3.76%～−11.99%`

当前数据没有显示“输出路径只在大 payload 才明显变重”的单调趋势。完整输出路径在全部 payload 上都造成了较大的额外吞吐量损失。

## BPFtime 相对 kernel eBPF 的逐级变化

计算方式：

```text
BPFtime relative to kernel =
    (BPFtime RPS / Kernel RPS - 1) × 100%
```

正数表示 BPFtime 吞吐量高于 kernel eBPF，负数表示低于 kernel eBPF。

| Payload | V1 Empty | V2 No-copy/No-output | V3 No-output | V4 Full aligned |
|---|---:|---:|---:|---:|
| 16B | +8.39% | +5.65% | +3.60% | **−8.39%** |
| 1KB | +12.96% | +7.68% | +4.95% | **−7.39%** |
| 2KB | +6.76% | +10.99% | +2.92% | **−6.16%** |
| 4KB | +7.35% | +5.83% | +3.91% | **−4.86%** |
| 16KB | +9.53% | +10.14% | +4.92% | **−12.28%** |
| 128KB | +10.88% | +15.18% | +3.95% | **−4.86%** |
| 256KB | +14.90% | +10.92% | +4.18% | **−8.82%** |

V1 到 V3 中，BPFtime 始终优于 kernel。性能关系在 V4 恢复完整输出路径后统一翻转，因此当前最主要的吞吐量分界出现在 `bpf_perf_event_output` 及其后续传输和消费链路。

## 各阶段引入的 BPFtime 吞吐量损失

每个版本首先相对其同轮 baseline 计算吞吐量损失：

```text
Throughput impact =
    (Baseline RPS - BPFtime RPS) / Baseline RPS × 100%
```

再比较相邻版本的 impact 差值，单位为百分点（percentage points，pp）。

| Payload | V2−V1：helper/map/准备 | V3−V2：明文复制 | V4 aligned−V3：完整输出路径 |
|---|---:|---:|---:|
| 16B | +7.67 pp | +2.46 pp | **+17.81 pp** |
| 1KB | +6.89 pp | +2.63 pp | **+17.49 pp** |
| 2KB | +6.21 pp | +3.23 pp | **+16.15 pp** |
| 4KB | +5.98 pp | +3.91 pp | **+15.56 pp** |
| 16KB | +6.48 pp | +4.23 pp | **+19.70 pp** |
| 128KB | +6.37 pp | +3.80 pp | **+17.69 pp** |
| 256KB | +5.83 pp | +5.29 pp | **+18.27 pp** |

从吞吐量角度看：

- helper、map 和事件准备路径带来约 `5.83～7.67 pp` 的额外损失。
- `bpf_probe_read_user` 路径带来约 `2.46～5.29 pp` 的额外损失。
- 完整输出路径带来约 `15.56～19.70 pp` 的额外损失，是前三段增量中最大的一段。

这些数值是不同时间执行的相邻消融版本之间的 benchmark 吞吐量差异，不是函数级 CPU 时间，也不应解释为严格可加的绝对执行成本。

## 字节对齐修复消除了 V4 双峰

`full-unaligned/` 保存未应用 software perf record 8 字节对齐修复的两轮 V4：

```text
full-unaligned/run01/
full-unaligned/run02/
```

未对齐 V4 会使 libbpf perf-buffer 消费者在跨 ring 边界读取 record header 时停滞，引起消费者空转、隐藏丢事件和双峰吞吐量。该目录只作为 bug 对照，不参与正式 V1–V4 路径归因。

| Payload | V1 CV | V2 CV | V3 CV | V4 unaligned CV | V4 aligned CV |
|---|---:|---:|---:|---:|---:|
| 16B | 1.36% | 0.89% | 0.74% | 16.58% | **2.11%** |
| 1KB | 0.62% | 1.51% | 1.10% | 14.89% | **3.46%** |
| 2KB | 1.31% | 1.40% | 2.36% | 12.63% | **2.13%** |
| 4KB | 1.12% | 2.81% | 1.86% | 12.46% | **3.76%** |
| 16KB | 0.67% | 1.30% | 1.95% | 15.28% | **3.96%** |
| 128KB | 0.58% | 1.18% | 3.25% | 14.17% | **4.37%** |
| 256KB | 1.58% | 2.71% | 2.66% | 8.73% | **4.47%** |

七种 payload 的 V4 BPFtime 平均 CV：

```text
unaligned：13.54%
aligned：   3.47%
下降：     74.4%
```

例如未对齐 V4 `run01` 的 16B BPFtime 同时出现约 10k 和约 14k RPS：

```text
10436 10358 14182 14354 10387
14210 10381 10574 9953 14266
```

修复后不再出现这一明显双峰。这说明字节对齐修复主要恢复了 perf-buffer 消费的正确性和测量稳定性；未对齐版本中被隐藏丢事件抬高的部分吞吐量不能视为真实性能。

## 结果文件的读取方式

每个完整 run 中：

- `size_benchmark_*.txt`：七种 payload 的汇总表和完整文本输出。
- `size_benchmark_*.json`：七种 payload 的结构化汇总结果。
- `benchmark_results_*.json`：单个 payload 中 baseline、kernel 和 BPFtime 各 10 次的原始吞吐量样本。
- `absolute_performance.png`：该次完整 run 生成的吞吐量图。
- `*.log`：完整执行日志。

`no-copy_no-output/run01/` 没有单独保存 `benchmark_results_*.json` 和 `size_benchmark_*.json`，但 `size_benchmark_20260724_171625.txt` 的 raw output 中包含全部 210 个吞吐量样本，可以恢复并核对均值与方差。

以下文件时间戳早于本轮路径消融实验，分析时未纳入：

```text
benchmark_results_20260711_081448.json
```

`v4-ablation/` 保存对 V4 内部子路径的后续短测和 fair-scope 验证，不与本 README 中四个完整 `draw_figture.py` run 的主表混合。

## 结论边界与后续问题

这组吞吐量消融能够回答：

> 启用某段 `sslsniff` 路径后，完整 `ssl-nginx` benchmark 的吞吐量发生了多大变化？

它不能单独回答：

> 某个 helper 或函数实际执行了多少 CPU 指令或消耗了多少 CPU 时间？

当前最可靠的结论是：

1. BPFtime 的基础 probe/runtime 路径在七种 payload 上均优于 kernel eBPF。
2. metadata helper、map 操作和明文复制逐步缩小 BPFtime 的吞吐量优势。
3. aligned V4 的完整输出路径带来最大的吞吐量损失，使 BPFtime 在七种 payload 上均转为低于 kernel。
4. 8 字节对齐修复消除了未对齐 V4 的明显双峰，使 Full 结果从异常状态恢复为可用于路径归因的稳定测量。
5. 相邻版本在不同时间顺序执行，因此各阶段差值是近似吞吐量归因；如需更强的因果证据，应采用同一时段交错执行 V1–V4。

后续如果继续分解 V4，应分别消融 `bpf_perf_event_output` 的 producer helper、software perf ring 写入以及 userspace consumer，并保持相同 attach scope、网络模式、构建产物和执行顺序。
