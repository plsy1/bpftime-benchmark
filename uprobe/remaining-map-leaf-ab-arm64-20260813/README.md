# 剩余四项 map 操作叶子级 A/B（Jetson ARM64，2026-08-13）

## 目标和方法

本轮把调查矩阵中剩余的四项推进到与 per-CPU hash lookup 相同的 S4 深度：per-CPU hash update、per-CPU array update、per-CPU array lookup 和 ordinary array update。

所有结果都来自同一个诊断 runtime 二进制，通过环境变量切换单个生产路径 A/B。每个模式运行5个独立 wall 进程和3个独立 PMU 进程；每次 BPF invocation 内执行1000次 helper，并减去相同 operation-specific control。perf flat profile只确认源码符号，不把嵌套模板 sample 百分比当成可加归因。

这些开关用于诊断，不是正式优化。

## 结论总表

| 操作 | 严格边界 | 主要叶子成本 | 当前解释程度 |
|---|---:|---|---|
| Per-CPU hash update-existing | `impl.find()` 125.585 ns | Boost container remainder 86.340 ns；equality 25.730 ns；hash 13.399 ns；命中后 value copy 另为57.011 ns | find按与lookup相同口径拆分，value copy独立量化 |
| Per-CPU array update-existing | concrete body 53.970 ns | `std::function` wrapper 45.559 ns（84.4%）；CPU selection 3.472 ns；address 1.788 ns；copy 1.568 ns | concrete body基本闭合 |
| Per-CPU array lookup-hit | concrete body 9.691 ns | wrapper 4.669 ns（48.2%）；CPU selection 2.682 ns；checks 1.277 ns；address 1.063 ns | concrete body闭合 |
| Ordinary array update-existing | concrete body 3.146 ns | copy 1.824 ns；address 1.328 ns；外层 generic handler 4.922 ns，SHM fd/variant lookup 1.378 ns | concrete body闭合，外层两个主要分发块独立量化 |

## 1. Per-CPU hash update-existing

### Wall归因

| 组成 | ns/helper | 占完整 find |
|---|---:|---:|
| 逐字节 hash | 13.399 | 10.7% |
| 通用 shared-memory vector equality | 25.730 | 20.5% |
| hash/equality 交互 | 0.115 | 0.1% |
| Boost bucket/node/`offset_ptr` 等组合剩余路径 | 86.340 | 68.8% |
| 完整 `impl.find()` | 125.585 | 100.0% |

find之外还独立测得：

- `key_vec.assign()` 相对固定 `memcpy()` 多7.396 ns/helper；
- `value_vec.assign()` 相对固定 `memcpy()` 多7.441 ns/helper；
- find命中后复制8-byte value到当前CPU slot需要57.011 ns/helper。

最后一项远高于普通8-byte copy，原因是生产代码通过 Boost shared-memory vector iterator完成目标定位和复制；PMU对应98.649 cycles、371.908 instructions/helper。完整 find 的 PMU 为215.134 cycles、604.980 instructions/helper，wall和PMU方向一致。

正式路径保持 update-existing。第一个进程的计时外 setup负责插入1000个key；warm-up和所有 timed update均命中已有元素。hash/equality诊断模式的1000个目标key保持相同 hash、bucket/collision 和比较结果。

## 2. Per-CPU array update-existing

| concrete body组成 | ns/helper | 占比 | instructions/helper |
|---|---:|---:|---:|
| `ensure_on_current_cpu(std::function)` wrapper | 45.559 | 84.4% | 247.055 |
| userspace CPU selection | 3.472 | 6.4% | 23.030 |
| per-CPU目标地址计算 | 1.788 | 3.3% | 17.991 |
| 8-byte value copy | 1.568 | 2.9% | 25.000 |
| flags/key/bounds checks remainder | 1.584 | 2.9% | 21.958 |
| 完整 concrete body | 53.970 | 100.0% | 335.035 |

此前 `no_copy` 开关会同时消除目标地址计算。本轮新增 `address_only` 和 `no_address`，因此已把 address和真正的8-byte copy分开。结果确认大头不是copy或`sched_getcpu()`，而是runtime在生产路径中引入的类型擦除/间接调用wrapper。

## 3. Per-CPU array lookup-hit

| concrete body组成 | ns/helper | 占比 | instructions/helper |
|---|---:|---:|---:|
| `ensure_on_current_cpu(std::function)` wrapper | 4.669 | 48.2% | 30.950 |
| userspace CPU selection | 2.682 | 27.7% | 25.996 |
| per-CPU目标地址计算 | 1.063 | 11.0% | 12.020 |
| key/null/bounds checks remainder | 1.277 | 13.2% | 14.010 |
| 完整 concrete body | 9.691 | 100.0% | 82.977 |

lookup没有value copy，因此四段可以直接闭合完整concrete body。wrapper仍是最大单项，但不像update那样占80%以上；CPU selection是第二大项。

## 4. Ordinary array update-existing

### Concrete body

| 组成 | ns/helper | 占比 | instructions/helper |
|---|---:|---:|---:|
| 8-byte value copy | 1.824 | 58.0% | 23.004 |
| shared-memory vector目标地址计算 | 1.328 | 42.2% | 11.966 |
| flags/key/bounds checks remainder | -0.006 | 约0；wall噪声 | 8.035 |
| 完整 concrete body | 3.146 | 100.0% | 43.004 |

### Concrete body之外的生产分发

| 外层路径 | ns/helper | cycles/helper | instructions/helper |
|---|---:|---:|---:|
| generic `bpf_map_handler` type dispatch | 4.922 | 8.470 | 44.990 |
| SHM fd/variant lookup | 1.378 | 2.301 | 22.016 |

因此ordinary array update自身很轻；BPFtime更显著的固定成本在generic handler和SHM fd/type分发。ARM64/x64 winner reversal的kernel内联差异仍由既有跨架构报告解释，本轮只闭合BPFtime侧源码叶子。

## 边界

- Per-CPU hash update中的86.340 ns是完整find减去严格hash/equality A/B后的组合剩余量；perf确认bucket、node、`offset_ptr`、iterator等符号，但不能把嵌套sample百分比相加。
- Array的`no_body`、`no_address`和`address_only`保留同一helper/handler/map对象，只用于确定生产concrete-body边界。
- 各组件是同binary A/B effect；除表中明确闭合的concrete body外，不应跨层任意相加。
- 本轮不修改kernel，不把BPFtime内部绝对成本误称为kernel完全不存在的逻辑工作。

## 可复核文件

- `leaf-attribution.csv`：最终wall/PMU归因；
- 每个操作目录中的`wall-raw.csv`、`wall-summary.csv`、`pmu-raw.csv`、`pmu-summary.csv`；
- `raw/smoke/`、`raw/wall/`、`raw/pmu/`和`raw/profile/`；
- `run-operation.sh`：精确执行脚本；
- `summarize.py`：统一解析和归因；
- `environment.txt`：环境、构建和提交信息。
