# 给 NUC 会话的交接：验证 x86 跳板寄存器修复，再重跑 syscount-nginx（2026-10-29 夜）

接着你的 `summry/x86-nuc` 下的 `x86-nuc/bpftime-x86-nuc-syscount-nginx-findings-20261029.md` 第 1 节。**回复用户用中文**，
NUC 上不改开发分支，推送前问用户。方法、容器、频率沿用 `syscall/x86-nuc-syscount-nginx-handoff-20261029.md` 和你上一轮的脚本。

## 0. 改了什么（开发分支 `0a602133`，在 `59e5e154` 之后）

提交名："text segment transformer: keep every register across the x86 hook"。Jetson 编不了 x86，只做了两项检查：
clang 交叉汇编、x86 目标的语法检查。**还没有在 x86 上编译和运行过，这正是要你做的。**

- **`syscall_hooker_asm`（x86-64）**：
  - 调 C 函数之后，从栈上恢复 rdi、rsi、rdx、r10、r8、r9。返回值 rax 是 C 函数的返回值，rcx 和 r11 本来就会被 `syscall` 改掉。
  - 向量状态用 `XSAVE`/`XRSTOR` 保存和恢复，掩码 0xe7（x87、SSE、AVX、opmask、AVX-512 的两部分；不含 AMX，和 glibc 延迟绑定的跳板一样）。保存区放在栈上、64 字节对齐，大小由 `setup_syscall_tracer` 用 CPUID 算出。没有 OS 启用的 XSAVE 时不改写任何代码，只记日志。
  - 栈帧从调用方 128 字节 red zone 的下面开始：先 `lea -120(%rsp)`，返回前再加回来。原来的代码直接在 red zone 里压栈。只有被改写那条 `call *%rax` 本身写入 red zone 顶部的 8 字节，仍然无法避免。
  - 汇编改成文件级的 `.pushsection .text` 块（和 aarch64 一样），避免 LTO 把它丢掉。
- **新测试**：`bpftime_text_segment_transformer_tests` 在 x86 上新增 `x86: a rewritten syscall keeps registers and the red zone`，有两个 SECTION：
  - 文件：`test/test_syscall_rewrite_x86.cpp` 加 `test/regs_check_x86_64.S`。
  - 做法：把 rdi、rsi、rdx、r10、r8、r9、rbx、r12–r15、xmm0–15 和 red zone（rsp−24 到 rsp−128）都设成已知值，经改写后的 `syscall` 调一次 `getppid`，再逐项比较。
  - 两种情况：不挂 hook（C 入口直接发起系统调用，相当于没挂程序时 agent 的直通路径）；挂一个会用到寄存器、浮点和 memset 的 hook（相当于挂了程序）。
  - 需要 root 和 `vm.mmap_min_addr=0`，否则 SKIP。ymm 的高半部分没测，测试只用 SSE 指令。
- CLAUDE.md 新增一节 "x86-64 System Call Hook Registers"。

## 1. 要做的事（按顺序）

1. **构建**：拉到 `0a602133` 或更新，用和 `build-59e5e154` 相同的选项建新目录。编不过就停下，把错误写进发现文件。
2. **测试**：
   - 每日过滤器、`bpftime_syscall_trace_attach_tests`（root）。
   - root 加 `vm.mmap_min_addr=0` 跑 `bpftime_text_segment_transformer_tests`，应当 3/3 通过：2 个 pthread key 用例，加 1 个新用例的两个 SECTION。连跑 3 次。原值记下来，测完恢复。
3. **确认新测试在修复前会失败**（"不修就失败"）：
   - 在一个临时 worktree 里检出 `0a602133`，再只把 `attach/text_segment_transformer/text_segment_transformer.cpp` 换回 `59e5e154` 的版本（`git checkout 59e5e154 -- <该文件>`），保留新测试，构建并运行。
   - 预期：挂 hook 的 SECTION 报 rdi…r9 和 xmm 被改；不挂 hook 的 SECTION 至少报 red zone 被改。
   - 把输出记下来。这个 worktree 只在本地用，不提交。
4. **你的 `regcheck`**：用新构建跑一遍"跳板不挂程序"和"跳板挂 syscount 程序"两种情况，应当 0 个寄存器改变。
5. **nginx**：用 Jetson 的 `repro-crash.sh` 方式（先加载 syscount，再用跳板启动 nginx）确认 nginx 能起来；`audit.py` 的 transformer 一组应当和内核计数一致（约 9.02 次/请求）。
6. **正式测量**（上一轮没跑成的部分）：
   - `run_correct.py --rounds 10`，6 组轮换，CPU 0–7 钉 1.7 GHz、关 turbo，和你的官方脚本对照用同一个设置。
   - nginx 不能崩，bpftime 的计数要和内核一致。
   - 测前用 `ps` 检查残留进程；后台命令里不要出现 `syscount`、`syscall`、`bpftime`、`victim`。
7. **可选：perf**，方法同 Jetson（`profile_nginx.sh`）。x86 上 zpoline 的 nop 滑道会占一部分（nginx 的调用号各不相同，不只是 write 的 1）。

## 2. 结果放哪里

- 原始数据：`benchmark-results/x86-nuc` 的 `syscall/x86-nuc-syscount-nginx-20261029/`。在原目录里加一个子目录（例如 `regs-fix/`）或新开目录都行，写清楚用的是哪个提交。
- 结论：写进 `summry/x86-nuc`，把上一轮对比表里"x86 正确跑法"那一列填上。
- 发现的问题写进发现文件，NUC 上不改开发分支。推送前先问用户。
