# Jetson 普通 hash map 路径分层（2026-08-01）

> 数据版本说明：本目录记录的是 `ead56c9` 原始 benchmark 的行为，因此其中
> delete 数据是 delete-miss。此后本地工作树已把 hash/per-CPU hash delete
> 改为每个计时样本前在计时区外重新填充 map；修复后的 delete 结果应按
> delete-hit 解释，不能与本目录的 delete-miss 行混用。

## 结论

普通 hash map 的 `lookup`、`update`、`delete` 三项已经全部拆开。稳定状态与
`benchmark/uprobe` 的实际执行顺序一致：

- `lookup`：hit；
- `update`：existing key + `BPF_ANY`；
- `delete`：miss。

在相同 LLVM JIT BPF 指令流中，只把 helper 函数地址从 no-op 换成真实 hash
helper，得到：

| 操作 | 状态 | 净 ns/helper | 净 instructions/helper | 净 cycles/helper |
|---|---|---:|---:|---:|
| lookup | hit | **41.56** | **270.91** | **71.75** |
| update | existing | **52.54** | **316.91** | **90.73** |
| delete | miss | **23.86** | **160.00** | **41.20** |

因此 Jetson 上普通 hash 的 BPFtime userspace helper 本身确实是一条较长路径，
不能只用“ARM kernel 路径较轻造成分母变小”解释相对倍率。

## benchmark 状态为什么这样定义

`uprobe.bpf.c` 的每个 BPF 程序一次执行 1000 个 helper，key 为 `0..999`，map
容量为 1024。victim 的顺序是 update、lookup、delete，每个函数重复 100000 次：

1. update 第一次把 key 插入；此后几乎全部是 existing-key update；
2. lookup 紧接着命中已存在的 key；
3. delete 只有第一遍成功，此后 99999 遍都是 miss。

所以这里不能把原始 `__bench_hash_map_delete` 解释成“成功删除成本”。它在统计上
几乎完全是 delete-miss。

## 环境

- 机器：Jetson Orin Nano，Cortex-A78AE，6 个在线 CPU
- kernel：`6.8.12-1021-tegra`
- 代码：`codex/official-no-btf`，`ead56c9`
- 构建：`RelWithDebInfo`、LLVM JIT、runtime LTO
- C/C++ 编译器：GCC 13.3；LLVM JIT：LLVM 15
- Boost：1.83
- 电源模式：`MAXN_SUPER`
- CPU/GPU/EMC 已锁频；CPU 为 1.728 GHz
- 被测进程固定在 CPU 5
- 普通 hash：1024 max entries，4-byte key，8-byte value

诊断调用者关闭 IPO，runtime 保持 LTO。L0 使用配置和内容相同的独立
`fix_size_hash_map_impl`，L1-L3 使用全局共享内存中的同一张 map；两者的 key、
value、容量和稳定状态相同。

## L0–L3 定义

| 层 | 入口 | 包含的主要工作 |
|---|---|---|
| control | 诊断循环和间接调用 | 不访问 map |
| lock | 单独一次 uncontended spin lock/unlock | 不做 map 分发和数据访问 |
| L0 | `fix_size_hash_map_impl::elem_*` | hash、桶索引、linear probing、key 比较、value 访问 |
| L1 | `bpf_map_handler::map_*_elem` | L0 + spin lock + map-type 分发、`offset_ptr` |
| L2 | `bpftime_shm::bpf_map_*_elem` | L1 + fd bounds、handler variant 检查和提取 |
| L3 | `bpftime_map_*_elem_helper` | LLVM JIT 实际注册的完整 helper 入口 |

## Direct 分层结果

wall time 是 3 个 10M-op round 的平均值。硬件计数包含同一进程中的 1M-op
warm-up 和 30M 个正式操作，扣除对应 control 后按 31M 次归一化。

### Lookup hit

| 层 | 净 ns/op | 净 instructions/op | 净 cycles/op |
|---|---:|---:|---:|
| lock-only | 11.45 | 43.00 | 19.81 |
| L0 hash 本体 | 36.41 | 169.91 | 62.90 |
| L1 handler + lock | 39.97 | 248.91 | 69.03 |
| L2 shm/fd/variant | 41.42 | 282.91 | 71.51 |
| L3 完整 helper | 41.26 | 285.91 | 71.25 |

L0→L1 增加 79 条指令，其中单独的无竞争锁路径是 43 条；L1→L2 再增加固定
34 条。lookup 的主体是 hash/probe 本体，但公共 handler、锁和 fd/variant
路径仍占 L2 的约 113 条指令。

### Update existing

| 层 | 净 ns/op | 净 instructions/op | 净 cycles/op |
|---|---:|---:|---:|
| lock-only | 14.45 | 43.00 | 24.95 |
| L0 hash 本体 | 37.82 | 203.91 | 65.31 |
| L1 handler + lock | 47.24 | 296.91 | 81.58 |
| L2 shm/fd/variant | 52.63 | 337.91 | 90.88 |
| L3 完整 helper | 53.20 | 337.91 | 91.86 |

L0→L1 增加 93 条指令，其中 lock-only 为 43 条；L1→L2 再增加 41 条。
existing update 还要完成 key 比较和 8-byte value copy，所以比 lookup 更长。

### Delete miss

| 层 | 净 ns/op | 净 instructions/op | 净 cycles/op |
|---|---:|---:|---:|
| lock-only | 11.63 | 43.00 | 20.07 |
| L0 hash 本体 | 18.72 | 66.00 | 32.33 |
| L1 handler + lock | 20.57 | 138.00 | 35.53 |
| L2 shm/fd/variant | 25.16 | 172.00 | 43.42 |
| L3 完整 helper | 24.59 | 175.00 | 42.42 |

delete-miss 遇到空桶即可返回，因此 L0 明显短于 lookup-hit/update-existing。
不过 lock 与公共分发成本不随操作缩短，所以在这项中占比更高。

Direct 各层的 wall time 不是严格可加的；不同入口会改变内联、代码布局和 IPC。
上面的逐层 instructions 增量更适合说明“执行了哪些额外工作”，独立 lock-only
只用于估计锁路径量级。

## 与普通 array 对照

同一台 Jetson、同一构建下，先前 array JIT A/B 的净成本为 lookup 85 条指令 /
10.06 ns、update 134 条指令 / 14.51 ns。hash 的对应数字是：

| 操作 | array 净指令 | hash 净指令 | hash/array | array 净时间 | hash 净时间 | hash/array |
|---|---:|---:|---:|---:|---:|---:|
| lookup | 85.00 | 270.91 | 3.19× | 10.06 ns | 41.56 ns | 4.13× |
| update | 134.00 | 316.91 | 2.37× | 14.51 ns | 52.54 ns | 3.62× |

差异不是 cache miss 主导：hash-minus-noop 的 cache-misses/helper 仍接近零。
lookup/update 各增加约 0.12 次 branch miss/helper，说明 hash/probe 的控制流确有
额外代价，但主要证据仍是 L0 本体多执行了约 170–204 条指令，再叠加 43 条左右
的锁路径和通用分发。

代码上普通 hash 使用连续共享内存 buffer、4-byte occupied 标记、key/value，
并执行 4-byte key hash、取模、open addressing/linear probing、`memcmp`；它不是
per-CPU hash 使用的 `unordered_map<vector, vector>` 路径。

## LLVM JIT helper A/B

诊断 BPF 程序每次执行 1000 次 helper。no-op 与 hash 两组使用字节级相同的
BPF 指令流和 helper ID，只替换 VM 注册的函数地址。wall time 使用 3×20M
helper；perf raw counters 另包含 1M helper warm-up，因此差值按 61M 次归一化。

这组 A/B 抵消 JIT 循环、BPF stack 上 key/value 准备及 JIT→C 调用 ABI，所得
41.56/52.54/23.86 ns 是本轮最接近生产 helper 函数体的数字。

## 数据与代码

- `direct-layers.csv`：L0–L3 及 lock-only 的 raw counter 与归一化结果
- `jit-helper-ab.csv`：相同 BPF 指令流的 no-op/hash helper A/B
- 诊断代码：
  `benchmark/uprobe/diagnostics/hash_map_path_layers.cpp`、
  `hash_helper_jit_layers.cpp`、`array_helper_jit.bpf.c`

这些改动只增加诊断 target，没有修改 runtime 生产逻辑。
