# v4-ablation 16B 短测：预注册判定标准（2026-07-27，跑前写定）

## 实验序列（每个 = benchmark.py 一次：baseline/kernel/bpftime × 10 wrk@16B，约 7 分钟）

```
original → no-affinity → no-mapops → no-ringwrite → noop-helper → no-affinity(#2) → original(#2)
```

- original 首尾各一次（bookend）：两次 impact 差 >2pp 判定批内漂移，全批降级为"仅方向性"。
- no-affinity 两轮共 20 样本（承载核心预言）。
- 每 run 归档 env-snapshot（cpufreq/温度）。

## 统计口径

- 主指标：各腿 **中位数** RPS；均值与 CV 并列报告。
- impact = (baseline − mode) / baseline，同 session 三腿。
- 离群判据：|x − median| > 3×MAD，标记并出示剔除前后两版数字。

## 核心预言判定（full(original) − no-affinity 的 bpftime impact 差）

| 结果 | 判定 |
|---|---|
| 收回 ≥10pp | affinity 序列为 ④ 段绝对大头，整体冗余成立 |
| 收回 4–10pp | 部分成立；ring 写入/消费者份额显著，需结合 no-ringwrite 分解 |
| 收回 <4pp | 预言失败；按序检验备择假设：(a) 消费者轮询占核（事件审计的 CPU 数据）、(b) 迁移风暴非 pin 所致、(c) 其他 |

次要预注册观察：若 pin 引发的迁移/陈旧 mask 竞态是离群低值来源，
no-affinity 的 bpftime CV 应从 2–4% 收敛向 kernel 水平（~1%）。

## 有效性门槛

1. **事件守恒审计**（批后对 original 与 no-affinity 各做一次独立审计腿）：
   sslsniff 收到事件数 ≈ 2 × wrk 完成请求数（±收尾误差）。不守恒的变体，
   其吞吐数字不承认（防"靠静默丢事件换吞吐"）。同时采集消费者 CPU 占用
   与 nginx worker 迁移计数（/proc/PID/sched nr_migrations）。
2. kernel 腿为 stock 全局挂载（PID -1，含 wrk 追踪）——与历史数据同口径，
   仅作参考系；变体间差分只用 bpftime 腿，不受此影响。
3. 短测结论仅覆盖 16B；payload 无关性由后续 256KB 补验。
