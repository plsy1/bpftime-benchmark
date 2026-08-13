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
| Ordinary array update | 稳定 update-existing | ARM64 Kernel 更快；x64 BPFtime 更快 | S3；kernel 侧已有源码/反汇编解释 | BPFtime 侧主要是 SHM dispatch 和 generic handler；跨平台翻转主要来自 ARM64 kernel 将 `bpf_obj_memcpy()` 内联，而 x64 保留 out-of-line copy 路径 | 可继续做 BPFtime runtime 叶子拆分，优先级低于 per-CPU 项 |
| Ordinary array delete | Array map 不支持 delete | 无有效性能含义 | S0 | 仅测量 unsupported/error-return path，不是元素删除 | Excluded |
| Ordinary hash lookup | 稳定 lookup-hit | Kernel / Kernel 更快 | S4 / Closed | 已定位 userspace outer path、通用 key comparison、probing 和重复除法等主要叶子成本 | 已闭环 |
| Ordinary hash update | 稳态 update-existing；首次有极少量 insert | BPFtime / BPFtime 更快 | S1 | 顶层方向已确认，不属于“BPFtime 为什么更慢”的问题 | 不继续做慢路径归因；若研究 BPFtime 为什么快，再单独立项 |
| Ordinary hash delete | 修正后为稳定 delete-hit | BPFtime / BPFtime 更快 | S1 | 原 benchmark 的 delete-miss 语义已修正；修正后两个平台均为 BPFtime 更快 | 不继续做慢路径归因 |
| Per-CPU array lookup | 稳定 lookup-hit | Kernel / Kernel 更快 | S3 | 已定位 SHM dispatch、generic handler、`std::function` wrapper 和 userspace CPU selection | 可继续拆 wrapper、`data_at()`、地址计算和 shared-memory pointer；完整性项目 |
| Per-CPU array update | 稳定 update-existing | Kernel / Kernel 更快 | S3 | `std::function`/间接访问组合块约 `44.11 ns/helper`；CPU selection 约 `2.74 ns`，8-byte copy 约 `3.66 ns` | 高优先级 S4：拆类型擦除、间接调用、`data_at()` 和目标地址访问 |
| Per-CPU array delete | Per-CPU array map 不支持 delete | 无有效性能含义 | S0 | 与 ordinary array delete 相同，只是 unsupported/error-return path | Excluded |
| Per-CPU hash lookup | 稳定 lookup-hit | Kernel / Kernel 更快 | S3 | Boost.Interprocess `unordered_map::find()` 约 `121.67 ns/helper`，与此前 ARM64 顶层 gap 同量级；PMU 同向 | 最高优先级 S4：拆 key assign、hash、equality、bucket/node、`offset_ptr` 和 value-address extraction |
| Per-CPU hash update | 稳定 update-existing | Kernel / Kernel 更快 | S3 | Boost find 约 `120.63 ns/helper`；existing shared-memory value copy 约 `55.81 ns/helper` | 高优先级 S4：复用 lookup 的 find 拆分，再拆 value preparation、destination 和 copy |
| Per-CPU hash delete | 修正后为稳定 delete-hit | Kernel / Kernel 更快 | S4 / Closed | 延迟析构/回收使生产路径从约 `998.56` 降至 `211.98 ns/helper`；同步 vector/node 析构与 SHM reclamation 贡献约 `786.59 ns/helper` | 已闭环；只有准备优化 allocator/reclamation 时才继续细拆 |

## 数量汇总

```text
12 个名义操作
├── 2 个无有效性能语义：ordinary/per-CPU array delete
├── 2 个 BPFtime 已明显更快：ordinary hash update/delete-hit
└── 8 个进入成本调查
    ├── 2 个基本闭环：ordinary hash lookup、per-CPU hash delete-hit
    ├── 2 个生产操作级解释已足够：ordinary array lookup/update
    └── 4 个仍可进入源码叶子级
        ├── 高优先级：per-CPU hash lookup/update、per-CPU array update
        └── 完整性项目：per-CPU array lookup
```

如果把 ordinary array update 的 BPFtime runtime 公共分发也要求拆到每个语句，则“仍可继续”的项目是5项；但它已有足够的顶层方向和主要机制解释，因此不属于最高优先级。

## 下一步顺序

1. Per-CPU hash lookup：2×2 hasher/equality A/B，另测 key preparation，剩余 Boost container path 用 perf/反汇编确认。
2. Per-CPU hash update：复用 lookup 的 find 结论，拆 `value_vec.assign()`、entry destination 和 `std::copy`。
3. Per-CPU array update：比较原 `std::function` wrapper、模板 inline wrapper、direct body、precomputed destination。
4. Per-CPU array lookup：复用 array update 的 wrapper/CPU/address 分析，补 lookup-specific return path。
5. Ordinary array update：仅在需要全矩阵叶子闭合时，继续拆 fd lookup、variant extraction、map-type switch 和 handler。

## 相关报告

- [普通 map 系统报告](../ordinary/bpftime-uprobe-ordinary-map-systematic-report-20260804.md)
- [Per-CPU map 路径诊断](../percpu/bpftime-uprobe-percpu-path-diagnosis-20260805.md)
- [ARM64 matched 顶层差距](bpftime-uprobe-matched-kernel-gap-arm64-20260812.md)
- [ARM64 生产路径归因](bpftime-uprobe-production-path-attribution-arm64-20260813.md)
