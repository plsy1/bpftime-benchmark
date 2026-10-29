# 交给 Jetson 会话：c23442ac 的测试复查和寄存器修复的成本（2026-10-29 夜，续二）

来源：`summry/jetson` 的 `syscall/x86-nuc-x86-hook-registers-handoff-3-20261029.md`。原始记录在 `benchmark-results/x86-nuc` 的
`syscall/x86-nuc-syscount-nginx-20261029/regs-fix/`（README 第 7–8 节，`build-c23442ac/`、`mutation-c23442ac/`、`layers-cost/`）。
NUC 上只做了测量，没有改代码。

## 0. 已确认

- **构建**：`c23442ac` 在 x86 上能构建。
- **测试**：每日过滤器 113/113，`bpftime_syscall_trace_attach_tests` 8/8；`bpftime_text_segment_transformer_tests` 以 root 运行、`vm.mmap_min_addr=0`，**连跑 3 次都是 3/3**。上一份发现文件第 1 节的问题已解决。
- **变异补丁**：打上补丁后，新用例两个 SECTION 都因为 red zone 或 xmm 的检查失败（3 次结果相同），不再是 capstone 的退出码 1。

## 1. 建议：新用例抓不到"参数寄存器没恢复"

- **现象**：在变异构建上，"挂 hook"的 SECTION 只报了 xmm0–2 和 red zone 被改，**没有报 rdi…r9**。
- **原因**：
  - `clobbering_hook` 只在调用 `next_hook` **之前**用寄存器。
  - `next_hook`（`call_orig_syscall`）会从参数重新装回 rdi、rsi、rdx、r10、r8、r9。
  - 所以在旧跳板下，这几个寄存器回到调用方时碰巧保持了原值。
- **影响**：
  - 当初让 nginx 崩溃的，是系统调用返回**之后**运行的 exit 程序改了 r8。regcheck 在 `59e5e154` 和变异构建上都能复现这一点，这个用例却复现不了。
  - 如果以后有改动去掉了参数寄存器的恢复、但保留了 red zone 和 XSAVE，这个用例不会报错。
- **建议**（没改，没验证）：让 `clobbering_hook` 在 `next_hook` 返回**之后**也改动参数寄存器。
  - 例如再调用一次带 6 个参数的 noinline 函数，或者用 asm 写入 rdi…r9 并把它们声明为被改动的寄存器。
  - 改完后，在变异构建上应能报出 rdi…r9。

## 2. 供决策：寄存器修复每次系统调用固定多约 255 个周期（x86，写循环）

**测量**：CPU 2 钉 1.7 GHz、关 turbo，`run_layers.py`，A B A B 各 5 轮，对比 `build-59e5e154` 和 `build-c23442ac`。

| 层（相对 none） | 修复前 | 修复后 | 差 |
|---|---:|---:|---:|
| rewrite（只有跳板） | 111 周期（65.0 ns） | 369 周期（216.1 ns） | **+258 周期（+151 ns）** |
| agent-other | 152 周期 | 411 周期 | +260 周期 |
| agent-exit（一个空程序） | 263 周期 | 515 周期 | +252 周期 |
| agent-count（两个计数程序） | 378 周期 | 632 周期 | +254 周期 |

**perf**（rewrite 层，2000 万次 write）：
- 每次 write 从 562 个周期涨到 816 个，指令只从 1368 条涨到 1396 条。
- `xsave (%rsp)` 约 103 个周期（12.6%），`xrstor (%rsp)` 约 127 个周期（15.6%），两条合计约 230 个周期。
- nop 滑道修复前后都是约 131 个周期。

**在 nginx 里的含义**（syscount-nginx 正确跑法，e299cd3f）：只加载跳板时每次调用多约 440 个周期，可以大致拆成：
- XSAVE 修复本身约 255 个周期；
- nop 滑道约 118 个周期：按 nginx 的调用分布加权，平均每次约 459 个 nop；
- 分发约 41 个周期。

这几项都是按写循环的单价估算的，不是在 nginx 里直接测的。

**可选方向**（交接里列的，没有测过）：
- **`xsavec`**：只写入非初始状态的组件。nginx 和 agent 多半不碰 AVX 上半部分。
- **汇编里先判断有没有程序**：没有程序的调用号在 XSAVE 之前就直接发起系统调用。这条路径上不进 C 代码，不需要保存向量状态；但参数寄存器仍然要保住，`syscall` 本身也会改 rcx 和 r11。
- **nop 滑道**：约 118 个周期，是 zpoline 方案本身的成本。

是否改、怎么改，由 Jetson 会话和用户决定。修复是为了正确性，必须保留。
