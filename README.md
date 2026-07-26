# Jetson `ssl-nginx` 路径消融实验说明

## 技术摘要

本目录保存 Jetson 上 `ssl-nginx` benchmark 的四组路径消融实验结果。四个版本逐步恢复 `sslsniff.bpf.c` 中的执行路径，通过观察端到端吞吐量（Requests/sec）的变化，判断不同路径对 kernel eBPF 和 BPFtime 的性能影响。

当前结果表明：

- V1 空 Probe 下，BPFtime 在全部 payload 上均比 kernel eBPF 快，优势为 `6.76%～14.90%`。
- 恢复 metadata helper、map 和事件准备逻辑后，BPFtime 仍保持优势。
- 恢复 `bpf_probe_read_user` 后，BPFtime 的优势收窄到 `2.92%～4.95%`。
- 恢复完整 event-output 路径后，256KB 下 BPFtime 从比 kernel 快 `4.18%` 变为慢 `5.45%`。
- 当前 V4 Full 使用的是未包含 software perf record 8 字节对齐修复的 runtime，BPFtime 吞吐量存在明显双峰，变异系数为 `8.73%～16.58%`。这组结果仅作为未对齐 bug 的异常对照，不能用于量化正常完整输出路径的吞吐量影响。

本文只分析当前目录中的新实验，不包含 `old/` 目录。

## 一个 `run` 具体执行了什么

每个 `runXX/` 都表示完整执行了一次：

```bash
python3 benchmark/ssl-nginx/draw_figture.py
```

`draw_figture.py` 会依次生成并测试以下七种 payload：

```text
16B、1KB、2KB、4KB、16KB、128KB、256KB
```

对于每一种 payload，脚本都会调用一次：

```bash
python3 benchmark/ssl-nginx/benchmark.py
```

`benchmark.py` 中 `NUM_RUNS = 10`，因此每一种 payload 都包括：

- Baseline：10 次 `wrk`
- Kernel sslsniff：10 次 `wrk`
- BPFtime sslsniff：10 次 `wrk`

每个 `wrk` 测试使用 100 个连接并持续 10 秒：

```bash
wrk https://127.0.0.1:4043/index.html -c 100 -d 10
```

因此：

- 一次完整 `draw_figture.py` 包含 `7 × 3 × 10 = 210` 个吞吐量样本。
- V1、V2、V3 各执行了一次完整 run，每种 payload、每种模式各有 10 个样本。
- V4（未对齐 runtime）执行了两次完整 run，每种 payload、每种模式合计各有 20 个样本。

这里的“一次 run”不是单次 `wrk`，而是包含七种 payload 和三种运行模式的完整 benchmark。

## 四个消融版本

### V1：Empty Probe

目录：

```text
empty-probe/run01/
```

`SSL_read`、`SSL_write` 和 handshake 的 entry/return probe 均直接返回 `0`。

该版本保留：

- uprobe/uretprobe 触发
- 上下文保存与恢复
- BPF 程序分发
- kernel BPF 或 BPFtime JIT 执行
- 返回 nginx

该版本排除：

- helper 调用
- map 操作
- event metadata 准备
- `bpf_probe_read_user`
- `bpf_perf_event_output`
- perf-buffer 传输和消费者处理

因此，V1 反映基础 probe/runtime 路径对 benchmark 吞吐量的影响。

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

该版本仍禁用：

```c
bpf_probe_read_user(...);
bpf_perf_event_output(...);
```

V2 相对 V1 的吞吐量变化，近似反映 metadata helper、map 操作和 event preparation 对端到端吞吐量的影响。

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

V3 相对 V2 的吞吐量变化，近似反映 SSL 明文复制路径对端到端吞吐量的影响。

### V4：Full（未对齐 runtime，异常对照）

目录：

```text
full-unaligned/run01/
full-unaligned/run02/
```

该版本恢复原始完整 `sslsniff.bpf.c` 路径，包括：

- probe/runtime 执行
- metadata helper
- map 操作
- `bpf_probe_read_user`
- `bpf_perf_event_output`
- perf-buffer 数据传输
- userspace `sslsniff` 消费者

这两次 V4 使用的 Docker 镜像未包含 software perf record 8 字节对齐修复。启用 `bpf_perf_event_output` 后会触发已知的未对齐 record bug，引起消费者空转、隐藏丢事件和吞吐量双峰。因此，这组 V4 只能用于展示未修复状态，不能用来估算正常 event output、数据传输和消费者路径的吞吐量影响。

后续应用对齐修复重新运行的结果应保存到：

```text
full-aligned/run01/
full-aligned/run02/
```

## BPFtime 相对 kernel eBPF 的吞吐量

下表计算：

```text
BPFtime relative to kernel =
    (BPFtime RPS / Kernel RPS - 1) × 100%
```

正数表示 BPFtime 吞吐量高于 kernel eBPF，负数表示低于 kernel eBPF。

| Payload | V1 Empty | V2 No-copy/No-output | V3 No-output | V4 Full（未对齐，仅供参考） |
|---|---:|---:|---:|---:|
| 16B | +8.39% | +5.65% | +3.60% | +4.99% |
| 1KB | +12.96% | +7.68% | +4.95% | +3.65% |
| 2KB | +6.76% | +10.99% | +2.92% | +14.00% |
| 4KB | +7.35% | +5.83% | +3.91% | +9.90% |
| 16KB | +9.53% | +10.14% | +4.92% | +3.91% |
| 128KB | +10.88% | +15.18% | +3.95% | +5.51% |
| 256KB | +14.90% | +10.92% | +4.18% | **−5.45%** |

V1 到 V3 的结果比较稳定：BPFtime 始终优于 kernel，但随着 BPF 程序逐步恢复复制等实际工作，优势逐渐缩小。

未对齐 V4 中 256KB 的性能翻转，以及 2KB 和 4KB 的较高优势，均受到双峰和隐藏丢事件影响，不能解释为正常完整输出路径的稳定性能。

## BPFtime 各阶段引入的吞吐量损失

每个版本首先相对其同轮 baseline 计算吞吐量损失：

```text
Throughput impact =
    (Baseline RPS - BPFtime RPS) / Baseline RPS × 100%
```

再比较相邻版本的 impact 差值。单位为百分点（percentage points，pp）。

| Payload | V2−V1：helper/map/准备 | V3−V2：明文复制 | V4−V3：未对齐异常路径 |
|---|---:|---:|---:|
| 16B | +7.67 pp | +2.46 pp | +8.79 pp |
| 1KB | +6.89 pp | +2.63 pp | +9.94 pp |
| 2KB | +6.21 pp | +3.23 pp | +3.26 pp |
| 4KB | +5.98 pp | +3.91 pp | +4.73 pp |
| 16KB | +6.48 pp | +4.23 pp | +10.14 pp |
| 128KB | +6.37 pp | +3.80 pp | +8.34 pp |
| 256KB | +5.83 pp | +5.29 pp | **+16.29 pp** |

从吞吐量角度看：

- helper、map 和事件准备路径带来约 `5.83～7.67 pp` 的额外损失。
- `bpf_probe_read_user` 路径带来约 `2.46～5.29 pp` 的额外损失，并随 payload 增大呈增强趋势。
- 未对齐 V4 与 V3 的表面差值为 `3.26～16.29 pp`，但它混入了 record 未对齐 bug，不能作为正常完整输出路径的成本。

这些数值是相邻消融版本的 benchmark 吞吐量差异，不是函数级 CPU 执行时间，也不应被理解为严格可加的绝对路径成本。

## 稳定性：未对齐 V4 触发已知双峰

各版本 BPFtime 吞吐量样本的变异系数（CV）如下：

| Payload | V1 Empty | V2 No-copy/No-output | V3 No-output | V4 Full（未对齐） |
|---|---:|---:|---:|---:|
| 16B | 1.36% | 0.89% | 0.74% | **16.58%** |
| 1KB | 0.62% | 1.51% | 1.10% | **14.89%** |
| 2KB | 1.31% | 1.40% | 2.36% | **12.63%** |
| 4KB | 1.12% | 2.81% | 1.86% | **12.46%** |
| 16KB | 0.67% | 1.30% | 1.95% | **15.28%** |
| 128KB | 0.58% | 1.18% | 3.25% | **14.17%** |
| 256KB | 1.58% | 2.71% | 2.66% | **8.73%** |

Baseline 和 kernel 样本整体稳定，V1–V3 的 BPFtime 样本也基本稳定。显著波动出现在使用未修复 runtime 并启用 `bpf_perf_event_output` 的 V4 BPFtime 中。

例如 V4 `run01` 的 16B BPFtime 样本同时包含约 10k RPS 和约 14k RPS 两组状态：

```text
10436 10358 14182 14354 10387
14210 10381 10574 9953 14266
```

因此，V4 的均值混合了两个不同的异常运行状态。此前已经确认，该双峰来自 software perf record 未按 8 字节对齐，而不是完整输出路径在正常实现下必然具有的性能特征。必须使用修复后的 runtime 重跑 V4，才能计算有效的 `V4 − V3`。

## 结果文件的读取方式

每个 run 中：

- `size_benchmark_*.txt`：七种 payload 的汇总表和完整文本输出。
- `size_benchmark_*.json`：七种 payload 的结构化汇总结果。
- `benchmark_results_*.json`：单个 payload 中 baseline、kernel 和 BPFtime 各 10 次的原始吞吐量样本。
- `absolute_performance.png`：该次完整 run 生成的吞吐量图。
- `*.log`：完整执行日志。

`no-copy_no-output/run01/` 当前没有单独保存 `benchmark_results_*.json` 和 `size_benchmark_*.json`，但 `size_benchmark_20260724_171625.txt` 的 raw output 中仍包含全部 210 个吞吐量样本，因此可以恢复并核对均值与方差。

以下文件虽然存在于部分新结果目录中，但时间戳明显早于本轮路径消融实验，分析时未纳入：

```text
benchmark_results_20260711_081448.json
```

## 结论边界与后续验证

这组实验采用吞吐量消融法，能够回答：

> 启用某段 `sslsniff` 路径后，完整 `ssl-nginx` benchmark 的吞吐量发生了多大变化？

它不能单独回答：

> 某个 helper 或函数实际执行了多少 CPU 指令或消耗了多少 CPU 时间？

当前最可靠的结论是：

1. 空 Probe 下，BPFtime 的基础 runtime/JIT 路径在所有 payload 上均优于 kernel eBPF。
2. metadata helper、map 操作和明文复制会逐步缩小 BPFtime 的吞吐量优势。
3. 当前未对齐 V4 在恢复 event-output 路径后触发了已知 record alignment bug，因此其吞吐量均值、性能翻转和方差不能作为正常 V4 结论。
4. V1、V2、V3 不调用 `bpf_perf_event_output`，不受该字节对齐 bug 影响，可以继续保留。

下一步只需使用包含 8 字节对齐修复的 runtime 重跑 V4，并将结果保存到 `full-aligned/`。完成后再用 aligned V4 与 V3 比较，分析正常完整输出路径的吞吐量影响。
