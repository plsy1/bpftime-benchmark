# BPFtime official-no-btf `ssl-nginx` 路径消融：各执行路径的吞吐量成本分解

## 技术结论

本报告基于 `benchmark-results/` 下 2026-07-24 至 2026-07-26 的四组路径消融实验（V1 empty-probe → V4 full-aligned），把 BPFtime `sslsniff` 造成的端到端吞吐量损失分解到四段执行路径。V4 使用包含 software perf record 8 字节对齐修复的 runtime，两轮共 20 个样本，不受此前的未对齐 bug 影响。

以"相对同轮 baseline 的吞吐量损失（百分点，pp）"计，七种 payload 的均值：

| 阶段 | BPFtime 成本 | kernel eBPF 成本 | BPFtime − kernel |
|---|---:|---:|---:|
| ① probe 触发/分发（V1） | +8.25 pp | +16.64 pp | **−8.39 pp（优势）** |
| ② helper / map / metadata（V2−V1） | +6.49 pp | +5.45 pp | +1.04 pp（持平） |
| ③ `bpf_probe_read_user`（V3−V2） | +3.65 pp | −0.52 pp | +4.17 pp |
| ④ **event output（V4−V3）** | **+17.52 pp** | +9.10 pp | **+8.43 pp（最大劣势）** |

核心结论：

1. **最重的路径是 ④ event output**（`bpf_perf_event_output` + perf buffer 传输 + userspace 消费者）。BPFtime 总损失约 36 pp，这一段独占 +17.5 pp，接近另外三段之和；16KB 时最严重（+19.7 pp，比 kernel 同段贵 12.6 pp）。
2. **④ 也是 BPFtime 输给 kernel 的地方**。①段 BPFtime 有结构性优势（用户态 uprobe 免内核陷入，比 kernel 便宜 8.4 pp），②段与 kernel 持平；但 ③+④ 合计比 kernel 贵约 12.6 pp，把 ①段优势全部抵消并倒亏，解释了 aligned V4 在全部 payload 上落后 kernel 3.8%～12.6% 的现状。
3. **③ 是次要目标**：kernel 的 probe_read 在吞吐量上几乎免费，BPFtime 有真实成本，且随 payload 增大而加重（256KB 时 +5.29 pp）。
4. 粗略推算：若 ④ 段成本压到 kernel 同水平（约 9 pp），BPFtime 总损失将从约 36% 降到约 27%，反超 kernel（kernel 总损失约 31%）。

## 实验与数据来源

四个消融版本通过注释 `sslsniff.bpf.c` 中的路径实现，具体差异见 `benchmark-results/README.md`。每个 run 为一次完整 `draw_figture.py`：7 种 payload × 3 种模式（baseline / kernel sslsniff / BPFtime sslsniff）× 10 次 `wrk`（100 连接、10 秒），共 210 个吞吐量样本。

| 版本 | 恢复的路径 | 数据目录 | 样本数/模式/payload |
|---|---|---|---:|
| V1 empty-probe | 仅 probe 触发，程序体直接返回 0 | `benchmark-results/empty-probe/run01/` | 10 |
| V2 no-copy_no-output | + metadata helper、map 操作、事件准备 | `benchmark-results/no-copy_no-output/run01/` | 10 |
| V3 no-output | + `bpf_probe_read_user` | `benchmark-results/no-output/run01/` | 10 |
| V4 full-aligned | + `bpf_perf_event_output`（完整路径，对齐修复后 runtime） | `benchmark-results/full-aligned/run01/`、`run02/` | 20（两轮合并） |

说明：

- `full-aligned/run02/`（2026-07-26 16:49–17:35 JST 执行，日志 `bpftime-official-no-btf-ssl-20260726_164952.log`）原始结果由容器 `bpftime-official-no-btf` 内 `/bpftime/benchmark/ssl-nginx/` 归档而来，含 7 个 `benchmark_results_20260726_*.json`、`size_benchmark_20260726_083556.{json,txt}` 与 `absolute_performance.png`。
- V2 目录没有单独的 `benchmark_results_*.json`，本文数值从 `size_benchmark_20260724_171625.txt` 的 raw output 解析全部 210 个样本得到（16B 组均值 baseline 16552.9 / kernel 12946.0 / BPFtime 13676.8，可对照原文件核验）。
- `full-unaligned/run01,run02` 为未对齐 runtime 的异常对照（BPFtime CV 8.73%～16.58%、双峰），不参与本文任何成本计算。
- 各目录中时间戳为 `20260711_081448` 的 JSON 属于更早的短测，未纳入。

## 计算方法

```text
Throughput impact = (Baseline RPS − Mode RPS) / Baseline RPS × 100%
Stage cost       = Impact(Vn) − Impact(Vn−1)        （单位 pp）
```

每个版本相对**同轮** baseline 计算 impact，以消除跨日环境漂移；相邻版本 impact 差值作为该段路径的吞吐量成本。该方法假设各段成本近似可加。跨 run 噪声约 ±2～3 pp——kernel 在 2KB/128KB 的 ③ 段出现 −2.8/−4.5 pp 的小负值即为噪声水平的体现（kernel 在 V2→V3 间执行的工作相同，理论差值为 0）。因此 ②③ 之间的细微排序不应过度解读；④ 的 +17.5 pp 远超噪声，结论稳固。

## 各版本相对 baseline 的吞吐量损失

BPFtime | kernel，单位 %：

| Payload | V1 | V2 | V3 | V4 aligned |
|---|---:|---:|---:|---:|
| 16B | 9.71 \| 16.70 | 17.38 \| 21.79 | 19.83 \| 22.62 | 37.64 \| 31.93 |
| 1KB | 9.58 \| 19.95 | 16.47 \| 22.42 | 19.10 \| 22.92 | 36.59 \| 31.53 |
| 2KB | 9.56 \| 15.28 | 15.77 \| 24.11 | 19.00 \| 21.29 | 35.15 \| 30.89 |
| 4KB | 8.66 \| 14.91 | 14.64 \| 19.34 | 18.55 \| 21.62 | 34.11 \| 30.75 |
| 16KB | 7.14 \| 15.22 | 13.62 \| 21.58 | 17.85 \| 21.70 | 37.55 \| 28.81 |
| 128KB | 7.09 \| 16.20 | 13.45 \| 24.86 | 17.25 \| 20.40 | 34.94 \| 31.62 |
| 256KB | 6.05 \| 18.23 | 11.87 \| 20.55 | 17.16 \| 20.48 | 35.43 \| 29.18 |

## 分阶段成本（逐 payload）

BPFtime 各段成本（pp），括号内为 BPFtime − kernel 差值：

| Payload | ① probe（V1） | ② helper/map（V2−V1） | ③ 明文复制（V3−V2） | ④ event output（V4−V3） |
|---|---:|---:|---:|---:|
| 16B | +9.71（−6.99） | +7.67（+2.58） | +2.46（+1.63） | **+17.81（+8.49）** |
| 1KB | +9.58（−10.37） | +6.89（+4.41） | +2.63（+2.14） | **+17.49（+8.88）** |
| 2KB | +9.56（−5.73） | +6.21（−2.61） | +3.23（+6.04） | **+16.15（+6.56）** |
| 4KB | +8.66（−6.26） | +5.98（+1.56） | +3.91（+1.63） | **+15.56（+6.44）** |
| 16KB | +7.14（−8.08） | +6.48（+0.13） | +4.23（+4.10） | **+19.70（+12.59）** |
| 128KB | +7.09（−9.11） | +6.37（−2.30） | +3.80（+8.27） | **+17.69（+6.47）** |
| 256KB | +6.05（−12.19） | +5.83（+3.51） | +5.29（+5.35） | **+18.27（+9.58）** |

绝对吞吐对照（BPFtime 均值 RPS，V4 列附同轮 kernel）：

| Payload | V1 | V2 | V3 | V4 aligned | V4 时的 kernel |
|---|---:|---:|---:|---:|---:|
| 16B | 14790 | 13677 | 13229 | 10315 | 11259 |
| 1KB | 14324 | 13234 | 12875 | 10079 | 10884 |
| 2KB | 13657 | 12716 | 12273 | 9813 | 10457 |
| 4KB | 13099 | 12145 | 11677 | 9442 | 9924 |
| 16KB | 9194 | 8526 | 8145 | 6193 | 7060 |
| 128KB | 2737 | 2579 | 2446 | 1930 | 2029 |
| 256KB | 1534 | 1444 | 1366 | 1062 | 1164 |

值得注意：V1～V3 下 BPFtime 全部 payload 快于 kernel（V3 仍保有 +2.9%～+5.0% 优势），性能翻转恰好发生在恢复 ④ 之后。

## Aligned V4 详情（run01 + run02 合并，20 样本/模式）

| Payload | Baseline | Kernel | BPFtime | BPFtime vs kernel | kernel CV | BPFtime CV |
|---|---:|---:|---:|---:|---:|---:|
| 16B | 16541.8 | 11259.4 | 10315.3 | −8.39% | 1.05% | 2.11% |
| 1KB | 15896.1 | 10883.9 | 10079.1 | −7.39% | 1.69% | 3.46% |
| 2KB | 15131.0 | 10457.0 | 9812.7 | −6.16% | 1.24% | 2.13% |
| 4KB | 14330.2 | 9924.3 | 9441.8 | −4.86% | 0.57% | 3.76% |
| 16KB | 9916.6 | 7060.0 | 6193.3 | −12.28% | 0.85% | 3.96% |
| 128KB | 2966.6 | 2028.7 | 1930.0 | −4.86% | 3.13% | 4.37% |
| 256KB | 1644.2 | 1164.5 | 1061.7 | −8.82% | 1.75% | 4.47% |

两轮之间方向与幅度一致（例如 256KB：run01 −10.34%、run02 −7.27%），也与 2026-07-22 仅加对齐修复的原始配置全量跑（`old/latest/full/original-only-alignment-20260722_150356/`，−4.46%～−11.00%）吻合，可信度高。

稳定性：对齐修复后未对齐 V4 的双峰（CV 8.73%～16.58%）消失，BPFtime CV 降到 1.2%～5.6%（逐轮）；但 BPFtime 的 CV 仍系统性高于 kernel（约 2～4 倍），且存在偶发离群低值样本（如 run01 4KB 的 8272、run02 1KB 的 8766、run02 256KB 的 938 RPS）。这是残余的次级不稳定性，与双峰不同量级，暂未定位来源。

## 平台差异：为什么 x64 上 BPFtime 全面快 20–30%，Jetson 上反而慢

x64 上 BPFtime 对 kernel eBPF 全 payload 快 20–30%（另见 07-22 的 x64 对照：BPFtime
的 nginx tracing delta 仅为 kernel 的 0.49–0.57×），与 Jetson 的结果方向相反。
结合本文分解与 2026-07-27 在本机的 syscall 微基准，解释如下：

| 操作（Jetson Orin 实测，无竞争紧循环） | 耗时 |
|---|---:|
| 裸 syscall（getpid） | 284.6 ns |
| `sched_getcpu`（glibc 经 rseq，纯内存读） | 3.5 ns |
| `sched_getaffinity` | 332.4 ns |
| 完整 affinity 序列（getcpu + getaffinity + 绑核 + 恢复） | **3,143 ns** |

1. BPFtime 的结构性优势（免陷入用户态 hook 替代 kernel uprobe trap）在两个平台
   都成立：Jetson 上 ① 段 BPFtime 仍比 kernel 便宜 8.4 pp。
2. 翻转来自 ④ 段的实现选择：`bpf_perf_event_output` 用 affinity 绑核模拟
   kernel"关抢占执行 per-CPU 逻辑"的语义。单次 `sched_setaffinity` 在本机约
   1.4 µs（普通 syscall 的 5 倍），序列无竞争 3.1 µs、负载下实测 6.7 µs；
   2 事件/请求即每请求 6–13 µs 硬成本。同一代码在 x64 上（setaffinity 数百 ns、
   基线吞吐更高）只是零头。
3. ③ 段 `bpf_probe_read_user`（+4.2 pp 对 kernel）为次要因素，受 Jetson
   内存子系统限制在大 payload 下放大。
4. 可检验预言：`v4-ablation` 的 no-affinity 变体应能收回 ④ 段对 kernel
   8.4 pp 劣势的大部分。`sched_getcpu` 仅 3.5 ns（rseq），且 ring 写入为
   per-thread shard、不依赖绑核保证正确性，该序列在此路径上很可能整体冗余；
   若验证成立，ARM 上有望恢复与 x64 一致的领先格局。

### x64 实测（GitHub 托管 runner，stock 全局挂载口径）

有效数据点：commit `fe47c3b`（**字节对齐修复之后、绑核序列仍在**）的完整
draw_figture.py 一轮：

| Payload | Baseline | Kernel | BPFtime | BPFtime vs kernel | kernel imp | BPFtime imp |
|---|---:|---:|---:|---:|---:|---:|
| 16B | 22553 | 10023 | 15852 | **+58.2%** | 55.6% | 29.7% |
| 1KB | 21890 | 9909 | 15815 | +59.6% | 54.7% | 27.8% |
| 2KB | 21282 | 9936 | 15262 | +53.6% | 53.3% | 28.3% |
| 4KB | 20266 | 9593 | 14539 | +51.6% | 52.7% | 28.3% |
| 16KB | 15310 | 6476 | 10615 | +63.9% | 57.7% | 30.7% |
| 128KB | 4424 | 2158 | 3337 | +54.6% | 51.2% | 24.6% |
| 256KB | 2699 | 1213 | 2066 | **+70.3%** | 55.1% | 23.5% |

判读：

1. **绑核序列在 x64 上不是瓶颈**：绑核未删时 x64 已全 payload 领先 kernel
   +52%～+70%。与微基准（x64 上 sched_setaffinity 仅数百 ns）及"性能翻转仅发生
   在 ARM"的定位完全自洽——删除绑核的收益集中在 syscall 昂贵的平台（Jetson：
   fair 口径 −9.6% → +7.6%）。删除对 x64 的净效应未实测，预期 2–4 pp 量级。
2. 数据甄别教训：Actions 历史里 07-23/07-24 的 x64 run checkout 的分支 HEAD
   当时携带 **V2/V3 消融版 sslsniff**（d28ca42/73e9b78），两腿 impact 均偏低
   （bpftime 11–16%、kernel 33–35%），不可作为完整 benchmark 对照——引用历史
   run 前必须核对该 run 实际 checkout 的 commit 与 sslsniff 状态。
3. 托管 runner 硬件抽签真实存在（各 run baseline 22.2k–31.3k），跨 run 的
   impact 数值对比不可靠；只有同一 run 内的三腿对比（同 VM 同 session）有效。
4. "trap 计价 vs 代码计价"框架：x64 完整程序下 kernel uprobe impact 55.6%
   （虚拟化放大陷入成本）vs Jetson 本地稳定 29–32%，方向支持"平台差异主要在
   kernel 一侧"；bpftime impact（x64 该 run 23.5–30.7% vs Jetson ~25%）量级
   接近，但因 x64 仅一轮有效数据且含绑核，定量的跨平台不变性**不下结论**。

## ④ 段内部消融：短测结果（2026-07-27，预注册后执行）

方法：单 payload 短测（benchmark.py 一次 = baseline/kernel/bpftime × 10 wrk），
16B 批次共 7 run（original 首尾 bookend、no-affinity 两轮、其余变体各一轮），
判定阈值跑前预注册于 `benchmark-results/v4-ablation/short/PREREGISTRATION.md`。
主统计口径为中位数。批次有效性：bookend 漂移 1.00pp（≤2pp ✓），
kernel 腿 7 run 稳定于 11161–11341 RPS。

### 16B 分解（full 合并 impact 37.10%）

| 变体 | bpftime 中位数 RPS | vs kernel | 相对 full 收回 | 结论 |
|---|---:|---:|---:|---|
| original ×2 | 10407 / 10249 | −8.0% / −9.6% | — | 参照组 |
| **no-affinity ×2** | **12402 / 12408** | **+9.8% / +10.5%** | **+12.59 pp** | **affinity 序列 = ④ 段成本的 72%** |
| no-mapops | 10427 | −6.8% | +0.59 pp | map 查询/lookup 近乎免费 |
| no-ringwrite | 10841 | −2.9% | +3.06 pp | ring 写入（+少量消费者）≈18% |
| noop-helper | 13131 | +17.2% | +17.51 pp | helper 全部成本 |

交叉验证：noop 收回的 17.51 pp 与完整消融 V4−V3 的 17.5 pp 吻合；
可加性 12.59+0.59+3.06=16.24 pp vs 17.51 pp（残差 1.3 pp，噪声内）；
affinity 占比 72% 与 07-22 插桩计时的 70.9% 吻合；
noop 绝对值 13131 ≈ 历史 V3 的 13229（helper 分发成本 ≈0）。

### 有效性审计（original 与 no-affinity 各一次独立审计腿）

- **事件守恒 ✓**：两者均 2.001 事件/请求、零丢失（no-affinity 收 248,082 个事件
  vs 期望 247,884，多出部分为 handshake），吞吐提升不是靠静默丢事件。
- **消费者不占核**：消费者 CPU 仅 6–7%（0.61s/0.72s per 10s 窗口）。
- **迁移风暴假设排除**：两配置下 nginx worker `nr_migrations` 均为 **0**；
  affinity 序列的伤害是纯 syscall 序列化成本压在单 worker（`worker_processes 1`，
  实测 ~90% 饱和）关键路径上——每请求 2×3.1µs（无竞争）与消融值量级闭合。
- CV 收敛预言命中：original 5.14%/5.17%（含 MAD 离群 9013/9402）→
  no-affinity 1.14%/2.68%，收敛至 kernel 水平。离群低值与 pin 代码的两个
  已知 bug（错误路径不恢复 affinity 导致线程被钉死单核、restore 竞态覆盖
  应用 mask）高度相关，预期随修复一并消解。

### 256KB 补验（payload 无关性）

| 变体 | bpftime 中位数 | vs kernel | 收回 |
|---|---:|---:|---:|
| original | 1046.3 | −12.4% | — |
| no-affinity | 1318.7 | **+10.1%** | **+16.00 pp** |

256KB 收回幅度更大（每请求 SSL_write 事件数随 payload 增多，affinity 按事件计费）。

### 判定

预注册最高档命中且跨 payload 成立：**`bpf_perf_event_output` 中的
affinity 绑核序列是 Jetson/ARM 上 BPFtime 输给 kernel eBPF 的主因，
且在此路径上功能冗余（事件零丢失、零迁移、per-thread shard 无需绑核）。
去掉后 BPFtime 在 16B 与 256KB 均反超 kernel 约 10%，方差同步收敛**——
与 x64 上 BPFtime 全面领先 20–30% 的格局方向一致。

## Fair 口径验证（修复后，发表级主口径，2026-07-27）

方法：`v4-ablation/fair-test.sh`——kernel sslsniff 以 `-p <nginx worker PID>`
等范围挂载（与 bpftime 只注入 nginx 对等），三腿**逐 rep 交错**
（baseline → kernel-fair → bpftime，每 rep 各自重启进程），每 payload 3 轮 × 10 reps
= 30 样本/腿。被测 runtime 为已提交的修复版（commit `076e3e4`）。
数据：`benchmark-results/v4-ablation/fair/fixed/`（16B: run02/05/06；256KB: run01–03）。

| Payload | baseline 中位 | kernel-fair 中位 | bpftime 中位 | **bpftime vs kernel-fair** | 各轮方向 |
|---|---:|---:|---:|---:|---|
| 16B | 16558 | 11398 | 12263 | **+7.58%**（均值口径 +6.50%） | +6.94 / +8.45 / +6.98，3/3 一致 |
| 256KB | 1699.7 | 1182.0 | 1297.9 | **+9.81%**（均值口径 +8.17%） | +8.87 / +8.87 / +11.95，3/3 一致 |

判定：

1. **等范围口径下翻转成立**：修复前 fair 差距 −9.57%（07-22，16B）→ 修复后
   **+7.58%**，净改善约 17 pp；256KB 同向 +9.81%。六轮全部同方向，无一例外。
   "BPFtime 在 ARM 上反超 kernel eBPF"的结论不再依赖挂载范围不对等的口径。
2. 全局挂载对 kernel 的额外负担实测很小（fair kernel 11398 vs stock kernel
   ~11259，约 1%），历史 stock 口径的结论未被显著放水。
3. **残余离群未根除**：60 个 bpftime 样本中 4 个 MAD 离群（约 −10%～−15%，如
   16B 的 10501/9892），pooled CV 4.5%（16B）/6.9%（256KB），仍高于 kernel 的
   1.5%/2.6%。绑核删除消掉了持续性高方差，但偶发低值另有来源（待查，见 P3）；
   中位数口径下不影响上述判定。

## 结论边界与下一步

本方法测的是端到端吞吐量消融差值，不是函数级 CPU 时间；各段成本近似可加，噪声约 ±2～3 pp。

下一步（2026-07-27 经三视角评审修订；④ 段内部消融已完成，见上节）：

1. **P1 正式修复**：在 `bpf_perf_event_output` 中无条件删除 affinity 绑核序列
   （cpu 值保留现有 `my_sched_getcpu()` 快照即可，本机实测 3.5 ns/rseq）。
   正确性论证按三条路径写全：(a) userspace PERF_EVENT_ARRAY——lookup key 是
   快照 cpu、写入走 per-(pid,tid) shard，与 pin 无关；(b) KERNEL_USER 共享路径
   ——cpu 归属由 kernel 侧 transporter 以 BPF_F_CURRENT_CPU 决定，用户态 pin 无效；
   (c) per-CPU map 早已是纯快照实现。顺带修掉 pin 代码的两个既有 bug
   （`bpf_helper.cpp:520/:532` 错误路径不恢复 affinity；restore 使用陈旧 mask
   与应用自身 setaffinity 竞态）——修复定性为 bugfix + perf。
2. **P1 验证口径**：fair（kernel 只挂 nginx worker）为主结论口径（对标 07-22
   fair 差距 −9.57% 的收敛），stock 全局挂载仅作历史衔接附录；修复版 ≥3 轮
   完整 benchmark、与 kernel/baseline 同 session 交错；事件守恒数字随吞吐归档；
   验证 no-affinity 短测的 CV 收敛在修复版上复现（离群问题应随之消解）。
3. **上游 PR 切分**：PR#1 仅去 pin（三路径论证 + 两个 bug 说明 + ARM/x64 微基准
   + 消融/短测吞吐证据 + 迁移计数为零的数据）；flags/BPF_F_INDEX_MASK 被忽略的
   问题独立成 PR#2 或 issue；ring 满静默丢弃 + 无 PERF_RECORD_LOST 单独开 issue
   （属功能缺失，不混入去 pin diff）。动手前先 diff 上游 master 确认 shard 结构
   同构（本仓库含本地修改）。
4. **P2（降级为代码走读）**：③ 段 `bpf_probe_read_user` 的 +3.7 pp 效应与噪声
   同量级且仅一轮数据，先补跑 V2/V3 各一轮确认跨轮复现，再决定是否投入实现调查。
5. **待补对照**：x64 机器上的同款 affinity 微基准（分钟级，替换文中推断值）；
   修复验证阶段可在 x64 补一轮 no-affinity vs full 差分（预期 ≈0）作对照证据。

## 更新记录

- 2026-07-27：**Fair 口径验证完成**（3 轮 × 10 reps × 2 payload，逐 rep 交错）：修复版 bpftime 在等范围挂载下 16B **+7.58%**、256KB **+9.81%** 反超 kernel，六轮全同向；修复前 fair 差距为 −9.57%，净改善 ~17pp。残余离群（60 样本中 4 个，−10~−15%）未根除，列入 P3。数据 `benchmark-results/v4-ablation/fair/fixed/`。

- 2026-07-27：**正式修复已提交**：`076e3e4` "Remove per-event CPU affinity pinning from bpf_perf_event_output"（分支 codex/official-no-btf；同时 `99adf3a` 把 `sslsniff.bpf.c` 恢复为上游 c796f45 干净完整版——注意此前 HEAD 里提交的是 V2 消融版）。修复验证：单元测试 8,358 断言全过；16B 短测 bpftime 中位数 12,329 RPS、**反超 kernel +8.8%**、CV 1.22%，与 no-affinity 变体一致（数据 `benchmark-results/v4-ablation/short/fixed/16b-run01/`）。注意：修复后 worktree 不再匹配 `v4-ablation/original/` 与 `v1-v4-ablation/v4-full/` 的冻结副本（实验历史产物），如需再做消融应以修复后代码为新基线重建参照。
- 2026-07-27：新增独立解释文档 `bpftime-perf-event-output-affinity-redundancy-20260727.md`：绑核为何冗余（快照先于绑核 / per-thread shard / 绑核非锁 / KERNEL_USER 路径无效）、两个既有 bug（错误路径钉死单核、陈旧 mask 竞态）、删除后的行为边界。
- 2026-07-27：完成 ④ 段内部消融短测（16B 7-run 预注册批次 + 事件守恒审计 + 256KB 补验）：affinity 绑核序列 = ④ 段成本的 72%（+12.6 pp），功能冗余；去掉后 BPFtime 在 16B/256KB 均反超 kernel 约 10%，CV 收敛至 kernel 水平。"下一步"整节按三视角评审意见改写（P1 正式修复 + fair 主口径 + 上游 PR 切分）。原始数据：`benchmark-results/v4-ablation/short/`。

- 2026-07-26：首版。归档 `full-aligned/run02/`（自容器 `bpftime-official-no-btf` 拷出），完成 V1–V4 四阶段成本分解。
- 2026-07-27：④ 段内部消融的 runtime 变体已备好并预编译：`bpftime-offical-no-btf/v4-ablation/`（no-affinity / no-mapops / no-ringwrite / noop-helper，各含 `bpf_helper.cpp` 副本与 agent `.so`，用法见该目录 README）。代码走读确认 ④ 段无 wakeup 通知路径（消费者纯轮询），ring 写入走 per-thread producer shard。结果约定存 `benchmark-results/v4-ablation/<variant>/runXX/`，待跑。
- 2026-07-27：新增"平台差异"一节：syscall 微基准定位 x64/ARM 翻转的主因为 `sched_setaffinity`（本机单次 ~1.4 µs，affinity 序列无竞争 3.1 µs/事件），并给出 no-affinity 变体的可检验预言。
- 2026-07-27：V1–V4 的 `sslsniff.bpf.c` 变体也已归档并预编译：`bpftime-offical-no-btf/v1-v4-ablation/`（V2 = git `d28ca42`、V3 = git `73e9b78`、V4 = worktree pristine、V1 为按定义重构——当时源码未提交，续跑前建议 16B smoke 对照 empty-probe/run01 ≈14.8k RPS）。每个变体目录可整体 mount 顶掉容器内 sslsniff 目录，见该目录 README。
