# Jetson ARM64 per-CPU hash delete-hit 定位

日期：2026-08-05。目标是把 BPFtime per-CPU hash 的 `delete-hit` 从已经确认的
`find + erase` 大区间继续拆开，并用 kernel 的预分配/非预分配 hash 对照验证
回收机制差异。

## 结论

这轮已经确认主要差异来自**同步节点析构和 shared-memory allocator 回收**，不是
`sched_getcpu()`，也不是 hash 查找本身。

### BPFtime 分层（5 轮 wall）

| 层 | 平均 ns/op | 含义 |
|---|---:|---|
| `control` | 2.976 | 循环控制 |
| `cpu_key` | 14.112 | `sched_getcpu` + key vector 复制 |
| `find_only` | 109.760 | 成功查找，不修改 map |
| `find_extract_hold` | 147.963 | 查找并从 hash 中摘出节点，node handle 延迟释放 |
| `find_extract_drop` | 981.335 | 摘出后立即析构节点 |
| `find_erase` | 998.532 | 当前 `find + erase` 路径 |
| `l0` | 966.596 | 生产 `per_cpu_hash_map_impl::elem_delete` |

关键差额：

```text
find_only - control                 = 106.784 ns/op
find_extract_hold - find_only      =  38.202 ns/op
find_erase - find_extract_hold     = 850.569 ns/op
find_erase - find_extract_drop     =  17.197 ns/op
```

生产 L0 相对 control 的增量是 `963.620 ns/op`；延迟释放 A/B 的
`850.569 ns/op` 约占其 **88.3%**。因此，当前最合理的描述是：

> BPFtime 删除一个 per-CPU hash entry 时，Boost.Interprocess
> `unordered_map::erase()` 在删除路径内同步销毁 key/value vector、hash node，
> 并调用 shared-memory segment allocator 回收；查找和摘链只占较小部分。

`find_extract_drop` 与 `find_erase` 几乎相同，说明大头确实在 node destruction/
allocator，而不是 `erase` 与 `extract` 的接口差异。

### PMU（仅删除区间，单次 attach）

程序支持 `BPFTIME_DELETE_LAYERS_WAIT=1`：完成预填充后等待 `SIGUSR1`，perf
attach 后才开始删除；删除完成后等待 `SIGUSR2`，所以延迟 node 的清理不进入
删除区间。相对 control 的每操作差额：

| 层 | cycles/op | instructions/op |
|---|---:|---:|
| `find_only` | +206.6 | +599.9 |
| `find_extract_hold` | +300.5 | +861.4 |
| `find_extract_drop` | +1691.2 | +5884.7 |
| `find_erase` | +1746.2 | +5974.6 |

`find_erase - find_extract_hold` 约为 `1445.6 cycles/op`，与 wall 的
`850.6 ns/op`（CPU 约 1.69 GHz）一致。

### kernel 预分配对照（5 轮）

`real-minus-control` 已除以每个 BPF 程序内的 1000 次 helper：

| map | flags | kernel ns/helper |
|---|---|---:|
| ordinary hash | 默认预分配 | 131.74 |
| per-CPU hash | 默认预分配 | 174.91 |
| ordinary hash | `BPF_F_NO_PREALLOC` | 209.58 |
| per-CPU hash | `BPF_F_NO_PREALLOC` | 578.04 |

关闭预分配后：

- ordinary hash 增加约 `77.83 ns/helper`；
- per-CPU hash 增加约 `403.13 ns/helper`，约为默认路径的 `3.30×`。

这说明 kernel 默认 per-CPU hash 的低删除成本与预分配节点/freelist 机制高度
相关。kernel perf callgraph 还出现了 `htab_map_delete_elem`、`free_htab_elem`、
`pcpu_freelist_push` 和 `__update_cpu_freelist_fast`；本轮 profile 同时包含了
计时外的 userspace `prime_map`，所以这些符号用于确认路径，不用于给 kernel
删除函数做精确百分比分摊。

## 与已有顶层结果的关系

此前固定 delete-hit 顶层结果（不是本轮合成层）是 ARM：kernel `150.21`、
BPFtime `981.38 ns/helper`。本轮 BPFtime 生产 L0 `966.60 ns/op` 与该量级一致；
两者测试入口不同，不能逐数相减，但共同指向同一条同步 Boost/shared-memory
删除路径。

ordinary hash 不能直接当作 per-CPU hash 的内部对照：BPFtime ordinary hash 使用
另一套 fixed hash 实现，而 per-CPU hash 使用 Boost.Interprocess
`unordered_map<bytes_vec, bytes_vec>`；因此 ordinary delete 较快不表示通用
`bpf_map_delete_elem` helper 没有成本。

## 原始数据和复现

- `raw-wall/`：BPFtime 7 层各 5 轮完整输出；
- `raw-pmu/*.timed.perf`：附着到删除区间的 PMU 输出；
- `raw-perf/*.timed.data`：`find_erase`/`find_extract_hold` 删除区间调用图；
- `raw-kernel/kernel-delete-runtime.csv`：kernel 四种 map 的 5 轮原始输出；
- `raw-kernel/kernel-delete-perf-report.txt`：kernel cycles 调用图；
- `wall-summary.csv`、`pmu-summary.csv`、`kernel-net-summary.csv`：整理后的表。

BPFtime 诊断构建目标：

```bash
cmake --build build-array-layers --target per-cpu-hash-delete-layers -j2
```

kernel 诊断构建目标：

```bash
make -C benchmark/uprobe kernel-percpu-hash-delete-diagnostic \
  CLANG=/usr/lib/llvm-15/bin/clang
```

完整命令记录在 `commands.txt`。kernel loader 运行时必须临时打开
`kernel.bpf_stats_enabled`；本轮结束后已恢复为 `0`。

源码诊断提交是 `codex/official-no-btf` 的 `deb8f56`（基于 `e0240a1`）。
没有修改 `runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp` 的生产实现，也没有修改
官方 benchmark 语义。

## 边界

`extract` A/B 证明同步析构/回收占大头，但没有把一次 `erase` 内部的 key vector
释放、value vector 释放、hash node 释放和 segment allocator 合并操作逐个计时。
调用图已经确认这些 Boost.Interprocess 路径存在；如果需要更细的叶子比例，下一步
只需在 allocator/deallocate 层做诊断计数，不必重跑普通 map 或其它 per-CPU 操作。
