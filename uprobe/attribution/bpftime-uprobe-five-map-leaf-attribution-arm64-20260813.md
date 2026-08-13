# BPFtime五项map操作叶子级归因（Jetson ARM64，2026-08-13）

## 目标与方法

本报告统一汇总五项仍需深入的map操作：per-CPU hash lookup、per-CPU hash update、per-CPU array lookup、per-CPU array update和ordinary array update。全部结果来自同一诊断runtime二进制内的语义等价A/B；每个模式运行5轮wall和3轮PMU，每次BPF invocation执行1000次helper，并减去operation-specific control。perf flat profile仅用于确认源码路径。

这些开关用于成本定位，不是正式优化。

## 总结

| 操作 | 严格边界 | 主要成本 |
|---|---:|---|
| Per-CPU hash lookup-hit | `impl.find()` 125.275 ns | Boost容器组合剩余82.476 ns（65.8%）；equality 27.280 ns；hash 15.186 ns；key assign相对fixed copy另多7.147 ns |
| Per-CPU hash update-existing | `impl.find()` 125.585 ns | Boost容器组合剩余86.340 ns（68.8%）；equality 25.730 ns；hash 13.399 ns；命中后value copy另为57.011 ns |
| Per-CPU array lookup-hit | concrete body 9.691 ns | `std::function` wrapper 4.669 ns（48.2%）；CPU selection 2.682 ns；checks 1.277 ns；address 1.063 ns |
| Per-CPU array update-existing | concrete body 53.970 ns | `std::function` wrapper 45.559 ns（84.4%）；CPU selection 3.472 ns；address 1.788 ns；copy 1.568 ns |
| Ordinary array update-existing | concrete body 3.146 ns | copy 1.824 ns；address 1.328 ns；外层generic handler 4.922 ns，SHM fd/variant lookup 1.378 ns |

## 1. Per-CPU hash lookup-hit

| `impl.find()`内部组成 | ns/helper | 占完整find |
|---|---:|---:|
| 逐字节Boost hash | 15.186 | 12.1% |
| 通用shared-memory vector equality | 27.280 | 21.8% |
| hash/equality交互 | 0.332 | 0.3% |
| Boost container组合剩余路径 | 82.476 | 65.8% |
| 完整`impl.find()` | 125.275 | 100.0% |

PMU中完整find为216.313 cycles和661.305 instructions/helper；容器组合剩余约142.104 cycles和434.523 instructions。`key_vec.assign()`位于find边界之外，相对固定长度`memcpy()`多7.147 ns、12.182 cycles和46.812 instructions/helper。

五个正式模式的语义计数完全一致：10000次lookup、10000 hit、0 miss、10000次hasher和13360次equality。cached hash返回原始hash的精确同值，因此没有改变bucket、collision或equality-call分布。

## 2. Per-CPU hash update-existing

| `impl.find()`内部组成 | ns/helper | 占完整find |
|---|---:|---:|
| 逐字节Boost hash | 13.399 | 10.7% |
| 通用shared-memory vector equality | 25.730 | 20.5% |
| hash/equality交互 | 0.115 | 0.1% |
| Boost container组合剩余路径 | 86.340 | 68.8% |
| 完整`impl.find()` | 125.585 | 100.0% |

PMU中完整find为215.134 cycles和604.980 instructions/helper。find之外，key vector assign相对fixed copy多7.396 ns，value template assign多7.441 ns；find命中后把8-byte value复制到当前CPU slot需要57.011 ns、98.649 cycles和371.908 instructions/helper。该路径包含Boost shared-memory vector iterator、目标定位和复制，不是裸8-byte store。

第一个进程仅在计时外setup阶段插入1000个key；warm-up和所有timed update均为existing-hit。

## 3. Per-CPU array lookup-hit

| concrete body组成 | ns/helper | 占比 | instructions/helper |
|---|---:|---:|---:|
| `ensure_on_current_cpu(std::function)` wrapper | 4.669 | 48.2% | 30.950 |
| userspace CPU selection | 2.682 | 27.7% | 25.996 |
| key/null/bounds checks | 1.277 | 13.2% | 14.010 |
| per-CPU目标地址计算 | 1.063 | 11.0% | 12.020 |
| 完整concrete body | 9.691 | 100.0% | 82.977 |

lookup没有value copy，四段可以闭合完整body。wrapper是最大单项，CPU selection是第二大项。

## 4. Per-CPU array update-existing

| concrete body组成 | ns/helper | 占比 | instructions/helper |
|---|---:|---:|---:|
| `ensure_on_current_cpu(std::function)` wrapper | 45.559 | 84.4% | 247.055 |
| userspace CPU selection | 3.472 | 6.4% | 23.030 |
| per-CPU目标地址计算 | 1.788 | 3.3% | 17.991 |
| 8-byte value copy | 1.568 | 2.9% | 25.000 |
| flags/key/bounds checks | 1.584 | 2.9% | 21.958 |
| 完整concrete body | 53.970 | 100.0% | 335.035 |

`address_only`和`no_address`把旧`no_copy`中混合的地址计算与真正copy分开。结果确认大头不是`sched_getcpu()`或8-byte copy，而是runtime引入的类型擦除/间接调用wrapper。

## 5. Ordinary array update-existing

| concrete body组成 | ns/helper | 占比 | instructions/helper |
|---|---:|---:|---:|
| 8-byte value copy | 1.824 | 58.0% | 23.004 |
| shared-memory vector目标地址计算 | 1.328 | 42.2% | 11.966 |
| flags/key/bounds checks | 接近wall噪声 | — | 8.035 |
| 完整concrete body | 3.146 | 100.0% | 43.004 |

Concrete body之外，generic `bpf_map_handler` type dispatch为4.922 ns/44.990 instructions，SHM fd/variant lookup为1.378 ns/22.016 instructions。因此ordinary array update自身很轻，BPFtime更显著的固定成本位于generic userspace dispatch。

ARM64/x64 winner reversal仍由既有kernel分析解释：ARM64 kernel将`bpf_obj_memcpy()`内联，x64保留out-of-line copy路径。本轮只闭合BPFtime侧源码叶子。

## 统一结论

1. 两项per-CPU hash操作的find成本结构一致：约三分之二来自Boost shared-memory container traversal，约三分之一来自通用hash/equality；update还叠加了昂贵的shared-memory value-vector copy。
2. 两项per-CPU array操作的最大共同成本是`ensure_on_current_cpu(std::function)`；update中的wrapper尤其突出，占concrete body的84.4%。
3. Ordinary array update的实际map body很轻，主要固定成本位于generic handler和SHM fd/type分发。
4. 五项均已达到足以解释主要机制的S4深度。只有进入优化时，才需要继续拆Boost bucket/node/`offset_ptr`，或改写`std::function` wrapper后做端到端验证。

## 方法边界与结果

- 所有定量值来自同binary、同输入、成对A/B；没有相减不同构建的绝对值。
- Hash container remainder是完整find减去严格hash/equality A/B后的组合量；perf确认bucket/node/`offset_ptr`等符号，但嵌套sample百分比不可相加。
- 诊断开关不是通用优化，本轮没有修改kernel。
- 代码分支：`codex/official-no-btf`，提交`a199a4c2`。
- 结果分支：`benchmark-results/jetson`。
- Lookup结果：`uprobe/percpu-hash-lookup-leaf-ab-arm64-20260813/`。
- 其余四项结果：`uprobe/remaining-map-leaf-ab-arm64-20260813/`。
