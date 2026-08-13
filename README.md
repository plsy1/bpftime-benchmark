# BPFtime 性能调查文档索引

本分支保存 Jetson ARM64、x64 跨平台性能调查的结论、实验设计、源码路径分析和
运行手册。原始数据位于同一仓库的 `benchmark-results/jetson` 分支。

## 建议阅读顺序

1. [2026-08-05 组会材料](meeting-20260805.md)：
   普通 map/per-CPU map 跨平台确认与 Jetson 路径归因的精炼汇报；配套
   [讲稿](meeting-20260805-speaker-notes.md)。
2. [普通 map 系统报告](bpftime-uprobe-ordinary-map-systematic-report-20260804.md)：
   当前 uprobe 普通 map 调查的完整结论，包含六项 benchmark 语义、跨平台结果及
   array lookup/update、hash lookup 的路径归因。
3. [ARM 性能根因总览](bpftime-arm-performance-root-cause-20260727.md)：
   `ssl-nginx` 调查的一页纸总结，说明 probe/runtime、输出路径和已修问题。
4. [性能分析方法手册](performance-analysis-playbook.md)：
   从本轮工作提炼出的可复用实验与归因流程。
5. [ARM64 matched 顶层对照补充](bpftime-uprobe-matched-kernel-gap-arm64-20260812.md)：
   统一 operation-specific control 下的 BPFtime−kernel 差额与 PMU 复核，并明确
   不能把跨 harness 代数余项解释为独立 runtime 阶段。
6. [ARM64 生产路径归因](bpftime-uprobe-production-path-attribution-arm64-20260813.md)：
   用同一诊断 runtime 二进制执行生产 helper 路径 A/B，并以 PMU 验证具体操作对
   BPFtime−kernel 顶层差距的贡献量级。

## uprobe：普通 map

| 文档 | 内容 |
|---|---|
| [系统报告](bpftime-uprobe-ordinary-map-systematic-report-20260804.md) | **主入口**：六项结果、ARM64/x64 趋势、三个重点项目及 hash lookup 叶子级闭环 |
| [ARM64 matched 顶层对照补充](bpftime-uprobe-matched-kernel-gap-arm64-20260812.md) | 普通/per-CPU lookup/update 的统一 matched 差额和 PMU 复核 |
| [ARM64 生产路径归因](bpftime-uprobe-production-path-attribution-arm64-20260813.md) | fd/variant、generic handler、per-CPU array、Boost hash/find/value copy 和 delete 回收的生产路径 A/B |
| [map 路径调查指南](bpftime-uprobe-map-path-investigation-guide-20260731.md) | 源码阅读顺序、L0–L3 分层、操作矩阵与测量边界 |
| [hash 六种操作语义](bpftime-uprobe-hash-map-six-operation-semantics-20260801.md) | lookup/update/delete 的 hit、miss、insert、existing 语义，以及 delete benchmark 修正 |
| [跨架构实验方案](bpftime-uprobe-array-hash-cross-architecture-experiment-plan-20260801.md) | 顶层、JIT A/B、direct runtime 与 kernel runtime 的 matched-path 设计 |
| [ARM64 kernel runtime](bpftime-uprobe-kernel-map-runtime-arm64-20260801.md) | Jetson 普通 array/hash lookup、update 的 kernel helper 基线 |
| [x64 实验交接](bpftime-uprobe-x64-kernel-map-runtime-handoff-20260802.md) | x64 环境、命令、统计口径和归档要求 |
| [array 跨架构诊断](bpftime-uprobe-array-cross-architecture-diagnosis-20260731.md) | array lookup/update 的 ARM64/x64 趋势和 kernel update 差异 |
| [Jetson array 路径分解](bpftime-uprobe-array-jetson-path-decomposition-20260801.md) | array userspace L0–L3、JIT/helper 与 generic dispatch 成本 |
| [Jetson hash 路径分解](bpftime-uprobe-hash-jetson-path-decomposition-20260801.md) | hash lookup 的哈希、查找、复制和容器访问路径 |

## uprobe：per-CPU map

| 文档 | 内容 |
|---|---|
| [per-CPU 跨架构分析](bpftime-uprobe-percpu-cross-architecture-analysis-20260728.md) | ARM64/x64、kernel/BPFtime 的 per-CPU map 结果和倍率解释 |
| [ARM per-CPU 根因](bpftime-percpu-arm-root-cause-20260729.md) | Jetson userspace per-CPU map 的源码路径与主要成本 |
| [x64 per-CPU hash delete 交接](bpftime-uprobe-percpu-hash-delete-x64-handoff-20260804.md) | 修复 delete helper 语义后的 x64 单轮重建、固频运行、解析和归档步骤 |
| [Jetson per-CPU 路径分层诊断](bpftime-uprobe-percpu-path-diagnosis-20260805.md) | array/hash 的 CPU 选择、key/hash 查找、value copy、公共 handler/SHM，以及 per-CPU hash delete 同步节点析构/shared-memory allocator 根因 |

普通 map 调查与 per-CPU map 调查是两条不同路径：前者重点分析普通
array/hash helper，后者包含 CPU 选择和 per-CPU 数据布局，不应把两组结论直接
互相替代。

## ssl-nginx 与 perf-event 输出路径

| 文档 | 内容 |
|---|---|
| [ARM 性能根因总览](bpftime-arm-performance-root-cause-20260727.md) | **主入口**：`ssl-nginx` 端到端结论和修复后状态 |
| [CPU affinity 语义说明](bpftime-perf-event-output-cpu-affinity-explanation-20260728.md) | `bpf_perf_event_output` 临时绑核的目的、冗余条件和兼容边界 |
| [No-BTF 修改记录](bpftime-official-no-btf-change-log.md) | official no-BTF 分支已做与未做的修改 |
| [ssl-nginx 运行手册](bpftime-official-no-btf-ssl-nginx-benchmark-runbook.md) | Docker/host 流程、变体、脚本和结果归档口径 |

## 工程与协作材料

| 文档 | 内容 |
|---|---|
| [性能分析方法手册](performance-analysis-playbook.md) | 实验控制、matched A/B、PMU、统计和结论边界 checklist |
| [上游 issue 草稿](upstream-issue-drafts.md) | 可整理后提交上游的问题描述 |

源码阅读指南位于独立分支
[`docs/source-reading-guide`](https://github.com/plsy1/bpftime-benchmark/tree/docs/source-reading-guide)。

## archive：历史调查过程

| 文档 | 内容 |
|---|---|
| [ssl-nginx 路径消融](archive/bpftime-official-no-btf-ssl-nginx-path-ablation-20260726.md) | V1–V4 消融方法和完整数据解释 |
| [perf-event-output affinity 冗余](archive/bpftime-perf-event-output-affinity-redundancy-20260727.md) | 临时绑核成本及冗余条件的代码级分析 |
| [software perf record 对齐修复](archive/bpftime-software-perf-record-alignment-fix-20260722.md) | 8 字节对齐 bug、隐藏丢事件与方差下降 |
| [latest bridge 路径分析](archive/bpftime-latest-jetson-ssl-nginx-bridge-path-analysis-20260721.md) | 早期 Docker bridge/host 网络环境调查 |
| [v0.2.0 host 网络验证](archive/bpftime-v020-jetson-host-network-validation.md) | v0.2.0、Tailscale/netfilter 和 network namespace 对照 |

`archive/` 中的材料保留调查过程和历史口径；需要引用当前结论时，优先使用顶层的
系统报告或根因总览。
