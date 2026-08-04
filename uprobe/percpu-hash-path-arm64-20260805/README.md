# Jetson ARM64 per-CPU hash 路径分层诊断（2026-08-05）

## 目的

本轮定位 BPFtime userspace 的 per-CPU hash `lookup-hit`、`update-existing` 和
`delete-hit` 路径，不修改 runtime 生产实现。测试固定在 Jetson CPU5、MAXN_SUPER
和 1.728 GHz 锁频条件下；同一构建同时采集普通 fixed hash 对照。

## 分层定义

per-CPU hash 源码核心路径是：

```text
cpu = my_sched_getcpu()
key_templates[cpu].assign(key)
impl.find(key_templates[cpu])
value pointer = itr->second + cpu * value_size
```

update-existing 还会执行：

```text
single_value_templates[cpu].assign(value)
copy(single_value, itr->second + cpu * value_size)
```

诊断层：

| 层 | 包含内容 |
|---|---|
| `control` | 诊断循环和间接函数调用，不访问 map |
| `sched` | `my_sched_getcpu()` |
| `key_copy` | CPU 选择 + `bytes_vec::assign(key)` |
| `hash_find` | 上述两步 + Boost unordered-map `find` |
| `value_select` | `hash_find` + per-CPU value slot 地址选择（lookup） |
| `value_copy` | CPU/key 后再执行 `single_value_templates[cpu].assign(value)`（update） |
| `copy_existing` | `value_copy` + hash hit 后的 per-CPU value copy（update） |
| `l0` | 生产 `per_cpu_hash_map_impl::elem_*` |
| `l1` | `bpf_map_handler::map_*_elem` |
| `l2` | `bpftime_shm::bpf_map_*_elem` |
| `l3` | `bpftime_map_*_elem_helper` |

synthetic 层使用相同的 `bytes_vec`、Boost unordered-map 和 6 个 CPU slot；
`l0` 则是独立构造的生产类，避免把 synthetic 近似误认为生产实现。

## 测量口径

- lookup/update：每层 5 个 wall-clock round，每轮 10,000,000 次；
- PMU：每层 3 个独立进程，每个进程 50,000,000 次，统计
  `task-clock,cycles,instructions,branches,branch-misses`；
- delete：每轮先在计时区外重新插入 1000 个 key，再在计时区内成功删除这 1000
  个 key，5 轮。这样不会把第一轮 hit 和后续 miss 混在一起；delete 没有使用
  进程级 PMU，因为计时外的重新插入会污染该计数。

## lookup-hit 结果

wall-clock 平均值（ns/op）：

| 层 | ns/op | 相对 control 增量 |
|---|---:|---:|
| control | 2.904 | 0 |
| sched | 4.635 | +1.731 |
| key_copy | 13.646 | +10.743 |
| hash_find | 104.939 | +102.035 |
| value_select | 105.956 | +103.053 |
| l0（生产实现） | 116.302 | +113.399 |
| l1 | 122.168 | +119.264 |
| l2 | 127.411 | +124.507 |
| l3 | 127.915 | +125.011 |

从 `key_copy` 到 `hash_find` 增加约 **91.29 ns/op**，说明主要成本集中在
Boost unordered-map 的 key hash/查找和容器访问；per-CPU value slot 选择只增加
约 **1.02 ns/op**。生产 L0 比 synthetic `value_select` 再高约 **10.35 ns/op**，
这是生产类使用的共享内存容器对象、key template 容器和实际 iterator 路径的综合
差异，不应归结为单个指令。

PMU instructions/op：

| 层 | instructions/op | cycles/op | branch-misses/op |
|---|---:|---:|---:|
| control | 15.60 | 7.44 | 0.004 |
| sched | 46.19 | 10.32 | 0.004 |
| key_copy | 136.99 | 26.34 | 0.004 |
| hash_find | 626.43 | 187.50 | 0.363 |
| value_select | 632.56 | 188.97 | 0.365 |
| l0 | 737.38 | 207.42 | 0.365 |
| l1 | 791.45 | 217.51 | 0.362 |
| l2 | 826.14 | 226.93 | 0.366 |
| l3 | 829.20 | 228.30 | 0.367 |

hash `find` 相对 key-copy 层增加约 **489 instructions/op**，并引入约
**0.36 branch-miss/op**；这与 lookup wall-clock 的大头位置一致。

## update-existing 结果

wall-clock 平均值（ns/op）：

| 层 | ns/op | 相对 control 增量 |
|---|---:|---:|
| control | 2.904 | 0 |
| sched | 4.673 | +1.769 |
| key_copy | 13.089 | +10.186 |
| value_copy | 21.424 | +18.520 |
| hash_find | 112.584 | +109.680 |
| copy_existing | 173.935 | +171.031 |
| l0（生产实现） | 181.894 | +178.991 |
| l1 | 188.837 | +185.933 |
| l2 | 198.836 | +195.932 |
| l3 | 198.025 | +195.122 |

这里有两个主要增量：

- `key_copy → value_copy`：约 **8.34 ns/op**，是 8-byte value 的
  `single_value_templates[cpu].assign(value)`；
- `hash_find → copy_existing`：约 **61.35 ns/op**，是命中后的 per-CPU value
  copy；
- `copy_existing → l0`：约 **7.96 ns/op**，是生产类完整路径相对 synthetic
  近似的剩余差异。

PMU instructions/op：

| 层 | instructions/op | cycles/op | branch-misses/op |
|---|---:|---:|---:|
| control | 17.64 | 7.44 | 0.004 |
| sched | 48.23 | 10.55 | 0.004 |
| key_copy | 132.92 | 25.56 | 0.004 |
| value_copy | 214.53 | 40.04 | 0.004 |
| hash_find | 702.95 | 201.42 | 0.367 |
| copy_existing | 1103.61 | 308.50 | 0.383 |
| l0 | 1189.36 | 323.20 | 0.393 |
| l1 | 1246.49 | 335.27 | 0.402 |
| l2 | 1288.33 | 352.20 | 0.400 |
| l3 | 1288.33 | 351.87 | 0.396 |

## delete-hit 结果

每轮都是 1000 个真实成功删除，wall-clock 平均值（ns/op）：

| 层 | ns/op | 相对 control 增量 |
|---|---:|---:|
| control | 3.098 | 0 |
| sched | 4.947 | +1.850 |
| key_copy | 13.940 | +10.842 |
| hash_find（synthetic erase） | 996.696 | +993.598 |
| l0（生产 erase） | 973.898 | +970.801 |
| l1 | 919.688 | +916.590 |
| l2 | 924.962 | +921.864 |
| l3 | 933.359 | +930.261 |

delete 的大头是 Boost hash `find + erase` 以及节点/分配器回收；synthetic 与生产
对象的 allocator/bucket 状态不同，所以 `hash_find` 与 l0/l1 的绝对顺序不能用来
推断“handler 比 map 本体更快”。本轮 delete 的价值是确认修复后的 hit 语义和路径
位置，不把首轮成功与后续 miss 混成一个数。

## 与普通 fixed hash 的同条件对照

`shared-layer-comparison.csv` 只用于观察公共层的量级，不能把两种 map 当成同一个
数据结构：普通 hash 使用 fixed hash map 和 handler spin lock，per-CPU hash 使用
Boost unordered-map 且 `should_lock=false`。

生产 L0 的 per-CPU − ordinary 差值：

| 操作 | wall 差值 | instructions/op 差值 |
|---|---:|---:|
| lookup | **+76.96 ns/op** | **+549.4** |
| update | **+141.18 ns/op** | **+965.7** |

进入公共层后，差值仍然存在但主要增加的是相近的 handler/SHM 分发：lookup 的
L1/L2 差值约 +79.30/+83.16 ns，update 约 +138.58/+143.08 ns。因此普通 map 的
公共分发不是 per-CPU hash 慢的主要来源；主要差异已经在 L0 的 hash 容器与
per-CPU value 处理路径中出现。

## 当前结论与边界

1. Jetson ARM64 上 per-CPU hash lookup 的主要成本已经定位到 **Boost unordered
   map 的 key hash/find**，不是 `sched_getcpu()` 或 per-CPU slot 指针加法。
2. update-existing 在 hash find 之外还有明显的 **8-byte value template/copy**；
   `hash_find → copy_existing` 约 61 ns/op，是 update 的第二个大块。
3. handler/SHM/helper 层在 L0 之后各增加约 5–10 ns（lookup）或约 7–17 ns
   （update），但不是最大项。
4. delete-hit 的 erase/allocator 成本约 0.9–1.0 μs/op；其前置状态已按轮重建，
   但 synthetic 与生产 allocator 状态不同，因此只用于定位大致路径，不用于精确
   逐层相减。
5. 本轮没有修改 hash runtime，也没有做优化；尚不能仅凭 ARM64 一个平台断言
   这是 ARM 特有退化。要做跨平台结论，需在 x64 使用同一诊断程序、同一提交和
   同一 PMU 口径复测。

## 文件

- `raw-wall/`：per-CPU lookup/update/delete-hit 各层 5 轮完整 stdout；
- `raw-pmu/`：per-CPU lookup/update 各层 50M、`perf stat -r 3` 完整 stdout；
- `raw-ordinary-wall/`、`raw-ordinary-pmu/`：同条件普通 fixed hash 对照；
- `wall-summary.csv`、`pmu-summary.csv`：per-CPU 解析结果；
- `ordinary-wall-summary.csv`、`ordinary-pmu-summary.csv`：普通 hash 解析结果；
- `shared-layer-comparison.csv`：两种 map 的 L0–L3 差值；
- `key-results.csv`：紧凑结果表；
- `parse.py`：可重复解析脚本。
