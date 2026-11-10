# BPFtime uprobe map-helper ARM64 matched 顶层对照

日期：2026-08-12

## 目的

本轮补测使用 operation-specific matched control，在 Jetson ARM64 上统一确认普通/per-CPU array/hash lookup 和 update 的完整 BPFtime−kernel 差额。它补充顶层定量证据，不重新进行 L0–L3 或叶子级路径拆分。

## 测量定义

real case 与 control 保持相同的 BPF 程序形状、1000 次循环、key/value 准备、victim、CPU、频率和 map 状态，只在 real case 中执行目标 map helper：

```text
engine net cost = (real case - matched control) / 1000
top-level gap   = BPFtime net cost - kernel net cost
```

测试环境为 Jetson Orin Nano、CPU5、MAXN_SUPER、`jetson_clocks` 锁频、host root loader/victim、LLVM/Clang 15、GCC 13、Boost 1.83、LLVM JIT 和 LTO 开启。Wall-time 每个 engine/family 5 轮，PMU 每个 case/metric 3 轮配对测试。

## 严格 matched 顶层结果

| 操作 | Kernel ns/helper | BPFtime ns/helper | BPFtime−kernel | 额外 cycles | 额外 instructions |
|---|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.396 | 12.847 | **+11.452** | +19.76 | +98.04 |
| Ordinary array update | 10.696 | 16.584 | **+5.888** | +9.81 | +66.02 |
| Per-CPU array lookup | 2.008 | 21.739 | **+19.732** | +33.83 | +164.05 |
| Per-CPU array update | 14.178 | 64.353 | **+50.175** | +86.31 | +358.06 |
| Ordinary hash lookup | 25.726 | 53.704 | **+27.978** | +46.95 | +242.46 |
| Ordinary hash update | 91.977 | 52.002 | **−39.975** | −69.74 | −71.02 |
| Per-CPU hash lookup | 26.956 | 153.994 | **+127.039** | +218.07 | +800.83 |
| Per-CPU hash update | 65.767 | 228.254 | **+162.487** | +279.74 | +1134.20 |

Wall-time gap 的样本标准差为 `0.048–0.485 ns/helper`。PMU 的 cycles、instructions 与 wall-time 方向一致，说明这些差异来自实际执行工作量，而不是随机抖动。

## 与已有路径调查的关系

此前的调查已经完成主要路径定位：

- 普通 array lookup/update：主要内部成本位于 generic handler 和 SHM/fd/variant dispatch；
- 普通 hash lookup：已定位 spin lock、TRACE、通用 `memcmp` 和 probing 循环取模，并在完整官方 benchmark 中验证叶子 A/B 效应；
- per-CPU array lookup/update：已定位 CPU 选择、`std::function`、value copy 和公共 handler/SHM 分派；
- per-CPU hash lookup/update：已定位 Boost.Interprocess vector key、hash/find 和 per-CPU value-vector copy；
- per-CPU hash delete-hit：已定位同步 vector/node 析构和 Boost shared-memory allocator 回收。

本轮结果回答“完整 BPFtime 相对完整 kernel 多多少”；原 L0–L3/leaf 实验回答“BPFtime 内部主要重在哪里”。两组证据互相支持，但不是同一次加法分解实验。

## 重要口径修正

不能用本轮顶层结果减去另一套 standalone L0–L3 数据，再把差值命名为 `production-context residual`。该数值只是不同 harness 边界的代数余项，不是一个独立测得的 runtime 阶段，也不能作为下一步待定位函数。

因此，本轮不生成跨 harness 的归因闭合表，也不继续追踪所谓 residual。

## 当前完成度

当前已经完成：

1. benchmark 语义审计和 hash delete-hit 修正；
2. ARM64/x64 普通及 per-CPU map 顶层确认；
3. ARM64 kernel matched runtime 对照；
4. BPFtime ordinary/per-CPU map L0–L3 分层；
5. ordinary hash lookup 和 per-CPU hash 操作的叶子级 A/B；
6. ARM64 统一 matched BPFtime−kernel 顶层差额和 PMU 复核。

以“查明主要成本路径”为目标，实验部分可以收束。

## 后续工作边界

继续工作应作为新目标，而不是追踪跨 harness residual：

- 完善 benchmark 语义覆盖：增加 lookup-miss、update-insert、delete-miss；或
- 开始性能优化：针对已识别路径设计通用优化，并重新验证正确性、并发语义和完整顶层性能。

## 结果位置

- `benchmark-results/jetson` 分支：`uprobe/relative-kernel-attribution-arm64-20260812/`
- array matched raw：`uprobe/helper-map-closure-arm64-20260812/`
- hash matched raw：`uprobe/helper-hash-closure-arm64-20260812/`
- 普通 map 主报告：`bpftime-uprobe-ordinary-map-systematic-report-20260804.md`
- per-CPU map 主报告：`bpftime-uprobe-percpu-path-diagnosis-20260805.md`
