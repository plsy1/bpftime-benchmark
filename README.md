# summry — bpftime benchmark 分析文档

## 顶层（活跃维护）

| 文档 | 内容 |
|---|---|
| `bpftime-arm-performance-root-cause-20260727.md` | **一页纸根因报告**：ARM 上为什么输给 kernel eBPF（三层根因 + 修复后状态），入口首选 |
| `bpftime-uprobe-percpu-cross-architecture-analysis-20260728.md` | uprobe 原版/修复版与 ARM64/x64 per-CPU map 对比，解释 ARM 相对倍率更大的分母效应 |
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
