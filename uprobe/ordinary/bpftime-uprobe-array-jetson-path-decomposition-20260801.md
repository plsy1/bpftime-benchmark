# BPFtime 普通 array map 在 Jetson 上的路径分解（2026-08-01）

## 结论

Jetson 上普通 array 的 BPFtime 成本已经拆到具体软件层。结果不支持“差距完全
只是 kernel 分母更快”，也不支持“Boost array 存储本身很重”。

- lookup 的 array 本体约 14 instructions/op；handler 类型分发和共享内存
  fd/variant 路径合计再增加约 84 条。
- update 的 array 本体约 48 instructions/op；handler 与 fd/variant 路径
  合计再增加约 101 条。
- 在相同 LLVM JIT BPF 指令流下，用真实 array helper 替换 no-op helper，
  lookup 增加约 85 instructions / 10.06 ns，update 增加约
  134 instructions / 14.51 ns。
- miss counter 没有显示 cache miss 主导；普通 array 也不执行 per-CPU
  `sched_getcpu()`，且 `should_lock=false`。

因此当前根因应描述为：

> BPFtime 没有像 kernel array lookup 那样把 map 类型特化到很短的执行路径，
> 而是保留通用 userspace helper、fd/handler/variant 检查和 map-type 分发。
> Jetson 上这些公共层是 ordinary array 成本的主体。

## 分层入口

| 层 | 入口 |
|---|---|
| L0 | `array_map_impl::elem_lookup/update` |
| L1 | `bpf_map_handler::map_lookup_elem/update_elem` |
| L2 | `bpftime_shm::bpf_map_lookup_elem/update_elem` |
| L3 | `bpftime_map_lookup_elem_helper/update_elem_helper` |

同一 harness 还使用完全相同的 BPF 字节码做 no-op helper 与真实 helper A/B，
避免把 JIT 循环、key/value 栈准备和 helper-call ABI 错归到 map 实现。

## 核心数字

### Direct API，扣除相同 control

| 操作 | L0 | L1 | L2 | L3（LTO 后完整 helper） |
|---|---:|---:|---:|---:|
| lookup instructions/op | 14 | 64 | 98 | 89 |
| lookup ns/op | 0.228 | 4.989 | 9.947 | 8.920 |
| update instructions/op | 48 | 108 | 149 | 131 |
| update ns/op | 2.771 | 8.937 | 14.878 | 13.178 |

L3 少于 L2 是 runtime LTO 跨层内联后的结果，不能把 `L3-L2` 解释成 wrapper
成本。L0/L1/L2 用于定位层级；L3 用于表示生产构建最终形成的完整 helper。

### LLVM JIT，相同 BPF 指令流

| 操作 | no-op ns/helper | array ns/helper | 净时间 | 净 instructions |
|---|---:|---:|---:|---:|
| lookup | 1.856 | 11.918 | 10.063 ns | 85 |
| update | 2.326 | 16.833 | 14.507 ns | 134 |

## 对跨架构结论的影响

ARM kernel array 路径很轻，确实放大了 Jetson 上的 BPFtime/kernel 倍率；这仍
是跨平台结果的重要一部分。但本轮也给出了 ARM BPFtime 分子侧的直接证据：
它为普通 array helper 执行约 85–134 条净增指令，而且主要是通用分发路径。

因此后续表述应为：

> ARM 上较大的相对差距由“很轻的 kernel 基线”和“仍然较长的 BPFtime 通用
> userspace map 路径”共同造成。是否存在 ARM 特有的 BPFtime 执行效率问题，
> 还需在 x64 上运行相同分层 harness 和 perf counters；不能从现有跨机器绝对
> 纳秒值直接推断。

完整环境、方法和原始计数见：
`benchmark-results/uprobe/array-path-arm-diagnosis-20260801/`。
