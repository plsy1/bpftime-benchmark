# 官方 benchmark/syscall：x86 对照（2026-10-29，NUC）

开发分支 `7a7d29b8` 的 `benchmark/syscall`，程序和脚本不改：`syscall.bpf.c` 在 `sys_exit_read`、`sys_exit_write`
上挂空程序（每次 write 跑一个），`victim`（Makefile 的 `-g`，不优化）做 10 × 100,000 次 5 字节 `write(2)` 到 `/dev/null`；
`benchmark.py` 每种配置跑 10 次。构建：`make -C benchmark/syscall CLANG=clang-19`（`make.log`），bpftime 用
`build-7a7d29b8`（gcc-13、LLVM 19.1.7、LTO）。机器、内核、`vm.mmap_min_addr`（原值 65536，测量时 0，测完恢复）
和频率设置见 `../x86-nuc-breakdown-20261029/README.md` 和 `../x86-nuc-syscall-session-20261029`。

运行方式与 Jetson 的 `run_official.sh` 相同：在一个只含软链接的视图目录里跑（`benchmark/` → 开发树的 `benchmark/`，
`build/` → `build-7a7d29b8`），脚本复制成 `runbench.py`，`sudo env HOME=<空目录>`（走 LD_PRELOAD 方式）。
脚本的清理会 `kill -9` 命令行里带 `victim`、`syscall`、`bpftime` 的所有进程，所以驱动脚本放在不含这些词的路径下运行，
命令行里也不带输出目录。第一次运行时，我自己的一个监视进程（命令行带结果路径）被它杀掉了（`4.4ghz-turbo-onecpu/run.log` 第 5 行），
测量本身没受影响。

## 结果

| 配置 | 不追踪 | kernel tracepoint | bpftime | kernel 增量 | bpftime 增量 | 倍数 |
|---|---:|---:|---:|---:|---:|---:|
| Jetson（1.728 GHz，均值） | 353.45 | 438.22 | 640.00 | +84.8 | +286.6 | 3.4× |
| x86 1.7 GHz（`1.7ghz-noturbo/`，均值） | 235.74 | 293.57 | 459.95 | +57.8 | +224.2 | 3.9× |
| x86 1.7 GHz（中位） | 234.99 | 289.15 | 459.94 | +54.2 | +225.0 | 4.2× |
| x86 4.4 GHz（`4.4ghz-turbo/`，均值） | 91.60 | 112.52 | 178.99 | +20.9 | +87.4 | 4.2× |

（单位 ns/write。1.7 GHz 的 kernel 第 1 次是 334.5，其余 9 次都在 288–290，所以同时给出中位数。）

- 和拆层测量一致：x86 4.4 GHz 拆层的 kernel-exit 是 +22.0、agent-exit 是 +86.6；1.7 GHz 是 +52.0 / +225.7。
- 官方仓库的 x86 参考值（`benchmark/syscall/example_results.md`，内核 6.11）是 kernel +34.5、bpftime +80.2，和这里 4.4 GHz 的数量级相同。

## 无效的第一次：`4.4ghz-turbo-onecpu/`

第一次把整个脚本用 `taskset -c 2` 绑在一个 CPU 上，测出 kernel 225.3、bpftime 358.4（不追踪 92.1）。
原因：加载器 `syscall.c` 在整个 leg 期间忙等（`while (!exiting);`），绑在同一个 CPU 上就和 victim 平分 CPU，
victim 测到的墙钟时间翻倍（225 ≈ 2 × (91 + 21)，358 ≈ 2 × 179；不追踪那一段没有加载器，所以正常）。之后两组让脚本跑在
CPU 2 和 4 两个 P 核上（两个都钉同一频率），加载器和 victim 各占一个核，结果就和拆层一致了。保留作记录，不用于对比。
Jetson 的那次没有用 taskset，所以不受影响；以后谁要给这个 benchmark 绑核，至少要给两个 CPU。

每个目录：`run.log`（脚本输出）、`syscall_benchmark_results_*.json` 和 `results.md`（脚本写在 `benchmark/syscall/` 下的结果，
跑完移过来）、`freq-before.txt` / `freq-after.txt`（前后的忙检查）。
