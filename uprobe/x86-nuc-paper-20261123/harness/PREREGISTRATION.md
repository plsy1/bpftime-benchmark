# PREREGISTRATION — userspace eBPF paper re-measurement (frozen 2026-11-23)

Written before the first formal measurement (E1). Identical copies are committed on benchmark-results/x86-nuc and benchmark-results/jetson; its sha256 is recorded in SHA256SUMS next to it. It is not edited afterwards; deviations are written up separately with reasons.

## Frozen code
- O = official 4708f409 (CPU runtime identical to 177a1dc4); Odiag = ladder/odiag-4708f409 4bef75b7 (switches default off); L3port = ladder/l3port-4708f409 8ac1ec70; D = dev 9ccda7e8, CUDA off; Opc/Dpc = O/D with ENABLE_PROBE_READ_CHECK=ON and ENABLE_PROBE_WRITE_CHECK=ON.
- sslsniff: official 4708f409 example; counting loader (SSLSNIFF_CNT line after a final perf_buffer__consume); stages S1/S2h/S2hm/S2m/S3/S4 by SSLSNIFF_STAGE and SSLSNIFF_NO_* defines; S4 program sections byte-identical to stock (checked on x86 2026-11-23).
- Headline affinity policy M (x86: nginx {2,6}, sslsniff {4}, wrk {8-11} -t4, harness {12-14}, freq-log {15}; P cores 3.5 GHz, E cores 2.5 GHz, C6+ off. Jetson: nginx {4,5}, sslsniff {3}, wrk {0,1} -t2, harness {2}). S and U/C are sensitivity rows only.
- BPFTIME_AOT_CACHE default (on) is the headline, regardless of results; =0 is a sensitivity row.

#### 2.0.1 session 守卫（F#2、F#20、M#6、M#13、M#19）

**开始时检查**（任何一项不过就拒绝启动）：
- **时钟**
  - Jetson：cpu5 的 `scaling_{min,max,cur}_freq` 三个值都等于 1728000，并 dump 一次 `sudo -n jetson_clocks --show`（含 EMC）。
  - x86：先执行 `pin-cpus.sh pin`，再跑 `busy-check.py 2112 <pins>`（APERF/MPERF 偏差在 3% 以内，最多重试 15 次），然后启动 `freq-log.py`，放在 CPU 15 上。
- **进程残留**：遍历 `/proc/*/exe`，按可执行文件路径匹配 `benchmark/test`、`uprobe/uprobe`、sslsniff、nginx、wrk、`bpftime*`、memleak、bpftrace、java，排除守卫自身的进程树。不再用 `pgrep -f`：经 ssh 或 zsh 包一层时，外层 argv 含有匹配模式，会误报。
- **共享内存**：`find /dev/shm -mindepth 1 -name 'bpftime*'` 为空。
- **负载**：loadavg < 0.3，**只在开始时检查**。
- **仅 Jetson**：没有 llama 或 run_meas 进程。
- **timer 规则**：session 窗口及其前后 20 min 内不能有以下 timer 触发：apt-daily（约 18:27）、apt-daily-upgrade（约 06:15）、man-db、fstrim（周一约 00:30）、e2scrub（周日 03:10）。
  - 其余 timer（anacron 每小时、fwupd-refresh 每小时、x86 的 tailscale-watchdog 每 60 s）不再要求避开，事后用 `journalctl --since` 取触发时刻，与触发重叠的 rep 只做标记。
  - **tailscale-watchdog 绝不能停**，它负责远程访问恢复。

**结束时检查**：时钟、进程残留、shm、sha256。

**每个 leg 记录**：
- 环境：`env | grep BPFTIME`、`uname -r`、`/proc/cmdline`、vulnerabilities；
- CPU：`/proc/interrupts` 增量；nginx 所在核的 `/proc/stat` 增量（user+system+irq+softirq）；nginx worker 和 sslsniff 的 utime+stime；wrk 各核的忙碌率；
- 仅 Jetson：tj-thermal 温度和节流计数。

**x86 额外操作**：
- plymouthd 移到 CPU 12–14，结束后移回；
- 设置 trap，退出时恢复频率和 idle 状态；
- 整体套 `timeout`。

**Jetson 重启预案**（F#10）：
- 若发现 uptime 被重置，campaign 立即暂停。
- 重跑 `jetson_clocks` 需要 sudo，必须先得到用户批准。
- 容器在每个 session 结束时删除。

#### 2.0.2 leg 轮转
- 每个 rep 内所有 leg 各跑一次，顺序是按 `seed=hash(session_id, rep)` 生成的随机排列。
- 每个 leg 都重启 nginx、sslsniff 和 wrk。
- 附录报告残留效应检查：x 对 `pred_leg` 做回归。

#### 2.0.3 统计口径（全文统一）
- **e2e 吞吐比值（主口径，M#17）**
  - 配对对数比 ℓ = ln(rps(leg,r)/rps(K,r))。
  - 报告几何均值比 GMR = exp(mean ℓ) 和 95% t-CI。
  - 辅助报告：median(ρ) 加 BCa bootstrap CI（10,000 次重采样，按 round 分层，固定 seed）、n_faster/N、pooled median rps 和 CV。
  - 中位数之比只放附录。
- **每请求额外时间**：x(leg,r) = 1/rps(leg,r) − 1/rps(B,r)，报告配对均值和 95% t-CI。Figure 1 和 ladder 都用这个口径，因为只有均值能逐段相加。
- **每事件成本**（M#3）：x ÷ **工作量事件数** E_work，所有 tracer、所有 stage 都用同一个 E_work。
  - E_work 是负载的属性：16 B 时为 2 × req_nginx / requests，约 2.000；256 KB 时取 K@S4 的 (delivered+reported)/req，并加注"按 kernel 估计"。
  - official 的 delivered/expected 单独列一列，**不再除以它自己交付的事件数**：它丢掉的事件已经做完了 helper 和 map 工作，只省了 append。
- **每 hit 成本**：x ÷ hits/request（来自 E7 kernel 的 `run_cnt`）。前提是不变性判据通过（M#15）：K@S1、K@S3、K@S4 三档的 run_cnt/req 相差 ≤ 1%。不通过时，bpftime 只报每请求和每事件成本。
- **等价闸门用 TOST**（M#5）：配对 ℓ 差的 90% CI 必须完全落在 ±margin 内。
  - 允许预登记的一次扩样：先跑 10 reps；点估计在 margin 内但 CI 越界时，扩到 20 reps；仍不满足就判"未证明等价"，进入退路。
- **微基准**
  - `--iter 10`，每次运行得到 10 个样本，每个样本 10 万次调用。主值用中位数，同时给均值和 CV。
  - 比值同时对两个 kernel 基准计算：同一次运行内的 kernel group，以及运行前后两次 kernel bookend 的均值。
  - per-op 同时给两种：raw/1000，以及扣除 uprobe 后的 (row − uprobe)/1000。array 和 per-CPU array 的 delete 走 EINVAL 路径，排除。
  - 抖动规则：同一样本序号在至少 3 个 map 行上同时偏离 5×MAD，就把该样本在这个 leg 的所有行一起剔除，并写入报告。
- **cycles** = ns × freq-log 实测中位频率（Jetson 用 1.728 GHz）。ATC 和 HotOS 引用同一组数。
- **跨平台 CPU 时间**（M#6）：x86 没有开 `CONFIG_IRQ_TIME_ACCOUNTING`，Jetson 开了，所以 task-clock 和 utime+stime 跨平台不可比。
  - nginx busy 改用独占核的 `/proc/stat` user+system+irq+softirq 计算。
  - task-clock/request 只在平台内部比较，并在图注里写明。

#### 2.0.4 事件审计与正确性判据（M#1、M#3、F#3）
- **适用范围**：只对 `@S4` 且不带 NOOP_OUTPUT 的 tracing leg 做正确性判定。
  - S1、S2h、S2hm、S3 和 `+NOOP_OUTPUT` 的 leg 只断言 delivered=0、lost=0、wrk errors 正常、nginx 存活。
- **记录字段**：`req_wrk, req_nginx, read_ev, write_ev, write_bytes, exp_write_bytes, reported_lost, tail_ev, ev_per_req, del_over_exp`。
- **"无未报告丢失"判据（用于 K、D、Dpc）**
  - 定义：missing_ev = (exp_read_ev − read_ev) + ⌈(exp_write_bytes − write_bytes) / 16384⌉。16384 是 nginx 的 ssl_buffer_size，也就是单次 SSL_write 的上限。
  - 判据：missing_ev ≤ reported_lost + τ，同时 write_bytes ≤ exp_write_bytes。
  - τ 预登记为 max(1e-4 × exp_ev, 2 × conns)，并用 E1 里 K 的实测分布标定：K 若越界，就是 harness 的问题，先修 harness，不归咎于 D。
- **"静默丢失"判据（用于 O）**：write_bytes < exp_write_bytes，而 reported_lost × 16384 不足以解释缺口。
- **harness 失效作废**：K 或 D 的 (delivered+reported) < 0.99 × exp_ev，说明 harness 坏了。
- **旧口径审计**：每个主 session 末尾，对每个 tracing leg 在 16 B 和 256 KB 各补一次 `-c 10 -d 3`，同样走 sslsniff-cnt，约 3 min，用于和历史数据衔接，不写 capture。

#### 2.0.5 作废规则（只用客观条件，不按数值剔除 rep）
满足任一条即作废：
- 时钟断言失败；
- wrk errors > requests 的 0.1%；
- sha 不符；
- 进程或 shm 残留；
- nginx worker 中途消失；
- cnt 的 JSON 缺失（loader 异常退出）；
- 2.0.4 的 harness 失效条件。

处理方式：
- 作废的 rep 连同它的所有 leg 一起丢弃，在 session 末尾补跑。
- 一个 session 作废超过 2 个 rep，整场作废。
- MAD 稳健 z > 3.5 只做标记，另给一张含与不含的敏感性表。
- 与 timer 触发重叠的 rep 只做标记，不作废。

### 3.5 可加性与闭合检查（阈值写进 PREREG）
1. **恒等式**：各段之和等于 x(S4)，这是对脚本本身的自检。
2. **跨 session 闭合**：E6 中 K、O、D 的 @S4 值及其差（O−K、D−K），与 E4 的 16 B 结果比较，差的 90% CI 落在 ±5% 内（TOST，M#5）。
3. **单项敲除之和与全敲除对比**：Δ(NO_PIN) + Δ(UIDGID 单项) + Δ(PIDTID 单项) 对比 Δ(ALL3)，残差 ≤ 15% 或在 CI 内（7 月的值是 7%）。另外从累积顺序和单项敲除分别得到 pin 的估计，两者比较。
4. **NOOP_OUTPUT 闭合**：Odiag@S4+NOOP_OUTPUT 应约等于 Odiag@S3，即 helper 分发成本约为 0。
5. **跨方法闭合**（只用 E7 的 `cyc` 模式）：
   - 每个 stage 的 Δ(cycles:u+cycles:k)/req ÷ 实测频率，应接近该 stage 的吞吐 Δx，误差 ≤ 25%。
   - 每个 stage 同时报告 Δinstructions，IPC 变化 > 10% 的 stage 标注为"干扰（cache/TLB）"。
   - O 与 ALL3 之间的 Δcycles:k/req 应与重入斜线一致；D 应约为 0。
   - 用 `sc` 模式核对 O 的 syscall 出现在对应的 stage：S2h 每请求约 +8，S4 约 +10。
6. **预测模型**：Δt_pred = Σ_i n_i/req（E7 `sc` 模式测得的次数）× c_i（E2 pinbench 中 loaded 状态的单价），与实测的 ladder 步进比较。实测/预测的比值作为"间接成本系数"报告，两个平台各一个，不画成柱子。
7. **噪声**：同一 session 前后两个 block 的各段估计在 CI 内一致。x86 上若某段的 CI 宽于 ±1 μs/req，在机动日加跑一个 10-rep block。
8. **P1**：S2m 顺序得到的 helper′ 和 map′，与主顺序相比，差 < max(0.5 μs/事件, 25%)。

第 2–5 条或第 8 条不通过时（见 G8），helper 和 map 合并成一根柱，或画成区间加交互残差，正文不声称分解精确。

---

## 5. 决策检查点（判据预先写定）

| # | 时间 | 判据 | 对论文叙事的影响与处理 |
|---|---|---|---|
| G0 | 11-25 / 11-27 | E1 的 (a)–(g)，按 TOST | **(a) 不过**：stdout 改回 stock loader，每个 round、每个 leg、每个 payload 插 3 个独立的 cnt 审计 rep。E4 和 E6 约 ×1.3，砍掉 E6 的 S2m 和 E4 的 Opc@256K。<br>**(b) 不过**：放弃 Odiag，另建 L1/L2 真补丁构建。x86 当天完成；Jetson 在 11-28 夜间或 12-01 上午构建，不影响 12-02 的 E6。<br>**(c) 不过**：只作为敏感性行报告，headline 不变。<br>**(d) 不过**：查构建差异，论文改用 177a1dc 原样构建。<br>**(e) 不过**：hook 柱改用 worker task-clock/request 口径并加注；Jetson 改回 wrk `-t3`。<br>**(f) 不过**：转到 G6。<br>**(g) 不过**：修 harness，重新标定 τ，再进入 E4。 |
| **G1** | CP1 | x86 上同 rep 配对的交互项 Δx_pin(M) − Δx_pin(S)；pinbench 中 M 和单核两种 mask 的单价之比 | **x86 在 M 下 pin 也贵，且 official 在 x86 M 下的领先消失或接近 0**：说明"平台翻转"部分来自 harness。故事改为"结论取决于部署亲和性和平台 syscall 价格"，摘要第一句和 Figure 1 图注都要重写，S/U 并列报告。<br>**两种策略下 x86 的 pin 都便宜**：平台解释成立，补上 x86 裸机 pin 的数字（填 ATC 的 \todo{x86 pin ablation}）。 |
| G2 | CP1 | Jetson 在 S 下 official 是否仍落后 K（GMR 的 CI 上界 < 1） | 不再落后："Arm64 上会输"要明确限定为"在未钉核的部署下"，S 写进限制条件。 |
| G2b | CP1 | Jetson 上 C 与 S 的 output 段差异 > 0.5 μs/event | 跨 cluster 是一个独立因素：Figure 1 图注写明，C 的数字作为敏感性行进正文。 |
| **G3** | CP2 | Jetson 16 B 下 Dpc 对 K 的 GMR，CI 下界是否 > 1 | **CI 含 1**：摘要改为"checks 关闭时领先 X%，开启时与 kernel 持平"，主表两列并列。<br>**Dpc 显著慢于 K**：贡献收窄为"不开 checks 时的特化"，廉价的安全读写成 HotOS 的开放问题。冻结点之后不改代码。 |
| G4 | CP2 | D 在 16 B 的领先：Jetson 至少 +12%，x86 至少 +35% | 数字照实报告，**headline 不换配置也不换 commit**（M#12）。可在机动日做一次 5567f2a2 对冻结点的 16 B 诊断（构建和 15 min 测量算在缓冲里），只用于解释原因。 |
| G5 | 每次审计 | K、D 或 Dpc 违反 2.0.4 的"无未报告丢失"判据（τ 已在 E1 标定） | "delivers or reports every event"不成立。写 bug 报告交给 Jetson 会话，修复落在从 9ccda7e8 拉出的分支上，重新冻结，D leg 全部重跑。11-30 之后才发现的，论断限定到实测范围，或写进限制条件。K 自己越界时，说明是 harness 的问题。 |
| G6 | CP2 | O 在 16 B 满载下 del_over_exp < 0.995 | 16 B 也不再是等工作量。所有 official 数字都并列 del_over_exp，摘要改成"even at 16 B it drops X%"，Figure 1 中 official 的 output 柱标"低估"。 |
| G7 | CP2 | O 在 256 KB 满载下按字节计交付 ≥ 90% | "约一半、不到三分之一"按实测数字改写。 |
| G8 | CP3 | 3.5 节第 2–5 条 | **不通过**：helper 和 map 合并成一根柱，或画成区间加交互柱；跨 session 偏差写进图注；只做同一 tracer 内部的分段。<br>**Jetson 上 official 的 (helper+output) ≤ hook 节省**：改写"spends it again"一句。 |
| G9 | CP4 | 各 session 中 O 和 D 对 K 的 16 B GMR，彼此相差 > 3 pp | 机动日加 R4，报告所有 session 的范围，不挑 session。 |
| G10 | CP2 | `sc` 模式实测的每请求 syscall 数与约 18 相差 > 10%，或 D 明显 > 0 | 先查清来源（握手、WANT_READ 带来的额外 hit，用 run_cnt 佐证），再按实测数改写 C5。 |
| G11 | CP2 | hits/request 偏离 4，或不变性判据不过 | 所有 per-hit 说法改用实测的 hits；不变性不过时，bpftime 不报 per-hit。per-event 结论不受影响。 |
| G12 | CP1 | E0 显示 arm64 的 probe 点首条指令是 nop（可被 6.13 simulate） | 把内核版本升级为显著混杂因素：写进 threats，请用户决定是否在场时测一次 6.13+ 内核，或者批准云端测量。是 paciasp、bti 或 stp 时只写一句"6.13 只会让 cache flush 变快"。 |
| G13 | 11-26（Jetson E12） | D 少于 17/18（get_stack_id 除外） | 在 CP2 之前修复，或者把"18 tools exact"改写为实测比例并列出原因。 |
| G14（P1） | E9 | x86 在 1.7 GHz 下 official 的领先缩小到接近 Jetson 的水平 | 说明主因是时钟而不是 ISA 或微架构，改写"为什么平台起决定作用"一段。 |
| G15 | CP2 | Jetson E3 中 D 某个 map 行比历史的 518a6b99 慢 > 10% | 在冻结点能动之前查清；属实就按 G5 的流程处理。 |

---

## Known x86 implementation notes (2026-11-23)
- The x86 session script moves only this Claude session's process tree to CPUs 12-14; other idle Claude sessions on the NUC are left alone and their CPU use is recorded via top-before.txt.
- plymouthd is not moved (deviation from 2.0.1; recorded).
- nginx expected work comes from access.log (log_format paper: $status $bytes_sent $body_bytes_sent).
