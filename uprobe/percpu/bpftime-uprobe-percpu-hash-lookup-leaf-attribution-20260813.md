# BPFtime per-CPU hash lookup 叶子级归因（Jetson ARM64，2026-08-13）

## 结论

此前已确认 per-CPU hash lookup 的主要生产成本位于 Boost.Interprocess `unordered_map::find()`。本轮使用同一个诊断 runtime 二进制、相同 lookup-hit 输入和语义等价 A/B，将完整 `impl.find()` 的 **125.275 ns/helper** 继续拆分为：

| `impl.find()` 内部组成 | ns/helper | 占完整 find | 定位状态 |
|---|---:|---:|---|
| 原始逐字节 Boost hash | 15.186 | 12.1% | 严格 A/B |
| 通用 shared-memory vector equality | 27.280 | 21.8% | 严格 A/B |
| hash/equality 交互 | 0.332 | 0.3% | 2×2 A/B |
| Boost container 组合剩余路径 | 82.476 | 65.8% | 边界差值 + perf 符号确认 |

PMU 与 wall 同向：完整 find 为 216.313 cycles 和 661.305 instructions/helper；其中 hash/equality/交互共约 74.209 cycles 和 226.782 instructions，容器剩余路径约 142.104 cycles 和 434.523 instructions。

`key_vec.assign()` 位于完整 find 边界之外。相对固定长度 `memcpy()`，通用 vector assign 多 **7.147 ns、12.182 cycles、46.812 instructions/helper**。这只是 assign 相对 fixed copy 的附加成本，不是完整 key preparation 成本。

## 语义控制

五个正式模式均完成 `10000` 次 lookup-hit，计数完全一致：

```text
lookups=10000
hits=10000
misses=0
hash_calls=10000
equal_calls=13360
```

cached-hash 模式对 benchmark 的 0..1023 四字节 key 返回预计算的**同一个原始 hash 值**，因此没有改变 bucket、collision 或 equality-call 分布。fixed-equality 模式仅把通用 vector equality 换为语义等价的固定四字节比较。

5轮 wall 均值和 sample SD：

| 模式 | mean ns/helper | SD |
|---|---:|---:|
| base | 171.408 | 0.151 |
| fixed key copy | 164.261 | 0.276 |
| exact cached hash | 155.889 | 0.224 |
| fixed 4-byte equality | 143.796 | 0.084 |
| cached hash + fixed equality | 128.609 | 0.066 |

hash 在两种 equality 上分别贡献 15.519 和 15.186 ns；equality 在两种 hash 上分别贡献 27.613 和 27.280 ns。交互只有 0.332 ns，说明两项归因稳定。

## 剩余 65.8% 的含义

剩余 82.476 ns 不是未定位到任何源码的“未知开销”。flat perf profile 确认它位于 Boost container 的 bucket selection/access、node traversal、shared-memory `offset_ptr`、iterator 和 value-address extraction 等路径。由于模板符号互相嵌套，perf sample 百分比不能相加，所以当前只把它报告为严格边界内的组合剩余量。

若目标是诊断主要机制，当前已足以确认：per-CPU hash lookup 的额外成本并非单一 hash 函数，而是通用 key representation/hash/equality 加上更大的 Boost shared-memory container traversal。如果进入优化阶段，再把 82.476 ns 继续拆为 bucket、node、`offset_ptr` 和返回值地址四组即可。

## 方法边界

- 所有定量差值来自同 binary、同输入、成对轮次；没有相减不同构建的绝对值。
- cached hash/fixed equality/no-find 都是诊断开关，不是通用优化方案。
- perf 只用于源码路径确认，不用于给嵌套模板函数做可加百分比归因。
- 本轮没有修改 kernel，也没有声称 BPFtime 与 kernel 的实现组件可以一一对应。

## 可复核内容

- 代码分支：`codex/official-no-btf`，提交 `0b65eb42`；
- 结果分支：`benchmark-results/jetson`；
- 结果目录：`uprobe/percpu-hash-lookup-leaf-ab-arm64-20260813/`；
- 结果目录包含全部 wall/PMU raw、语义计数、perf data/report、执行脚本、解析脚本、环境和最终归因 CSV。
