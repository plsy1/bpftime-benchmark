# bpftime 源码阅读指南

针对 [bpftime](https://github.com/eunomia-bpf/bpftime) 的深度阅读指南，
面向第一次读大型系统代码的读者。行号基于 `codex/official-no-btf` 的
`ead56c9`（含三个性能/正确性修复），与上游 master 差异很小，可直接对照阅读。

**组织方式**：以 ssl-nginx 场景（nginx + sslsniff uprobe 追踪）为锚点，
先纵向跟完一个事件的完整生命周期，再横向逐子系统展开。每章含注释版代码选段、
mermaid 图、关键调用链、易误解点，以及带答案（折叠块）的自测题。

## 阅读策略

1. **验证式阅读远快于干读**：每读一段就动手验证——`bpftimetool export` 看
   共享内存、加一行日志重编、跑个短测。第 8 章给了具体实验。
2. 主线（第 1 章）一天可走完；六个子系统章每章半天到一天，按依赖顺序。
3. 行号会随上游演进漂移，用函数名 grep 定位；`.hpp` 先于 `.cpp`。
4. 各章末尾的自测题答不上来 = 该段没读透，答案在折叠块里。

## 目录

| 章节 | 内容 |
|---|---|
| [0. 全景架构](00-architecture.md) | bpftime 是什么、组件关系图、目录导览、术语表 |
| [1. 一个事件的完整生命周期](01-lifecycle.md) | **主线必读**：13 步从 syscall 拦截到消费者，配时序图 |
| [2. runtime 核心：共享内存与 handler 注册表](02-runtime-shm.md) | "内核替身"：假 fd、handler_variant、epoch seqlock |
| [3. agent 与 syscall-server](03-agent-syscall-server.md) | 两个 LD_PRELOAD 入口层：符号拦截与 attach 装配 |
| [4. attach：Frida uprobe](04-attach-frida.md) | inline hook、pt_regs 构造、override/replace 机制 |
| [5. vm：执行引擎与 LLVM JIT](05-vm-llvm-jit.md) | 字节码→本机码、helper 符号绑定、bpf-to-bpf 调用 |
| [6. bpf_map 与 bpf_helper](06-maps-helpers.md) | per-CPU 模拟、perf event output 全文带读、probe_read 机制 |
| [7. 外围](07-periphery.md) | daemon 内核劫持、verifier、bpftimetool/cli、单测 |
| [8. 实践](08-practice.md) | 阅读节奏、验证实验、**PR 辩护精读清单**、代码-注释不一致清单 |

## 相关文档

- 性能调查战役的结论与方法论：分支 [`summry/jetson`](../../tree/summry/jetson)
  （根因报告、bug 修复记录、性能分析 playbook）
- benchmark 运行方式：分支 [`main`](../../tree/main) 的 README
