# x86 NUC：uprobe 完整十轮（4.4 GHz，turbo 开、钉死），开发分支 vs 官方 177a1dc vs kernel（2026-10-26）

官方 `benchmark/uprobe/benchmark.py --iter 10`（kernel、userspace、embedded 三组）在 x86 NUC 上的第一次完整十轮。
同一会话里先跑开发分支 `a7d54d7e`、再跑官方 `177a1dc`，每段都带自己的 kernel 组，倍率只用同段 kernel。
结果表：`full10-4.4ghz-turbo/comparison.md`（`summarize.py` 生成）。

## 条件

- 机器：`debian-ichi-nuc`，i5-1240P，Debian 13，内核 `6.12.113+deb13-amd64`（`machine.txt`）。
- 构建：`../x86-nuc-build-regression-20261026`（gcc-13、LLVM/clang 19.1.7、Jetson 基线 CMake 选项）。运行时产物冻结在本机
  `/home/y/src/bpftime-x86-runs/full10-20261026/artifacts/{dev,official}`（819 MB，不提交），哈希见下。
- 工作负载：两段相同，都来自开发分支 `benchmark/`（含 hash delete 语义修正）：`benchmark/test`（`make -C benchmark test`，gcc 14.2）、
  `benchmark/uprobe/uprobe`（clang-19 / gcc-13，CO-RE，仓库自带 x86 `vmlinux.h`）。只换 agent、syscall-server 和 embed 基准。
- **频率（用户 10/26 选定）**：CPU 2（P 核，SMT 兄弟 CPU 3 空着）turbo 开、`scaling_min_freq = scaling_max_freq = 4.4 GHz`、
  governor `powersave`，CPU 2/3 禁用 C6/C8/C10（`freq.sh`）。整段测量期间在 E 核 CPU 15 上每秒读 CPU 2 的 APERF/MPERF（`freq-log.py`）：
  **1,540 个忙碌采样全部在 4,417–4,424 MHz**；每段前后的忙碌 + 空闲后短突发检查都通过（`*/freq-*.txt`）。测完恢复原设置（`freq-restored.txt`）。
  测量期间机器空闲（`top-before.txt`，99.5% idle），没有其他测量或编译。
- 时间：开发分支 15:42–15:52，官方 15:52–16:08。

## 频率固定的坑（这台机器）

前三次尝试被频率检查拦下（`aborted-1.4ghz/`），第四次因脚本 `top | head` 的 SIGPIPE 退出（`aborted-sigpipe/`），都没有测量数据。
- 核心从 C6 及更深的空闲状态醒来先跑 1.4 GHz、约 6 秒后才到请求的频率（读 MSR 确认 HWP 请求 min = max 正确，`MSR_CORE_PERF_LIMIT_REASONS`
  无活动原因，HWP_STATUS 报"跌破最低请求"）；禁用 CPU 2/3 的 C6+ 后消失。改设置后的头几秒也有，所以固定后先等忙碌检查通过再测。
- 关 turbo 后 min = max = 1.7 GHz 也受同样影响；用户最终选 turbo 开、钉 4.4 GHz（单核持续 60 秒实测 4.42 GHz、63°C）。

## 事件审计

`audit-events.py`（`event-audit/`，测量前、未固定频率）：不挂探针、kernel、官方、开发分支各跑一次 `test 1 100000`，
17 个用例在三组探针下都明显慢于不挂探针（不挂探针约 1–17 ns/调用，挂上后 ≥ 120 ns），开发分支有 2 行 `[array-jit-inline] inlined`。

## 结果（倍率 = 同段 kernel；map 行为每次操作 ns = 每次调用 / 1,000）

| 操作 | 开发分支/kernel | 官方/kernel | 开发分支 vs 官方 | Jetson a7d54d7e/kernel |
|---|---:|---:|---:|---:|
| 普通 array lookup（不使用返回值） | 0.25× | 3.19× | −92.3% | 0.31× |
| 普通 array update | 0.48× | 1.20× | −60.0% | 0.88× |
| 普通 hash lookup | 0.81× | 2.08× | −58.4% | 0.82× |
| 普通 hash update | 0.29× | 0.67× | −57.3% | 0.29× |
| 普通 hash delete | 0.30× | 0.53× | −44.6% | 0.26× |
| per-CPU array lookup（不使用返回值） | 0.70× | 5.49× | −87.2% | 0.95× |
| per-CPU array update | 0.64× | 2.43× | −73.9% | 0.87× |
| per-CPU hash lookup | 0.66× | 4.25× | −84.5% | 0.94× |
| per-CPU hash update | 0.49× | 2.79× | −82.1% | 0.66× |
| per-CPU hash delete | 0.27× | 0.58× | −52.7% | 0.67× |
| uprobe（ns/调用：124 / 1,108） | 0.11× | 0.11× | +0.9% | 0.16× |
| uretprobe | 0.14× | 0.14× | +0.8% | 0.16× |
| probe read | 0.08× | 0.09× | −11.3% | 0.26× |
| probe write | 0.07× | 0.08× | −14.7% | 0.24× |

- **开发分支在 x86 上 10 项 map 操作全部快于 kernel**（0.25–0.81×）；官方只有 hash update/delete 快于 kernel，其余 1.2–5.5×。
- 与 Jetson 的倍率：hash lookup/update/delete 几乎相同（0.81/0.29/0.30 对 0.82/0.29/0.26）；array update、per-CPU 各项在 x86 上更低。
- x86 kernel uprobe 约 1.1 µs/次（int3 陷入），bpftime 124 ns，倍率 0.11×，比 Jetson 的 0.16× 更低。
- 样本离散度（10 个样本的变异系数）多数 < 1%，最大 3.7%（官方 hash delete）。两段 kernel 组之间除 hash lookup（−5.9%）外都在 ±2% 内。
- uprobe/uretprobe 开发分支比官方慢 0.8–1.1%，Jetson 上曾确认这一量级属代码布局效应，这里未用 `perf stat` 复核。
- 注意：array lookup 两行的 workload 不使用返回值，开发分支的 JIT 内联后几乎没有工作，不能当作收益结论（Jetson 用 consuming 测试另测）。
  频率（4.4 GHz 对 Jetson 1.728 GHz）、编译器、LLVM 版本（19 对 16）都不同，跨机器只比倍率。

## 文件

- `run-full.sh`（`4400000 turbo-on full10-4.4ghz-turbo`）、`freq.sh`、`busy-freq.py`、`freq-log.py`、`audit-events.py`、`summarize.py`。
- `full10-4.4ghz-turbo/{dev,official}/`：`benchmark-output.json`、`results.md`、`benchmark-logs.txt`、`runner.log`、频率检查；`freq-log.txt` 和 `freq-log-marks.txt`（每段对应的行）。
- 冻结产物哈希：`artifact-sha256.txt`（复制自 `/home/y/src/bpftime-x86-runs/full10-20261026/`），测量前后各核对一次。
