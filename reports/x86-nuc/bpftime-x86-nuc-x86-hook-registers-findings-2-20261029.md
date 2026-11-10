# 交给 Jetson 会话：x86 跳板寄存器修复（e299cd3f）在 x86 上的验证（2026-10-29 夜，续）

来源：`summry/jetson` 的 `syscall/x86-nuc-x86-hook-registers-handoff-2-20261029.md`。原始记录在 `benchmark-results/x86-nuc` 的
`syscall/x86-nuc-syscount-nginx-20261029/regs-fix/`（README、`build-e299cd3f/`、`mutation/`、`regcheck-e299cd3f.log`、
`audit-e299cd3f.jsonl`、`correct/`）。NUC 上没有改开发分支。

## 0. 已确认

- **构建**：`e299cd3f` 在 x86 上用 GCC 13 LTO 链接成功，`libbpftime-agent-transformer.so` 和 `bpftime_text_segment_transformer_tests` 都生成了；10 条警告和以前同类。第一份发现文件第 1 节的问题已解决。
- **寄存器修复有效**：用 regcheck（走真正的 transformer `.so`）复查：
  - 不挂程序、挂 syscount 程序两种情况下都是 **0 个寄存器改变**，包括 rdi…r9 和 xmm0–15。
  - 修复前的 `59e5e154` 是 6 个寄存器加 xmm 被改，不挂程序时 rdx 也被改。
- **变异检查**（`x86-hook-before-fix-mutation.patch`，临时 worktree，不提交）：打上补丁后 regcheck 和 `59e5e154` 完全一样坏，所以修复前确实会出错。但新测试本身分辨不出修复前后，见第 1 节。
- **nginx**：先加载 syscount 再用跳板启动 nginx，能正常起来。审计里 bpftime 每个请求数到 9.016 次系统调用，内核是 9.015。
- **其他测试**：每日过滤器 113/113，`bpftime_syscall_trace_attach_tests` 8/8。

## 1. 需要修：新测试在 x86 上必然失败（测试没注册 capstone 的 x86 架构）

- **现象**：root 加 `vm.mmap_min_addr=0` 运行 `bpftime_text_segment_transformer_tests`，3 次都是 2/3。
  - 新用例 `x86: a rewritten syscall keeps registers and the red zone` 的两个 SECTION 都失败，子进程退出码 1。
  - 日志里是 `Failed to open capstone instance: 2, Invalid/unsupported architecture(CS_ERR_ARCH)`（`build-e299cd3f/tests-transformer-*.log`）。
- **原因**：
  - 测试的子进程直接调用 `bpftime::setup_syscall_tracer()`，但 frida-gum 带的 capstone 要先调用 `cs_arch_register_x86()` 才能打开 x86 实例。
  - `rewrite_segment` 里 `cs_open` 失败时会直接 `exit(1)`，所以寄存器检查根本没执行。
  - 正式路径不受影响：`agent-transformer.cpp` 在 `setup_syscall_tracer()` 之前调用了 `cs_arch_register_x86()`。
- **影响**：
  - 打上变异补丁后，这个用例也同样以退出码 1 失败（`mutation/tests-transformer.log`）。所以它现在"修没修都失败"，起不到回归保护的作用。
  - 修复是否正确，这次靠 regcheck 证明。
- **修法建议**（没改，没验证）：
  - 在测试子进程里调用 `setup_syscall_tracer()` 之前，先调用 `cs_arch_register_x86()`；
  - 或者把这次注册挪进 x86 的 `setup_syscall_tracer()` 本身，这样任何调用方都不用记得这一步。
  - 修好之后，在 e299cd3f 上应该 3/3 通过，打上变异补丁后应该失败。NUC 可以复测。

## 2. 值得看一下：x86 上只加载跳板就要 −11.9%（不算 bug）

- **数据**：`run_correct.py` 10 轮，CPU 0–7 钉 1.7 GHz，60 次全部有效。
  - transformer_only（只加载跳板、不挂程序）吞吐 −11.92%，每次系统调用 +257 ns，约 440 个周期。
  - Jetson 是 −3.4%（+139 ns）。
  - bpftime 追踪 nginx 是 −18.7%（+437 ns，约 750 个周期），Jetson 是 −11.4%（+510 ns，约 880 个周期）。
- **可能的原因**（都没有单独测过）：
  - zpoline 的 nop 滑道：nginx 平均调用号约 53，每次约 460 个 nop；write 的 511 个 nop 在写循环里测过约 130 个周期。
  - `0a602133`/`e299cd3f` 在每次系统调用上都新加了 XSAVE/XRSTOR（掩码 0xe7）和参数寄存器的保存与恢复，即使没有程序也是如此。
- **建议**：用写循环拆层，对比 `build-59e5e154` 和 `build-e299cd3f` 的 rewrite 一层，量出修复本身的成本。需要的话 NUC 可以做。
  - 如果 XSAVE 很贵：现在每条路径都要进 C 代码，可以把"这个调用号有没有程序"的判断前移到汇编里（例如查一张按调用号索引、由 agent 维护的标志表）。没有程序时，在 XSAVE 之前就直接发起系统调用。
  - 另一个办法是让 agent 编译时不用向量寄存器，但 JIT 代码和 libc 难以保证这一点。
