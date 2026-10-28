# 系统调用测量的驱动脚本与会话记录（2026-10-29，NUC）

`measure.sh <khz> <turbo-on|turbo-off> <配置名>` 跑一个"会话"：钉频、`vm.mmap_min_addr=0`、拆层测量
（`../x86-nuc-breakdown-20261029`）、官方 benchmark/syscall（`../x86-nuc-official-syscall-20261029`），退出时恢复。

- 钉频：`freq.sh`（同 `../../uprobe/x86-nuc-full10-20261026/freq.sh`）把 CPU 2 和 4（P 核；SMT 兄弟 3、5 空闲）
  设成 min = max、powersave，并关掉两个核所有线程的 C6 以上。钉好后用 `busy-check.py`（APERF/MPERF，同
  `../../uprobe/x86-nuc-ssl-nginx-sweep-20261026/busy-check.py`）检查到通过为止，每段测量前后再查一次；
  `freq-log.py`（同 ssl-nginx-sweep 那份）在 E 核 15 上每秒记录 CPU 2、4 的实际频率（`freq-log.txt`，`freq-log-marks.txt` 标出各段起点）。
- 拆层只用 CPU 2（`taskset -c 2`）；官方脚本用 CPU 2 和 4（`taskset -c 2,4`），原因见官方目录的 README。
- `vm.mmap_min_addr`：原值 65536，测量时 0，退出时恢复（`mmap_min_addr.txt`，三组都恢复成 65536）。
- 运行前要把这几个文件复制到一个路径里不含 `victim`/`syscall`/`bpftime` 的目录再运行（官方脚本的清理会杀掉命令行带这些词的进程）。
  这次用的是 Claude 的临时目录。

每个配置目录：`env.txt`（CPU、内核、提交、编译器、LLVM、漏洞缓解）、`top-before.txt`（开始前确认没有其他 CPU 密集任务）、
`freq-before*.txt`（钉频前的状态）、`freq-pinned.txt`（钉频后状态和忙检查）、`freq-restored.txt`、`exit-status.txt`（三组都是 0）。

| 配置 | 说明 |
|---|---|
| `4.4ghz-turbo-onecpu/` | 第一次。用的是这个脚本的早期版本：只钉 CPU 2，官方脚本也只绑 CPU 2（导致官方结果无效，拆层有效）。 |
| `4.4ghz-turbo/` | 当前脚本，turbo 开，4.4 GHz。两核同时钉时实测 4.17–4.40 GHz，中位 4359 MHz（忙检查容差 3% 内）。 |
| `1.7ghz-noturbo/` | 当前脚本，关 turbo，1.7 GHz，实测中位 1709 MHz。 |

`1.7ghz-noturbo/restore-fix.txt`：这一组退出时 CPU 4 的 `scaling_max_freq` 被恢复成了 1700000。原因是 CPU 2 先以关 turbo 方式钉频，
内核随即把所有 CPU 的上限压到基频，之后才记录 CPU 4 的"原状态"。已手动改回 4400000（会话前的值），其余设置核对过都已恢复。
`measure.sh` 随后加了修正（钉频前先记下 CPU 4 的状态 `freq-orig-cpu4.txt`，恢复时以它为准），修正后的版本没有再跑过。
