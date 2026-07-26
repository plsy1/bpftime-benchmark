# bpftime 源码阅读指南

针对 `~/src/bpftime-offical-no-btf`（分支 codex/official-no-btf，行号基于 2026-07-27
commit `076e3e4` 之后的代码）。读者定位：已深入研究过 ssl-nginx benchmark 热路径
（uprobe → `bpf_perf_event_output` → software perf buffer per-thread shard → 消费者），
熟悉 `bpf_helper.cpp` 与 `perf_event_handler.cpp` 的相关段落。本指南以这条热路径为
锚点向外扩展：**先纵向跟完一个事件的完整生命周期，再横向逐子系统展开**。

## 阅读策略

1. 你手里有全套可运行的 benchmark 环境——**验证式阅读**远快于干读：每读一段就用
   `bpftimetool export` 看 shm、加一行 SPDLOG_INFO 重编跑一轮 smoke、或写个最小
   复现。本文每章末尾的"验证性问题"就是为此设计的：读完答不上来 = 漏了关键代码。
2. 全局主线一天可以走完（第一部分）；六个子系统按依赖顺序每个半天到一天。
3. 行号会漂移，用函数名 grep 定位；`.hpp` 先于 `.cpp`。

## 目录

- [第一部分：一个事件的完整生命周期（主线，必读）](01-lifecycle.md)
- [1. runtime 核心：共享内存与 handler 注册表（"内核替身"）](02-runtime-shm.md)
- [2. agent 与 syscall-server（两个 .so 的入口薄层）](03-agent-syscall-server.md)
- [3. attach/：事件挂载层（Frida uprobe）](04-attach-frida.md)
- [4. vm/：执行引擎（LLVM JIT）](05-vm-llvm-jit.md)
- [5. bpf_map/ 与 bpf_helper.cpp（map 实现与 helper 注册）](06-maps-helpers.md)
- [6. 外围：daemon / verifier / tools / 单测](07-periphery.md)
- [第三部分：整体节奏、验证实验与已知不一致](08-practice.md)

## 更新记录

- 2026-07-27：首版（行号对应 commit `076e3e4`）。
