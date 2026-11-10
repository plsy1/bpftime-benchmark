# 交给 Jetson 会话：x86 上 syscount-nginx 的发现（2026-10-29 晚，NUC）

结论报告：[bpftime-x86-nuc-syscount-nginx-20261029.md](bpftime-x86-nuc-syscount-nginx-20261029.md)。
原始数据：`benchmark-results/x86-nuc` 的 `syscall/x86-nuc-syscount-nginx-20261029/`，README 第 3 节有细节。
NUC 上没有改开发分支。

## 1. 需要修：x86 跳板没有保住系统调用要保持的寄存器（上游 bug，官方 177a1dc 也有）

**现象**
- `59e5e154` 修好 pthread key 冲突后，按正确顺序（先加载 syscount 程序，再用跳板启动 nginx）启动，nginx 仍然起不来。
- 位置：在 `__libc_sigaction`（由 `ngx_init_signals` 调用）的 `movups %xmm0,0x8(%r8)` 处 SIGSEGV，r8 = 0x28。
- 来源：`repro-crash-regs-59e5e154.log`，用 Jetson 的 `repro-crash.sh` 加打印寄存器得到。
- glibc 跨过 `rt_sigaction` 的 `syscall` 指令继续使用 r8 里的 `oact` 指针。内核约定，一次系统调用只改 rax、rcx、r11。

**原因**
- 位置：`attach/text_segment_transformer/text_segment_transformer.cpp` 里 x86 的 `syscall_hooker_asm`。
- 它把 rax、rdi、rsi、rdx、r10、r8、r9 压栈，只是为了当作参数传给 `syscall_hooker_cxx`；调用返回后直接 `leave; ret`，这些寄存器和 xmm 都没有恢复。

**最小复现**：`src/regcheck.c`
- 做法：把寄存器设成已知值，通过裸 `syscall` 调用 `getppid` 1000 次，比较前后的值。
- 结果（`regcheck.log`）：

| 构建 | 跳板，不挂程序 | 跳板，挂 syscount 程序 |
|---|---|---|
| 官方 177a1dc | 0 个改变 | rdi、rsi、rdx、r10、r8、r9、xmm0–4、xmm6 改变，之后段错误 |
| 开发 `2e850c45` / `59e5e154` | **rdx 改变** | rdi、rsi、rdx、r10、r8、r9、xmm0、5、7 改变 |

- rbx、rbp、r12–r15 一直完好，因为它们是 C 的被调用者保存寄存器。
- 不挂程序时 rdx 也会变，是 `2e850c45` 的直通分发引入的：`7a7d29b8` 和官方原版不挂程序时都是 0 个改变（`regcheck-7a7d29b8-noprog.log`）。
- 这和用户 7 月的笔记对得上："去掉 SSL 的 nginx 改写后 SIGSEGV"。
- ARM64 不受影响：那边的跳板保存了 x0–x30 和 q0–q31。

**修法建议**（只是建议，没有改代码）
- 在 `call syscall_hooker_cxx` 之后、`leave` 之前，从压栈的位置把 rdi、rsi、rdx、r10、r8、r9 读回来。
- 在调用前后保存并恢复 xmm0–15。
- 如果 agent 或 JIT 代码会用 AVX，内核保留的是完整的向量状态，严格做要考虑 ymm 的上半部分（例如 `xsave`/`xrstor`）。这一点我没有验证 JIT 是否会用到。
- 直接路径（rt_sigreturn、clone、clone3）不调用 C 代码，不受影响。

**测试建议**
- x86 上的 `bpftime_text_segment_transformer_tests` 目前只有 pthread key 用例。可以把 `regcheck` 的逻辑做成一个 x86 用例，对应 aarch64 的 `regs_check_aarch64.S`。
- 用例要在"有程序"和"无程序"两种情况下都检查。它在官方 177a1dc、`2e850c45`、`59e5e154` 上都会失败，满足"不修就失败"。

**影响**：x86 上无法按正确方式跑 syscount-nginx。`run_correct.py` 和 perf 没有跑，修复推送后 NUC 可以重跑。

## 2. 已确认：pthread key 修复在 x86 上有效

- **keycheck**：修复前 4 个新 key 读到 agent 的指针（key 0、1、2、4）；修复后进程拿到 key 32..39，0 个被改坏。和 Jetson 完全一致。
- **nginx**：修复前报 `SSL_get_ex_new_index() failed` 和 `munmap_chunk(): invalid pointer`，就是用户 7 月的报错；修复后这个报错消失，nginx 改为死于第 1 节的寄存器问题。
- **测试**：`bpftime_text_segment_transformer_tests` 2/2，每日过滤器 113/113，`bpftime_syscall_trace_attach_tests` 8/8。

## 3. 其他记录（不是 bug）

- **审计和 Jetson 一致**：内核每个请求数到 9.015 次系统调用；官方方式和"跳板但 nginx 先启动"都数到 0。
- **官方脚本原样跑**（CPU 0–7 钉 1.7 GHz）：内核 −4.27% / −2.92%，bpftime −1.07% / −1.57%（什么都没追踪）。
- **容器坑**：Docker 默认不挂 tracefs，syscount 会报 `failed to determine tracepoint 'raw_syscalls/sys_exit' perf event ID: -ENOENT`。需要 `-v /sys/kernel/tracing:/sys/kernel/tracing`。Jetson 的镜像或主机可能本来就有，所以没遇到。
