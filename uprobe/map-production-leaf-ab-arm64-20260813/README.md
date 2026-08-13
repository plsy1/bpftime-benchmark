# Jetson ARM64 map production-path A/B attribution

日期：2026-08-13。代码基线：`deb8f56a66a4a36850810b634dba0c94a9fc63d3`。本轮目标不是继续做合成层内部占比，而是把已经观察到的 BPFtime/kernel 顶层差距连接到**生产 runtime 中的具体操作**。

## 结论

生产路径 A/B 已经确认主要成本位置，并由独立 PMU 复核：wall-time 降低时，cycles 和 instructions 同向降低。最明确的四个结论是：

1. **per-CPU hash lookup**：Boost.Interprocess hash/find 约 `121.67 ns/helper`，对应 `219.91 cycles` 和 `676.96 instructions`；它约等于此前严格顶层 gap 的 `95.8%`。
2. **per-CPU hash update**：Boost hash/find 约 `120.63 ns/helper`，existing-value shared-memory vector copy 约 `55.81 ns/helper`；两项分别对应约 `598.09` 和 `392.77 instructions/helper`。二者来自不同成对 A/B，不能简单相加为严格归因百分比。
3. **per-CPU array update**：绕过 `std::function`/间接访问块节省约 `44.11 ns/helper`（`80.83 cycles`、`258.02 instructions`），是该操作最主要的 BPFtime 生产路径成本；CPU 选择和 8-byte copy 分别约 `2.74` 和 `3.66 ns/helper`。
4. **per-CPU hash delete-hit**：在生产 `elem_delete` 中摘除节点但把析构/SHM 回收延迟到下一次计时外 setup，成本从 `998.56` 降到 `211.98 ns/helper`。因此同步 vector/node 析构与 Boost shared-memory reclamation 贡献约 `786.59 ns/helper`；此前 delete 专项 PMU 已独立确认同一路径。

普通 array 也有明确的 runtime 固定成本：fd/variant 查找约 `1.84–1.85 ns/helper`，generic handler dispatch 约 `4.29–4.73 ns/helper`。这解释了为什么极轻的 array lookup 中，BPFtime 的通用 userspace runtime 框架本身会占主要部分。

## 主要生产 A/B 结果

下表列均为 5 轮配对均值；“此前 gap”来自 `../relative-kernel-attribution-arm64-20260812/top-gap.csv`。单项百分比只表示该 A/B effect 与此前 gap 的量级关系，不表示各行可加。

| 操作 | 具体路径 | A/B 节省 (ns/helper) | PMU 节省 (cycles/helper) | PMU 节省 (instructions/helper) | 相对此前 gap |
|---|---|---:|---:|---:|---:|
| ordinary array lookup | SHM fd/variant lookup | 1.84 | 4.20 | 26.40 | 16.1% |
| ordinary array lookup | generic handler dispatch | 4.73 | 10.04 | 51.59 | 41.3% |
| ordinary array update | SHM fd/variant lookup | 1.85 | 4.10 | 26.38 | 31.4% |
| ordinary array update | generic handler dispatch | 4.29 | 9.35 | 53.59 | 72.8% |
| per-CPU array lookup | std::function wrapper | 4.92 | 13.51 | 48.97 | 24.9% |
| per-CPU array lookup | userspace CPU selection | 2.72 | 5.06 | 29.69 | 13.8% |
| per-CPU array update | std::function/indirect-access block | 44.11 | 80.83 | 258.02 | 87.9% |
| per-CPU array update | 8-byte value copy | 3.66 | 6.98 | 38.87 | 7.3% |
| per-CPU hash lookup | Boost hash/find | 121.67 | 219.91 | 676.96 | 95.8% |
| per-CPU hash update | Boost hash/find | 120.63 | 218.78 | 598.09 | 74.2% |
| per-CPU hash update | existing value copy | 55.81 | 101.16 | 392.77 | 34.3% |

完整 20 个 A/B 项见 `effects.csv`、`pmu-effects.csv` 和 `attribution.csv`。

## 实验设计

- 使用一个诊断 runtime 二进制，通过 `BPFTIME_MAP_PRODUCTION_AB` 切换路径，避免不同构建造成代码布局差异。
- loader 和 victim 固定到 CPU5；MAXN_SUPER + `jetson_clocks`，实测 CPU5 为 1.728 GHz；Jetson 无 SMT sibling。
- wall-time：array 每模式 5×50,000 invocation，hash 每模式 5×20,000 invocation；每次 BPF invocation 内执行 1000 次 helper，并减去匹配的 control program。
- PMU：每个具体 case 单独运行，3 轮 `cycles,instructions`，计数器未 multiplex；模式顺序轮换。
- fd/variant A/B 使用 `cache_control` 与 `cached_handler`：两边都执行同一个 TLS cache 访问，前者仍走正常 fd/variant 查找，后者使用缓存结果，修正了早期 `base-cached` 被 TLS 自身成本污染的问题。
- delete A/B 保持 delete-hit 语义：每次计时前恢复 1000 个 key；计时内仍完成摘链和删除，只把 node/vector 析构及 shared-memory allocator 回收延迟到下一次 setup。

## 如何理解“BPFtime 特有”

SHM fd/variant dispatch、generic handler、`std::function` 包装和 Boost.Interprocess 对象/allocator 是 BPFtime runtime 的实现机制，kernel BPF 没有这些相同路径。CPU 选择、value copy、hash lookup 则是两边逻辑上都需要的工作；本轮定位的是 BPFtime 在 userspace 中采用的具体实现（`sched_getcpu`、shared-memory vector copy、Boost find），不能表述为 kernel 完全不做同类工作。

## 边界

- A/B 改动用于诊断，不是可合入优化；特别是延迟回收会改变释放时机。
- 多个 A/B 可能存在交互，而且严格顶层 gap 来自此前独立正式批次，因此 `effect/gap` 只用于判断量级，不能把百分比求和。
- ordinary hash lookup 的 spin lock、memcmp、linear probing 等生产/叶子 A/B 已在此前报告中完成，本轮不重复。
- ordinary hash update/delete 在 ARM64 顶层已经快于 kernel，不属于“BPFtime 为什么更慢”的待解释项；array delete 不受支持。

## 文件

- `wall-raw.csv`、`wall-summary.csv`、`effects.csv`：生产路径 5 轮 wall 数据；
- `pmu-raw.csv`、`pmu-effects.csv`：3 轮硬件计数器数据；
- `delete-raw.csv`、`delete-summary.csv`：生产 delete-hit 延迟回收 A/B；
- `attribution.csv`：此前严格顶层 gap 与本轮具体操作 A/B 的连接表；
- `run-wall.sh`、`run-pmu.sh`、`run-delete.sh`：精确复现脚本；
- `source.patch`、`source-environment.txt`：诊断源码与构建环境；
- `raw/`：完整 stdout、`/usr/bin/time -v` 和 perf stat 原始输出。
