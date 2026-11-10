# BPFtime 普通 hash map 在 Jetson 上的路径分解（2026-08-01）

> 本文数值来自 `ead56c9` 原始 benchmark；其中 delete 是 delete-miss。后续本地
> 修复已在每个 hash/per-CPU hash delete 样本前于计时区外恢复 map，新的 delete
> 测试语义为 delete-hit。历史 miss 数据仍保留用于解释此前结果。

## 结论

普通 hash map 的三项操作已经全部拆开。与 `benchmark/uprobe` 的真实稳定状态
一致，本轮测量的是 lookup-hit、update-existing 和 delete-miss。

在相同 LLVM JIT BPF 指令流下，只把 helper 地址从 no-op 换成真实 hash helper：

| 操作 | 状态 | 净时间 | 净 instructions | 净 cycles |
|---|---|---:|---:|---:|
| lookup | hit | 41.56 ns | 270.91 | 71.75 |
| update | existing | 52.54 ns | 316.91 | 90.73 |
| delete | miss | 23.86 ns | 160.00 | 41.20 |

这直接证明 Jetson 上 BPFtime hash helper 的“分子”路径本身较长，不应只用 ARM
kernel 路径更轻的分母效应解释 userspace/kernel 倍率。

## 成本落点

Direct API 扣除相同 control 后：

| 操作 | L0 hash 本体 | L1 handler+lock | L2 shm/fd/variant |
|---|---:|---:|---:|
| lookup instructions/op | 169.91 | 248.91 | 282.91 |
| lookup ns/op | 36.41 | 39.97 | 41.42 |
| update instructions/op | 203.91 | 296.91 | 337.91 |
| update ns/op | 37.82 | 47.24 | 52.63 |
| delete-miss instructions/op | 66.00 | 138.00 | 172.00 |
| delete-miss ns/op | 18.72 | 20.57 | 25.16 |

单独的 uncontended spin lock/unlock 是 43 instructions/op，约
11.45–14.45 ns。共享内存 fd/handler variant 层稳定增加 34 条 lookup/delete
指令、41 条 update 指令。

所以普通 hash 的成本由两类工作共同构成：

1. hash 本体：4-byte key hash、取模、occupied 检查、linear probing、
   `memcmp`，update 另含 value copy；
2. 公共路径：spin lock、map-type switch、共享内存 fd/handler/variant 查找。

它与此前 per-CPU hash 的 `unordered_map<vector, vector>` 是不同实现路径。

## 与普通 array 对照

在相同 JIT A/B 方法下：

| 操作 | array | hash | hash/array |
|---|---:|---:|---:|
| lookup instructions/helper | 85.00 | 270.91 | 3.19× |
| lookup ns/helper | 10.06 | 41.56 | 4.13× |
| update instructions/helper | 134.00 | 316.91 | 2.37× |
| update ns/helper | 14.51 | 52.54 | 3.62× |

hash lookup/update 各有约 0.12 branch miss/helper，但 cache miss 增量仍接近零。
因此当前首先应归因于 hash/probe、锁和通用分发执行的指令量，而不是 cache
capacity miss。

## Delete 结果的边界

原 benchmark 先 update，再 lookup，最后 delete；每个 BPF 程序调用 100000 次，
每次处理 key `0..999`。delete 只有第一遍成功，其余几乎全是 miss。因此本轮
delete 数据准确复现原 benchmark 的稳态，但不能代表成功删除成本。

完整环境、分层定义、raw perf counters 和测量方法见：
`benchmark-results/uprobe/hash-path-arm-diagnosis-20260801/`。
