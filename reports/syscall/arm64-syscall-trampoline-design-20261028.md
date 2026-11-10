# ARM64 系统调用跳板：svc-hook 源码整理与 bpftime 方案（2026-10-28）

bpftime 在 ARM64 上的普通模式拦不到系统调用：`attach/text_segment_transformer` 的 aarch64 分支自 2024-01（上游 PR #151）
起只有 `// TODO: implement syscall trace trampoline`，上游至今未做（RISC-V #664、QNX #661 都是直接关掉系统调用追踪）。
x86 用的是 zpoline（地址 0 跳板 + `call *%rax`），ARM64 不能照搬：指令必须 4 字节对齐（不能跳到地址 = 调用号），
`bl` 会覆盖 x30。本文整理 svc-hook 的做法和问题，给出 bpftime 的方案。

参考：svc-hook（Akira Moroo，ACM/IFIP Middleware 2025，Apache-2.0，源码 `/home/jetson/src/refs/svc-hook`，提交 `f2f5c8c`），
ASC-Hook（arXiv 2412.05784）。

## 1. svc-hook 的结构

全部在 `main.c` 一个文件（约 1000 行），`LD_PRELOAD` 后在构造函数里依次：扫描 → 建系统调用表 → 建跳板 → 改写 → 加载用户的钩子库。

| 步骤 | 做法 |
|---|---|
| 找 `svc` | 读 `/proc/self/maps` 的可执行映射（跳过 `[stack]`、`[vdso]`）。有文件路径的 ELF：读节头，只扫 `SHF_EXECINSTR` 的节（避开同一段里的 `.rodata`）；否则整段按 4 字节逐字匹配 `svc #imm`。跳过自己的 `svc` |
| 分组 | 按 `b` 的可达范围（±128 MiB）把记录分组，每组一块跳板内存 |
| 分配跳板 | 在可达范围内从低到高逐页 `mmap`（不带 `MAP_FIXED`，不覆盖已有映射），直到落在范围内 |
| 每处 `svc` 的门（stage 1，6 条指令） | `movz x6, #imm`；4 条 `movz/movk x14 = 返回地址`；`b 公共跳转` |
| 每组公共跳转（stage 2，5 条） | 4 条 `movz/movk x15 = asm_syscall_hook`；`br x15` |
| 全局入口（stage 3，汇编） | `rt_sigreturn`(139) 直接 `svc`；`clone`(220)/`clone3`(435) 带 `CLONE_VM` 时把保存的寄存器复制到子线程栈顶；其余：在栈上保存 x0–x30，调用 C 函数 `syscall_hook`，恢复，`br x14` 回去 |
| 真正发起系统调用 | `enter_syscall` 跳到一张预生成的表（每个 `#imm` 一项：`svc #imm; ret`），表里的 `svc` 不改写 |
| 改写 | 逐页 `mprotect` 成 RWX，写一条 `b`，刷指令缓存，恢复权限 |
| 钩子库 | `dlmopen(LM_ID_NEWLM, $LIBSVCHOOK)`（与 bpftime 一样用独立命名空间，钩子库自己的系统调用不被拦截），调用其 `__hook_init` 换掉回调 |

开销（其补充文档，Cortex-A72）：`getpid` 走跳板比直接调用多 11 ns（ASC-Hook 36 ns）；初始化 25 ms（ASC-Hook 1890 ms，
因为它要反汇编找给 x8 赋值的指令）；跳板内存每处 28 字节，libc + ld.so 约 20 KB。SQLite、PostgreSQL、Samba 吞吐下降 0.01–2.6%。

## 2. svc-hook 对 bpftime 不够的地方

1. **会改掉 x6、x14、x15**：门和公共跳转直接用它们当临时寄存器，从未保存（源码注释"uses the following temporary
   registers"）。内核的系统调用保留除 x0 外的所有寄存器，编译器可以在 `svc` 前后把值放在这些寄存器里。
   svc-hook 的默认钩子很简单，问题不易暴露；bpftime 要拦所有程序，不能赌。
2. **不保存浮点/SIMD 寄存器和标志位**：只存 x0–x30。C 函数可以随意改 q0–q7、q16–q31、v8–v15 的高 64 位和 NZCV，
   而内核会保留它们。bpftime 的回调是 C++（eBPF JIT、map、日志），`memcpy` 等都会用 SIMD 寄存器。
3. **clone 依赖钩子函数没有栈帧**（按源码推断，未实测）：子线程在 `enter_syscall` 里醒来，靠 `ret` 回到 `asm_syscall_hook`，再从新栈顶读回
   复制好的寄存器。这只在 `syscall_hook` 和钩子函数都被编译成尾调用、不建栈帧时成立；钩子函数一旦有栈帧（示例里的
   `printf` 钩子就有），子线程会从新栈读到垃圾。这正是 x86 版在上游 #649 之前的同类问题。
4. **出错时 `assert` 或 `exit(1)`**：bpftime 的规矩是注入的代码不能结束宿主进程。

## 3. bpftime 的 ARM64 方案

沿用 svc-hook 的总体结构（逐字或按节扫描、按可达范围分组、附近分配、每处一个门、`b` 改写），以及 bpftime 现有的
`dlmopen` 隔离和与 x86 共用的 C 分发函数 `syscall_hooker_cxx`。不同之处：

**每处 `svc` 的门：不占用任何寄存器（8 条指令，32 字节）**

```
入口:   stp  x16, x17, [sp, #-16]!   // 先把要用的两个寄存器存到栈上
        adr  x16, 入口               // 用 PC 相对地址记下"我是哪一处"，不需要 4 条 movz/movk
        b    公共跳转                 // 公共跳转再用 x17 装绝对地址跳到全局入口
返回:   ldp  x16, x17, [sp], #16     // 全局入口恢复完其他寄存器后 br 到这里
        b    原svc + 4
直通:   ldp  x16, x17, [sp], #16     // clone/clone3/rt_sigreturn：所有寄存器（含 sp）恢复成原样
        svc  #0                      // 在门里直接发起
        b    原svc + 4               // 父子线程都从这里回去，不用栈，也不用任何寄存器
```

**全局入口（汇编）**：保存 x0–x15、x18–x30、NZCV、FPSR、q0–q31（约 800 字节，在应用栈上，ARM64 Linux 没有红区，
压栈安全），调用 C 分发函数，按返回结果恢复所有寄存器（x0 换成返回值），`br` 到本处的"返回"或"直通"。

**特殊系统调用**：`clone`、`clone3`、`rt_sigreturn` 一律走"直通"，不在 C 函数里发起，所以不依赖任何栈帧布局。
`clone` 可以先跑 `sys_enter` 的 eBPF 程序再直通（x86 版完全不追踪 clone）；`exit`/`exit_group` 与 x86 一样直接发起。

**扫描**：有路径的 ELF 按节头只扫可执行节，否则逐字扫描（本机 libc 逐字扫描 517 处，与反汇编完全一致）；跳过
`[vdso]`、`[stack]`、跳板区和 bpftime 自己发起系统调用的那条 `svc`。附近找不到空间、`mprotect` 失败等：跳过该处并记日志，
不退出宿主。

**改写**：每处只写一个对齐的 4 字节字并刷指令缓存。ARM 架构允许在执行中把 `svc` 换成 `b`，所以 `bpftime attach`
注入运行中的进程也安全。

**已知局限（与 x86 相同）**：之后 `dlopen` 的库、JIT 生成的代码不会被改写；vDSO 里的调用本来就不进内核。

## 4. 测试与验证

1. 单元测试（每项先确认在没有正确处理时失败）：
   - 寄存器保持：用内联汇编把 x1–x30、q0–q31、NZCV 设成已知值，发起被改写的 `getpid`，检查全部不变（svc-hook 式的
     临时寄存器用法会在这里失败）；
   - 参数与返回值：`read`/`write`/`openat` 等，含错误返回；
   - `pthread_create`（`clone` 带新栈）、`fork`、`posix_spawn`（`CLONE_VM|CLONE_VFORK`）、`clone3`；
   - 带信号处理函数的程序（`rt_sigreturn`），以及在被拦截的系统调用中途来信号；
   - eBPF `sys_enter`/`sys_exit` 程序能看到正确的调用号和参数，`bpf_override_return` 能改返回值。
2. 官方 `benchmark/syscall`：与同轮 kernel tracepoint 对比（官方 x86 数据：bpftime 446 ns 对 kernel 365 ns）。
3. `syscount-nginx`：先审计确实拦截到系统调用，再采信吞吐。
4. 真实工具：opensnoop、syscount 等 `tracepoint/syscalls/*` 工具；之后 `fuse`。
5. 代码注释和文档注明参考 svc-hook 与 ASC-Hook。
