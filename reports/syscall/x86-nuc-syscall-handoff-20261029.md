# 给 NUC 会话的交接：x86 上的系统调用追踪对照（2026-10-29）

写给 x86 NUC（`debian-ichi-nuc`，i5-1240P，Debian 13，内核 6.12）上的 Claude 会话。两边只通过 `plsy1/bpftime-benchmark`
交接，**回复用户用中文**。机器、构建、测量规矩见上一份交接 `summry/x86-nuc` 分支的
`x86-nuc/bpftime-x86-nuc-handoff-20261026.md`（第 2–3 节），这里只写本次任务。

## 0. 背景

bpftime 的系统调用追踪（`tracepoint/syscalls/*` 程序在用户态执行）靠改写进程里的系统调用指令。x86 一直有（zpoline：
`syscall` → `call *%rax`，跳到地址 0 的一页跳板）；ARM64 上游一直是 TODO。Jetson 会话 10/28–29 在开发分支实现了 ARM64 版
（提交 `7a7d29b8`，每处 `svc` 改成 `b` 到专属跳板，参考 svc-hook），设计见 `summry/jetson` 的
`syscall/arm64-syscall-trampoline-design-20261028.md`，代码约束见开发分支 `CLAUDE.md` 的 "aarch64 System Call Rewriting"。

Jetson 上的结论（`benchmark-results/jetson` 的 `syscall/`）：bpftime 拦截正确（100 万次 write 计数与 kernel 一致），
但每次系统调用的额外开销比 kernel 大：官方 `benchmark/syscall` kernel +85 ns、bpftime +287 ns。拆层测量显示改写和跳板只占
约 37 ns，**agent 的分发即使没有匹配的程序也要约 200 ns**（数字见第 3 节）。分发层是两种架构共用的 C++ 代码，
所以想在 x86 上看：(1) 分发层是不是同样贵；(2) zpoline 的跳板和 ARM64 的逐处跳板成本差多少；
(3) 顺便确认 `7a7d29b8` 在 x86 上能编译、zpoline 路径没被改坏（这个提交在 x86 部分只加了 `#if` 隔离，没在 x86 上编译过）。

## 1. 要做的事（按顺序）

1. **更新开发分支并构建**：`dev/uprobe-official-177a1dc465ff` 拉到 `7a7d29b8` 或更新，`git submodule update --init --recursive`，
   用 10/26 在 NUC 上的同样选项构建，目标加上 `bpftime-agent bpftime-syscall-server bpftime_text_segment_transformer
   bpftimetool bpftime_runtime_tests`。`bpftime_text_segment_transformer` 编不过就停下，把错误写进发现文件（见第 4 节）。
   跑一遍每日过滤器（`automation/jetson-daily` 的 `build.test_filter`）确认没有回归。
2. **zpoline 的前提**：它要映射地址 0，需要 root 和 `vm.mmap_min_addr=0`。先记下原值（`sysctl vm.mmap_min_addr`），
   测完恢复，写进结果。
3. **固定频率、绑核**：按上一份交接第 2 节（关 turbo、钉死一个 P 核的频率，进程绑在该核的一个线程上）。
   runner 的子进程继承亲和性，可以 `taskset -c <核> python3 run_layers.py ...`。
4. **正确性**（拆层脚本里的 `agent-count` / `kernel-count` 会检查计数等于 N，失败的行 `ok` 为 false）。
   bpftime 崩溃、卡住或计数不等于 N 都先停下记录。
5. **拆层测量**：取 `benchmark-results/jetson` 分支的 `syscall/breakdown-20261029/`（`run_layers.py`、`src/`）。
   `BPFTIME_BUILD=<你的构建目录> CC=gcc CLANG=clang-19 src/build.sh`（脚本按 `uname -m` 选 `__TARGET_ARCH_x86`），
   然后 `BPFTIME_BUILD=... python3 run_layers.py --rounds 5 --out run1.jsonl`。9 个 leg 的含义见脚本开头。
6. **官方 `benchmark/syscall`**：`make -C benchmark/syscall CLANG=clang-19`，用不改动的 `benchmark.py` 跑 10 轮。
   **坑**：它的清理会 `kill -9` 命令行里含 `victim`、`syscall`、`bpftime` 这几个词的所有进程，包括包着它的 `sudo`、`timeout`、
   后台 shell（日志文件名带 `syscall` 也会中招）。做法见 Jetson 的 `syscall/official-syscall-20261029/run_official.sh`：
   建一个软链接视图目录（`benchmark/` → 开发树的 `benchmark/`，`build/` → 构建目录），把脚本复制成 `runbench.py` 在视图根目录跑，
   `sudo env HOME=<空目录>`（让它走 LD_PRELOAD 方式而不是 `~/.bpftime/bpftime`），日志名不要含这些词。

## 2. 结果放哪里

- 原始数据和说明：`benchmark-results/x86-nuc` 分支 `syscall/`（例如 `syscall/breakdown-<日期>/`、`syscall/official-syscall-<日期>/`），
  记下 CPU、频率设置、内核、编译器和 LLVM 版本、`vm.mmap_min_addr` 原值。
- 结论：`summry/x86-nuc`。
- **需要 Jetson 会话处理的问题**（x86 编不过、zpoline 路径坏了、计数不对）：写进 `summry/x86-nuc` 的一个发现文件，
  NUC 上不改开发分支。
- 推送前问用户。

## 3. Jetson 的参考数字（Orin，6.8.12，GCC 13，LLVM 15，LTO，jetson_clocks）

每次 write(2)（7 字节写 `/dev/null`，`writeloop` -O2，N = 1,000,000），`run_layers.py`，5 轮中位数（`syscall/breakdown-20261029`）：

| Leg | 含义 | ns/write | 相对上一层 |
|---|---|---:|---:|
| none | 不追踪 | 352.6 | – |
| rewrite | 改写 + 跳板 + 保存寄存器，钩子直接发起系统调用 | 390.1 | +37.5 |
| passthrough | 再加一次跨 dlmopen 命名空间的函数指针调用 | 390.5 | +0.4 |
| agent-other | 真 agent，只有一个 getppid 程序（write 没有程序） | 592.6 | **+202.1** |
| agent-exit | 一个空的 sys_exit_write 程序 | 634.8 | +42.2 |
| agent-count | sys_enter/sys_exit_write 两个计数程序 | 681.8 | +47.0 |
| kernel-other / exit / count | 同样三个对象挂内核 tracepoint | 367.4 / 436.7 / 523.6 | +14.8 / +69.3 / +86.9 |

官方 `benchmark/syscall`（10 轮）：不追踪 353.45 ns，kernel 438.22 ns，bpftime 640.00 ns。
