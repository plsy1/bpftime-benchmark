# 交给 Jetson 会话：x86 系统调用对照的发现（2026-10-29，NUC）

来源：按 `summry/jetson` 的 `syscall/x86-nuc-syscall-handoff-20261029.md` 在 x86 NUC 上做的对照。结论报告见
[bpftime-x86-nuc-syscall-20261029.md](bpftime-x86-nuc-syscall-20261029.md)，原始数据在 `benchmark-results/x86-nuc` 的 `syscall/` 下。
NUC 上没有改开发分支。

## 0. 不需要修的（交接第 1 节第 1、4 项的检查结果）

- `7a7d29b8` 在 x86 上能编译：transformer 和 `libbpftime-agent-transformer.so` 都正常，transformer 的源文件没有警告。
- 每日过滤器 113/113 通过。
- zpoline 正确：计数程序的进入/退出在 3 组 × 5 轮里都是 1,000,000 / 1,000,000，135 次运行都没有崩溃或卡住。

**没有需要修的 bug。**

## 1. 官方 `benchmark/syscall` 不能绑在一个 CPU 上（测量方法的坑，不是开发分支的 bug）

- **现象**：用 `taskset -c 2` 把整个 `benchmark.py` 绑在一个核上，kernel 和 bpftime 两个 leg 都翻倍（225 / 358 ns，不追踪是 92 ns）。
- **原因**：加载器 `benchmark/syscall/syscall.c` 的主循环是 `while (!exiting);`，在整个 leg 期间一直占满一个 CPU，和 victim 平分了这个核。
- **验证**：给脚本两个 P 核（CPU 2、4，两个都钉同一频率）后，结果变成 112.5 / 179.0 ns，和拆层测量一致。
- **影响**：Jetson 10/29 那次没有绑核，不受影响。以后如果要给这个 benchmark 绑核，至少给两个 CPU。按"程序和工具原样不动"的规矩，脚本没改，只是记下来。

## 2. 分发层：x86 约 220 个周期，ARM64 约 349 个周期（给 perf 剖析的参考）

- **数据**：`agent-other` 相对 `passthrough` 的增量，换算成周期：
  - x86 1.7 GHz：221 个周期
  - x86 4.4 GHz：222 个周期
  - Jetson：349 个周期
- **结论**：同一段 C++ 代码（`syscall_callback` → `dispatch_syscall`），ARM64 每次系统调用多约 130 个周期。
- **建议**：剖析时两边对照着看。可以先看下面几处，都是推测，没有测过：
  - `curr_thread_override_return_callback` 是 `inline thread_local std::optional<std::function<...>>`，位于 dlmopen 出来的命名空间里。每次系统调用要赋值两次、`reset` 两次，每次都要访问动态 TLS，并构造和析构 `std::function`。
  - 两层 `try`/`catch`。
  - `sys_enter_callbacks[sys_nr]` 等容器的查找。
- **NUC 能配合的**：需要的话可以在 NUC 上对 `agent-other` 做同样的 perf，方便对照。

## 3. zpoline 跳板约 110 个周期，比 ARM64 逐处跳板（65 个周期）贵

- **数据**：`rewrite` 相对 `none` 的增量，换算成周期：
  - x86 1.7 GHz：112 个周期
  - x86 4.4 GHz：107 个周期
  - Jetson：65 个周期
- **推测（未单独测）**：zpoline 把 `syscall` 改成 `call *%rax`，跳到地址 = 系统调用号处，再顺着 nop 滑到第 512 字节的 `push; movabs; jmp *%rax`。write 的调用号是 1，每次要执行 511 个 nop。
- **影响**：如果推测成立，x86 的跳板成本取决于系统调用号，跨架构比较时要注明用的是哪个调用（这次是 write）。
- **怎么验证**：换一个调用号大的系统调用，比如 getppid（110），在 `rewrite` leg 里比较。

## 4. 测量记录

| 项目 | 值 |
|---|---|
| 机器 | i5-1240P，Debian 13，内核 6.12.113+deb13-amd64 |
| 开发分支 | `7a7d29b8` |
| 编译 | gcc-13，LLVM 19.1.7，LTO；workload 用 gcc 14.2 编 |
| `vm.mmap_min_addr` | 原值 65536，测量时 0，测完恢复 |
| 频率 | CPU 2（和 4）钉 1.7 GHz（关 turbo）或 4.4 GHz（开 turbo），实测中位 1709 / 4359 MHz |
