# 系统调用分发修复（2e850c45）在 x86 上的重测（2026-10-29 下午，NUC）

按 `summry/jetson` 的 `syscall/x86-nuc-dispatch-fix-handoff-20261029.md` 做。修复前的数据是本分支 10/29 上午的
`../x86-nuc-{build,breakdown,official-syscall,syscall-session}-20261029/`（开发分支 `7a7d29b8`）；这里是 `2e850c45`
（"syscall trace: pass system calls without programs straight through"，在 `7a7d29b8` 之上一个提交），方法和脚本不变，只换构建目录。

| 目录 | 内容 |
|---|---|
| `build/` | 构建（`run-build.sh`，同上午的选项，加 `bpftime_syscall_trace_attach_tests`，构建目录 `build-2e850c45`）和单测 |
| `session/` | `measure.sh`（上午那份复制过来，只改了 `BUILD` 和三个输出目录）、`freq.sh`、`busy-check.py`、`freq-log.py`，每组的频率与 `vm.mmap_min_addr` 记录 |
| `breakdown/<配置>/` | 拆层测量（上午 `../x86-nuc-breakdown-20261029` 的 `run_layers.py` 和 `src/`，原样） |
| `official/<配置>/` | 官方 `benchmark/syscall`，10 轮，CPU 2 和 4 |
| `perf/` | 可选的 perf：`profile.sh`（Jetson 的复制，只改路径）和 `perf-session.sh`（修复前后各一次），`page0.py` 统计落在 nop 滑道的样本 |
| `compare.py` / `compare.md` | 修复前后、Jetson 前后的拆层对比 |

## 正确性

- 构建：退出码 0，10 条警告和上午完全相同（`build/build.log`）。
- 每日过滤器：113/113（`build/tests-filter.log`）。
- `bpftime_syscall_trace_attach_tests` 以 root 运行：**8/8 通过**（`build/tests-syscall-trace.log`，用例名见
  `tests-syscall-trace-names.txt`），包括新加的 4 个（没挂程序时零分配并直通、有程序时零分配、越界调用号只跑全局程序、改写返回值）。
  Jetson 上因 arm64 没有 `fork` 而失败的两个旧用例（查 `fork` 的 `Test syscall dispatch - multiple` 和 `Test resolving private data`）在 x86 上通过。
- 拆层：两组各 45 次全部有效，计数 leg 的进入/退出都是 1,000,000 / 1,000,000。

## 设置

和上午相同：i5-1240P，内核 6.12.113，gcc-13 / LLVM 19.1.7 / LTO。
- `1.7ghz-noturbo`：关 turbo，实测中位 1709 MHz。
- `4.4ghz-turbo`：开 turbo，CPU 2 和 4 同时钉，实测中位 4346 MHz（4230–4390）。
- 拆层绑 CPU 2，官方脚本 `taskset -c 2,4`。
- `vm.mmap_min_addr`：65536 → 0 → 65536，两组和 perf 都已恢复。
- 两组退出时的频率设置（包括 CPU 4 的上限，上午 1.7 GHz 那组出过问题）都已核对，恢复到会话前的状态。

## 拆层：修复前后（中位 ns/write；括号里是相对下一层的增量和周期数）

1.7 GHz（x86）与 Jetson（1.728 GHz）：

| Leg | x86 修复前 | x86 修复后 | Jetson 修复前 | Jetson 修复后 |
|---|---:|---:|---:|---:|
| none | 234.0 | 234.3 | 352.6 | 352.4 |
| rewrite | +65.8 ns, 112 cyc | +64.5 ns, 110 cyc | +37.5 ns, 65 cyc | +37.5 ns, 65 cyc |
| passthrough | +0.8 ns | −0.1 ns | +0.4 ns | +1.1 ns |
| agent-other（分发，无匹配程序） | **+129.6 ns, 221 cyc** | **+24.2 ns, 41 cyc** | **+202.1 ns, 349 cyc** | **+27.8 ns, 48 cyc** |
| agent-exit（一个空程序） | +29.5 ns, 50 cyc | +65.3 ns, 112 cyc | +42.2 ns, 73 cyc | +79.7 ns, 138 cyc |
| agent-count（两个计数程序） | +30.4 ns, 52 cyc | +70.3 ns, 120 cyc | +47.0 ns, 81 cyc | +101.5 ns, 175 cyc |
| kernel-other / exit / count | 270.7 / 286.0 / 429.1 | 271.0 / 285.9 / 428.2 | 367.4 / 436.7 / 523.6 | 367.8 / 438.1 / 524.3 |
| **bpftime 一个空程序（agent-exit − none）** | **+225.7 ns, 386 cyc** | **+153.9 ns, 263 cyc** | +282.2 ns, 488 cyc | +146.1 ns, 252 cyc |

4.4 GHz（x86）：分发 +51.0 ns（222 cyc）→ **+8.9 ns（39 cyc）**；agent-exit − none +86.6 ns（377 cyc）→ **+59.8 ns（260 cyc）**；
跳板 107 → 109 cyc，kernel 各 leg 不变。完整表见 `compare.md`。

- 分发（无匹配程序）在 x86 上从约 220 周期降到约 40 周期，降了 82%。修复后和 Jetson 的 48 周期几乎一样：上午看到的"ARM64 比 x86 多约 130 周期"，基本就在这次删掉的那部分里（两次堆分配和动态 TLS）。
- agent-exit 和 agent-count 的增量变大（50 → 112 周期），原因和 Jetson 相同：设置 override 回调（TLS）现在只在有程序时才做，这部分成本从 agent-other 那一步挪到了这里。总数仍然下降：挂一个空程序时 386 → 263 周期，降了 32%。
- 修复后挂一个空程序的总开销，x86 是 263 周期，Jetson 是 252 周期，基本持平。x86 的跳板贵约 45 周期，分发和程序部分便宜一些。

## 官方 benchmark/syscall（10 轮，均值 ns/write）

| | x86 1.7 GHz 修复前 | x86 1.7 GHz 修复后 | x86 4.4 GHz 修复前 | x86 4.4 GHz 修复后 | Jetson 修复前 | Jetson 修复后 |
|---|---:|---:|---:|---:|---:|---:|
| 不追踪 | 235.74 | 234.70 | 91.59 | 91.08 | 353.45 | 353.14 |
| kernel tracepoint | 293.57（+57.8） | 288.47（+53.8） | 112.52（+20.9） | 112.05（+21.0） | 438.22（+84.8） | 437.76（+84.6） |
| bpftime | 459.95（+224.2） | **387.36（+152.7）** | 179.00（+87.4） | **151.98（+60.9）** | 640.00（+286.6） | 515.51（+162.4） |
| bpftime 开销 / kernel 开销 | 3.9× | **2.8×** | 4.2× | **2.9×** | 3.4× | 1.9× |

- 上午 1.7 GHz 的 kernel 第 1 次是离群值，改用中位数时修复前是 4.2×，修复后中位数仍是 2.8×。
- x86 修复后的倍数（2.8–2.9×）比 Jetson（1.9×）高，原因不在 bpftime：两边 bpftime 的增量差不多（同频下 +153 对 +162 ns），而 x86 内核挂一个空程序便宜得多（+54 对 +85 ns）。
- 官方仓库公布的 x86 结果（2025 年，内核 6.11）是 2.3×。

## perf（`perf/`，CPU 2 钉 1.7 GHz 关 turbo，2000 万次 write，`perf stat` + `perf record -g`）

| Leg | cycles / write | instructions / write |
|---|---:|---:|
| rewrite（修复后，transformer 没变） | 563 | 1367 |
| agent-other 修复前 | 794 | 2106 |
| agent-other 修复后 | **608** | **1481** |
| agent-exit 修复前 | 837 | 2265 |
| agent-exit 修复后 | 721 | 1806 |

（只有 `cpu_core` 计数器有值，进程一直在 P 核上；数字含内核态和进程启动。perf 运行时 ns/write 比拆层高约 10%。）

- **agent-other**：
  - 修复后，分发比跳板多 45 周期、114 条指令。Jetson 是 51 周期、138 条指令。
  - 修复前，热点里有 `cfree` 4.1%、`malloc` 1.5%、`__tls_get_addr` 3.1% + PLT 1.0%、TLS init 函数 2.3%，以及 `std::function` 的若干项。修复后这些全部消失，只剩 `syscall_callback` 4.6%。
- **agent-exit**（修复后）：剩下的是 override 回调的动态 TLS，约 7%：
  - `__tls_get_addr` 4.1%
  - TLS init 函数（`curr_thread_override_return_callback`）2.0%
  - `__tls_get_addr@plt` 1.0%
  - 对比：`bpftime_prog_exec` 2.6%。Jetson 上这部分也约占 7%。
- **zpoline 的 nop 滑道**（`page0.py`，结果在 `page0.txt`）：
  - rewrite leg 有 23.3% 的样本落在 nop 滑道（地址 1–511：2798 个样本），0x200 处的跳转只有 0.2%。
  - 换算成每次 write 约 130 个周期。其余几个 leg 也都在 125–132 周期之间（16.6% × 794、21.6% × 608、17.3% × 721）。
  - 这证实了上午的推测：zpoline 跳板约 110 周期的增量，主要花在 write（调用号 1）要走的 511 个 nop 上。
  - `perf.data` 没有提交，留在 NUC 的 `/home/y/src/bpftime-x86-runs/dispatch-fix-perf/`。

（`*.report.txt` 是以普通用户身份生成的，内核符号显示为地址。）
