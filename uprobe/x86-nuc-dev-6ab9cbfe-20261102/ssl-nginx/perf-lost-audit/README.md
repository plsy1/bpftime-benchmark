# x86 补测 perf 丢事件（2026-11-02 下午，NUC）

**结论：在 x86 上也不是 6ab9cbfe 的回归。** 偶发且有上报的丢失在 kernel、a7d54d7e 和 6ab9cbfe 上都有，频率差不多。上午"6ab9cbfe 2/4、a7d54d7e 0/4"只是轮数太少。

## 为什么补测

- 上午的审计（`../../README.md` 第 3 节）里，6ab9cbfe 4 次中 2 次丢事件，a7d54d7e 0/4。但那几次审计没带 kernel 腿。
- Jetson 复查（`benchmark-results/jetson` efad8076，`uprobe/perf-lost-audit-20261102/`）没有复现，结论是 kernel 也会偶尔丢。Jetson 建议在 x86 上多跑几轮，并在同一次审计里带上 kernel 腿。用户同意补测。

## 怎么测

- 脚本是 `run.sh`，由 `session.sh` 驱动，总超时 1 小时。每轮调用一次 `../audit-repeat/run_fair_stderr.py --audit`，这个脚本和原版 `run_fair.py` 只差两行，作用是保留 sslsniff 的 stderr。每条腿用 wrk 跑 10 个连接 3 秒，和原审计相同。
- 四条腿 kernel、官方、a7d54d7e、6ab9cbfe 在同一次审计里，每轮轮换顺序（`rounds.txt`）。
- 16 B 跑 16 轮，256 KB 跑 6 轮。
- 容器、镜像、冻结的构建、绑核和频率都和 `../run.sh` 相同：nginx 在 CPU 2，sslsniff 在 CPU 4，都是 3.5 GHz；wrk 在 E 核 8–9，2.5 GHz。
  - 测前测后都核对了产物哈希。
  - 测前用忙碌检查确认频率正常（`check-before.txt`），测后设置已恢复（`pin-restored.txt` 和 `pin-state.txt` 一致）。
  - plymouthd 测时绑到 E 核 12–14，测完改回 0–15。
  - 测前没有残留进程（`leftover-processes.txt` 为空）。`leftover-after.txt` 里只有我自己用来看进度的监视脚本。
  - 容器测完已删（`cleanup.log`），Docker 里只剩用户的 sing-box。
- 第一次启动时，残留进程检查匹配到了本目录脚本自己的路径（路径里含 "ssl-nginx"），所以还没测就拒绝启动了，记录在 `first-try-refused/`。修正 `session.sh` 后重跑。
- 丢失数是把 sslsniff stderr 里 libbpf 打印的 `lost N events on CPU #c` 累加起来。dev 构建从 10/23 起会把丢弃作为 PERF_RECORD_LOST 上报，官方 177a1dc 不上报。不计时，不做吞吐结论。
- 原始数据：`out/<set>-r<n>.json`，旁边是各腿的 sslsniff stderr；日志是 `<set>-r<n>.log`。
- sslsniff 的 stdout 共 47 GB，不提交，放在本机 `/home/y/src/bpftime-x86-runs/dev-6ab9cbfe-20261102/ssl-nginx/captures/perf-lost-audit/`，哈希在 `captures-sha256.txt`。

## 结果（`summarize.py` → `summary.md`）

| 组 | 腿 | 有丢失的轮数 | 每请求丢失 均值（最小–最大） | 每请求事件 均值 | 送达比例 |
|---|---|---:|---:|---:|---:|
| 16 B | kernel | 5/16 | 0.0004（0–0.0021） | 2.000 | 99.980% |
| 16 B | 官方 | 0/16（不上报） | — | 1.999 | — |
| 16 B | a7d54d7e | 6/16 | 0.0003（0–0.0016） | 2.000 | 99.986% |
| 16 B | 6ab9cbfe | 7/16 | 0.0004（0–0.0019） | 2.000 | 99.981% |
| 256 KB | kernel | 6/6 | 0.068（0.0001–0.187） | 17.94 | 99.62% |
| 256 KB | 官方 | 0/6（不上报） | — | 4.49 | — |
| 256 KB | a7d54d7e | 6/6 | 0.022（0.009–0.056） | 17.99 | 99.88% |
| 256 KB | 6ab9cbfe | 6/6 | 0.040（0.008–0.166） | 17.97 | 99.78% |

- **16 B**：6ab9cbfe 7/16，a7d54d7e 6/16，kernel 5/16。Fisher 精确检验：6ab9cbfe 对 a7d54d7e 的 p = 1.0，对 kernel 的 p = 0.72。每次丢失都在 CPU 2（nginx 所在的核），量级是每轮 9 到 461 个。
  - 把上午的 4 次合进来，6ab9cbfe 是 9/20，a7d54d7e 是 6/20，差别仍然不显著。
- **256 KB**：三条腿每轮都丢，每请求丢失的范围互相重叠。6ab9cbfe 的均值被第 2 轮的 3,056 个拉高，kernel 自己也有一轮丢了 2,708 个。
- **没有悄悄丢的**：kernel、a7d54d7e、6ab9cbfe 的"送达加上报"都是每请求 2.000（16 B）和 18.01（256 KB），和预期一致，多出的一点是 wrk 统计时还在途中的请求。
- **官方 177a1dc**：stderr 里没有丢失报告，但事件数偏少。16 B 下比 2 个/请求少 2,645 个，256 KB 下只有 4.49 个/请求（应为 18）。原因是官方不上报丢失，不是没丢；上午的表里就已经标了"丢事件"。这是官方原版的已知行为，和 6ab9cbfe 无关。

## 和 Jetson 对照

和 Jetson 的复查一致：16 B 下偶尔丢、有上报，每次约 0.1% 或更少，kernel 也会丢；256 KB 下各腿每轮都丢。x86 上 16 B 的丢失比 Jetson 更频繁（三条腿都是 1/3 左右，Jetson 是 0–1/8），256 KB 的丢失比 Jetson 少（每请求 0.02–0.07，Jetson 是 0.2）。这两点都同时出现在 kernel 腿上，所以是平台和负载的差别，不是 bpftime 版本的差别。
