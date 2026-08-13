# Uprobe map operation investigation matrix

最后更新：2026-08-13

本表维护 `benchmark/uprobe` 中12个名义 map 操作的语义、平台方向、调查深度和后续优先级。它用于避免把不支持的 array delete、BPFtime 已经更快的操作和仍需深入的操作混在一起。

## 调查阶段

| 阶段 | 定义 |
|---|---|
| S0：语义审计 | 确认 lookup-hit/miss、update-existing/insert、delete-hit/miss，以及操作是否受 map 类型支持 |
| S1：顶层确认 | 在 ARM64/x64 上完成 kernel/BPFtime 顶层对比 |
| S2：Runtime 分层 | 定位 L0 concrete map、L1 generic handler、L2 SHM dispatch、L3 production helper 中的主要成本层 |
| S3：生产操作归因 | 在同一诊断 runtime 二进制中用 production-path A/B 定位具体组合操作，并用 PMU 复核 |
| S4：源码叶子归因 | 继续拆到 hash、equality、copy、间接调用、bucket/node、allocator 等叶子操作 |
| Closed | 主要问题已经闭环；除非进入优化阶段，否则不再继续拆分 |

## 12项操作总表

| Map/操作 | 有效语义 | ARM64 / x64 方向 | 当前阶段 | 已经确认的主要结论 | 后续状态 |
|---|---|---|---|---|---|
| Ordinary array lookup | 稳定 lookup-hit | Kernel / Kernel 更快 | S3 | Concrete array access 很轻；SHM fd/variant lookup 约 `1.84 ns/helper`，generic handler 约 `4.73 ns/helper` | 当前解释已足够；若追求完全叶子闭合，可继续拆 fd lookup、variant 和 type switch |
| Ordinary array update | 稳定 update-existing | ARM64 Kernel 更快；x64 BPFtime 更快 | S4 | Concrete body `3.146 ns/helper`：copy `1.824 ns`、address `1.328 ns`；外层 generic handler `4.922 ns`、SHM lookup `1.378 ns`；跨平台翻转来自既有 kernel 内联差异 | 叶子归因已完成；除非优化，不继续拆 |
| Ordinary array delete | Array map 不支持 delete | 无有效性能含义 | S0 | 仅测量 unsupported/error-return path，不是元素删除 | Excluded |
| Ordinary hash lookup | 稳定 lookup-hit | Kernel / Kernel 更快 | S4 / Closed | 已定位 userspace outer path、通用 key comparison、probing 和重复除法等主要叶子成本 | 已闭环 |
| Ordinary hash update | 稳态 update-existing；首次有极少量 insert | BPFtime / BPFtime 更快 | S1 | 顶层方向已确认，不属于“BPFtime 为什么更慢”的问题 | 不继续做慢路径归因；若研究 BPFtime 为什么快，再单独立项 |
| Ordinary hash delete | 修正后为稳定 delete-hit | BPFtime / BPFtime 更快 | S1 | 原 benchmark 的 delete-miss 语义已修正；修正后两个平台均为 BPFtime 更快 | 不继续做慢路径归因 |
| Per-CPU array lookup | 稳定 lookup-hit | Kernel / Kernel 更快 | S4 | Concrete body `9.691 ns`：wrapper `4.669 ns`（48.2%）、CPU selection `2.682 ns`、address `1.063 ns`、checks `1.277 ns` | 叶子归因已完成；除非优化，不继续拆 |
| Per-CPU array update | 稳定 update-existing | Kernel / Kernel 更快 | S4 | Concrete body `53.970 ns`：wrapper `45.559 ns`（84.4%）、CPU selection `3.472 ns`、address `1.788 ns`、copy `1.568 ns` | 叶子归因已完成；优化首选消除 `std::function` wrapper |
| Per-CPU array delete | Per-CPU array map 不支持 delete | 无有效性能含义 | S0 | 与 ordinary array delete 相同，只是 unsupported/error-return path | Excluded |
| Per-CPU hash lookup | 稳定 lookup-hit | Kernel / Kernel 更快 | S4 | 完整 `impl.find()` 为 `125.275 ns/helper`；hash `15.186 ns`、通用 equality `27.280 ns`、交互 `0.332 ns`，Boost bucket/node/`offset_ptr`/value extraction 组合剩余 `82.476 ns`；key assign 相对 fixed copy 多 `7.147 ns` | 主要叶子 A/B 已完成；仅在优化或要求完全闭合时继续拆 65.8% 容器组合剩余量 |
| Per-CPU hash update | 稳定 update-existing | Kernel / Kernel 更快 | S4 | Find `125.585 ns`：hash `13.399 ns`、equality `25.730 ns`、Boost container remainder `86.340 ns`；existing value copy另为`57.011 ns` | 叶子归因已完成；除非优化container/value representation，不继续拆 |
| Per-CPU hash delete | 修正后为稳定 delete-hit | Kernel / Kernel 更快 | S4 / Closed | 延迟析构/回收使生产路径从约 `998.56` 降至 `211.98 ns/helper`；同步 vector/node 析构与 SHM reclamation 贡献约 `786.59 ns/helper` | 已闭环；只有准备优化 allocator/reclamation 时才继续细拆 |

## 数量汇总

```text
12 个名义操作
├── 2 个无有效性能语义：ordinary/per-CPU array delete
├── 2 个 BPFtime 已明显更快：ordinary hash update/delete-hit
└── 8 个进入成本调查
    ├── 2 个基本闭环：ordinary hash lookup、per-CPU hash delete-hit
    ├── 5 个已完成源码叶子 A/B：ordinary array update、per-CPU array lookup/update、per-CPU hash lookup/update
    └── 1 个生产操作级解释已足够：ordinary array lookup
```

如果把 ordinary array update 的 BPFtime runtime 公共分发也要求拆到每个语句，则“仍可继续”的项目是5项；但它已有足够的顶层方向和主要机制解释，因此不属于最高优先级。

## 下一步

剩余慢项的主要成本已经定位。下一步不再默认继续拆分，而是先决定研究目标：若进入优化，优先处理per-CPU array的`std::function` wrapper、per-CPU hash的Boost container/value representation；若只需诊断报告，则当前深度已经足够。

## 相关报告

- [普通 map 系统报告](ordinary/bpftime-uprobe-ordinary-map-systematic-report-20260804.md)
- [Per-CPU map 路径诊断](percpu/bpftime-uprobe-percpu-path-diagnosis-20260805.md)
- [ARM64 matched 顶层差距](attribution/bpftime-uprobe-matched-kernel-gap-arm64-20260812.md)
- [ARM64 生产路径归因](attribution/bpftime-uprobe-production-path-attribution-arm64-20260813.md)
- [Per-CPU hash lookup 叶子级归因](percpu/bpftime-uprobe-percpu-hash-lookup-leaf-attribution-20260813.md)
- [剩余四项叶子级归因](attribution/bpftime-uprobe-remaining-map-leaf-attribution-arm64-20260813.md)
