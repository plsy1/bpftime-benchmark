# Jetson 普通 array map 路径分层（2026-08-01）

## 结论

这轮结果说明，Jetson 上 BPFtime 普通 array 操作确实承担了可观的 userspace
路径成本；不能只用“ARM kernel 分母更快”概括全部现象。

但成本主体也不是 array 数据访问本身：

- lookup 的 array 实现本体只有约 **14 instructions/op**；进入
  `bpf_map_handler` 后累计变为约 **64 instructions/op**，进入共享内存
  fd/handler 查找后累计变为约 **98 instructions/op**。
- update 的 array 实现本体约 **48 instructions/op**；handler 层累计约
  **108 instructions/op**，共享内存 fd/handler 层累计约
  **149 instructions/op**。
- 使用完全相同的 BPF 指令流，只把 helper 函数地址从 no-op 换成真实 array
  helper，lookup 增加约 **85 instructions / 10.06 ns**，update 增加约
  **134 instructions / 14.51 ns**。
- branch miss 和 cache miss 没有随路径长度出现相称增长。当前差异首先是执行了
  更多通用分发指令，不是锁竞争、`sched_getcpu()` 或 cache miss 主导。

因此当前可定位为：

> Jetson 上普通 array 的 BPFtime 成本主要位于通用 userspace map 路径，即
> map fd/handler/variant 检查和 map-type 分发；array 的下标检查、地址计算和
> 8 字节 value 访问只占一部分。

这回答了“能否拆开看 ARM 上 BPFtime 路径成本”。它仍不能单独证明这些 C++
路径在 ARM64 上比 x64 执行得更低效；要回答 ISA/微架构差异，还需要在 x64
使用同一 harness、同一提交和相同构建选项采集 instructions/cycles。

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
- 普通 array：1024 entries，4-byte key，8-byte value

诊断调用者关闭 IPO，避免把被测 runtime API 内联进 harness；runtime 本身仍按
LTO 构建。这与生产模式中 JIT 通过函数地址调用 helper 的边界更接近。

## L0–L3 定义

所有层共享同一张 map、相同 key `0..999` 和相同 value，只改变进入路径的位置：

| 层 | 入口 | 包含的主要工作 |
|---|---|---|
| control | 诊断循环和间接函数调用 | 不访问 map |
| L0 | `array_map_impl::elem_*` | key/flag 检查、地址计算；update 另含 8B copy |
| L1 | `bpf_map_handler::map_*_elem` | L0 + map type switch、`offset_ptr`、锁策略判断 |
| L2 | `bpftime_shm::bpf_map_*_elem` | L1 + fd bounds、handler variant 类型检查和提取 |
| L3 | `bpftime_map_*_elem_helper` | 实际注册给 LLVM JIT 的完整 helper 入口 |

## direct 分层结果

以下是扣除 control 后的累计成本。wall time 为 3 个 100M-op round 的平均值；
硬件计数使用相同三轮加 1M-op warm-up，并通过相同 control 相减后按 301M 次
归一化。

### Lookup

| 层 | 净 ns/op | 净 instructions/op | 净 cycles/op |
|---|---:|---:|---:|
| L0 array 本体 | 0.228 | 14 | 0.39 |
| L1 handler | 4.989 | 64 | 8.61 |
| L2 shm/fd/variant | 9.947 | 98 | 17.17 |
| L3 生产 helper 入口 | 8.920 | 89 | 15.40 |

从 L0 到 L1 增加约 50 条指令，从 L1 到 L2 再增加约 34 条。也就是说，在可逐层
相减的 L2 路径里，handler 与 fd/variant 分发占 84/98，约 **86%**；array
实现本体只占约 14%。

### Update

| 层 | 净 ns/op | 净 instructions/op | 净 cycles/op |
|---|---:|---:|---:|
| L0 array 本体 | 2.771 | 48 | 4.78 |
| L1 handler | 8.937 | 108 | 15.43 |
| L2 shm/fd/variant | 14.878 | 149 | 25.69 |
| L3 生产 helper 入口 | 13.178 | 131 | 22.75 |

从 L0 到 L1 增加约 60 条指令，从 L1 到 L2 再增加约 41 条。handler 与
fd/variant 分发占 L2 净指令的 101/149，约 **68%**；L0 的 flags/key 检查和
8 字节 copy 占约 48 条。

### 为什么 L3 比 L2 更短

L3 不是“L2 外面机械再包一层”的结果。runtime 使用 LTO 后，编译器把完整
helper 内部的 shm、handler 和 array 热路径跨函数内联并重新优化；导出的 L2
API 仍保留独立调用边界。因此 L3 比单独调用导出 L2 API 少 9 条 lookup 指令、
18 条 update 指令。

这不影响定位：L0/L1/L2 用于观察各公共层的成本，L3 表示生产构建最终形成的
完整 helper 成本。不能把 L3-L2 当成普通 wrapper 的单独成本。

## LLVM JIT helper A/B

诊断 BPF 程序每次执行 1000 次 helper。no-op 与 array 两组使用字节级相同的
BPF 指令流和相同 helper ID，只在 VM 注册阶段替换函数地址：

| 操作 | no-op ns/helper | array ns/helper | array-no-op | 增量 instructions | 增量 cycles |
|---|---:|---:|---:|---:|---:|
| lookup | 1.856 | 11.918 | **10.063** | **85** | 17.37 |
| update | 2.326 | 16.833 | **14.507** | **134** | 25.05 |

这个 A/B 抵消了 JIT 循环、BPF stack 上 key/value 准备和 JIT→C helper call
ABI，差值更接近生产模式下真实 map helper 函数体的成本。结果与 direct L3 的
数量级一致；JIT 上下文比普通 C++ direct 调用还多约 1.1–1.3 ns。

## 对“分母效应”的修正表述

此前跨架构结果显示，ARM kernel array 路径比某些 x64 runner 更轻，这确实会
放大 ARM 的 `BPFtime / kernel` 倍率。但本轮在 Jetson 内部直接测到：

- lookup 真实 userspace helper 相对 no-op 多约 85 条指令；
- update 多约 134 条指令；
- 大部分来自通用 handler 与 fd/variant 分发，而非 array 本体。

所以更准确的说法是：

> 相对倍率较大同时包含两件事：ARM kernel 基线很轻；BPFtime 仍执行一条约
> 85–134 条额外指令的通用 userspace helper 路径。现有数据已证明后者在
> Jetson 上客观存在，但尚未证明它相对 x64 有额外的 ARM 特有退化。

## 数据与代码

- `direct-layers.csv`：L0–L3 原始硬件计数和归一化结果
- `jit-helper-ab.csv`：相同 BPF 指令流的 no-op/array helper A/B
- 诊断代码：
  `benchmark/uprobe/diagnostics/array_map_path_layers.cpp`、
  `array_helper_jit.bpf.c`、`array_helper_jit_layers.cpp`

这些文件只增加诊断 target，没有修改 runtime 生产逻辑。
