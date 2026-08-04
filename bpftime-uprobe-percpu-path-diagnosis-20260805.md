# BPFtime uprobe per-CPU map 路径诊断（Jetson ARM64，2026-08-05）

## 结论先行

本轮没有修改 runtime，也没有做性能优化；目标是回答“per-CPU 操作在 ARM64 上
究竟哪一段重”。在同一台 Jetson、同一提交、同一锁频条件下做了分层 A/B 和 PMU
测量，结论如下：

1. **per-CPU array lookup** 的主要新增成本来自 `my_sched_getcpu()` 和
   `ensure_on_current_cpu(std::function)`；生产 `per_cpu_array_map_impl` 与等价
   synthetic `std_function` 层几乎重合。
2. **per-CPU array update** 除 CPU/function 路径外，还包含实际 value copy；
   `sched → std_function` 的增量约 32.43 ns/op。
3. **per-CPU hash lookup** 的大头是 Boost unordered-map 的 key hash/find，
   不是 CPU 选择或 value slot 指针加法。`key_copy → hash_find` 增加约
   91.29 ns/op、约 489 instructions/op。
4. **per-CPU hash update** 先承担 hash find，再承担命中后的 per-CPU value copy；
   `hash_find → copy_existing` 增加约 61.35 ns/op。
5. **per-CPU hash delete-hit** 每轮都在计时区外重新插入 1000 个 key，避免原
   benchmark 的“第一遍 hit、后续 miss”问题。`find + erase` 约 0.9–1.0 μs/op，
   但 synthetic 与生产 allocator 状态不同，delete 只用于确认路径位置，不做精确
   逐层相减。

## 测试口径

- 机器：Jetson Orin Nano，Cortex-A78AE，6 个 CPU 在线、无 SMT sibling；
- CPU5，MAXN_SUPER，`jetson_clocks` 后 CPU/GPU/EMC 锁频，CPU 1.728 GHz；
- 代码：`codex/official-no-btf`，runtime 基线
  `c6e2737aabddb23a9915f5eec2fe890f2c3fba07`；诊断 target 提交
  `1ee2eb6`；
- 构建：RelWithDebInfo、LLVM JIT ON、LTO ON、probe read/write checks OFF、
  GCC 13.3、Clang 15.0.7、Boost 1.83；
- wall：5 轮；array 每轮 100M，hash lookup/update 每轮 10M；
- PMU：3 个独立进程、每进程 50M，统计 task-clock/cycles/instructions/
  branches/branch-misses；
- 所有 map case 都是 hit 或 existing 状态；没有把完整 `benchmark/uprobe` 顶层
  结果与 direct runtime 微基准混在一起。

## Per-CPU array

### Lookup

| 层 | ns/op | 相对 control 增量 |
|---|---:|---:|
| control | 2.904 | 0 |
| fixed CPU + bounds/address | 3.297 | +0.393 |
| + `sched_getcpu()` | 7.089 | +4.185 |
| + `ensure_on_current_cpu(std::function)` | 12.644 | +9.740 |
| 生产 L0 | 12.995 | +10.090 |
| handler L1 | 18.483 | +15.579 |
| SHM/fd L2 | 23.937 | +21.032 |
| helper L3 | 23.689 | +20.785 |

生产 L0 与 `std_function` 层只差约 0.35 ns/op。PMU 中生产 L0 为约
118.93 instructions/op；L1/L2 再增加约 51/35 instructions/op，属于公共
handler/SHM 分发。

### Update

| 层 | ns/op | 相对 control 增量 |
|---|---:|---:|
| control | 2.906 | 0 |
| fixed CPU + bounds/address + copy | 5.832 | +2.926 |
| + `sched_getcpu()` | 9.375 | +6.469 |
| + `std::function` + copy | 41.806 | +38.900 |
| 生产 L0 | 41.853 | +38.948 |
| handler L1 | 56.683 | +53.777 |
| SHM/fd L2 | 68.410 | +65.504 |
| helper L3 | 68.363 | +65.457 |

`sched → std_function` 约 32.43 ns/op，主要是 function 路径和真实 8-byte copy；
生产 L0 与 synthetic 层只差约 0.05 ns/op。

## Per-CPU hash

### Lookup-hit

| 层 | ns/op | 相对 control 增量 |
|---|---:|---:|
| control | 2.904 | 0 |
| sched | 4.635 | +1.731 |
| key copy | 13.646 | +10.743 |
| hash find | 104.939 | +102.035 |
| value slot select | 105.956 | +103.053 |
| 生产 L0 | 116.302 | +113.399 |
| L1/L2/L3 | 122.168/127.411/127.915 | +119.264/+124.507/+125.011 |

`key_copy → hash_find` 约 91.29 ns/op；value slot 偏移只约 1.02 ns/op。PMU 中
hash find 约 626 instructions/op、0.36 branch-miss/op，生产 L0 约 737
instructions/op。

### Update-existing

| 层 | ns/op | 相对 control 增量 |
|---|---:|---:|
| control | 2.904 | 0 |
| key copy | 13.089 | +10.186 |
| value template copy | 21.424 | +18.520 |
| hash find | 112.584 | +109.680 |
| existing value copy | 173.935 | +171.031 |
| 生产 L0 | 181.894 | +178.991 |
| L1/L2/L3 | 188.837/198.836/198.025 | +185.933/+195.932/+195.122 |

`hash_find → copy_existing` 约 61.35 ns/op，是 update 的第二个大块；生产 L0
相比 synthetic `copy_existing` 再增加约 7.96 ns/op。

### Delete-hit

每轮先在计时区外重新插入 1000 个 key，再计时成功删除：

| 层 | ns/op |
|---|---:|
| control | 3.098 |
| sched | 4.947 |
| key copy | 13.940 |
| synthetic find+erase | 996.696 |
| 生产 L0 | 973.898 |
| L1/L2/L3 | 919.688/924.962/933.359 |

erase 的主要成本来自 hash find、节点删除和 allocator/bucket 状态。因为
synthetic 与生产 map 的分配器状态不同，不能解读为 handler 比 L0 更快；本轮
只确认了 delete-hit 语义和路径位置。

## 与普通 map 的公共路径对照

同一条件下还运行了普通 fixed hash/array 的 control/L0/L1/L2/L3。per-CPU hash
生产 L0 相比普通 fixed hash L0：

| 操作 | wall 增量 | instructions/op 增量 |
|---|---:|---:|
| hash lookup | +76.96 ns/op | +549.4 |
| hash update | +141.18 ns/op | +965.7 |

进入 L1/L2 后这部分差异仍保留，而 L1/L2 本身只增加约 5–17 ns/op。因此大头
已经在 per-CPU hash 的 L0 容器/key/value 路径中出现，不能归因于 SHM fd 分发。
普通 fixed hash 还使用 spin lock，所以上表只作为量级对照，不是同数据结构的
严格替代实验。

## 边界

这些结果证明了 Jetson ARM64 上 BPFtime per-CPU userspace 路径的具体成本位置，
但没有证明 ARM 指令执行本身相对 x64 更差。若要做跨平台结论，必须在 x64 用同一
诊断程序、同一 commit、同一 map 参数和 PMU 口径复测；本轮没有做优化，也没有
修改 kernel BPF。

## 可复核文件

- 结果分支：`benchmark-results/jetson`；
- array：`uprobe/percpu-array-path-arm64-20260804/`，提交 `7a58da4`；
- hash：`uprobe/percpu-hash-path-arm64-20260805/`，提交 `6f68112`；
- 源码诊断：`codex/official-no-btf`，提交 `1ee2eb6`；
- 两个结果目录均包含 raw stdout、PMU、CSV 和 `parse.py`。
