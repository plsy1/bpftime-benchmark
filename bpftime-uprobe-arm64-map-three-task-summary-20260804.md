# Jetson ARM64 `benchmark/uprobe` 普通 map 三项任务总结（2026-08-04）

## 结论

本轮围绕 Jetson ARM64 上三个普通 map 项目展开：

1. array update-existing；
2. array lookup-hit；
3. hash lookup-hit。

三个项目都已定位到足以解释性能方向的路径层级：

- **array lookup**：array 元素访问本身很轻，主要成本位于 BPFtime 的 generic
  handler 与 shm/fd/variant dispatch；同时 kernel array lookup 分母极轻。
- **array update**：BPFtime 同样承担 generic userspace map 路径，但 kernel
  update 自身也有明显工作量，因此顶层差距小于 lookup。
- **hash lookup**：除了 generic dispatch，还包含明显更重的 hash/probing 本体。
  最新同口径 A/B 进一步确认，外层 spin lock 和 `SPDLOG_TRACE` 各有真实成本，
  但两者合计只能解释约一半的 BPFtime/kernel 差距。

当前证据不支持只用“ARM kernel 分母较轻”解释全部差异。Jetson 上 BPFtime
userspace map/helper 路径本身执行了更多指令；不同操作的 kernel 固有成本又决定了
这些额外工作最终在顶层表现为多大的差距。

## 顶层问题定义

官方顶层 ARM64 结果使用同一进程内的 empty uprobe 作为基线，每个 map case 在
BPF 程序中执行 1000 次 helper。下表为 5 个独立 victim 进程的中位数：

| 操作 | kernel BPF | BPFtime | BPFtime−kernel | 结果 |
|---|---:|---:|---:|---|
| array lookup-hit | 2.563 ns/helper | 12.901 ns/helper | **+10.338 ns** | kernel 更快 |
| array update-existing | 11.858 ns/helper | 16.426 ns/helper | **+4.568 ns** | kernel 更快 |
| hash lookup-hit | 27.808 ns/helper | 54.608 ns/helper | **+26.800 ns** | kernel 更快 |

这三个顶层差额是要解释的现象，不直接等同于某一个 runtime 函数的成本。

## 测量口径与证据边界

本调查使用了三类测量：

1. **顶层 uprobe benchmark**：比较完整 kernel BPF 与 BPFtime 运行路径。
2. **BPFtime L0–L3/JIT A/B**：在同一诊断 harness 内比较具体 map 实现、handler、
   shm/fd dispatch 和生产 helper。
3. **kernel runtime A/B**：使用 matched control/real BPF program 的
   `run_time_ns/run_cnt` 测量 kernel map 程序本体。

同一 harness 内的相邻差值可以用于路径归因；不同 harness 的绝对值只能用于量级
对照。早期报告曾把顶层、direct、JIT 和 kernel runtime 的独立实验做代数闭合，
其中所谓“顶层整合残差”不是实际独立测得的 runtime 阶段，不再把它作为源码归因
证据。

## 任务一：array update-existing

### BPFtime 路径分层

以下为 direct harness 扣除 control 后的累计成本：

| 层 | 累计 ns/op | 累计 instructions/op | 相邻层主要工作 |
|---|---:|---:|---|
| L0 array 实现 | 2.771 | 48 | key/flag 检查与 8-byte value copy |
| L1 generic handler | 8.937 | 108 | 比 L0 增加 6.166 ns、60 instructions |
| L2 shm/fd/variant | 14.878 | 149 | 比 L1 增加 5.941 ns、41 instructions |
| L3 生产 helper | 13.178 | 131 | LTO 后完整生产路径 |

L3 经过 whole-path LTO 重塑，不能把 `L3−L2` 解释成一个物理 wrapper 的负成本。
L0/L1/L2 用于观察公共层，L3 表示最终生产 helper 的整体成本。

相同 BPF 指令流的 JIT helper A/B 中，真实 array update helper 相对 no-op 增加：

```text
14.507 ns/helper
134 instructions/helper
25.047 cycles/helper
```

kernel matched runtime A/B 为：

```text
10.952 ns/helper
82.029 instructions/helper
19.034 cycles/helper
```

### 结论

BPFtime array update 的主要毛成本不是单纯的 8-byte copy，而是具体 array 实现
之外的 generic handler 与 shm/fd/variant dispatch。BPFtime 生产 helper 确实比
kernel runtime 更重，但 kernel update 自身也需要完成 value 更新，因此它不像
kernel array lookup 那样接近零成本，顶层最终只表现为约 4.568 ns/helper 的差距。

状态：**路径层级已定位；尚未对 handler、fd lookup 和 variant dispatch 做逐源码
开关 A/B。**

## 任务二：array lookup-hit

### BPFtime 路径分层

| 层 | 累计 ns/op | 累计 instructions/op | 相邻增量 |
|---|---:|---:|---:|
| L0 array 实现 | 0.228 | 14 | — |
| L1 generic handler | 4.989 | 64 | +4.760 ns、+50 instructions |
| L2 shm/fd/variant | 9.947 | 98 | +4.959 ns、+34 instructions |
| L3 生产 helper | 8.920 | 89 | LTO 后完整生产路径 |

在可逐层比较的 L2 路径里，handler 与 fd/variant dispatch 合计占 98 条净指令
中的 84 条，约 86%；array 下标检查和地址计算本身只有约 14 条指令。

相同 BPF 指令流的 JIT helper A/B 为：

```text
10.063 ns/helper
85 instructions/helper
17.374 cycles/helper
```

kernel matched runtime A/B 只有：

```text
1.384 ns/helper
10.997 instructions/helper
2.422 cycles/helper
```

### 结论

array lookup 的主要原因已经定位到 BPFtime 的 generic userspace map 路径，而
不是 array 元素读取。这里同时存在两件事：BPFtime 需要执行约 85–98 条通用路径
指令；kernel 的 array lookup 路径又只有约 11 条净指令。因此不能只说“kernel
分母轻”，也不能忽略轻分母对相对倍率的放大。

状态：**路径层级已定位；尚未对 generic handler 与 shm/fd/variant 内部做逐源码
开关 A/B。**

## 任务三：hash lookup-hit

### lock/TRACE 同口径 A/B

2026-08-04 在 Jetson CPU5 锁频条件下，使用
`(hash_hit − loop_control) / 1000` 作为同一 helper-ladder 的 wall-time 指标。
每个变体运行 6 轮：

| 变体 | BPFtime | kernel | BPFtime−kernel |
|---|---:|---:|---:|
| base | 53.854 ns/helper | 26.779 ns/helper | **+27.075 ns** |
| no lock | 44.662 ns/helper | 25.940 ns/helper | +18.722 ns |
| no TRACE | 46.879 ns/helper | 25.995 ns/helper | +20.884 ns |
| no lock + no TRACE | 40.306 ns/helper | 26.448 ns/helper | **+13.858 ns** |

相对于 base 的 BPFtime 均值：

- 去掉普通 hash lookup 外层 lock：降低 **9.192 ns/helper**；
- 编译掉 lookup 内的 `SPDLOG_TRACE`：降低 **6.975 ns/helper**；
- 两者同时去掉：降低 **13.549 ns/helper**。

`no lock` 仅是单线程诊断变体，不能据此直接删除生产 runtime 的锁。

PMU 结果与 wall-time 方向一致：

| 变体 | cycles/helper | instructions/helper |
|---|---:|---:|
| kernel | 45.800 | 121.550 |
| BPFtime base | 92.800 | 362.004 |
| BPFtime no lock | 77.321 | 319.979 |
| BPFtime no TRACE | 80.879 | 289.989 |
| BPFtime no lock + no TRACE | 69.648 | 250.985 |

两项同时去掉后，BPFtime 仍比 kernel 多约 23.85 cycles 和 129.43
instructions/helper。因此 lock 与 TRACE 是实质成本，但只解释约一半差距。

### hash 直接路径

同一 direct harness 的 lookup-hit 分层为：

| 边界 | 增量 wall-time | 增量 instructions |
|---|---:|---:|
| L0 hash/probing/memcmp 本体 − control | 37.655 ns | 213.95 |
| L1 handler − L0 | 9.294 ns | 94.00 |
| L2 shm/fd − L1 | 4.919 ns | 34.01 |
| L3 helper wrapper − L2 | 约 0 ns | 3.00 |

主要工作在 L0 hash 表本体，其次是 L1/L2 的 handler 与共享内存分发；最外层
helper wrapper 不是主因。

### 负载率与 linear probing

固定 map 容量 1024，在计时区外填充不同数量的 key：

| active keys | L0 hit | L0 miss | L3 hit | L3 miss |
|---:|---:|---:|---:|---:|
| 1 | 27.877 ns | 24.408 ns | 42.498 ns | 39.571 ns |
| 512 | 30.850 ns | 33.878 ns | 45.527 ns | 48.739 ns |
| 1000 | 40.710 ns | 166.241 ns | 54.351 ns | 185.117 ns |

1000 个 active key 时，调整后的 bucket 数为 1031，接近满载。miss 会扫描较长的
linear-probing 探测链，hit 成本也随负载率升高。官方顶层项目测量的是 lookup-hit，
因此极端 miss 结果用于确认实现机制，不能直接代替顶层 hit 成本。

状态：**已完成源码级 lock/TRACE A/B、L0–L3 分层和负载率验证。** 如需继续优化
定位，剩余工作是分别测量 `hash_func`、取模、`is_empty`、`memcmp` 与每次 probe
的独立成本。

## 三项任务当前状态

| 任务 | 已确认 | 尚未细分 |
|---|---|---|
| array update-existing | array 本体、generic handler、shm/fd/variant 和 kernel 固有 update 成本 | handler/fd/variant 内部逐源码 A/B |
| array lookup-hit | 主要成本在 generic handler 与 shm/fd/variant；kernel lookup 极轻 | 通用分发内部逐源码 A/B |
| hash lookup-hit | lock、TRACE、hash 本体、handler/shm 分发和负载率效应 | hash/probe 内部各叶子操作 |

如果目标只是回答“为什么 Jetson 上 BPFtime 普通 map 项目输给 kernel”，当前证据
已经足够。如果目标是形成具体优化 patch，则优先级应为：

1. hash lookup 的 hash/probing 本体；
2. array/hash 共用的 generic handler 与 shm/fd/variant dispatch；
3. 在保持并发正确性的前提下重新评估普通 hash lookup 的锁粒度；
4. 避免热路径中无必要的 TRACE 检查。

## 数据质量修正

`hash-lookup-ablation-arm64-20260804/parse_wall.py` 初版在计算 `effects.csv` 时错误
复用了循环变量，把各变体轮次都与 base 第 6 轮比较。原始数据、各变体均值、
BPFtime−kernel 差额和 PMU 结果不受影响。解析脚本已按实际 run ID 修正；正确的
wall 降幅为 9.192、6.975 和 13.549 ns/helper。

## 结果索引

原始数据位于 `benchmark-results/jetson` 分支：

- `uprobe/uprobe-top-arm64-20260803/`
- `uprobe/array-path-arm-diagnosis-20260801/`
- `uprobe/kernel-map-runtime-arm64-20260801/`
- `uprobe/arm64-bpftime-vs-kernel-paths-20260803/`
- `uprobe/helper-map-ladder-arm64-20260804/`
- `uprobe/top-hash-residual-arm64-20260803/`
- `uprobe/hash-lookup-ablation-arm64-20260804/`

hash 源码级 A/B 使用 `codex/official-no-btf` 的
`47f853ccfa86e054ff033e734dadd17f2d60166c`；诊断修改位于独立 worktree，没有
改动主源码工作树。
