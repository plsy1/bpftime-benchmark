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
5. **per-CPU hash delete-hit** 已从原先的 `find + erase` 大区间继续拆开。延迟
   node 析构/释放的 `find_extract_hold` 约 147.96 ns/op，立即执行完整
   `find + erase` 约 998.53 ns/op，两者相差 **850.57 ns/op**。因此 delete 的
   绝对大头不是 CPU 选择、key copy、hash find 或摘链，而是 key/value vector、
   hash node 的同步析构和 Boost.Interprocess shared-memory allocator 回收。
6. kernel 的 matched 对照进一步支持回收机制差异：per-CPU hash delete 在默认
   预分配下约 174.91 ns/helper，使用 `BPF_F_NO_PREALLOC` 后升至
   578.04 ns/helper。kernel 默认节点池/freelist 显著降低了删除成本；BPFtime
   当前 Boost erase 则在 helper 返回前同步完成节点销毁和共享内存回收。

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

delete 叶子级补测沿用 CPU5、MAXN_SUPER、CPU 1.728 GHz、GCC 13.3、Clang 15
和 Boost 1.83：

- BPFtime wall：每层 1000 个真实 delete-hit，5 轮；每轮计时前重新插入；
- BPFtime PMU：程序完成预填充后暂停，perf attach 后才开始删除；删除结束后先
  detach perf，再释放延迟持有的 node，避免把计时外 setup/cleanup 算进删除路径；
- kernel：ordinary/per-CPU hash × 默认预分配/`BPF_F_NO_PREALLOC`，5 轮、
  每轮 control/real 各 10 个样本；`run_time_ns` 只统计 BPF delete 程序；
- delete 补测的源码诊断提交为 `deb8f56`（基于 `e0240a1`），仅增加诊断
  target，没有修改生产 runtime。

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

每轮先在计时区外重新插入 1000 个 key，再计时成功删除。新诊断使用 Boost
node handle 把“从 hash 表摘出节点”和“析构/释放节点”分开：

| 层 | 平均 ns/op | 相对 control 增量 | 含义 |
|---|---:|---:|---|
| control | 2.976 | 0 | 循环与函数调用 |
| CPU + key copy | 14.112 | +11.136 | `sched_getcpu` 与 key template assign |
| find only | 109.760 | +106.784 | 成功 hash find，不修改 map |
| find + extract + hold | 147.963 | +144.987 | 摘出节点，析构和释放延迟到计时外 |
| find + extract + drop | 981.335 | +978.359 | 摘出后立即析构和释放 |
| find + erase | 998.532 | +995.556 | 与当前生产逻辑等价的 synthetic 路径 |
| 生产 L0 | 966.596 | +963.620 | `per_cpu_hash_map_impl::elem_delete` |

相邻关键差额：

```text
find only - CPU/key copy                 =  95.648 ns/op
find + extract + hold - find only       =  38.202 ns/op
find + erase - find + extract + hold    = 850.569 ns/op
find + erase - find + extract + drop    =  17.197 ns/op
```

`find + erase` 相对 control 的 synthetic 完整增量为 995.556 ns/op；其中延迟
回收 A/B 的 850.569 ns/op 占约 **85.4%**。生产 L0 相对 control 是
963.620 ns/op，与 synthetic 完整路径同量级。若用生产 L0 作为量级分母，回收
差额约为 88.3%，但这不是严格可加的生产路径百分比。

`find_extract_hold` 与 `find_erase` 都使用相同初始 map、相同 key 顺序，并都会
从表中摘除节点；区别是前者把 node handle 的析构和共享内存释放推迟到 perf/
计时结束以后。`find_extract_drop` 与 `find_erase` 只差约 17 ns/op，进一步说明
大头来自 node destruction/allocator，而不是 `erase` 与 `extract` 的接口差异。

附着式 PMU 单次结果也与 wall 一致：

| 层 | 相对 control cycles/op | 相对 control instructions/op |
|---|---:|---:|
| find only | +206.6 | +599.9 |
| find + extract + hold | +300.5 | +861.4 |
| find + extract + drop | +1691.2 | +5884.7 |
| find + erase | +1746.2 | +5974.6 |

`find_erase - find_extract_hold` 约为 **1445.6 cycles/op**，在约 1.69 GHz 的
实际 PMU 频率下与 850.6 ns/op 的 wall 差额相符。删除区间调用图出现了：

- `boost::unordered::detail::table::delete_node`；
- `boost::container::vector::~vector`；
- `boost::interprocess::rbtree_best_fit::deallocate`；
- shared-memory allocator 使用的 `interprocess_mutex`。

因此，当前可以把 BPFtime per-CPU hash delete 的主要原因明确为：

> `unordered_map<bytes_vec, bytes_vec>::erase()` 在 helper 路径内同步析构 key
> vector、包含所有 CPU slot 的 value vector 和 hash node，并通过
> Boost.Interprocess segment allocator 同步回收共享内存；该回收路径约占本轮
> control-adjusted synthetic delete 成本的 85%。

### Kernel 预分配对照

kernel 侧使用相同的 4-byte key、8-byte value、1024 max entries 和每个程序
1000 次 delete helper。`real-minus-control` 已除以 1000：

| map | 分配方式 | 平均 net ns/helper | 标准差 |
|---|---|---:|---:|
| ordinary hash | 默认预分配 | 131.741 | 5.825 |
| per-CPU hash | 默认预分配 | 174.910 | 1.890 |
| ordinary hash | `BPF_F_NO_PREALLOC` | 209.575 | 1.075 |
| per-CPU hash | `BPF_F_NO_PREALLOC` | 578.038 | 12.193 |

关闭预分配使 ordinary hash 增加约 77.83 ns/helper，使 per-CPU hash 增加约
403.13 ns/helper；后者变为默认预分配路径的 **3.30×**。kernel perf 调用图中
确认出现 `htab_map_delete_elem`、`free_htab_elem`、`pcpu_freelist_push` 和
`__update_cpu_freelist_fast`。

这组实验支持“kernel 默认预分配/freelist 与 BPFtime 同步动态回收不同”这一机制
解释，但不能把 kernel `run_time_ns` 与 BPFtime direct wall 逐数相减。kernel
perf record 还包含计时外 userspace `prime_map` 的系统调用，因此调用图只用于
确认 kernel 路径，不能用全局 sample 百分比给 delete 子函数精确分摊。

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
并把 hash delete 从 `find + erase` 继续定位到同步 node/vector 析构和
shared-memory allocator 回收。但仍有以下边界：

1. delete 的 `extract` A/B 能严格区分“延迟释放”和“立即释放”，但没有继续把
   key vector、value vector、hash node 三次释放逐项单独计时；
2. attached PMU 是单次删除区间采集，主要用于验证 wall 差额对应真实 cycles/
   instructions，不作为跨轮方差结论；
3. kernel 默认/`NO_PREALLOC` 实验验证了分配策略影响，但两端实现和计时器不同，
   不能声称 850 ns 全部由 kernel/BPFtime 分配策略差异严格解释；
4. 本轮没有证明 ARM 指令执行本身相对 x64 更差。跨平台结论仍需要 x64 使用同一
   delete leaf 诊断、同一 map 参数和 PMU 口径复测；
5. 本轮没有做性能优化，也没有修改 BPFtime 生产 runtime 或 kernel BPF。

## 可复核文件

- 结果分支：`benchmark-results/jetson`；
- array：`uprobe/percpu-array-path-arm64-20260804/`，提交 `7a58da4`；
- hash：`uprobe/percpu-hash-path-arm64-20260805/`，提交 `6f68112`；
- hash delete 叶子级：`uprobe/percpu-hash-delete-layers-arm64-20260805/`，包含
  wall、附着式 PMU、BPFtime 调用图、kernel 预分配对照及命令记录；
- 源码诊断：`codex/official-no-btf`，既有提交 `1ee2eb6`；
- delete 叶子级诊断提交：`deb8f56`（基于 `e0240a1`）；
- 各结果目录包含 raw stdout、PMU/perf、汇总 CSV 和复现命令。
