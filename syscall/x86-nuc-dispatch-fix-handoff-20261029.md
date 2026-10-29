# 给 NUC 会话的交接：x86 上重测系统调用分发修复（2026-10-29 下午）

接着上一份交接 `syscall/x86-nuc-syscall-handoff-20261029.md`（同在 `summry/jetson`），以及 NUC 自己 10/29 的结果
（`summry/x86-nuc` 的 `x86-nuc/bpftime-x86-nuc-syscall-20261029.md`，数据在 `benchmark-results/x86-nuc` 的 `syscall/x86-nuc-*-20261029/`）。
**回复用户用中文**，NUC 上不改开发分支，推送前问用户。

## 0. 背景

NUC 的对照显示，没挂程序时 agent 分发在 x86 上约 220 个周期，是 bpftime 最大的一层。Jetson 在开发分支修好了这一层，提交
`2e850c45`（"syscall trace: pass system calls without programs straight through"，已推送）：

- `dispatch_syscall` 先查有没有程序，没有就直接发起系统调用；
- 有程序时，override 回调只捕获一个指针，不再 `malloc`；
- 修了越界读：调用号不在 0–511 时只跑全局程序。

改动的是两种架构共用的 C++ 代码，CLAUDE.md 对应的一节是 "System Call Dispatch"。

Jetson 修复前后的数据（`benchmark-results/jetson` 的 `syscall/dispatch-fix-20261029/README.md`）：

| | 修复前 | 修复后 |
|---|---:|---:|
| agent-other 相对 passthrough | +202.1 ns（349 周期） | +27.8 ns |
| agent-exit 相对 none | +282.2 ns | +146.1 ns |
| 官方 benchmark/syscall 的 bpftime | 640.0 ns | 515.5 ns |
| 官方 benchmark/syscall 的 kernel | 438.2 ns | 437.8 ns |
| bpftime 开销 / kernel 开销 | 3.4 倍 | 1.9 倍 |

官方仓库公布的 x86 结果（`benchmark/syscall/example_results.md`，2025 年）是 2.3 倍。NUC 修复前测的是 3.9 倍（1.7 GHz）和 4.2 倍（4.4 GHz）。
这次的目的是在同一台 x86 上得到修复前后的对照。

## 1. 要做的事

1. **更新并构建**：开发分支拉到 `2e850c45`，用 10/29 的同样选项建一个新构建目录（例如 `build-2e850c45`），目标和上次相同，
   再加 `bpftime_syscall_trace_attach_tests`。
2. **正确性**：
   - 跑每日过滤器；
   - 用 root 跑 `bpftime_syscall_trace_attach_tests`（有的用例要读 tracefs）。x86 上应当 8/8 通过。Jetson 上有 2 个旧用例因为
     arm64 没有 `fork` 而失败，在 x86 上不该失败；
   - 新加的 4 个用例：没挂程序时零分配，有程序时零分配，越界调用号，改写返回值。
   - 不通过就停下，写进发现文件。
3. **重测**：完全沿用 10/29 的做法和脚本（`x86-nuc-syscall-session-20261029/` 里的 `measure.sh`、`freq.sh`、忙检查），只把构建目录换成新的：
   - 两组频率：1.7 GHz 关 turbo，4.4 GHz 开 turbo；
   - 拆层测量绑 CPU 2，`run_layers.py --rounds 5`；
   - 官方 `benchmark/syscall` 10 轮，**给两个 CPU**（2 和 4），不能只绑一个；
   - `vm.mmap_min_addr` 记原值，测完恢复。
4. **可选：perf**。对 `agent-other` 和 `agent-exit` 各做一次，方法同 Jetson 的 `syscall/dispatch-fix-20261029/profile.sh`
   （20M 次 write，`perf stat` 加 `perf record -g`，`PERF` 环境变量指向 NUC 的 perf）。看点：
   - 每次 write 的周期数和指令数；
   - `malloc`/`free`、`std::function`、`__tls_get_addr` 或 TLS 初始化函数是否还在热点里。Jetson 上，挂了程序时剩下的是
     `curr_thread_override_return_callback` 的动态 TLS，约占 7%。

## 2. 结果放哪里

- 原始数据：`benchmark-results/x86-nuc` 的 `syscall/x86-nuc-dispatch-fix-20261029/`。
- 对比表：修复前（10/29 上午的 NUC 数据）、修复后、Jetson 修复后，各层既列 ns 也列周期。
- 结论写进 `summry/x86-nuc`。需要 Jetson 处理的问题（编不过、测试失败、计数不对）写进发现文件。
