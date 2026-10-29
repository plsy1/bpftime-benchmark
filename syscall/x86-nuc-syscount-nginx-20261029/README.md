# x86 上的 syscount-nginx（2026-10-29 晚，NUC）

按 `summry/jetson` 的 `syscall/x86-nuc-syscount-nginx-handoff-20261029.md` 做，开发分支 `59e5e154`（"text segment
transformer: keep the agent's pthread keys apart from the process's"，在 `2e850c45` 之上）。Jetson 的结果和脚本在
`benchmark-results/jetson` 的 `syscall/syscount-nginx-20261029/`。

**结果**：
- pthread key 冲突在 x86 上同样存在，`59e5e154` 修好了它。
- 但挂着程序时，nginx 在 x86 上启动仍然会崩，原因是第二个 bug：x86 的 zpoline 跳板没有保住系统调用必须保持不变的寄存器（第 3 节）。官方 177a1dc 也有这个问题。
- 因此 x86 上无法按正确方式测量，`run_correct.py` 没有运行；作为对照，跑了不改动的官方脚本（第 4 节）。

## 文件

| 文件 | 内容 |
|---|---|
| `build/` | `run-build.sh`（选项同 `build-2e850c45`，加 `bpftime_text_segment_transformer_tests`）、日志、测试；`official-177a1dc-transformer-build.log`（在官方 177a1dc 的 `build-jit` 里补建 transformer 目标） |
| `audit.py`、`run_correct.py`、`summarize_correct.py`、`repro-crash.sh`、`profile_nginx.sh`、`profile_leg.py`、`src/keycheck.c` | 从 Jetson 原样复制，来源提交见 `copied-from-jetson-commit.txt`，`copied-sha256.txt` 核对一致；`jetson-run_official.sh.txt` 仅作参考 |
| `keycheck.sh`、`keycheck.log` | 第 2 节：修复前后的 keycheck |
| `docker/` | `Dockerfile`（基于 debian:trixie：nginx 1.26.3、wrk 4.1.0、python3 3.13、glibc 2.41，与主机相同，见 `versions.txt`）、`start-container.sh`、`build.log` |
| `view/`、`view-2e850c45/`、`view-sha256.txt` | 脚本的工作目录，布局同 Jetson：`benchmark/syscount-nginx/`；`build` 指向开发构建；`example/libbpf-tools/syscount/syscount` 是 `example/tracing/syscount` 不改源码、用 `make CLANG=clang-19` 编出来的（`syscount-make.log`）。`view-2e850c45` 用的是修复前的构建 |
| `audit-*.jsonl`、`audit-*.err`、`audit-mmap_min_addr.txt` | 第 3 节的审计。`audit-attempt1-no-tracefs/` 是第一次尝试：容器里没挂 tracefs，syscount 挂不上 |
| `repro-crash-59e5e154.log`、`repro-crash-regs.sh`、`repro-crash-regs-59e5e154.log` | 第 3 节 gdb 回溯（Jetson 的脚本；`-regs` 那份多打印 r8、rdx 和出错指令） |
| `src/regcheck.c`、`regcheck.sh`、`regcheck.log` | 第 3 节：寄存器经过改写后的 `syscall` 是否保持不变。`regcheck-gpr-only.log` 是只查通用寄存器的第一版 |
| `session/official.sh`，以及 `pin-cpus.sh`、`busy-check.py`、`freq-log.py`、`machine.txt` | 第 4 节的驱动脚本和钉频 |
| `official/` | 第 4 节官方 benchmark.py：`run.log`、`benchmark_results_*.json`、`reports.md`、`benchmark_chart.png`、`summary.md`、钉频记录、`restore-fix.txt` |

崩溃产生的 core 文件已移到 NUC 本机 `/home/y/src/bpftime-x86-runs/syscount-nginx-cores/`，没有提交。

## 1. 构建与测试

- `build-59e5e154`：退出码 0，警告和 `build-2e850c45` 的 10 条相同。
- 每日过滤器：113/113。
- `bpftime_syscall_trace_attach_tests`（root）：8/8。
- `bpftime_text_segment_transformer_tests`：**2/2**，即两个 pthread key 用例；x86 上没有改写指令的用例。

## 2. x86 上的 pthread key 冲突（`keycheck.log`）

在主机上以 root 跑，`vm.mmap_min_addr` 65536 → 0 → 65536，先启动带 syscall server 的 `syscount -d 6`：

| 构建 | 不追踪 | 跳板，不挂程序 | 跳板，挂 syscount 程序 |
|---|---|---|---|
| `2e850c45`（修复前） | key 0..7，0 个坏 | key 0..7，0 个坏 | key 0..7，**4 个新 key 读到 agent 的指针（0、1、2、4）** |
| `59e5e154`（修复后） | key 0..7，0 个坏 | key 32..39，0 个坏 | **key 32..39，0 个坏** |

和 Jetson 完全一致。两种构建下 syscount 都数到了 1000 次 getppid。

## 3. 审计，以及 x86 的寄存器 bug

### 审计（`audit.py` 在容器里跑，每种方式 5 秒）

- 容器 `ngx-x86` 由 `docker/start-container.sh` 启动：`--privileged --pid=host --shm-size=1g --cpuset-cpus 0-7`，开发树只读挂载，并挂上主机的 tracefs/debugfs。
- 只查正确性，所以没有钉频。
- `vm.mmap_min_addr` 审计期间设为 0，之后恢复为 65536。

| 方式（`59e5e154`） | 5 秒内的请求数 | 数到的系统调用 |
|---|---:|---:|
| 内核 syscount `-p <nginx>` | 474,082 | 4,273,799（**每个请求 9.015 次**：setsockopt 2 次；close、recvfrom、sendfile、write、fstat、openat、writev 各 1 次） |
| 官方方式（预加载 agent，之后再启动 syscount） | 579,043 | **0** |
| 跳板，但 nginx 先启动 | 490,905 | **0** |
| 先加载程序，再用跳板启动 nginx | – | **nginx 启动即退出** |

- **`2e850c45` 的最后一种方式**（`audit-2e850c45.err`）：先报 `[alert] SSL_get_ex_new_index() failed`，再报 `munmap_chunk(): invalid pointer`。这是 pthread key 冲突，也正是用户 7 月在 x86 上记下的报错。
- **`59e5e154` 的最后一种方式**：nginx 在打印 "Rewriting executable segments" 之后无声退出。gdb 结果（`repro-crash-regs-59e5e154.log`）：
  - 在 `__libc_sigaction`（由 `ngx_init_signals` 调用）里收到 SIGSEGV，出错指令是 `movups %xmm0,0x8(%r8)`，此时 **r8 = 0x28**。
  - glibc 把 `oact` 指针放在 r8 里，跨过 `rt_sigaction` 的 `syscall` 指令继续使用。内核允许这样做：一次系统调用只会改 rax、rcx、r11。

### 原因：x86 跳板没有恢复调用者保存的寄存器（`regcheck.log`）

`text_segment_transformer.cpp` 里 x86 的 `syscall_hooker_asm`：
- 把 rax、rdi、rsi、rdx、r10、r8、r9 压栈，只是为了把它们当作 C 函数的参数传进去；
- 调用 `syscall_hooker_cxx`；
- 用 `leave; ret` 返回。

所以返回后 rdi、rsi、rdx、r10、r8、r9 和 xmm0–15 里是 agent 的 C++ 代码留下的值。

`src/regcheck.c` 的做法：把这些寄存器设成已知值，用裸 `syscall` 发起 `getppid`，重复 1000 次，比较前后是否一致。

| 构建 | 跳板，不挂程序 | 跳板，挂 syscount 程序 |
|---|---|---|
| 不追踪 | 0 个改变 | – |
| 官方 177a1dc | 0 个改变 | **rdi、rsi、rdx、r10、r8、r9 和 xmm0–4、xmm6 改变；之后段错误（退出码 139）** |
| 开发 `7a7d29b8`（分发修复前，`regcheck-7a7d29b8-noprog.log`） | 0 个改变 | 未测 |
| 开发 `2e850c45` | **rdx 改变** | **rdi、rsi、rdx、r10、r8（= 0x28）、r9 和 xmm0、5、7 改变** |
| 开发 `59e5e154` | **rdx 改变** | 同 `2e850c45` |

- 被调用者保存的寄存器（rbx、rbp、r12–r15）在所有情况下都完好。
- 这是上游 bug：官方 177a1dc 的跳板代码相同。
- 开发分支 `2e850c45` 的直通分发让不挂程序时也会改掉 rdx（`7a7d29b8` 不挂程序时 0 个改变）。nginx 只加载跳板（"跳板，但 nginx 先启动"）时碰巧没出问题。
- ARM64 不受影响：它的跳板保存并恢复 x0–x30 和 q0–q31。

## 4. 不改动的官方 benchmark.py（`official/`）

**怎么跑的**：
- `view/runbench.py`（即 `benchmark.py`，sha256 见 `view-sha256.txt`）在容器里运行：`sudo docker exec ngx-x86 python3 runbench.py`。
- 每种配置 10 次，wrk `-c 100 -d 10`，顺序按脚本本身。
- CPU 0–7（4 个 P 核的全部线程，即容器的 cpuset）钉在 1.7 GHz、关 turbo（`session/official.sh`）：
  - 前后的忙检查都 PASS（1709–1738 MHz）；
  - 记录到的忙时频率 1642–1710 MHz，中位 1710。
- 开始前没有残留进程占 CPU（`ps-before.txt`）。时间 17:51–18:08。

| 配置 | 每秒请求数 | 标准差 | 相对不追踪 |
|---|---:|---:|---:|
| 不追踪 | 53,884 | 171 | – |
| 内核 syscount，追踪 nginx | 51,583 | 198 | −4.27% |
| 内核 syscount，追踪别的进程（`-p 1`） | 52,312 | 216 | −2.92% |
| bpftime syscount，追踪 nginx（**什么都没追踪**，见第 3 节） | 53,306 | 299 | −1.07% |
| bpftime syscount，追踪别的进程（什么都没追踪） | 53,037 | 254 | −1.57% |

和 Jetson 以及用户 7 月的 x86 结果（内核 −6.5% / −4.8%，bpftime −1.3% / −0.7%）一样，官方脚本让 bpftime 看起来几乎没有开销，原因是它的 bpftime 两组什么都没追踪。

**恢复出错**（`restore-fix.txt`）：
- 原因：`pin-cpus.sh`（来自 ssl-nginx 扫测）对每个列出的 CPU 都会记录一遍它所在核的所有线程。SMT 兄弟两个都钉时，状态文件里每项都出现两次，恢复时写入两行值而中止。
- 处理：已按 `pin-state.txt` 手动恢复并核对。
- 之后修好了 `session/pin-cpus.sh`（改为 `get ... | head -n 1`），修正后的版本没有再跑过。

## 没做的

- **`run_correct.py`（6 组 × 10 轮）和可选的 perf**：bpftime 的几组需要先加载程序、再启动 nginx，而在跳板保住寄存器之前，这在 x86 上一定会崩。等修复后再跑。
- **收尾**：容器已删除，镜像 `x86-nginx-wrk:20261029` 保留；Docker 已重新停掉（开始前它就是停止且不自启的）。

## 后续：x86 跳板寄存器修复（`regs-fix/`）

开发分支 `0a602133` 修了第 3 节的寄存器问题，但在 x86 上用同样的选项（LTO）链接失败，详见 `regs-fix/README.md`。正确跑法的测量仍然没做。
