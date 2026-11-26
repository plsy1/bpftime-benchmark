# PREREG-2 — ATC 2027 评测增补预注册（x86 NUC，写于 2026-11-25，C1 之前）

本文件补充 PREREGISTRATION.md（2026-11-23 冻结，下称 PR），不修改它。与 PR 冲突处以本文件为准，并在结果仓库 README 的 Deviations 段登记。
来源：paper-drafts/atc-2027/notes/eval-plan.md §1.3、§2.0、§2.1 C1/C2。写完后计算 sha256，记入 PREREG-2.sha256，之后不改；如需改动，另写 PREREG-2a 并说明原因。

## 0. 偏离 PR 的地方（审稿意见驱动）
- PR:125（G4/M#12）规定 headline 不换配置也不换 commit。本轮换：Tern 用 c71a9987（Dn/Dnpc，已在 E18 测过），基线加入上游 head 70f7bc1f 的补丁变体。原因：模拟审稿 W1（Tern ≈ 上游 + 三个补丁）只能用这些变体回答。
- PR:130（G9）"报告所有 session，不挑 session" 继续有效：C1、C2 以及任何作废重跑都列在附录。

## 1. 新 build（全部 RelWithDebInfo、LTO、gcc-13、clang-19，选项同 PR 的冻结 build）
| 键 | 源 | 内容 |
|---|---|---|
| OlL3 / OlL3pc | ladder/oll3-70f7bc1f 81986a3f | 70f7bc1f + be93dd87（uid/gid 缓存，agent.cpp 一处改名冲突手工解决）；checks off / on |
| Ol730x | ladder/ol730x-70f7bc1f cd00d95f | 70f7bc1f 去掉 #730（c778e574）的每 CPU 执行锁；platform_utils.cpp 恢复到 #730 之前的 blob 08f0f07a，bpftime_prog.cpp / bpftime_shm_internal.cpp 的删除行与 c778e574 的新增行逐行互逆（只多留了几行 include 和空 namespace 括号） |
| OlL3x730 / OlL3x730pc | ladder/oll3x730-70f7bc1f 3cbd9fd5 | Ol730x + be93dd87 |
| OlL3x730R | 同 OlL3x730 | 布局对照：同源，加 `-falign-functions=64` |
| Dn730 | ladder/dn730-c71a9987 | c71a9987 + `git revert 24999e3c`（恢复上游的每 CPU 执行锁），无冲突 |
| DnR | dnew-c71a9987 c71a9987 | 布局对照：与 Dn 同源，加 `-falign-functions=64` |

偏离计划：eval-plan 写的布局对照是"换链接顺序并加 -falign-functions=64"。本次只加 `-falign-functions=64`，不换链接顺序（LTO 下链接顺序对最终布局的影响不可控，也无法复现地描述）。布局对照的用途不变：估计"同语义不同 build"的系统差。

## 2. 主对比（C2 确认性；C1 只是探索性，数字不进论文）
| 编号 | 对比 | 含义 |
|---|---|---|
| M1 | Dnpc / Kb | checked Tern 默认配置 |
| M2 | Dxn+EXACT / Kb | exact + checked |
| M3 | Dnpc/NAP0-WB32 / Kb | 同一通知策略 |
| M4 | Dnpc/NAP0 / K | 都是每样本唤醒 |
| M5 | Dn vs OlL3x730 | W1 等价检验 |
| M6 | Dn730 vs OlL3 | 同等正确性（都持锁） |

记法：K = 内核 sslsniff；Kb = `K+SSLSNIFF_PB_WAKEUP=32`；/NAP0 = `+BPFTIME_CONSUMER_NAP_US=0`；/NAP0-WB32 = `+BPFTIME_CONSUMER_NAP_US=0+SSLSNIFF_PB_WAKEUP=32`。

- 优越性：Holm 校正（每平台 6 项一族）。
- 等价：逐项 TOST（90% CI），交并检验；报告做了几项。
- 其余对比一律标"探索性、未校正 CI"。

## 3. Margin
- 每事件开销 x（µs/event）定义：x = (1/rps_leg − 1/rps_B) × 10^6 / 2（16 B 下每请求 2 个事件）。leg 间差 Δx = (1/rps_a − 1/rps_b) × 10^6 / 2，与 B 无关。
- x86：±1.5%，按 E18 Dn 中位数 89,614 rps 折算为 ±0.084 µs/event。Orin：±2%。
- M5 只用 µs 判据：配对差 Δx 的 90% CI 落在 ±0.084 µs/event 内即等价（x86）。

## 4. 样本量、配对、预热
- x86 n = 20（2 round × 10 rep），Orin n = 40。不再按 PR:59 扩样。
- 每个 rep 内 legs 用 seed = sha256(session:exp:round:rep) 随机排列（run_paper.py 已实现）。配对单位 = (round, rep)。
- 每个会话开头 1 个预热 rep（`--warmup 1`，行内 warmup=true），不进分析；另附"去掉每 round 第一个正式 rep"的敏感性表。
- 统计：配对几何均值比（GMR）+ 95% t-CI；差值用配对均值 + t-CI。

## 5. 残留与漂移
- 残留：对每个 leg，x 对 pred_leg 做回归（固定效应），报告 F 检验；p < 0.01 时在正文注明。
- C2 的两个 round 放在两个夜里，检验 leg × night 交互；CI 是会话内的，正文写明。

## 6. H1 计量与标记阈值（事先写定）
- 每行记录：全 16 核 busy（busy_all）、nginx worker 和 sniff 的 schedstat ns、worker 的 nr_migrations、被测核的中断总数和 iwlwifi 中断增量、worker/sniff smaps_rollup、/dev/shm/bpftime* 大小。
- 非被测核 = {0,1,3,5,7,15}（12–14 放 harness 和 Claude，不算）。任一非被测核 busy > 5% 的行标记 `noisy_core`；主结果照常包含，附录给去掉标记行的敏感性结果。阈值不随结果调整。
- 作废规则仍是 PR 2.0.5（只用客观条件，不按数值剔除）。

## 7. C1 预测（A6，写在 C1 之前）
- 单位：每事件。上游 head 在 S4 每请求 getuid 6、getgid 6、getpid 2（E17），即每事件 6 次凭据调用 + 1 次 getpid。OlL3 去掉凭据调用，保留每事件 1 次 getpid；Tern 为 0。
- 预测 P1：Dn 与 OlL3x730 之差 ≈ 1 次 getpid 的在位单价。pinbench 空闲单价 87 ns（E2）；在位单价按 E18 的 pid/tid 步（0.190 µs/event，getpid+gettid）估为约 0.1 µs/event。与 ±0.084 µs margin 同量级，所以 M5 可能"既不等价也不显著"。这种情况论文写"剩余差距是上游输出路径上每事件 1 次 getpid"。
- 预测 P2：Ol → Ol730x 的 Δx ≈ E17 中上游 head 对 official 的 hook 阶段增量 +0.106 µs/event 量级（锁的价格），CI 宽。
- 预测 P3：Ol → OlL3 的 Δx ≈ E18 的 uid/gid 步 0.562 µs/event。
- 预测 P4：Dn730 − Dn ≈ Ol − Ol730x（锁的价格与基底无关）。
- 预测 P5：DnR / Dn 与 OlL3x730R / OlL3x730 都在 [0.99, 1.01] 内（布局差 ≤ 1%，参照 XF:160 的 1.0087）。

## 8. 决策规则（12-08）
- |Dn − OlL3x730| ≤ getpid 在位单价（0.1 µs/event）：论文定位为分析型，系统贡献只看 E7（HAProxy nbthread=4）和 E7b。
- Dn 比 OlL3x730 快超过 0.1 µs/event：用 perf sc 和计数器归因，再决定写法。
- Dn/NAP0-WB32 对 Kb 的比值即 Q3 要的"同一通知策略"数字，由 C2 确认。
- 布局对照超出 [0.99, 1.01]：M5/M6 的等价判据改为把布局差并入 margin 之前，先在附录报告，不改主判据。

## 9. C1 会话（x86）
- legs（12）：K、Kb、Ol、OlL3、Ol730x、OlL3x730、OlL3x730R、Dn、DnR、Dn730、Dn/NAP0、Dn/NAP0-WB32。16 B，policy M，10 s，100 连接。
- round 0：1 预热 + 10；round 1：10；round 2：perf sc，3 rep × 20 s，legs OlL3、Ol730x、OlL3x730、Dn730、Dn/NAP0-WB32（均 @S4）。
- 运行前 smoke（A10，不进分析）：Dn、Dn/NAP0、Dn/NAP0-WB32 各 1 rep × 20 s，perf 记 sslsniff 和 nginx worker 的 futex 调用；判据：NAP0 时每样本约 1 次 futex wake，NAP0-WB32 时约每 32 样本 1 次。
