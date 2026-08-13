# Per-CPU hash lookup 叶子级 A/B（Jetson ARM64，2026-08-13）

## 结论

本实验在同一个诊断 runtime 二进制中，对 BPFtime per-CPU hash lookup-hit 的 key preparation、hash、equality 和剩余 Boost container 路径做了语义等价 A/B。它是源码成本定位，不是优化提交。

严格的 `base - no_find` 边界表明，完整 `impl.find()` 区间为 **125.275 ns/helper、216.313 cycles/helper、661.305 instructions/helper**。其中：

| `impl.find()` 内部组成 | ns/helper | 占完整 find |
|---|---:|---:|
| 原始逐字节 Boost hash，而非精确缓存 hash | 15.186 | 12.1% |
| 通用 shared-memory vector equality，而非固定 4-byte equality | 27.280 | 21.8% |
| hash/equality 的小交互项 | 0.332 | 0.3% |
| Boost container 剩余组合路径 | 82.476 | 65.8% |
| 合计 | 125.275 | 100.0% |

PMU 给出相同方向。hash、equality 和交互项合计约 **74.209 cycles/helper、226.782 instructions/helper**；Boost container 剩余路径约 **142.104 cycles/helper、434.523 instructions/helper**。

key preparation 位于 `impl.find()` 边界之外。把 `key_vec.assign()` 换成同尺寸、同地址的固定 `memcpy()` 可减少 **7.147 ns/helper、12.182 cycles/helper、46.812 instructions/helper**。这个差值只表示通用 vector assign 相对固定 copy 的附加成本，不等于完整 key preparation 成本。

因此，per-CPU hash lookup 的大头已经从笼统的 `unordered_map::find()` 继续定位为两部分：

1. 可独立量化的逐字节 hash 和通用 vector equality；
2. 更大的 Boost.Interprocess container 组合剩余路径，包括 bucket selection/access、node traversal、shared-memory `offset_ptr`、iterator 和 value-address extraction。

perf flat profile 中确实出现了 `bytes_vec_hasher`、`bytes_vec_equal`/`algo_equal`、`grouped_bucket_array::at`、`grouped_bucket_iterator`、`prime_fmod_size::position` 和 `per_cpu_hash_map_impl::elem_lookup`。这些模板符号彼此嵌套，sample 百分比不可相加，所以仅用于确认源码路径，不用于替代严格 A/B 归因。

## A/B 设计

| 模式 | 唯一变化 | 保持不变的语义 |
|---|---|---|
| `base` | 原生产实现 | lookup-hit |
| `raw_key_copy` | `key_vec.assign()` 改为固定长度 `memcpy()` | 相同 key bytes 和 key vector storage |
| `cached_hash` | 对 0..1023 的 4-byte benchmark key 返回预先计算的**精确同值** hash | hash 值、bucket 分布、碰撞和 equality 次数不变 |
| `fixed4_equal` | 4-byte key 用一次固定宽度比较代替通用 vector equality | 比较结果不变 |
| `cached_hash_fixed4_equal` | 同时启用上述 hash/equality A/B | lookup-hit 语义不变 |
| `no_find` | 保留 CPU selection 和 key assign，跳过 `impl.find()` | 只用于完整 find 边界，不作为可运行优化 |

smoke 计数对所有正式模式均完全一致：每个进程 `10000` 次 lookup、`10000` hit、`0` miss、`10000` 次 hasher、`13360` 次 equality。因此 cached hash 没有改变 bucket/collision 分布，fixed equality 也没有改变查找结果。

## 5轮 wall 结果

| 模式 | mean ns/helper | sample SD |
|---|---:|---:|
| base | 171.408 | 0.151 |
| raw key copy | 164.261 | 0.276 |
| cached hash | 155.889 | 0.224 |
| fixed 4-byte equality | 143.796 | 0.084 |
| cached hash + fixed equality | 128.609 | 0.066 |

2×2 A/B 中，normal-hash → cached-hash 在 normal equality 下节省 15.519 ns，在 fixed equality 下节省 15.186 ns；generic-equality → fixed4-equality 在 normal hash 下节省 27.613 ns，在 cached hash 下节省 27.280 ns。两条路径的交互只有约 0.332 ns，说明归因稳定。

## 边界

- 本结果定位 BPFtime runtime 自身的生产 lookup 路径，不把不同进程或不同 binary 的绝对值相减。
- cached hash 和 fixed equality 是语义控制 A/B，不建议直接作为通用 runtime 优化。
- `boost_container_remainder` 是完整 find 减去 hash/equality 严格 A/B 后的组合剩余量；在继续拆 bucket、node 和 `offset_ptr` 前，不能把 82.476 ns 全归给某一个函数。
- perf 仅作符号路径确认；最终定量值来自同 binary、同输入、成对轮次的 wall/PMU A/B。

## 复核文件

- `wall-raw.csv`、`wall-summary.csv`、`wall-effects.csv`：5轮 wall 原始值、统计和成对差值；
- `pmu-raw.csv`、`pmu-effects.csv`：3轮 cycles/instructions 及成对差值；
- `find-boundary.csv`：完整 `impl.find()` 的同 binary `base - no_find` 边界；
- `leaf-attribution.csv`：本报告使用的最终归因；
- `raw/smoke/`：lookup/hit/miss/hash/equality 计数；
- `raw/profile/`：base 和组合模式的 perf data/flat report；
- `run-*.sh`、`summarize*.py`、`analyze.py`：精确执行与解析脚本；
- `environment.txt`：环境、提交和正式测试参数。
