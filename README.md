# bpftime 源码阅读指南

针对 [bpftime](https://github.com/eunomia-bpf/bpftime) 的深度阅读指南，
面向第一次读大型系统代码的读者。**基线是官方上游 master 的 `177a1dc465ff`
（2026-10-18）**，文中路径、函数名、行号和代码选段都按这一提交核对，描述的是官方
代码本身的行为。

> 本指南初稿写于七月，对照的是 `ead56c9`（上游 `6bd5319d` + 我们的三个修复）。此后
> 上游又合入 58 个提交：perf record 对齐修复已以 #612 合入，probe_read 改为 no-fault
> 拷贝（#642），每事件绑核仍保留；另新增 riscv64 的 trap uprobe 后端（#665）、宿主进程
> 契约（#634）、`agent_config` → `runtime_config` 改名（#562）等。正文以官方现状为准，
> 七月工作只在相关处以一两句注明。

**组织方式**：以 ssl-nginx 场景（nginx + sslsniff uprobe 追踪）为锚点，
先纵向跟完一个事件的完整生命周期，再横向逐子系统展开。每章含注释版代码选段、
mermaid 图、关键调用链、易误解点，以及带答案（折叠块）的自测题。

## 阅读策略

1. **验证式阅读远快于干读**：每读一段就动手验证——`bpftimetool export` 看
   共享内存、加一行日志重编、跑个短测。第 8 章给了具体实验。
2. 主线（第 1 章）一天可走完；六个子系统章每章半天到一天，按依赖顺序。
3. 行号对应 `177a1dc465ff`，会随上游演进漂移，用函数名 grep 定位；`.hpp` 先于 `.cpp`。
4. 各章末尾的自测题答不上来 = 该段没读透，答案在折叠块里。

## 目录

| 章节 | 内容 |
|---|---|
| [0. 全景架构](00-architecture.md) | bpftime 是什么、组件关系图（含 Frida/trap 两种 uprobe 后端）、宿主进程契约、目录导览、术语表 |
| [1. 一个事件的完整生命周期](01-lifecycle.md) | **主线必读**：13 步从 syscall 拦截到消费者，配时序图 |
| [2. runtime 核心：共享内存与 handler 注册表](02-runtime-shm.md) | "内核替身"：假 fd、handler_variant、epoch seqlock |
| [3. agent 与 syscall-server](03-agent-syscall-server.md) | 两个 LD_PRELOAD 入口层：符号拦截、fail-open 兜底与 attach 装配 |
| [4. attach：Frida uprobe（附 trap 后端）](04-attach-frida.md) | inline hook、pt_regs 构造、override/replace 机制；riscv64 的断点 + SIGTRAP 后端 |
| [5. vm：执行引擎与 LLVM JIT](05-vm-llvm-jit.md) | 字节码→本机码、helper 符号绑定、bpf-to-bpf 调用、惰性 JIT 与已知问题 |
| [6. bpf_map 与 bpf_helper](06-maps-helpers.md) | per-CPU 模拟、perf event output 全文带读（官方仍逐事件绑核）、no-fault probe_read（`process_vm_readv`） |
| [7. 外围](07-periphery.md) | daemon 内核劫持、verifier、bpftimetool/cli、单测 |
| [8. 实践](08-practice.md) | 阅读节奏、验证实验、**PR 状态清单**（七月三个修复的上游去向，去绑核提案的论据）、代码-注释不一致清单 |

## 相关文档

- 我们优化分支（dev 分支）相对官方的改动与实测：分支
  [`summry/jetson`](../../tree/summry/jetson) 的
  [`realprog/`](../../tree/summry/jetson/realprog) 报告；本指南不重复这些内容
- 性能调查战役的结论与方法论：同在 [`summry/jetson`](../../tree/summry/jetson)
  （根因报告、bug 修复记录、性能分析 playbook）
- benchmark 运行方式：分支 [`main`](../../tree/main) 的 README
