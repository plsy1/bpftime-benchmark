# BPFtime uprobe map-helper ARM64 生产路径归因

日期：2026-08-13

## 目标

此前已经分别得到 BPFtime 内部分层成本和严格 matched 的 BPFtime−kernel 顶层差距。本轮不再计算跨 harness 的代数余项，而是在同一份诊断 runtime 二进制中切换生产路径 A/B，回答“哪些具体的 BPFtime runtime 操作造成可观测成本”。

## 方法

- Jetson Orin Nano、CPU5、MAXN_SUPER、`jetson_clocks`，CPU 1.728 GHz；
- GCC 13.3、LLVM/Clang 15.0.7、Boost 1.83、RelWithDebInfo、LLVM JIT/LTO 开启；
- wall-time 每个模式 5 轮；PMU 每个 case 3 轮，同时统计 cycles 和 instructions；
- 所有模式使用同一 agent/syscall-server 二进制，通过环境变量选择 A/B 路径；
- 每次 BPF invocation 内执行 1000 次 helper，并减去 operation-specific control；
- `cache_control` 与 `cached_handler` 执行相同 TLS cache 操作，仅保留是否执行正常 fd/variant 查找这一差别。

## 主要结果

| 操作 | 生产路径 A/B | 节省 ns/helper | 节省 cycles/helper | 节省 instructions/helper | 相对此前严格 gap |
|---|---|---:|---:|---:|---:|
| Ordinary array lookup | SHM fd/variant lookup | 1.84 | 4.20 | 26.40 | 16.1% |
| Ordinary array lookup | Generic handler dispatch | 4.73 | 10.04 | 51.59 | 41.3% |
| Ordinary array update | SHM fd/variant lookup | 1.85 | 4.10 | 26.38 | 31.4% |
| Ordinary array update | Generic handler dispatch | 4.29 | 9.35 | 53.59 | 72.8% |
| Per-CPU array lookup | `std::function` wrapper | 4.92 | 13.51 | 48.97 | 24.9% |
| Per-CPU array lookup | Userspace CPU selection | 2.72 | 5.06 | 29.69 | 13.8% |
| Per-CPU array update | `std::function`/indirect-access block | 44.11 | 80.83 | 258.02 | 87.9% |
| Per-CPU array update | 8-byte value copy | 3.66 | 6.98 | 38.87 | 7.3% |
| Per-CPU hash lookup | Boost.Interprocess hash/find | 121.67 | 219.91 | 676.96 | 95.8% |
| Per-CPU hash update | Boost hash/find | 120.63 | 218.78 | 598.09 | 74.2% |
| Per-CPU hash update | Existing shared-memory value copy | 55.81 | 101.16 | 392.77 | 34.3% |

这些百分比来自独立 A/B effect 与此前严格顶层 gap 的量级比较。不同 A/B 之间可能重叠，不能把各行百分比相加。

## Per-CPU hash delete-hit

生产 `elem_delete` A/B 保持 delete-hit 语义：每次计时前恢复 1000 个 key，计时内仍完成查找、摘链和删除，只把 node/vector 析构及 Boost shared-memory allocator 回收延迟到下一次计时外 setup。

| 模式 | ns/helper |
|---|---:|
| 原生产路径 | 998.56 |
| 延迟析构/回收 | 211.98 |
| 同步析构/回收贡献 | **786.59** |

这与此前 delete 分层实验一致：per-CPU hash delete 的大头是 helper 返回前同步执行 vector/node 析构和 shared-memory reclamation，而不是 CPU 选择或 hash find。

## 当前结论

1. Ordinary array 的 concrete access 很轻，BPFtime 的 generic userspace handler 和 SHM fd/type dispatch 构成主要固定成本。
2. Per-CPU array update 的主要额外工作位于 BPFtime 的 `std::function`/间接访问块；`sched_getcpu` 和 8-byte copy 不是大头。
3. Per-CPU hash lookup/update 的主要成本来自 BPFtime 使用的 Boost.Interprocess hash/vector 表示，其中 update 还包含 shared-memory value-vector copy。
4. Per-CPU hash delete-hit 的主要成本是同步对象析构和 shared-memory allocator 回收。
5. PMU 中 cycles、instructions 与 wall-time 同向下降，说明这些效应对应实际执行工作量，而不是测量波动。

SHM dispatch、generic handler、`std::function` 包装和 Boost.Interprocess allocator 是 BPFtime 特有实现机制。CPU 选择、value copy、hash lookup 在 kernel 中也有逻辑等价工作；本轮确认的是 BPFtime userspace 实现的具体成本，不能表述为 kernel 完全没有同类操作。

## 边界

- 这些开关用于诊断，不是准备直接合入的优化，尤其延迟回收改变了释放时机。
- Ordinary hash lookup 的 spin lock、memcmp 和 probing 已在此前实验完成，不在本轮重复。
- Ordinary hash update/delete 在 ARM64 已快于 kernel；array delete 不受支持，因此不属于本轮“BPFtime 为什么更慢”的调查对象。

## 结果与源码

- 结果分支：`benchmark-results/jetson`
- 完整目录：`uprobe/map-production-leaf-ab-arm64-20260813/`
- 顶层 matched 对照：`uprobe/relative-kernel-attribution-arm64-20260812/`
- 代码分支：`codex/official-no-btf`
- 诊断代码提交：`9c90b225`；closure benchmark 提交：`fea5a4b6`
