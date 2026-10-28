# bpftime 每次系统调用的开销拆层：x86 对照（2026-10-29，NUC）

和 Jetson 的 `benchmark-results/jetson` `syscall/breakdown-20261029` 同一套测量，在 x86 上跑。
脚本和源码原样复制（`run_layers.py`、`summarize.py`、`src/*.c`、`src/build.sh`，来源提交见
`copied-from-jetson-commit.txt`，`copied-sha256.txt` 核对一致）。`writeloop`（-O2）对 fd 42（`/dev/null`）做 N = 1,000,000 次
7 字节 `write(2)`，在进程内计时。9 个 leg 的含义见 `run_layers.py` 开头。`compare.py` 生成下面的对比表（`compare.md`）。

## 环境

- i5-1240P，Debian 13，内核 6.12.113+deb13-amd64，开发分支 `7a7d29b8`，构建 `build-7a7d29b8`
  （gcc-13、LLVM 19.1.7、LTO，见 `../x86-nuc-build-20261029`）。`src/` 用
  `BPFTIME_BUILD=<build-7a7d29b8> CC=gcc CLANG=clang-19 src/build.sh` 构建（gcc 14.2，`__TARGET_ARCH_x86`，`build-src.log`）。
- x86 的系统调用拦截是 zpoline：每个 `syscall` 改成 `call *%rax`，跳到地址 0 那一页（需要 root 和 `vm.mmap_min_addr=0`）。
  原值 65536，测量期间设成 0，测完恢复为 65536（`../x86-nuc-syscall-session-20261029/<配置>/mmap_min_addr.txt`）。
- 绑核与频率：进程绑在 CPU 2（P 核，SMT 兄弟 3 空闲），min = max、powersave、C6 以上关闭，每段前后用 APERF/MPERF 做忙检查，
  并每秒记录实际频率。驱动脚本和频率记录见 `../x86-nuc-syscall-session-20261029`。三组：

| 目录 | 设置 | 测得频率（中位） |
|---|---|---|
| `1.7ghz-noturbo/` | 关 turbo，1.7 GHz（与 Jetson cpu5 的 1.728 GHz 几乎同频） | 1709 MHz |
| `4.4ghz-turbo/` | turbo 开，钉 4.4 GHz（与 10/26 的 x86 uprobe 数据同设置），CPU 2 和 4 同时钉 | 4359 MHz |
| `4.4ghz-turbo-onecpu/` | 同上，只钉 CPU 2（第一次；拆层部分有效，官方部分无效，见官方目录） | 4401 MHz |

每组 5 轮，leg 顺序轮换，**3 × 45 次全部有效**；计数 leg 的计数都等于 N（`agent-count` 的进入/退出各 1,000,000）。
zpoline 路径没被 `7a7d29b8` 改坏。（x86 的 transformer 不打印 "Rewrote ..."，所以 `rewrote` 字段为 null。）

## 结果

每个单元格是中位 ns/write，括号里是相对下一层的增量和换算成的周期数（按测得频率）：

| Leg | Jetson 1.73 GHz | x86 1.7 GHz | x86 4.4 GHz | x86 4.4 GHz（第一次） |
|---|---:|---:|---:|---:|
| none | 352.6 | 234.0 | 91.7 | 91.0 |
| rewrite | 390.1 (+37.5 ns, +65 cyc) | 299.8 (+65.8 ns, +112 cyc) | 116.2 (+24.5 ns, +107 cyc) | 115.9 (+24.9 ns, +110 cyc) |
| passthrough | 390.5 (+0.4 ns, +1 cyc) | 300.6 (+0.8 ns, +1 cyc) | 116.2 (+0.0 ns, +0 cyc) | 116.1 (+0.2 ns, +1 cyc) |
| agent-other | 592.6 (**+202.1 ns, +349 cyc**) | 430.2 (**+129.6 ns, +221 cyc**) | 167.2 (**+51.0 ns, +222 cyc**) | 166.3 (+50.2 ns, +221 cyc) |
| agent-exit | 634.8 (+42.2 ns, +73 cyc) | 459.7 (+29.5 ns, +50 cyc) | 178.3 (+11.1 ns, +48 cyc) | 177.0 (+10.7 ns, +47 cyc) |
| agent-count | 681.8 (+47.0 ns, +81 cyc) | 490.1 (+30.4 ns, +52 cyc) | 190.3 (+12.0 ns, +52 cyc) | 190.2 (+13.2 ns, +58 cyc) |
| kernel-other | 367.4 (+14.8 ns, +26 cyc) | 270.7 (+36.7 ns, +63 cyc) | 106.6 (+14.9 ns, +65 cyc) | 105.9 (+14.9 ns, +66 cyc) |
| kernel-exit | 436.7 (+69.3 ns, +120 cyc) | 286.0 (+15.3 ns, +26 cyc) | 113.7 (+7.1 ns, +31 cyc) | 111.4 (+5.5 ns, +24 cyc) |
| kernel-count | 523.6 (+86.9 ns, +150 cyc) | 429.1 (+143.1 ns, +245 cyc) | 166.9 (+53.2 ns, +232 cyc) | 166.5 (+55.1 ns, +242 cyc) |

- x86 上 bpftime 各层的周期数在 1.7 GHz 和 4.4 GHz 下一致（例如分发 221/222 周期），所以这些都是 CPU 内的开销，可以按周期跟 Jetson 比。
- **agent 分发（没有匹配程序）**：x86 约 220 周期（1.7 GHz 下 130 ns，4.4 GHz 下 51 ns），Jetson 349 周期（202 ns）。
  在 x86 上它同样是 bpftime 最大的一层（1.7 GHz 下占 agent-exit 总开销 225.7 ns 的 57%，Jetson 是 72%），
  但同频下比 ARM64 少约 130 周期。分发代码两边相同，差出来的部分和架构有关，值得在两边各做一次 perf 对比。
- **跳板**：zpoline（改写 + 跳板 + 保存 7 个参数寄存器 + C 入口）约 110 周期（1.7 GHz 下 66 ns，4.4 GHz 下 25 ns），
  ARM64 逐处跳板（保存 x0–x30、标志、FPSR、q0–q31）65 周期（37.5 ns）。也就是 x86 保存的寄存器少得多，跳板反而多约 45 周期。
  推测（未单独测）：zpoline 的 `call *%rax` 跳到地址 = 系统调用号处，然后顺着 nop 滑到第 512 字节的跳转；write 的调用号是 1，
  每次要执行 511 个 nop，再加两次间接跳转。照这个解释，调用号越小越贵。
- 跨命名空间的函数指针调用两边都约等于 0。
- 每个程序：bpftime 约 50 周期（Jetson 73–81 周期）；x86 内核这边一个空程序只要 24–31 周期，计数程序要 230–245 周期。
- 不追踪时一次 write：x86 1.7 GHz 234 ns，Jetson 353 ns。

`*/run.jsonl` 是每次运行的原始记录，`*/run.log` 是控制台输出，`*/summary.md` 是 `summarize.py` 的输出，
`*/freq-before.txt` / `freq-after.txt` 是该段前后的忙检查。
