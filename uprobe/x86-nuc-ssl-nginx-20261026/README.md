# x86 NUC：ssl-nginx 端到端，不追踪 / kernel / 官方 177a1dc / a7d54d7e（2026-10-26）

Jetson `btf-kernel-20261026/ssl-nginx` 的同一套方法搬到 x86 NUC：nginx（1 个 worker）+ wrk `-c 100 -d 10`，
sslsniff（bpftime 自带示例，原样 CO-RE 编译）跟踪 nginx 的 SSL 读写；四组每轮各跑一次、顺序轮换，16 B 和 256 KB 各 2 轮 × 10 次。
结果表：`summary.md`（Jetson 的 `summarize.py` 原样）。

## 条件

- 机器：i5-1240P，Debian 13，内核 `6.12.113+deb13-amd64`（`log/kernel.txt`）。时间 16:30–17:05。
- 镜像：Jetson 的 `ssl-nginx-2x2-20261023/docker/Dockerfile` 原样（Ubuntu 24.04：nginx 1.24.0、OpenSSL 3.0.13、wrk），本机建为
  `bpftime-ssl-nginx:x86-20261026`；容器跑完已删（镜像保留）。
- 运行时产物：与 `../x86-nuc-full10-20261026` 同一份冻结产物（gcc-13、LLVM 19；`official` = 177a1dc 原版，含每事件绑核），
  放在本机 `/home/y/src/bpftime-x86-runs/ssl-nginx-20261026/builds/`；sslsniff 用官方树 `example/tracing/sslsniff` 以 clang-19 编译。
  哈希 `artifact-sha256.txt`，测前测后各核对一次。
- **绑核与频率（x86 新增）**：`run_fair.py` 加了 `NGINX_CPUS`/`SNIFF_CPUS`/`WRK_CPUS`（子进程 exec 前设亲和性，不套 taskset，
  以免 LD_PRELOAD 的 agent/syscall-server 先在 taskset 里初始化）。nginx 在 CPU 2、sslsniff 在 CPU 4（P 核，兄弟线程 3、5 空着），
  钉 3.5 GHz；wrk 在 E 核 8–9，钉 2.5 GHz；`run_fair.py` 本身在 E 核 10。turbo 开、min = max、C6+ 禁用（`pin-cpus.sh`）。
  4 个 P 核同时满载时 4.4 GHz 撑不住（3.5/2.9 GHz 不等），2 个 P 核 4.4 GHz + 2 个 E 核 3.3 GHz 一分钟内下滑（4392 → 4333 MHz、71°C），
  3.5/2.5 GHz 稳定，所以没有用 uprobe 十轮那次的 4.4 GHz。
- 频率记录（`log/freq-log.txt`，每秒，E 核 15 采样）：nginx 所在 CPU 2 忙碌 > 90% 的 1,482 个采样中 4 个偏离 3.5 GHz 超过 3%；
  测前测后的忙碌检查都通过（`log/check-*.txt`）。测完恢复原设置（`log/pin-restored.txt` 与测前一致）。
- 瓶颈：测量期间 CPU 2（nginx）约 99% 忙；wrk 的 E 核平均 68% 忙、峰值 99%，不追踪组时客户端接近饱和，"相对不追踪"一列可能略偏小。

## 事件审计（`audit-*.json`，每组 3 秒短测，sslsniff 输出计数）

| 负载 | kernel | 官方 | a7d54d7e |
|---|---:|---:|---:|
| 16 B（事件/请求） | 2.000 | 2.000 | 2.000 |
| 256 KB（事件/请求） | 17.69 | **5.46** | 18.00 |

官方在 256 KB 时丢掉约 70% 事件（perf 缓冲区满静默丢弃，开发分支 `db3d475f` 修过；Jetson 上官方为 9.46），所以官方的 256 KB 吞吐不是公平对比。
完整 sslsniff 输出在本机 `raw/audit-captures/`，仓库里只留首尾各 20 行（`*.head-tail.txt`）。

## 结果（每秒请求数，2 轮 × 10 次合并中位数）

| 负载 | 不追踪 | kernel | 官方 | a7d54d7e | a7d54d7e vs kernel | 官方 vs kernel | Jetson a7d54d7e vs kernel |
|---|---:|---:|---:|---:|---:|---:|---:|
| 16 B | 95,883 | 53,827 | 62,558 | 79,642 | **+48.0%**（20/20 次配对都更快） | +16.2% | +17.5% |
| 256 KB | 6,845 | 4,613 | 5,212 | 5,901 | **+27.9%**（20/20） | +13.0%（丢事件） | +10.4% |

- 每个请求比不追踪多出的时间（16 B）：kernel 8.1 µs，官方 5.6 µs，a7d54d7e 2.1 µs；256 KB：70.7 / 45.8 / 23.4 µs。
- 相对不追踪：16 B 时 kernel −43.9%、官方 −34.8%、a7d54d7e −16.9%（官方论文 x86 上 bpftime −7.41%、kernel −28.06%，机器和负载不同）。
- 与 Jetson 不同，官方在 x86 上也快于 kernel：x86 上 kernel 的 uprobe + uretprobe 一对约 1.34 µs（uprobe 十轮），
  bpftime 约 0.21 µs；16 B 时每个请求 2 次 SSL 读写各触发一对，kernel 光探针就约 2.7 µs。
- 两轮之间差 < 1%（16 B 官方 r1/r2 62,658/62,501），变异系数 0.4–3.7%。

## 文件

`run.sh`、`run_fair.py`（相对 Jetson 版只加了绑核）、`pin-cpus.sh`、`busy-check.py`、`freq-log.py`、`summarize.py`、`nginx.conf`、测试证书；
`fair-*.json/log`（每次样本）、`audit-*`、`log/`（频率、检查、时间、恢复）、`docker/Dockerfile`。

## 下次可以改进

- sslsniff 所在 CPU 4 只是轻度忙碌，保留了 C1E，平均忙碌频率在 2.5–3.5 GHz 之间浮动（四组条件相同）；可再禁用 C1E。
- E 核 8–11 共用一个频率，CPU 10 未钉频时 wrk 的核会到 2.6–2.7 GHz；可把整个 E 核簇一起钉住。
