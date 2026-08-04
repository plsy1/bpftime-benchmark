# BPFtime `benchmark/uprobe` 普通 map 跨平台调查报告

日期：2026-08-04

## 技术摘要

这轮调查从 `benchmark/uprobe` 的 map 操作语义开始，经过 benchmark 状态修正、
Jetson ARM64 与固定频率 x64 顶层对照，再进入 helper、runtime 和 kernel 路径
分解。主要结论如下：

1. 官方 victim 一共运行 17 个 case，其中普通 array/hash 各有
   lookup、update、delete，合计六个普通 map 名称；另外还有六个 per-CPU map
   名称以及五个基础 probe/read/write case。六个普通 map 名称并不都代表有效、
   稳定且唯一的操作语义。
2. hash helper 的真实执行路径由 key 是否存在决定，完整语义应拆成 lookup-hit、
   lookup-miss、update-existing、update-insert、delete-hit、delete-miss 六种。
   原 benchmark 的 hash update 使用 `BPF_ANY`，名字本身无法说明是 insert 还是
   existing；按实际运行状态，它只有第一次调用是 insert，后续 99,999 次调用均为
   existing，因此结果近似代表 update-existing。
3. 原 hash delete 只有第一次调用成功，后续几乎全部是 delete-miss。提交
   `8ed291e` 增加计时区外的 setup probe，在每一个 delete 样本前恢复 key
   `0..999`，使计时区中的 1000 次 helper 全部成为 delete-hit。lookup/update 的
   原执行逻辑没有改变。
4. 最终锁频 lookup/update 四项净结果显示：x64 上 kernel 赢两个 lookup，
   BPFtime 赢两个 update；Jetson 上 kernel 赢 array lookup、array update 和
   hash lookup，BPFtime 只赢 hash update。四项中唯一发生胜负翻转的是 array
   update；修正后的 ordinary hash delete-hit 则在两个平台都由 BPFtime 获胜。
5. 这不是“BPFtime 在 ARM64 上绝对执行得更慢”。四个 BPFtime 普通 map 路径在
   Jetson 上的绝对 ns/helper 都低于 x64。matched kernel-runtime A/B 进一步确认，
   ARM64 的 array update、hash lookup 和 hash update kernel 路径分别比固定频率
   x64 轻约 3.00×、1.80× 和 1.40×；array lookup 是接近持平的例外。趋势变化来自
   不同操作的分子、分母缩放不同。
6. 后续重点分析了三个 Jetson 上由 kernel 获胜的项目：array lookup、array
   update、hash lookup。array 两项的主要 BPFtime 成本位于 generic handler 与
   shm/fd/variant dispatch；hash lookup 还包含更重的 hash/probing 本体。分层
   A/B 已将 hash lookup 的主要额外成本定位到外层 spin lock、`SPDLOG_TRACE`、
   通用 `memcmp` key 比较以及 linear-probing 循环中的整数除法。两组组合效应按
   顶层量级粗略覆盖约九成差距，但由于来自不同实验组，不能视为严格可加闭合。

## 六项普通 map 结果总览

先看官方 victim 中普通 array/hash 六个名称的直接结果。表中数值是 5 个独立
victim 的中位数，单位为 `µs/invocation`；每次 invocation 内执行 1000 次 map
helper，因此除以 1000 后数值上也近似等于 `ns/helper`，但这里保留 victim 的原始
计时口径，不先做 empty-uprobe subtraction。

| 普通 map case | 当前实际语义 | ARM64 kernel | ARM64 BPFtime | ARM 结果 | x64 kernel | x64 BPFtime | x64 结果 |
|---|---|---:|---:|---|---:|---:|---|
| array lookup | lookup-hit | 3.792 | 13.281 | kernel 更快 | 6.122 | 13.829 | kernel 更快 |
| array update | update-existing | 13.091 | 16.805 | kernel 更快 | 37.493 | 18.974 | BPFtime 更快 |
| array delete | 不支持删除，错误返回路径 | 3.683 | 13.474 | **不解释胜负** | 10.704 | 13.737 | **不解释胜负** |
| hash lookup | lookup-hit | 29.035 | 54.984 | kernel 更快 | 53.316 | 77.921 | kernel 更快 |
| hash update | 近似 update-existing | 94.568 | 52.024 | BPFtime 更快 | 132.470 | 83.639 | BPFtime 更快 |
| hash delete | 修正后的 delete-hit | 108.539 | 29.125 | BPFtime 约快 3.73× | 112.116 | 40.970 | BPFtime 约快 2.74× |

六项结果先给出三个直接观察：

1. 两个平台的普通 lookup 都由 kernel 获胜；
2. hash update 与修正后的 hash delete-hit 都由 BPFtime 获胜；
3. array update 是唯一的平台胜负翻转项：ARM64 kernel 获胜，x64 BPFtime 获胜。

array delete 的数值只代表不支持操作的错误返回路径，不能称为 delete 性能。hash
delete 使用每个样本单独 setup 和计时，计时边界不同于非破坏性 case；它能回答
delete-hit 在同一平台上谁更快，但不与修正前的 delete-miss 数值直接比较。

后文用于精确分析 lookup/update 的四项顶层表会按同进程
`(map case − empty uprobe) / 1000` 计算净 ns/helper。这里先展示 raw 表，是为了让
六个官方普通 map 名称及其实际结果在报告开头完整可见。

## 官方 uprobe 中的 map case 是如何组织的

`benchmark/uprobe/uprobe.bpf.c` 对四种 map 类型统一生成三种 helper case：

```text
ordinary array       lookup / update / delete
ordinary hash        lookup / update / delete
per-CPU array        lookup / update / delete
per-CPU hash         lookup / update / delete
```

因此完整 victim 的 17 个 case 由以下部分组成：

| 类别 | 数量 | 内容 |
|---|---:|---|
| 基础 probe | 3 | uprobe、uretprobe、uprobe+uretprobe |
| 用户内存 helper | 2 | `bpf_probe_read_user`、`bpf_probe_write_user` |
| 普通 map | 6 | array/hash × lookup/update/delete |
| per-CPU map | 6 | per-CPU array/hash × lookup/update/delete |

本报告后续的跨平台普通 map 调查从下面六个名称出发：

| benchmark 名称 | 代码中的操作 | 实际语义 |
|---|---|---|
| `__bench_array_map_lookup` | 1000 次 lookup | array 元素预分配，稳定 lookup-hit |
| `__bench_array_map_update` | 1000 次 `BPF_ANY` update | 稳定 update-existing |
| `__bench_array_map_delete` | 1000 次 delete | array 不支持删除，属于错误返回路径 |
| `__bench_hash_map_lookup` | 1000 次 lookup | update 后执行，稳定 lookup-hit |
| `__bench_hash_map_update` | 1000 次 `BPF_ANY` update | 首轮 insert，稳态近似 update-existing |
| `__bench_hash_map_delete` | 1000 次 delete | 修正前近似 delete-miss；修正后 delete-hit |

这说明“六个 benchmark 名称”和“六个都可直接比较的 map 操作”不是一回事。
array delete 没有成功删除语义；hash update 与 delete 必须结合测量前的 map 状态
解释。

per-CPU 六项已在前一阶段单独调查：ARM64 上更大的 BPFtime/kernel 倍率主要来自
ARM64 kernel per-CPU map 更轻，而不是 BPFtime 在 ARM 上普遍执行得更慢；其
userspace 实现路径也与本报告的普通 array/fixed-size hash 实现不同。删除
`bpf_perf_event_output()` 中的临时 affinity 操作不属于 per-CPU map lookup/update
路径，因此没有把两项工作混在一起。

## Hash 的三个 helper 名称实际对应六种稳定语义

对于 hash map，key 是否存在会改变 lookup、update 和 delete 的执行路径。完整的
稳定语义矩阵如下：

| 稳定语义 | 测量前状态 | helper/flag | 结果 | 测量后状态 |
|---|---|---|---|---|
| lookup-hit | key 存在 | `bpf_map_lookup_elem` | 返回 value 指针 | 存在 |
| lookup-miss | key 不存在 | `bpf_map_lookup_elem` | 返回 `NULL` | 不存在 |
| update-existing | key 存在 | `bpf_map_update_elem(..., BPF_EXIST)` | 覆盖 value | 存在 |
| update-insert | key 不存在 | `bpf_map_update_elem(..., BPF_NOEXIST)` | 插入 key/value | 存在 |
| delete-hit | key 存在 | `bpf_map_delete_elem` | 成功删除 | 不存在 |
| delete-miss | key 不存在 | `bpf_map_delete_elem` | 返回失败 | 不存在 |

官方代码使用 `BPF_ANY`，同时允许 insert 和 existing update，因此
`__bench_hash_map_update` 这个名字本身语义不完整。要么通过 flag 和前置状态把
两种 update 分成独立 case，要么像本轮一样审计真实运行状态后准确命名结果。

### 当前 update 为什么可以近似称为 update-existing

每次进入 `__bench_hash_map_update`，BPF 程序对 key `0..999` 执行 1000 次：

```c
bpf_map_update_elem(&hash_map, &key, &value, BPF_ANY);
```

victim 默认调用该函数 100,000 次：

- 第一次函数调用：1000 次 update-insert；
- 后续 99,999 次函数调用：99,999,000 次 update-existing；
- insert 只占全部 100,000,000 次 helper 的 `0.001%`。

因此现有平均值可以准确描述为“近似 update-existing”，但不能拿它说明纯
update-insert 的性能。报告中不再使用无状态限定的笼统 “hash update”。

## Benchmark 修改把 hash delete 从 miss 修正为 hit

### 原逻辑为什么测成了 delete-miss

原 victim 顺序是 hash update → lookup → delete。update 先创建 key `0..999`，
但连续 100,000 次调用 delete 时没有恢复 map：

- 第一次调用：1000 次 delete-hit；
- 后续 99,999 次调用：99,999,000 次 delete-miss；
- delete-hit 同样只占 `0.001%`。

所以原始 `__bench_hash_map_delete` 的平均值实际上是高装载 map 被清空后的
delete-miss，不能解释成成功删除成本。

### 提交 `8ed291e` 的修改

修改只恢复 destructive case 的前置状态，不把 setup 时间混入 delete：

```text
__setup_hash_map_delete()：写入 key 0..999       不计时
start_timer()
__bench_hash_map_delete()：删除 key 0..999       计时
end_timer()
```

具体变更为：

- `benchmark/uprobe/uprobe.bpf.c`
  - 增加 `__setup_hash_map_delete`；
  - 增加 `__setup_per_cpu_hash_map_delete`；
  - setup 使用 `BPF_ANY` 写回 key `0..999`。
- `benchmark/test.c`
  - 增加 `get_function_time_with_setup()`；
  - 每个 delete 样本先调用 setup，再单独计时 delete；
  - 普通 hash 与 per-CPU hash 都使用相同恢复协议。

用于恢复状态的 1000 次 update 不进入计时区。修改后的 delete 每个样本都从相同
前置状态开始，计时区中的 1000 次操作全部为 delete-hit。

修正后的 delete 每个样本都单独调用一次 `clock_gettime` 开始/结束计时，而原
非破坏性 case 可以把多次调用放在一个连续计时区间内。每个 delete 样本包含 1000
次 helper，因此定时器固定成本相对较小；但修正前后不仅计时边界不同，操作语义也
从 miss 变为 hit，两组 delete 绝对值本来就不应直接比较。

lookup 和 update 没有修改：lookup 的状态原本就稳定；update 的 insert 占比只有
`0.001%`，当前仍按“近似 update-existing”解释。如果今后要测纯 update-insert，
应新增独立 case，使用 `BPF_NOEXIST` 并在每个样本前于计时区外删除目标 key。

### 同一提交增加的诊断工具

`8ed291e` 还增加了三类诊断 target，但没有改变生产 runtime：

1. JIT helper/no-op A/B：保持 BPF 指令流相同，只替换真实 helper 与 no-op helper；
2. L0–L3 direct 分层：分别进入具体 map 实现、handler、shm/fd 和生产 helper；
3. kernel runtime A/B：matched control/real BPF program，通过 kernel
   `run_time_ns/run_cnt` 测量净 map 成本。

后续提交又增加 value-size sweep、matched top hash 边界和 helper map ladder。
这些都属于诊断代码，没有修改官方 lookup/update 的循环、key、value 或运行顺序。

## 最终跨平台结果只有 array update 发生胜负翻转

### 可比条件与指标

最终顶层对照使用代码提交 `8ed291e130fe3f15f99955b0d259eb119efdaa7d`：

- ARM64：Jetson Orin Nano，CPU5 固定 1.728 GHz，MAXN_SUPER，无 SMT sibling；
- x64：Intel i7-8750H，CPU5 固定 2.2 GHz，turbo 关闭，SMT sibling 下线；
- 两端均为 LLVM JIT ON、LTO ON、probe read/write checks OFF；
- loader 与 victim 固定 CPU5，以 root 运行；
- 每个环境 5 个独立 victim，每个 victim 单线程、100,000 iterations；
- 每个普通 map case 在 BPF 程序中执行 1000 次 helper。

顶层净成本定义为同一进程内：

```text
net ns/helper
  = (map case ns/invocation - __bench_uprobe ns/invocation) / 1000
```

delete 没有进入这轮四项对照：array delete 是无效操作；hash delete 刚改变了测试
语义与计时边界，不适合作为最初 lookup/update 趋势问题的比较对象。

### 四项顶层结果

| 操作 | ARM64 kernel | ARM64 BPFtime | ARM 胜者 | x64 kernel | x64 BPFtime | x64 胜者 |
|---|---:|---:|---|---:|---:|---|
| array lookup-hit | 2.563 | 12.901 | kernel | 2.757 | 13.393 | kernel |
| array update-existing | 11.858 | 16.426 | kernel | 34.132 | 18.547 | **BPFtime** |
| hash lookup-hit | 27.808 | 54.608 | kernel | 49.948 | 77.501 | kernel |
| hash update-existing | 93.342 | 51.647 | **BPFtime** | 129.108 | 83.218 | **BPFtime** |

单位均为 ns/helper，表中使用 5 个同进程净值的中位数。

### 趋势如何变化

最终受控结果不是“x64 所有普通 map 都由 BPFtime 获胜”，而是更精确的两类趋势：

1. **lookup：两个平台都由 kernel 获胜。** array lookup 在两端几乎不随平台
   变化；hash lookup 在 ARM64 上两条路径都更快，但 kernel 相对下降更多。
2. **update：x64 两项都由 BPFtime 获胜；ARM64 只有 hash update 由 BPFtime
   获胜。** 唯一翻转的是 array update。

早期 GitHub Actions 数据用于发现“ARM64 与 x64 趋势不同”这一现象，但云 runner
硬件和频率不可控。上表的两台物理机锁频结果是最终顶层比较基线。

### 绝对值证明这不是 BPFtime 的 ARM64 普遍退化

| 操作 | ARM/x64 kernel | ARM/x64 BPFtime | 主要变化来源 |
|---|---:|---:|---|
| array lookup | 0.930× | 0.963× | 两边近似不变，kernel 分母略轻 |
| array update | 0.347× | 0.886× | kernel 路径大幅变轻，导致胜负翻转 |
| hash lookup | 0.557× | 0.705× | 两边都变轻，kernel 相对下降更多 |
| hash update | 0.723× | 0.621× | 两边都偏向 BPFtime，ARM 优势更强 |

四个 BPFtime 路径在 Jetson 上的绝对时间都低于 x64，因此数据不支持
“BPFtime userspace map 在 ARM64 上普遍执行得更慢”。但在同一 Jetson 内部，
BPFtime array lookup/update 与 hash lookup 确实比对应 kernel 路径重；这正是后续
需要拆分 BPFtime 分子和 kernel 分母的原因。

## Matched kernel runtime 证明 ARM64 的三条 kernel 路径更轻

顶层 elapsed time 之外，还使用相同的 kernel BPF 源码做了 matched
control/real runtime A/B。control 保留 1000 次循环、key/value stack 写入和指针
准备，real 只比 control 多真实 map helper；净成本定义为：

```text
kernel net ns/helper
  = (real program runtime - control program runtime) / 1000
```

固定频率两平台结果如下：

| kernel BPF 操作 | Jetson ARM64 | x64 2.2 GHz | x64/ARM64 | 解释 |
|---|---:|---:|---:|---|
| array lookup-hit | 1.384 ns | 0.929 ns | 0.671× | 两边都极轻；x64 direct runtime 略低 |
| array update-existing | 10.952 ns | 32.877 ns | **3.002×** | x64 明显更重，解释胜负翻转 |
| hash lookup-hit | 26.769 ns | 48.228 ns | **1.802×** | x64 kernel 路径明显更重 |
| hash update-existing | 90.823 ns | 127.492 ns | **1.404×** | x64 kernel 路径更重 |

所以“ARM 上 kernel BPF 更轻”有直接的 kernel runtime 证据，但应准确表述为：

- array update、hash lookup、hash update 在 Jetson 上明显更轻；
- array lookup 在两个平台都接近 1 ns，direct runtime 中反而是 x64 略轻；
- 官方顶层 array lookup 是 ARM64 2.563 ns、x64 2.757 ns，差别同样很小，说明
  这个极短路径容易受到 harness 边界影响，不应概括成明显的平台优势。

这一组数据也解释了为什么只比较 `BPFtime/kernel` 倍率会产生误导：BPFtime 在
ARM64 上的绝对时间虽然没有变重，但 kernel 分母在三条路径上下降得更多，尤其是
array update，于是 BPFtime 的相对优势会缩小甚至翻转。

## Array update 的翻转主要来自 Jetson kernel 路径更轻

array update 是唯一胜负翻转项，因此先对 kernel 分母做 matched runtime A/B。
固定频率结果为：

| 指标 | Jetson ARM64 | x64 2.2 GHz | ARM/x64 |
|---|---:|---:|---:|
| runtime wall-time | 10.952 ns/helper | 32.877 ns/helper | 0.333× |
| cycles/helper | 19.034 | 69.220 | 0.275× |
| instructions/helper | 82.029 | 110.157 | 0.745× |
| L1D loads/helper | 24.003 | 25.041 | 0.959× |

LLC miss 接近零，L1D load 数也接近，差距不是 cache miss 主导。Jetson 少执行约
28 条净指令，并且这些指令消耗的 cycles 更少。

运行 kernel 机器码显示：

```text
Jetson:
  array_map_update_elem
    -> 直接 __memcpy 短尺寸路径
    -> bpf_obj_free_fields

x64:
  array_map_update_elem
    -> bpf_obj_memcpy
         -> memcpy_orig
    -> bpf_obj_free_fields
```

Jetson 编译器把 `bpf_obj_memcpy` 包装逻辑内联进 `array_map_update_elem`，而 x64
保留了额外的 out-of-line wrapper 和 generic memcpy 调用。这与 ARM 少约 28 条
instructions/helper 的方向一致。

8B–256B value-size sweep 进一步排除了“x64 只是复制字节更慢”：8B 时 x64 比
ARM 多 22.996 ns/helper，256B 时差距反而缩小到 18.494 ns/helper。原 8B 异常
主要是固定 helper/map 路径成本，而不是随 value 长度增长的 memcpy 成本。

这个结论足以解释 benchmark 的胜负翻转来源，但不能把全部 cycles 差异只归因于
一次函数调用；两端 kernel 版本、编译器、ISA 和微架构不同。

## 为什么后续只重点分析三项

Jetson 顶层四项中，三个项目由 kernel 获胜：

| 项目 | Jetson BPFtime−kernel | 调查价值 |
|---|---:|---|
| array lookup-hit | +10.338 ns/helper | array 本体很轻，适合识别 generic userspace 固定成本 |
| array update-existing | +4.568 ns/helper | 唯一跨平台胜负翻转，需要拆分分子与 kernel 分母 |
| hash lookup-hit | +26.800 ns/helper | Jetson 最大正差额，且 hash/probing 路径明显更复杂 |
| hash update-existing | −41.695 ns/helper | BPFtime 已在两个平台获胜，不是本轮性能异常重点 |

因此源码和 PMU 分析范围收敛到 array lookup、array update、hash lookup。hash
update 保留为正向对照；delete 先解决语义正确性；per-CPU map 已由独立阶段处理。

## Array lookup 的主要成本是通用 userspace map 分发

### BPFtime direct L0–L3

| 层 | 累计 ns/op | 累计 instructions/op | 相邻增量 |
|---|---:|---:|---:|
| L0 array 实现 | 0.228 | 14 | array 下标检查与地址计算 |
| L1 generic handler | 4.989 | 64 | +4.760 ns、+50 instructions |
| L2 shm/fd/variant | 9.947 | 98 | +4.959 ns、+34 instructions |
| L3 生产 helper | 8.920 | 89 | LTO 后完整生产路径 |

在可逐层比较的 L2 路径里，handler 与 fd/variant dispatch 占 98 条净指令中的
84 条，约 86%；array 实现本体只有 14 条。

相同 BPF 指令流的 JIT helper/no-op A/B 为 10.063 ns、85 instructions、17.374
cycles/helper；kernel matched runtime A/B 只有 1.384 ns、10.997 instructions、
2.422 cycles/helper。

因此 array lookup 的大倍率同时包含两件事：BPFtime 执行了一条约 85–98 条指令
的 generic userspace 路径；kernel array lookup 又是一个极轻的分母。主要优化
对象不是 array 元素访问，而是 handler 与 shm/fd/variant dispatch。

## Array update 同样受通用分发影响，但 kernel 自身也很重

| 层 | 累计 ns/op | 累计 instructions/op | 相邻增量 |
|---|---:|---:|---:|
| L0 array 实现 | 2.771 | 48 | flags/key 检查与 8-byte copy |
| L1 generic handler | 8.937 | 108 | +6.166 ns、+60 instructions |
| L2 shm/fd/variant | 14.878 | 149 | +5.941 ns、+41 instructions |
| L3 生产 helper | 13.178 | 131 | LTO 后完整生产路径 |

JIT helper/no-op A/B 为 14.507 ns、134 instructions、25.047 cycles/helper；
Jetson kernel runtime 为 10.952 ns、82.029 instructions、19.034 cycles/helper。

BPFtime 的 generic handler 与共享内存分发仍占主要毛成本，但 kernel 也实际执行
value update、copy 和对象字段处理，所以最终 BPFtime 只比 kernel 多约 4.568
ns/helper，而不是像 array lookup 那样出现五倍倍率。

## Hash lookup 的主要额外成本已完成路径级定位

### 同一 helper-ladder 的 2×2 A/B

最新 Jetson 实验使用：

```text
(hash_hit − loop_control) / 1000
```

每个变体运行 6 轮，base 结果复现了官方顶层约 26.8 ns 的差距：

| 变体 | BPFtime | kernel | BPFtime−kernel |
|---|---:|---:|---:|
| base | 53.854 ns/helper | 26.779 ns/helper | **+27.075 ns** |
| no lock | 44.662 | 25.940 | +18.722 |
| no TRACE | 46.879 | 25.995 | +20.884 |
| no lock + no TRACE | 40.306 | 26.448 | **+13.858** |

相对于 base 的 BPFtime 均值：

- 去掉普通 hash lookup 外层 spin lock：降低 9.192 ns/helper；
- 编译掉 lookup 内 `SPDLOG_TRACE`：降低 6.975 ns/helper；
- 两者同时去掉：降低 13.549 ns/helper。

PMU 给出同方向结果：

| 变体 | cycles/helper | instructions/helper |
|---|---:|---:|
| kernel | 45.800 | 121.550 |
| BPFtime base | 92.800 | 362.004 |
| BPFtime no lock | 77.321 | 319.979 |
| BPFtime no TRACE | 80.879 | 289.989 |
| BPFtime no lock + no TRACE | 69.648 | 250.985 |

两项同时去掉后，BPFtime 仍比 kernel 多 23.85 cycles 和 129.43
instructions/helper。`no lock` 只是单线程诊断变体，不能直接作为生产优化；结果
只证明锁在当前路径中有成本。

### 剩余成本主要在 hash/probing 本体与公共 map 路径

| 边界 | 增量 wall-time | 增量 instructions |
|---|---:|---:|
| L0 hash/probing/memcmp − control | 37.655 ns | 213.95 |
| L1 handler − L0 | 9.294 ns | 94.00 |
| L2 shm/fd − L1 | 4.919 ns | 34.01 |
| L3 helper wrapper − L2 | 约 0 ns | 3.00 |

最大部分是 L0 固定大小 hash 表的 hash、取模、occupied 检查、linear probing 与
`memcmp`；L1/L2 generic handler 和 shm/fd dispatch 是第二部分；最外层 helper
wrapper 不是主因。

固定容量 1024 的负载率实验进一步验证了 linear probing：

| active keys | L0 hit | L0 miss | L3 hit | L3 miss |
|---:|---:|---:|---:|---:|
| 1 | 27.877 ns | 24.408 ns | 42.498 ns | 39.571 ns |
| 512 | 30.850 ns | 33.878 ns | 45.527 ns | 48.739 ns |
| 1000 | 40.710 ns | 166.241 ns | 54.351 ns | 185.117 ns |

1000 个 active key 对应 1031 个 bucket，接近满载。miss 的探测链显著拉长，hit
成本也会上升。官方顶层测量的是 lookup-hit，因此高负载 miss 只用于验证实现机制，
不能拿 185 ns 的 miss 值直接解释顶层 hit。

### 叶子级 A/B 定位到通用 key 比较和 probe 取模

针对固定容量 hash 表的直接 lookup 路径，进一步记录了实际 probe 分布。当前
benchmark 使用 1000 个 active key、1031 个 prime buckets：

| 指标 | 结果 |
|---|---:|
| 平均 probes/lookup | 2.972 |
| 首 bucket 命中 | 655/1000 |
| 最大 probe 数 | 18 |

这意味着 lookup-hit 并非固定一次比较：约三分之一的 key 需要继续 linear
probing，平均每次 lookup 接近三次 probe。源码叶子级 ABBA 测量得到：

| 诊断变体 | 相对完整 direct lookup 的节省 | 解释的逻辑 |
|---|---:|---|
| 预计算 hash 与首 bucket | 6.822 ns/lookup | hash 计算与初始取模 |
| 4B key 专用整数比较 | **11.700 ns/lookup** | 替代每个 probe 的通用 `memcmp` |
| 条件回绕 | **2.678 ns/lookup** | 替代 probing 循环中的 `% num_buckets` |
| 跳过 empty 检查 | 0.233 ns/lookup | 量级很小，接近噪声 |
| 缓存 bucket 地址 | 0.009 ns/lookup | 未观察到稳定贡献 |

组合诊断将 direct full lookup 从 29.067 ns 降至 12.538 ns，节省 16.530 ns
（56.9%）。PMU 与 wall-time 一致：

| direct 变体 | instructions/lookup | cycles/lookup |
|---|---:|---:|
| full | 185.608 | 51.209 |
| 4B key compare | 91.518 | 30.643 |
| 条件回绕 | 184.588 | 46.463 |
| 两者组合 | 81.403 | 22.109 |

机器码给出直接原因：生产路径因为运行时 `_key_size` 使用通用
`memcmp@plt`，因此在每次 probe 中都有函数调用和通用长度处理；
`(index + 1) % _num_buckets` 在 ARM64 probing 循环中生成 `udiv`。条件回绕
消除了循环中的重复 `udiv`，但没有消除首次 hash 后计算 bucket 的除法。

这里的固定 4B 比较与条件回绕都是**诊断 A/B**，用于证明成本来源，不等同于建议
把生产实现硬编码成 4B key，也没有回答并发和通用 key-size 设计应如何优化。

### 完整官方顶层确认叶子效应能够传递

为了避免 direct harness 的结果不能代表完整 benchmark，又在完整官方
`benchmark/uprobe` loader 与 `benchmark/test` victim 上做了 5 个独立 block。
loader、shared memory、TRACE 和锁均保持不变，只依次启用安全的 4B fast path，
再增加 probing 条件回绕；其他 key size 仍回退到 `memcmp`。指标仍为同一 victim
中的：

```text
(hash lookup − empty uprobe) / 1000
```

| 顶层变体 | mean ns/helper | median ns/helper | 相对 kernel |
|---|---:|---:|---:|
| kernel | 27.002 | 27.054 | 1.000× |
| BPFtime base | 54.920 | 55.000 | 2.034× |
| BPFtime 4B compare | 48.553 | 48.618 | 1.798× |
| BPFtime 4B compare + 条件回绕 | 42.860 | 42.737 | 1.587× |

同 block 配对差值在 5 轮中均为正：4B compare 平均节省 6.367 ns/helper，组合
平均节省 12.060 ns/helper，其中条件回绕的增量约为 5.693 ns/helper。隔离 helper
的 PMU 也确认相同方向：

| 顶层 helper 变体 | cycles/helper | instructions/helper |
|---|---:|---:|
| kernel | 45.181 | 121.496 |
| BPFtime base | 93.115 | 361.999 |
| BPFtime 4B compare | 82.164 | 293.635 |
| BPFtime 4B compare + 条件回绕 | 73.070 | 293.626 |

4B compare 相对 base 减少约 68.4 instructions 和 11.0 cycles/helper；再加入条件
回绕时 instructions 几乎不变，但又减少约 9.1 cycles/helper，符合整数除法主要体现
为 latency 而非大量动态指令的特征。

当前正式顶层 base−kernel 差距为 27.918 ns/helper。外层 `no lock + no TRACE`
组合效应为 13.549 ns/helper，顶层 `4B compare + 条件回绕` 组合效应为 12.060
ns/helper；二者合计 25.609 ns，约为差距的 91.7%。这是跨两个实验组的**量级覆盖**，
两组路径可能交互，不能当作严格的代数闭合；但已经足以说明当前高额成本主要来自
哪几段逻辑，而不是尚未发现的单一大函数。

## 三项重点调查的完成度

下表把三个重点项目的顶层现象、已经确认的路径和剩余问题集中到同一处。这里的
“完成”指已经足以解释 Jetson 上的性能方向，不表示已经得到可直接合入的优化 patch。

| 项目 | Jetson 顶层差距 | 已确认的主要原因 | 证据层级 | 当前状态 | 尚未拆分 |
|---|---:|---|---|---|---|
| array lookup-hit | BPFtime `+10.338 ns/helper` | array 访问本体只有 0.228 ns/14 instructions；主要成本在 generic handler 与 shm/fd/variant dispatch；kernel lookup 又极轻 | 顶层 5 进程、JIT real/no-op、direct L0–L3、kernel runtime/PMU | **路径级定位完成** | handler 内类型分派、fd lookup、`offset_ptr`、variant 提取的独立成本 |
| array update-existing | BPFtime `+4.568 ns/helper` | BPFtime 有 generic userspace 分发；同时 Jetson kernel update 比 x64 轻约 3.00×，导致跨平台胜负翻转 | 顶层、JIT/direct、两平台 kernel runtime/PMU、kernel 机器码、8B–256B sweep | **胜负翻转已解释** | BPFtime 通用分发的逐源码成本；kernel 编译器、版本、ISA 与微架构的严格分离 |
| hash lookup-hit | 官方 BPFtime `+26.800 ns/helper`；正式闭环 `+27.918 ns/helper` | 外层 spin lock/TRACE 与 hash 本体中的通用 `memcmp`/probe 取模是主要额外成本；两组组合效应按顶层量级粗略覆盖 91.7% | 顶层、matched helper ladder、2×2 源码 A/B、PMU、direct L0–L3、probe 分布、叶子级 ABBA、正式顶层 5-block 闭环 | **路径级归因基本完成** | generic handler/shm 的逐叶子成本及生产优化方案；均不影响本阶段归因结论 |

按研究目标区分，当前完成度为：

- 如果问题是“为什么 Jetson 上这三项由 kernel 获胜”，三项都已经有足够证据回答；
- 如果问题是“哪个 runtime 层最重”，array 两项落在通用 map 分发，hash 进一步
  落在 lock/TRACE、通用 key 比较与 probing 取模；
- 本轮目标是查明成本来源，不是形成生产优化 patch。当前证据已经满足该目标；若
  未来要合入优化，才需要重新设计通用 key fast path、锁方案并补并发正确性验证。

## 测量方法与证据强度

本轮不是用一次完整 benchmark 的差值猜测函数，而是使用多层 matched A/B：

1. **顶层同进程 empty-subtracted**：回答完整 kernel/BPFtime 哪条路径获胜。
2. **JIT real/no-op helper A/B**：保持 BPF 字节码、循环、stack 准备和 helper ID
   相同，只替换 helper 函数地址。
3. **Direct L0–L3**：在同一 C++ harness 中观察 map 实现、handler、shm/fd 与
   production helper 的累计成本。
4. **Kernel control/real runtime A/B**：保持 1000 次循环和 key/value 准备，
   只增加真实 kernel map helper。
5. **源码开关与 PMU 交叉验证**：hash lock/TRACE、key compare 与 probe wrap
   同时看 wall-time、cycles、instructions 和机器码。

同一 harness 内的相邻差值可以用于归因；不同 harness 的数值只做量级对照。早期
曾把顶层、direct、JIT 和 kernel runtime 的独立实验做代数“闭合”，其中所谓
“顶层整合残差”并不是一个实际独立测得的 runtime 阶段，本报告不再用它做源码
归因。

## 限制与稳健性

- ARM64 与 x64 使用不同 CPU、kernel 和编译器。锁频结果能比较受控平台表现，
  不能把差异全部归因于 ISA。
- L3 production helper 经过 whole-path LTO，可能比独立调用 L2 API 更短；
  `L3−L2` 不能机械解释成 wrapper 成本。
- hash `no lock` 是诊断变体。并发程序是否可以改变锁粒度，需要单独证明正确性。
- 当前 hash update 近似 update-existing，但不是严格由 `BPF_EXIST` 构造的纯 case。
- hash 表装载率接近 97%，lookup-hit 数值代表当前 benchmark 的高装载状态，不应
  外推到所有 hash map。
- hash wall 变体按完整组依次运行而非逐轮随机交错；PMU 与 wall 方向一致，支持
  路径判断，但 wall 小数点后的精细因果加法不应过度解释。
- 91.7% 是两个独立实验组组合效应相加后的量级覆盖，不是同一执行中的严格成本
  分解；它不能用于声称剩余成本精确等于 8.3%。
- `effects.csv` 初版曾错误复用循环变量，把各变体与 base 第 6 轮比较。原始数据、
  各变体均值和 PMU 不受影响；脚本已按 run ID 修正，本文使用修正后的 9.192、
  6.975、13.549 ns/helper。

## 后续工作边界

如果目标是完善 benchmark 语义，应新增三个明确命名的 hash case：

1. lookup-miss：明确空 map miss 或指定装载率下 miss；
2. update-insert：计时区外清空目标 key，计时区内使用 `BPF_NOEXIST`；
3. delete-miss：计时区外保证 key 不存在，再测失败删除。

本轮“定位 Jetson 上普通 map 路径成本”的工作到此收束，不再为了当前结论继续拆分。
如果未来目标改为形成 BPFtime 优化 patch，建议重新立项，并按以下顺序推进：

1. 设计通用且安全的 fixed-size key fast path，并确认不同 key size 的收益；
2. 用条件回绕替代 probing 热循环取模，验证所有 bucket 数和边界条件；
3. 对 array/hash 共用的 generic handler 与 shm/fd/variant dispatch 做同提交、
   同 harness 的源码级 A/B；
4. 在保持并发正确性的条件下评估普通 hash lookup 的锁粒度；
5. 避免热路径中无必要的 TRACE 格式化与检查。

## 仍需回答的问题

- 把 hash 的六种稳定语义全部做成独立 case 后，insert、existing、hit、miss 的
  平台胜负是否保持当前趋势？
- generic handler 与 shm/fd/variant dispatch 中，具体是哪一个分支或数据结构
  贡献了 array lookup/update 的固定成本？
- ordinary hash lookup 的锁能否缩小作用范围、改为读侧并发方案，或者在只读 map
  状态下安全省略？这需要并发正确性实验，不能由单线程 no-lock A/B 回答。
- fixed-size key fast path 和 probe 条件回绕能否以保持通用语义的形式进入生产代码？
  当前 A/B 只完成成本归因，没有提交或推荐生产优化。
- x64 kernel array update 的剩余 cycles 差距中，编译器内联、kernel 版本和 CPU
  微架构各占多少？若要区分，需要同源码、尽量同编译器配置的跨架构 kernel 构建。

## 结果归档

主要原始数据位于 `benchmark-results/jetson` 分支：

- `uprobe/uprobe-top-arm64-20260803/`
- `uprobe/uprobe-top-x64-20260803-fixedfreq/`
- `uprobe/uprobe-top-cross-arch-20260803/`
- `uprobe/array-path-arm-diagnosis-20260801/`
- `uprobe/kernel-map-runtime-arm64-20260803-profile/`
- `uprobe/kernel-map-runtime-x64-20260803-fixedfreq/`
- `uprobe/kernel-array-update-sizes-arm64-20260803/`
- `uprobe/kernel-array-update-sizes-x64-20260803-fixedfreq/`
- `uprobe/helper-map-ladder-arm64-20260804/`
- `uprobe/top-hash-residual-arm64-20260803/`
- `uprobe/hash-lookup-ablation-arm64-20260804/`
- `uprobe/hash-lookup-leaf-ab-arm64-20260804/`
- `uprobe/hash-lookup-top-optimization-arm64-20260804/`

关键代码提交：

- `8ed291e130fe3f15f99955b0d259eb119efdaa7d`：修正 hash delete 状态并增加
  array/hash 诊断；
- `176eb291ead95eb5f8a56280deae626fac46eaa9`：增加 kernel array value-size
  诊断；
- `98db0fd`：增加 matched top hash 边界；
- `47f853ccfa86e054ff033e734dadd17f2d60166c`：增加 matched helper map ladder。

hash lookup 的叶子级和顶层 fast-path 变体只作为诊断源码保存在上述结果目录的
`source.patch` 中，开关默认关闭；没有把这些诊断改动提交为生产 runtime 优化。

普通 map 调查与此前 per-CPU 调查的关系，分别见：

- `bpftime-uprobe-percpu-cross-architecture-analysis-20260728.md`
- `bpftime-uprobe-hash-map-six-operation-semantics-20260801.md`
- `bpftime-uprobe-array-jetson-path-decomposition-20260801.md`
- `bpftime-uprobe-hash-jetson-path-decomposition-20260801.md`
