# summry — bpftime benchmark 分析文档

## 顶层（活跃维护）

| 文档 | 内容 |
|---|---|
| `bpftime-arm-performance-root-cause-20260727.md` | **一页纸根因报告**：ARM 上为什么输给 kernel eBPF（三层根因 + 修复后状态），入口首选 |
| `bpftime-uprobe-percpu-cross-architecture-analysis-20260728.md` | uprobe 原版/修复版与 ARM64/x64 per-CPU map 对比，解释 ARM 相对倍率更大的分母效应 |
| `bpftime-uprobe-map-path-investigation-guide-20260731.md` | `benchmark/uprobe` map helper 路径阅读、分层测量、操作矩阵与跨架构验证指南 |
| `bpftime-uprobe-hash-map-six-operation-semantics-20260801.md` | hash map 六种稳定操作语义、原 delete-miss 问题及修正后的 delete-hit 计时设计 |
| `bpftime-uprobe-array-hash-cross-architecture-experiment-plan-20260801.md` | 普通 array/hash lookup/update 的 ARM64/x64 matched-path 实验设计：JIT A/B、L0–L3、kernel BPF runtime A/B |
| `bpftime-uprobe-kernel-map-runtime-arm64-20260801.md` | 第二阶段 Jetson kernel BPF runtime A/B：普通 array/hash lookup-hit 与 update-existing 的 ARM64 基线 |
| `bpftime-uprobe-x64-kernel-map-runtime-handoff-20260802.md` | x64 交接清单：用相同 harness 补齐 kernel map runtime A/B、归档并计算 x64/ARM64 比率 |
| `bpftime-uprobe-array-cross-architecture-diagnosis-20260731.md` | 普通 array map 的 ARM64/x64 差异：lookup 内联、无效 delete、x64 kernel update 分档 |
| `bpftime-uprobe-array-jetson-path-decomposition-20260801.md` | Jetson 普通 array map userspace 路径的 L0–L3 与 JIT/helper 分层结果 |
| `bpftime-uprobe-hash-jetson-path-decomposition-20260801.md` | Jetson 普通 hash map userspace 路径的查找、复制、哈希及容器访问分层结果 |
| `bpftime-uprobe-arm64-map-three-task-summary-20260804.md` | **Jetson 普通 map 三项任务总结**：array lookup、array update 与 hash lookup 的顶层现象、路径归因、证据边界及 hash 最新源码级 A/B |
| `bpftime-perf-event-output-cpu-affinity-explanation-20260728.md` | `bpf_perf_event_output` 临时绑核的用途、冗余条件与兼容性边界 |
| `bpftime-official-no-btf-change-log.md` | bug 修复记录（已修 / 未修清单） |
| `bpftime-official-no-btf-ssl-nginx-benchmark-runbook.md` | 操作手册：docker 流程、变体、脚本、归档口径 |
| `performance-analysis-playbook.md` | 性能分析方法论 checklist（从本战役提炼，可复用） |

> 源码阅读指南在独立分支 [`docs/source-reading-guide`](https://github.com/plsy1/bpftime-benchmark/tree/docs/source-reading-guide)。

## archive/（调查过程的详细报告，结论已并入根因报告）

- `bpftime-official-no-btf-ssl-nginx-path-ablation-20260726.md` — 消融方法与全部数据（最详尽）
- `bpftime-perf-event-output-affinity-redundancy-20260727.md` — 绑核为何冗余的代码级论证
- `bpftime-software-perf-record-alignment-fix-20260722.md` — 8 字节对齐 bug 定位与修复
- `bpftime-latest-jetson-ssl-nginx-bridge-path-analysis-20260721.md` — 早期 bridge 归因
- `bpftime-v020-*` — v0.2.0 时代的验证报告
