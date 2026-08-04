# Jetson ARM64 per-CPU array 路径分层诊断（2026-08-04）

## 目的

本轮只定位 BPFtime userspace 的 per-CPU array `lookup` 和 `update` 成本，
不修改 runtime 生产逻辑，也不把这组 direct C++ 微基准冒充完整 `benchmark/uprobe`
结果。所有层都固定在 CPU5，使用同一提交、同一构建和同一锁频条件。

## 测试方法

诊断程序：

`benchmark/uprobe/diagnostics/per_cpu_array_path_layers.cpp`

每个层执行相同的 key `0..999` 循环，wall-clock 运行 5 轮、每轮 100,000,000
次；PMU 运行 3 个独立进程、每个进程 50,000,000 次。PMU 使用：

```text
task-clock, cycles, instructions, branches, branch-misses
```

层的含义：

| 层 | 内容 |
|---|---|
| `control` | 诊断循环和间接函数调用，不访问 map |
| `fixed` | per-CPU array 的 key/null/bounds 检查和地址计算，CPU 固定为 5 |
| `sched` | `fixed` 加 `my_sched_getcpu()` |
| `std_function` | `sched` 的实现再通过 `ensure_on_current_cpu<T>(std::function)` 执行 |
| `l0` | 生产 `per_cpu_array_map_impl::{elem_lookup,elem_update}` |
| `l1` | `bpf_map_handler::map_*_elem` |
| `l2` | `bpftime_shm::bpf_map_*_elem` |
| `l3` | JIT helper 注册的 `bpftime_map_*_elem_helper` 入口 |

另外用同一 binary、同一 CPU 和锁频条件重跑了普通 array 的 `control/L0/L1/L2/L3`，
用于扣除公共 handler/SHM 路径，而不是跨日期引用旧数据。

## 结果：per-CPU array lookup

wall-clock 平均值，单位 ns/op；括号内为相对 `control` 的增加：

| 层 | ns/op | 增量 |
|---|---:|---:|
| control | 2.904 | 0 |
| fixed | 3.297 | +0.393 |
| sched | 7.089 | +4.185 |
| std_function | 12.644 | +9.740 |
| l0（生产实现） | 12.995 | +10.090 |
| l1（handler） | 18.483 | +15.579 |
| l2（SHM/fd） | 23.937 | +21.032 |
| l3（helper） | 23.689 | +20.785 |

关键差值：

- `fixed → sched`：约 **3.792 ns/op**，对应 `sched_getcpu()`。
- `sched → std_function`：约 **5.555 ns/op**，对应 `ensure_on_current_cpu` 的
  `std::function` 调用/构造路径。
- `std_function → l0`：约 **0.351 ns/op**，生产 per-CPU array 本体没有再引入
  同量级的额外成本。
- `l0 → l1`：约 **5.489 ns/op**，是 handler 层的公共分发。
- `l1 → l2`：约 **5.453 ns/op**，是 SHM/fd/variant 层的公共分发。

PMU 的 instructions/op：

| 层 | instructions/op | cycles/op |
|---|---:|---:|
| control | 13.86 | 6.69 |
| fixed | 34.26 | 7.39 |
| sched | 67.92 | 14.06 |
| std_function | 118.93 | 23.50 |
| l0 | 118.93 | 23.97 |
| l1 | 169.94 | 33.55 |
| l2 | 204.64 | 43.79 |
| l3 | 207.70 | 43.00 |

## 结果：per-CPU array update

| 层 | ns/op | 相对 control 增量 |
|---|---:|---:|
| control | 2.906 | 0 |
| fixed | 5.832 | +2.926 |
| sched | 9.375 | +6.469 |
| std_function | 41.806 | +38.900 |
| l0（生产实现） | 41.853 | +38.948 |
| l1（handler） | 56.683 | +53.777 |
| l2（SHM/fd） | 68.410 | +65.504 |
| l3（helper） | 68.363 | +65.457 |

关键差值：

- `fixed → sched`：约 **3.542 ns/op**，仍是 `sched_getcpu()`。
- `sched → std_function`：约 **32.431 ns/op**，主要是 `std::function` 路径加上
  8-byte value copy 的实际更新成本。
- `std_function → l0`：约 **0.048 ns/op**，生产实现与等价分层基本重合。
- `l0 → l1`：约 **14.829 ns/op**。
- `l1 → l2`：约 **11.727 ns/op**。

PMU 的 instructions/op：

| 层 | instructions/op | cycles/op |
|---|---:|---:|
| control | 15.90 | 6.70 |
| fixed | 65.88 | 11.85 |
| sched | 98.52 | 18.13 |
| std_function | 367.87 | 75.54 |
| l0 | 371.96 | 75.66 |
| l1 | 436.24 | 101.43 |
| l2 | 478.09 | 121.77 |
| l3 | 478.09 | 122.75 |

## 与普通 array 的同条件对照

`shared-layer-comparison.csv` 给出了同一轮普通 array 与 per-CPU array 的差值。
生产 L0 的差值为：

| 操作 | per-CPU L0 − ordinary L0 | 额外 instructions/op |
|---|---:|---:|
| lookup | **+9.886 ns/op** | **+91.6** |
| update | **+36.183 ns/op** | **+307.9** |

进入公共 handler/SHM 层后，这个差值仍大体保留：

- lookup：L1/L2 约 +10.54/+10.83 ns，额外约 91.6 instructions/op；
- update：L1/L2 约 +44.91/+50.83 ns，额外约 311 instructions/op。

这说明 per-CPU 额外成本不是由 handler/SHM 分发单独造成的；它在进入 L0 之前
已经出现，主要对应 per-CPU 实现的 CPU 选择、`std::function` 调用以及 update 的
实际 value copy。普通 map 的公共 L1/L2 分发成本则与 per-CPU 版本近似同量级。

## 当前结论与边界

1. ARM64 上 per-CPU array 的额外成本已经在 userspace runtime 内部定位到：
   `sched_getcpu()` 和 `ensure_on_current_cpu(std::function)` 是 lookup 的主要
   增量；update 还包含明显的 value copy 成本。
2. 生产 `per_cpu_array_map_impl` 与等价的 `std_function` 分层几乎重合，因此当前
   没有证据表明另一个隐藏的 map 实现层占据大头。
3. handler、SHM/fd 和 helper 层是普通 map 与 per-CPU map 共同承担的公共成本，
   不是本轮观察到的 per-CPU 特有差异来源。
4. 这组数据只能解释 BPFtime userspace map 路径；它不能单独给出 kernel BPF
   的成本，也不能仅凭 ARM64 一个平台断言“ARM 指令执行效率本身更差”。跨平台
   ISA/微架构结论仍需 x64 使用同一诊断程序和同一统计口径复测。
5. `l3` 与 `l2` 不应机械相减：生产 helper 在当前构建中有独立的编译/内联形态，
   因此 l3 略高或略低都不代表一个固定 wrapper 成本。

## 文件

- `raw-wall/`：per-CPU array 两种操作、各层 5 轮完整 stdout；
- `raw-pmu/`：各层 `perf stat -r 3` 完整 stdout；
- `raw-ordinary-wall/`、`raw-ordinary-pmu/`：同条件普通 array 对照；
- `wall-summary.csv`、`pmu-summary.csv`：per-CPU 解析结果；
- `ordinary-wall-summary.csv`、`ordinary-pmu-summary.csv`：普通 array 解析结果；
- `shared-layer-comparison.csv`：两种 map 的逐层差值；
- `key-results.csv`：README 中的紧凑结果表；
- `parse.py`：可重复解析脚本。
