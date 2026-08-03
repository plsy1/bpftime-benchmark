# Jetson kernel array-update value-size sweep

## 结论

本实验把 kernel BPF ordinary array `update-existing` 的 value size 从原来的
8B 扩展到 8/16/32/64/128/256B，并对每个尺寸使用匹配的 control/real BPF
program。Jetson 上呈现两个明显区间：

- 8–32B 是约 9 ns/helper 的短尺寸平台区，尺寸翻四倍仅增加 0.43 ns。
- 从 64B 开始复制长度成本明显出现；256B 达到 25.46 ns/helper。

因此，原 benchmark 的 8B update 位于“固定 helper/包装/短复制路径主导”的区间，
不是按字节线性增长的长复制区间。当前 Jetson 数据证明 value size 扫描能够分离短路径
平台区与长复制增长区；是否是 x64 的 `bpf_obj_memcpy` 包装造成原始 8B 跨平台差距，
仍需用同一源码完成 x64 扫描后才能定量回答。

## 正式结果

净 wall-time 定义：

```text
(real ns/program - control ns/program) / 1000 helpers/program
```

硬件计数器净值定义：

```text
(real counter/program - control counter/program) / 1000 helpers/program
```

| Value | ns/helper | cycles/helper | instructions/helper | L1D loads/helper | LLC misses/helper |
|---:|---:|---:|---:|---:|---:|
| 8B | 8.978 | 15.521 | 69.016 | 20.002 | 0.000072 |
| 16B | 9.212 | 16.157 | 67.014 | 20.000 | 0.000083 |
| 32B | 9.411 | 16.247 | 68.051 | 20.000 | 0.000094 |
| 64B | 10.412 | 18.195 | 72.021 | 23.990 | 0.000102 |
| 128B | 13.831 | 23.767 | 82.017 | 31.998 | 0.000436 |
| 256B | 25.463 | 43.783 | 108.013 | 50.008 | 0.001270 |

8B 到 256B 的增量为：

- wall-time：+16.485 ns/helper；
- cycles：+28.262 cycles/helper；
- instructions：+38.997 instructions/helper；
- L1D loads：+30.007 loads/helper。

LLC miss 即使在 256B 也只有约 0.00127/helper，不是增长的主要原因。wall-time 乘
1.728 GHz 与 PMU cycles 基本闭合，例如 8B 为 15.514 vs 15.521 cycles，256B
为 44.000 vs 43.783 cycles。

## 运行口径

- 源码：`codex/official-no-btf`，commit
  `176eb291ead95eb5f8a56280deae626fac46eaa9`。
- Jetson Orin Nano，Cortex-A78AE，CPU5，无 SMT sibling。
- MAXN_SUPER，`jetson_clocks`，CPU5 固定 1.728 GHz。
- GCC 13.3 host compiler；LLVM/Clang 15.0.7 BPF compiler。
- loader/victim 均为 root，并固定 CPU5。
- 每个 program 20,000 invocations × 5 rounds，另有 1,000 warm-up。
- 每个 real BPF program invocation 执行 1,000 次
  `bpf_map_update_elem(..., BPF_ANY)`。
- odd round control-first，even round real-first。
- wall-time 每尺寸 5 个净值，报告 mean/median/stdev/min/max。
- PMU 每个 metric、每个 value size 单独运行，一次只 profile 对应的 control/real。

## 数据有效性

- 每份正式 profile JSON 的 control/real `run_cnt` 都是 101,000。
- 每份正式 profile JSON 都满足 `enabled == running`，无 PMU multiplex。
- 每次 metric 单独采集。
- 正式结束后 `kernel.bpf_stats_enabled=0`，BPF link 数为 0。
- 早期尝试同时 profile 12 个程序，发现 `enabled != running`；该并发数据已判定无效，
  不在正式 `*-pairwise` 数据和汇总中使用。

## 与此前 8B diagnostic 的关系

此前 `kernel-map-runtime-arm64-20260803-profile` 的 8B program 使用循环内 `u64`
value 初始化；本实验为保证所有尺寸使用同一模板，使用定长结构体、循环前清零和循环内
首字节更新。因此本实验的 8B 绝对值不能替代此前诊断值。跨平台结论必须让 x64 使用
本提交中完全相同的 BPF source、control 和运行脚本；本实验内部的尺寸趋势可直接比较。

## 文件

- `raw-wall/raw.csv`：5 轮 wall-time control/real 和净值。
- `raw-profile-<metric>-pairwise/`：无 multiplex 的 JSON、xlated/JIT dump 和 raw CSV。
- `wall-summary.csv`：5 轮分布统计。
- `profile-summary.csv`：counter/program 与净 counter/helper。
- `combined-summary.csv`：wall-time、四项 PMU counter 和频率闭合。
- `jit-static-summary.csv`：各尺寸 control/real 的静态 xlated/JIT 指令数。
- `environment.txt`：源码、工具、哈希、CPU、频率和清理状态。
- `run-*.sh`、`profile-arm64.sh`、`summarize.py`：精确复现脚本。
- `X64-TASK.md`：x64 固定 2.2 GHz 的同口径执行任务。
