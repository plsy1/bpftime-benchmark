# x86 NUC：dev 6ab9cbfe 的官方 uprobe 十轮和 ssl-nginx（2026-11-02 早）

Jetson 会话转达的用户请求：用三轮 review 修复后的 `6ab9cbfe` 在 x86 上跑一次官方 benchmark 的 ssl-nginx 和 uprobe，
先编译、跑测试，再测量，和 NUC 之前的结果对比。这是对照检查，不做 x86 优化，开发分支没有改动。

- 对照对象：NUC 以前的 uprobe 和 ssl-nginx 测量只在 10/26 做过，对象是开发分支 `a7d54d7e` 和官方 `177a1dc`；`1aa74c4c` 上只跑过 syscall 测试，没有这两项测量。
- 所以这次把 10/26 冻结的 `a7d54d7e`（两项都测）和官方 `177a1dc`（ssl-nginx）在同一场里重测，对比不受日期影响。
- 机器：`debian-ichi-nuc`，i5-1240P，Debian 13，内核 `6.12.113+deb13-amd64`。
- 时间：05:12 构建，05:15–05:17 测试，05:18–06:25 测量，06:25–06:27 补测审计。

## 0. 结论

- **构建和测试**：6ab9cbfe 在 x86 上编译通过；没有发现 6ab9cbfe 引入的测试失败（`tests/README.md`）。
  - x86 syscall 改写测试连跑 3 次都是 5/5。
  - runtime 单测逐用例 228 个，需要 root 的几个用 sudo 补跑后全过；ringbuf 5/5，perf 16/16，Frida 19/19，syscall trace 9/9（root）。
- **uprobe 十轮**：探针、helper、数组、embed 和 a7d54d7e 持平（±0.6%）。
  - 哈希 map 的写入方向和 Jetson 一致，都变慢了，但每次操作只多 1.4–3.6 ns。
  - 普通哈希 lookup 方向相反：x86 上快了 38.7%（8.61 → 5.28 ns），Jetson 上是慢了 18.3%。
- **ssl-nginx**：6ab9cbfe 比 kernel 快 48.5%（16 B）和 26.8%（256 KB），20/20 次配对都更快；比 a7d54d7e 慢 0.3% 和 0.7%，和 Jetson 的 −1.6%、−0.8% 同方向，但更小。
- **perf 偶尔丢事件：下午补测后确认不是 6ab9cbfe 的回归**（第 3 节，`ssl-nginx/perf-lost-audit/`）。
  - 上午 16 B 审计里 6ab9cbfe 4 次中有 2 次丢事件（约 0.14%，sslsniff 自己有报告），a7d54d7e 0/4，但没带 kernel 腿。
  - 下午 16 轮、四条腿同场：有丢失的轮数 kernel 5/16、a7d54d7e 6/16、6ab9cbfe 7/16，差别不显著（p = 1.0 和 0.72）；kernel、a7d54d7e、6ab9cbfe 都没有悄悄丢的。和 Jetson 的复查结论一致。

## 1. uprobe 完整十轮（`full10/`）

- **方法**：官方 `benchmark/uprobe/benchmark.py --iter 10`，同 10/26（`../x86-nuc-full10-20261026`）。
  - CPU 2 开 turbo、钉 4.4 GHz，C6 及更深的空闲状态关闭。
  - 一场里先跑 6ab9cbfe，再跑 a7d54d7e，每段都带自己的 kernel 组。
  - 工作负载程序 `benchmark/test`、`benchmark/uprobe/uprobe` 与 10/26 完全相同（哈希没变；两个提交之间 `benchmark/uprobe/` 没有改动），只换 agent、syscall-server 和 embed 基准。
  - 每次 `benchmark.py` 都有 2400 秒超时，超时放在 LD_PRELOAD 外面。
- **事件审计**（`audit-events.log`，测前，没有钉频）：17 个用例在 kernel、a7d54d7e、6ab9cbfe 下都明显慢于不挂探针；两个开发分支各有 2 行数组 JIT 内联日志。PASSED。
- **频率**：
  - E 核 15 上每秒采样，CPU 2 忙碌 > 90% 的 1,224 个采样中，3 个落在 4,368–4,379 MHz，其余都在 4,380–4,460 MHz。
  - 每段前后的忙碌检查都在 4.33–4.40 GHz。
  - 测完 CPU 2 的设置和测前一致（`freq-restored.txt`）。
- **结果**：`full10-4.4ghz-turbo/comparison.md`（`summarize.py`）。map 行是每次操作的 ns：

| 操作 | 6ab9cbfe | a7d54d7e（同场） | 变化 | 6ab9cbfe/kernel | a7d54d7e/kernel | Jetson 6ab9cbfe 变化 | Jetson 6ab9cbfe/kernel |
|---|---:|---:|---:|---:|---:|---:|---:|
| 普通 array lookup（不用返回值） | 0.45 | 0.45 | −0.3% | 0.24× | 0.24× | 0 | 0.31× |
| 普通 array update | 3.36 | 3.37 | −0.2% | 0.45× | 0.45× | 0 | 0.88× |
| **普通 hash lookup** | **5.28** | 8.61 | **−38.7%** | 0.52× | 0.83× | +18.3% | 0.97× |
| **普通 hash update** | 11.98 | 10.05 | **+19.3%** | 0.34× | 0.29× | +51.8% | 0.45× |
| **普通 hash delete** | 13.71 | 10.16 | **+34.9%** | 0.40× | 0.30× | +53.9% | 0.40× |
| per-CPU array lookup | 1.28 | 1.28 | 0 | 0.70× | 0.71× | 0 | 0.91× |
| per-CPU array update | 4.73 | 4.76 | −0.6% | 0.64× | 0.64× | +2.9% | 0.90× |
| **per-CPU hash lookup** | 10.24 | 6.98 | **+46.7%** | 0.95× | 0.64× | +25.8% | 1.15× |
| **per-CPU hash update** | 16.14 | 12.65 | **+27.5%** | 0.63× | 0.49× | +30.6% | 0.85× |
| per-CPU hash delete | 31.87 | 30.47 | +4.6% | 0.29× | 0.27× | +6.0% | 0.70× |
| uprobe（ns/调用） | 122.8 | 123.6 | −0.6% | 0.11× | 0.11× | 0 | 0.16× |
| uretprobe | 183.8 | 183.0 | +0.5% | 0.14× | 0.14× | −1.3% | 0.16× |
| uprobe + uretprobe | 213.0 | 214.3 | −0.6% | 0.16× | 0.16× | 0 | 0.18× |
| probe read / write | 1,512 / 1,281 | 1,506 / 1,283 | +0.4% / −0.2% | 0.08× / 0.07× | 0.08× / 0.07× | 0 | 0.26× / 0.24× |
| embed（ns） | 63.8 | 63.3 | +0.8% | | | 0 | |

（Jetson 的"变化"是 6ab9cbfe 对 10/26 的 a7d54d7e，取自 `benchmark-results/jetson` 的 `uprobe/dev-6ab9cbfe-20261101`。）

- **可比性**：
  - 两段 kernel 组之间相差 −1.8% 到 +2.8%；和 10/26 相比在 ±2.2% 内，只有 hash lookup 是 −4.7%。
  - 同场重测的 a7d54d7e 和 10/26 相比都在 ±0.4% 内，只有 array update 是 −6%（3.58 → 3.37）。
  - 10 个样本的变异系数：bpftime 各项都 ≤ 1.2%；kernel 组除 probe read（2.9–3.4%）和一次 uprobe（1.4%）外都 ≤ 0.9%。
- **解读**：
  - **哈希写入变慢**：普通 hash update/delete、per-CPU hash 各项变慢，和 Jetson 的方向一致。Jetson 已经用 `hash-bisect/` 把它们归到分段写锁（cb3f29d2/f4fdcbcd）和 per-CPU hash 每次操作都拿 map 锁（cdc05f67/c98951a4）。
    - x86 每次操作只多 1.4–3.6 ns（Jetson 是 6–15 ns），相对比例也比 Jetson 小，只有 per-CPU hash lookup 是 +47%（Jetson +26%）。
    - per-CPU hash lookup 在 x86 上仍然快于 kernel（0.95×），Jetson 上已经慢于 kernel（1.15×）。
  - **普通 hash lookup 变快**：只有这一项方向相反，x86 上 −3.3 ns/次。Jetson 把它的变慢归到 9c3e0345（不加锁的 lookup）。一个可能的解释是：x86 上去掉锁省下的原子操作比新增的读侧检查更多，ARM64 上则相反。这次没有单独二分，只是推测。
  - 探针、helper、数组和 embed 都和 a7d54d7e 持平，与 Jetson 相同。

## 2. ssl-nginx（`ssl-nginx/`）

- **方法**：同 10/26（`../x86-nuc-ssl-nginx-20261026`）。
  - 镜像 `bpftime-ssl-nginx:x86-20261026`；nginx 1 个 worker 在 CPU 2，sslsniff 在 CPU 4，两个核钉 3.5 GHz；wrk 在 E 核 8–9，钉 2.5 GHz。
  - 负载 `wrk -c 100 -d 10`，16 B 和 256 KB 各 2 轮 × 10 次，每次 5 组轮换顺序。
  - 组别和 Jetson 11/01 相同：不追踪、kernel、官方 177a1dc、a7d54d7e、6ab9cbfe；sslsniff 是同一个 CO-RE 程序（哈希没变）。
  - 每次 `docker exec` 在宿主机上套 1500 秒超时；容器 `ssl-x86-6ab9` 跑完已删，镜像保留。
- **频率**：nginx 所在 CPU 2 忙碌 > 90% 的 1,866 个采样中，1 个偏离 3.5 GHz 超过 3%；测前测后忙碌检查都 PASS；测完各核设置已恢复。
- **结果**（`summary.md`，每秒请求数，20 次合并中位数）：

| 组 | 16 B | vs kernel | 256 KB | vs kernel | 10/26 x86（16 B / 256 KB） |
|---|---:|---:|---:|---:|---:|
| 不追踪 | 95,710 | +78.6% | 6,858 | +47.4% | 95,883 / 6,845 |
| kernel | 53,574 | 0 | 4,654 | 0 | 53,827 / 4,613 |
| 官方 177a1dc | 62,552 | +16.8% | 5,223 | +12.2%（丢事件） | 62,558 / 5,212 |
| a7d54d7e | 79,755 | +48.9% | 5,919 | +27.2% | 79,642 / 5,901 |
| **6ab9cbfe** | **79,551** | **+48.5%** | **5,901** | **+26.8%** | – |

- **配对比较**：
  - 6ab9cbfe 对 kernel：+47.8%（16 B）、+26.6%（256 KB），都是 20/20 次更快。
  - 6ab9cbfe 对 a7d54d7e：−0.33%（16 B，5/20 次更快）、−0.69%（256 KB，7/20 次更快）。
- **解读**：
  - 每个请求比不追踪多出的时间：16 B 时两个开发分支都是 2.1 µs；256 KB 时 6ab9cbfe 23.7 µs，a7d54d7e 23.1 µs。
  - Jetson 上 6ab9cbfe 对 a7d54d7e 是 −1.6%（16 B，约 +1.2 µs/请求），x86 上只有 −0.3%，差不多在噪声边缘。
  - 各组和 10/26 相比都在 ±1% 内，机器状态没有变化。
  - 对 kernel 的优势 x86 比 Jetson 大得多（+48% 对 +15%）：x86 上 kernel uprobe 走 int3 陷入，一次约 1.1 µs，而 bpftime 一次约 0.12 µs（第 1 节），10/26 已经讨论过这一点。

## 3. 6ab9cbfe 偶尔丢 perf 事件（16 B，x86）：补测后不是回归

> **下午补测的结论（`ssl-nginx/perf-lost-audit/README.md`）**：16 B 跑 16 轮、256 KB 跑 6 轮，kernel、官方、a7d54d7e、6ab9cbfe 四条腿同场，每轮轮换顺序。16 B 下有丢失的轮数是 kernel 5/16、a7d54d7e 6/16、6ab9cbfe 7/16，每次都在 CPU 2，都有上报，没有悄悄丢的。256 KB 下三条腿每轮都丢，范围互相重叠。所以下面上午的"2/4 对 0/4"只是轮数太少；下面列出的"可能相关的提交"和"建议"不再需要跟进。Jetson 的复查（`benchmark-results/jetson` efad8076）结论相同。

以下是上午的记录，原样保留。

- **第一次审计**（`audit-16.json`，每组 10 个连接 3 秒）：
  - kernel、官方、a7d54d7e 的事件数都等于请求数的 2 倍再多 16–18 个，即 2.000 个/请求。
  - **6ab9cbfe 少了 704 个**（1.997 个/请求）：READ 248,872、WRITE 248,870，请求 249,223。
  - 原脚本丢弃了 sslsniff 的 stderr，看不到原因。
- **补测**（`ssl-nginx/audit-repeat/`）：
  - `run_fair_stderr.py` 和原版只差两行：保留 sslsniff 的 stderr。同样的容器、绑核和频率，16 B，a7d54d7e 和 6ab9cbfe 各 3 次。
  - a7d54d7e 3 次都是 +20 到 +22 个事件，stderr 没有丢失报告。
  - 6ab9cbfe 第 1、2 次是 +18；**第 3 次 −344**，stderr 里有 `lost 1 events on CPU #2` ×2 和 `lost 360 events on CPU #2`，合计 362 个。CPU 2 是 nginx（生产者）所在的核。
  - 合计：6ab9cbfe 4 次中 2 次丢事件，a7d54d7e 0/4。
- **256 KB**：6ab9cbfe 18.002 个/请求，比预期少 29 个；a7d54d7e 多 6 个，kernel 少 7 个。只审计了一次，算不上信号。
- **判断**：
  - 这是 perf 输出的丢失回调报的，也就是生产者那边的 perf 缓冲区写满了。消费者 sslsniff 独占 CPU 4（3.5 GHz），条件和 a7d54d7e 完全相同。
  - 损失约 0.14%，量很小，对吞吐数字没有可见影响；但 a7d54d7e 在同样条件下一次都没丢。
  - Jetson 11/01 的审计里 6ab9cbfe 都是 2.000，但那边的 stderr 同样被丢弃，而且只跑了一次。
- **可能相关的提交**（只按提交说明列出，没有分析，代码没动）：
  - `e3e7d721` perf output: match shards per pid namespace, survive full shm, **rotate drains**
  - `d0d9adfa` perf output: reclaim only shards of the reclaimer's pid namespace
  - `2839d98a` perf output: drop a shard whose data area could not be allocated
  - `ce11e384` perf output: fork children do not use the thread's cached shards
- **建议**：
  - 在 Jetson 上把 `run_fair.py` 的 sslsniff stderr 留下来再审计几次，看 ARM64 上是否也丢。
  - 如果要定位，可以在 a7d54d7e..6ab9cbfe 的 perf output 提交之间二分；NUC 可以按交接来做。

## 4. 文件

- `build/`：`run-build.sh`、构建日志、`cmake-options.txt`、`source-state.txt`、`attempt1-wrong-target/`（目标名写错的第一次）。
- `tests/`：`run-tests.sh`、`run-each-case.py`、每项日志和退出码、`runtime-each/`、`frida-each/`、sudo 补跑日志、`README.md`。
- `full10/`：`run-full.sh`、`audit-events.py`、`summarize.py`、频率脚本、`jetson-reference/`、`full10-4.4ghz-turbo/`（每段的 `benchmark-output.json`、`results.md`、`runner.log`、频率检查、`comparison.md`）。
- `ssl-nginx/`：`run.sh`、`run_fair.py`（同 10/26 的 x86 版）、`summarize.py`（Jetson 11/01 原样）、`fair-*`、`audit-*`、`log/`、`audit-repeat/`、`perf-lost-audit/`（下午的丢事件补测；sslsniff 的完整输出 47 GB 同样不提交）。
  - sslsniff 的完整输出（8.1 GB）不提交，放在本机 `/home/y/src/bpftime-x86-runs/dev-6ab9cbfe-20261102/ssl-nginx/captures/`。仓库里只留首尾各 20 行（`*.head-tail.txt`）和哈希（`captures-sha256.txt`）。
- `run-all.sh`、`session/`：一次跑完审计、十轮和 ssl-nginx 的驱动脚本与记录。
  - 测前确认没有残留进程和 `/dev/shm/bpftime_maps_shm`；`plymouthd`（开机后一直占约 2.5% CPU）临时绑到 E 核 12–14，测完已改回 0–15。
- **冻结产物**：`/home/y/src/bpftime-x86-runs/dev-6ab9cbfe-20261102/`（不提交）。哈希的副本在本目录的 `full10/artifact-sha256.txt` 和 `ssl-nginx/artifact-sha256.txt`，测前测后各核对一次。
- **Docker**：用户 10/31 起在跑 `sing-box` 容器，Docker 服务保持原状，没有停；这次只删了自己的两个容器。
