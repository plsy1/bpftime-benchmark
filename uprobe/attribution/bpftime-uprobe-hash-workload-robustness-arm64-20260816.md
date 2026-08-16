# BPFtime hash workload 稳健性验证（Jetson ARM64）

日期：2026-08-16

## 结论

现有 hash 路径归因通过 load factor、key size 和 value size 三组 matched sweep 复核：

1. Ordinary hash 的 probing/key processing 成本会随负载和 key size 上升，支持此前对通用 `memcmp`、hash 和 linear probing 的归因。
2. Per-CPU hash lookup/update 对 key size 极其敏感。4B→64B key 给 lookup/update 分别增加 412.08/411.07 ns，二者斜率几乎相同，说明大头来自共同的 `key_vec.assign()`、Boost hash/equality/container 路径。
3. Per-CPU hash update 对 value size 极其敏感。8B→256B value 使 BPFtime 增加 800.29 ns 和约 4487 instructions/helper，而 lookup 不随 value size 变化，直接确认 shared-memory value-vector copy 是 update 特有大头。
4. 高 load 不是 per-CPU hash 巨大固定成本的主要原因：64→1000 active keys 只使 BPFtime lookup 增加 11.29 ns；其 145.75 ns 的低负载基线已经很高。
5. Ordinary hash update 的“BPFtime 更快”结论需要限定在当前短 key workload：4B/16B key 时 BPFtime 更快，64B key 时翻转为 kernel 更快。

因此，既有源码归因不只是在官方 `1000 keys / 4B key / 8B value` 单点成立；key/value sweep 还明确展示了不同 BPFtime 特有路径的独立 scaling signature。

## 方法

- Jetson Orin Nano，MAXN_SUPER，CPU5 固定 1.728 GHz；
- kernel 与 BPFtime 对每个配置使用同一个 BPF object；
- ordinary/per-CPU hash 均测 lookup-hit、update-existing；
- 状态准备位于计时区外；
- 每次 invocation 执行 1000 次 helper；
- real/control 保留相同循环和 key/value 初始化；
- 每个配置和 engine 运行 5 个独立 wall-time victim；
- 代表端点对 cycles/instructions 做 3 轮相邻配对 PMU；
- 净成本为 `(real - operation-specific control) / 1000`。

正式代码提交：`1a1dc0d6c2026a5e4b0e4faa964e3eea8ccdd241`。

## 基线复现

`1000 active keys / 1024 entries / 4B key / 8B value` 与 8 月 12 日严格 matched 结果的最大差异为 3.34%，四项方向全部一致。

| 操作 | Kernel ns/helper | BPFtime ns/helper | 差额 |
|---|---:|---:|---:|
| Ordinary hash lookup | 26.21 | 53.87 | +27.66 |
| Ordinary hash update | 89.98 | 52.53 | -37.45 |
| Per-CPU hash lookup | 26.06 | 157.04 | +130.99 |
| Per-CPU hash update | 66.07 | 232.61 | +166.54 |

## Load-factor 结果

固定 4B key、8B value、1024 max entries：

| 操作 | 64 keys Kernel/BPFtime | 1000 keys Kernel/BPFtime | BPFtime 增量 |
|---|---:|---:|---:|
| Ordinary lookup | 16.88 / 38.99 | 26.21 / 53.87 | +14.88 ns |
| Ordinary update | 84.11 / 37.40 | 89.98 / 52.53 | +15.13 ns |
| Per-CPU lookup | 17.98 / 145.75 | 26.06 / 157.04 | +11.29 ns |
| Per-CPU update | 62.34 / 220.68 | 66.07 / 232.61 | +11.93 ns |

Ordinary BPFtime lookup 从 285.99 增至 367.02 instructions/helper；kernel 仅从 119.10 增至 121.28。该方向与 BPFtime open-addressed table 在高负载下增加 probing 工作一致。

Per-CPU BPFtime lookup instructions 只从 920.41 增至 930.31，因此 Boost shared-memory container 的主要固定成本并不是高 load 才产生。

## Key-size 结果

固定 512 active keys、1024 max entries、8B value：

| 操作 | 4B key Kernel/BPFtime | 16B key Kernel/BPFtime | 64B key Kernel/BPFtime |
|---|---:|---:|---:|
| Ordinary lookup | 20.27 / 43.89 | 28.42 / 77.69 | 65.67 / 160.52 |
| Ordinary update | 86.80 / 41.87 | 92.63 / 75.82 | 130.58 / 159.60 |
| Per-CPU lookup | 21.68 / 153.26 | 28.58 / 234.43 | 64.96 / 565.34 |
| Per-CPU update | 62.76 / 227.27 | 69.08 / 310.31 | 105.71 / 638.34 |

4B→64B 的 BPFtime 增量：

| 操作 | BPFtime 增量 | 每增加 1B 的斜率 |
|---|---:|---:|
| Ordinary lookup | +116.64 ns | 1.944 ns/B |
| Ordinary update | +117.73 ns | 1.962 ns/B |
| Per-CPU lookup | +412.08 ns | 6.868 ns/B |
| Per-CPU update | +411.07 ns | 6.851 ns/B |

Lookup/update 在同一 map 类型中的斜率几乎相同，是本组最关键的证据。它将增量定位到二者共同执行的 key-processing 路径，而不是 update 独有的 value copy。

PMU 中，64B key 相对 4B key 给 per-CPU BPFtime lookup/update 分别增加约 2149/2144 instructions/helper；kernel 两项均只增加约 165 instructions/helper。

## Value-size 结果

固定 512 active keys、1024 max entries、4B key：

| 操作 | 8B value Kernel/BPFtime | 64B value Kernel/BPFtime | 256B value Kernel/BPFtime |
|---|---:|---:|---:|
| Ordinary lookup | 20.27 / 43.89 | 21.13 / 43.99 | 21.98 / 44.64 |
| Ordinary update | 86.80 / 41.87 | 87.33 / 39.68 | 111.07 / 49.11 |
| Per-CPU lookup | 21.68 / 153.26 | 21.83 / 154.26 | 21.31 / 153.54 |
| Per-CPU update | 62.76 / 227.27 | 61.96 / 403.30 | 81.96 / 1027.56 |

Lookup 基本不随 value size 变化。Per-CPU update 则出现非常明确的 scaling：

```text
BPFtime: 227.27 -> 1027.56 ns/helper，增加 800.29 ns
Kernel:    62.76 ->   81.96 ns/helper，增加  19.20 ns
Gap:      164.51 ->  945.60 ns/helper，增加 781.09 ns
```

PMU 中 BPFtime per-CPU update 从 1415.41 增至 5902.38 instructions/helper，增加约 4487；kernel 只增加约 39。这与 `value_vec.assign()` 加命中后 shared-memory current-CPU slot copy 的源码路径一致。

Ordinary hash update 在全部 value size 下仍由 BPFtime 获胜，说明普通连续存储的 value copy 与 per-CPU Boost value-vector 路径不是同一成本结构。

## 对原结论的更新

| 原结论 | 本轮证据 | 更新 |
|---|---|---|
| Ordinary hash lookup 的 compare/probing 很重 | load 和 key size 都提高 BPFtime time/instructions | 得到加强 |
| Per-CPU hash find 的 Boost key/hash/equality/container 很重 | 长 key 对 lookup/update 产生相同的约 412 ns 增量 | 得到加强 |
| Per-CPU hash update 的 value-vector copy 很重 | 大 value 只显著放大 update，增加约 800 ns | 强确认 |
| Per-CPU hash 大头可能主要来自高 load | 低负载仍有约 146/221 ns lookup/update 基线 | 排除为主要原因 |
| Ordinary hash update 由 BPFtime 获胜 | 64B key 时翻转 | 限定为短 key workload |

## 边界

- Long-key 结构使用 integer ID 加 zero padding，因此 key-size slope 同时包含字节处理长度和 hash/bucket 分布变化，不能全部命名为纯 hash-loop 成本。
- PMU 是完整进程计数，但 real/control 的 setup 和 warm-up 相同并相邻配对。
- 本轮只覆盖 hit/existing；miss/insert 是下一阶段独立语义工作。
- 本轮只验证 Jetson ARM64，不构成新的跨架构结论。
- 没有修改生产 runtime 算法。

## 结果

完整原始数据、执行脚本和解析脚本位于：

[hash-workload-robustness-arm64-20260816](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/jetson/uprobe/hash-workload-robustness-arm64-20260816)
