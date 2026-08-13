# BPFtime剩余四项map操作叶子级归因（Jetson ARM64，2026-08-13）

## 结论

在完成per-CPU hash lookup后，本轮把per-CPU hash update、per-CPU array update、per-CPU array lookup和ordinary array update推进到相同的S4源码叶子级。全部结果来自同一个诊断runtime二进制中的语义等价A/B，并由cycles/instructions复核。

| 操作 | 严格边界 | 主要成本 |
|---|---:|---|
| Per-CPU hash update-existing | `impl.find()` 125.585 ns | Boost容器组合剩余86.340 ns（68.8%）；equality 25.730 ns；hash 13.399 ns；命中后value copy另为57.011 ns |
| Per-CPU array update-existing | concrete body 53.970 ns | `std::function` wrapper 45.559 ns（84.4%）；CPU selection 3.472 ns；address 1.788 ns；copy 1.568 ns |
| Per-CPU array lookup-hit | concrete body 9.691 ns | wrapper 4.669 ns（48.2%）；CPU selection 2.682 ns；checks 1.277 ns；address 1.063 ns |
| Ordinary array update-existing | concrete body 3.146 ns | copy 1.824 ns；address 1.328 ns；外层generic handler 4.922 ns，SHM fd/variant lookup 1.378 ns |

## Per-CPU hash update

完整find为125.585 ns/helper，其中逐字节hash 13.399 ns（10.7%）、通用shared-memory vector equality 25.730 ns（20.5%）、交互0.115 ns，Boost bucket/node/`offset_ptr`等组合剩余86.340 ns（68.8%）。PMU中完整find为215.134 cycles和604.980 instructions/helper，方向一致。

find之外：key vector assign相对fixed copy多7.396 ns，value template assign相对fixed copy多7.441 ns；find命中后把8-byte value复制到当前CPU slot需要57.011 ns、98.649 cycles和371.908 instructions/helper。该copy路径包含Boost shared-memory vector iterator、目标定位和复制，不是裸8-byte store。

## Per-CPU array update

完整concrete body为53.970 ns/helper。`ensure_on_current_cpu(std::function)` wrapper贡献45.559 ns（84.4%），CPU selection 3.472 ns，per-CPU地址计算1.788 ns，真实8-byte copy 1.568 ns，checks remainder 1.584 ns。

本轮用`address_only`和`no_address`修正了旧`no_copy`同时省掉地址计算的问题。因此可以确认：大头不是`sched_getcpu()`或8-byte copy，而是runtime的类型擦除/间接调用wrapper。

## Per-CPU array lookup

完整concrete body为9.691 ns/helper。wrapper为4.669 ns（48.2%），CPU selection为2.682 ns（27.7%），地址计算1.063 ns，key/null/bounds checks remainder 1.277 ns。四段闭合完整body，PMU方向一致。

## Ordinary array update

concrete body只有3.146 ns/helper：8-byte copy为1.824 ns，shared-memory vector目标地址计算1.328 ns，checks在wall中接近计时噪声但对应约8 instructions/helper。

外层生产路径更重：generic `bpf_map_handler` type dispatch为4.922 ns/44.990 instructions，SHM fd/variant lookup为1.378 ns/22.016 instructions。由此可以把ARM64上的BPFtime固定成本明确归到generic userspace dispatch，而不是array update本身。

跨架构winner reversal仍由既有kernel分析解释：ARM64 kernel将`bpf_obj_memcpy()`内联，x64保留out-of-line copy路径。本轮没有重复kernel实验，只闭合BPFtime侧叶子。

## 方法边界

- 每个模式5轮wall、3轮PMU；每个BPF invocation执行1000次helper，并减去operation-specific control。
- Hash update保持update-existing；首个进程只在计时外setup阶段插入key。
- Hash的container remainder是严格find边界减去hash/equality A/B后的组合量；perf只确认bucket/node/`offset_ptr`等符号，不把嵌套sample百分比相加。
- 诊断开关不是优化；本轮没有修改kernel。

## 结果

- 代码分支：`codex/official-no-btf`，提交`a199a4c2`；
- 结果分支：`benchmark-results/jetson`；
- 结果目录：`uprobe/remaining-map-leaf-ab-arm64-20260813/`；
- 结果目录包含完整wall、PMU、perf、执行脚本和解析器，最大单文件约152 KB。
