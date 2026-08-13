# BPFtime per-CPU map 在 ARM64 上相对 kernel 偏慢的路径归因（2026-07-29）

## 技术摘要

本轮调查不支持“BPFtime 的 per-CPU 实现在 ARM64 上绝对执行得特别慢”这一概括。
此前 GitHub Actions 的 10 轮跨架构数据中，BPFtime ARM64 在 6 个 per-CPU
项目里的 5 项绝对耗时低于 x64；ARM64 上更大的 `BPFtime / Kernel` 倍率，
主要来自 ARM64 kernel per-CPU map 基线非常快。

Jetson 上的聚焦 `perf stat` 和 `perf record` 进一步确认了具体原因：

1. **kernel per-CPU map 只在普通 map 查找结果上增加很轻的 CPU-local
   地址选择。** kernel hash lookup 从普通 map 的约 135 条指令/helper 增加到
   per-CPU 的约 140 条，只多约 5 条。
2. **BPFtime per-CPU array 的额外成本主要来自每次 helper 调用都执行
   `sched_getcpu()`，以及 CPU 维度的索引和地址计算。** 固定 CPU 的诊断变体
   使 lookup 从 18.17 ns/helper 降到 16.43 ns/helper，解释了普通 array 与
   per-CPU array 时间差的约 46%。
3. **BPFtime per-CPU hash 的主因不是 `sched_getcpu()`，而是数据结构实现。**
   它使用共享内存中的 `boost::unordered_map<vector, vector>`，每次调用还要
   访问 CPU 对应的 key/value 模板、复制 key、执行 `offset_ptr` 换算、vector
   哈希与逐字节比较。固定 CPU 只使 lookup 从 135.99 ns/helper 降到
   132.85 ns/helper，只解释约 4% 的额外时间。
4. **Jetson 上没有出现低 IPC 或 cache-miss 主导的迹象。** BPFtime per-CPU
   hash lookup 的 IPC 为 3.33，高于 kernel 的 2.78；主要问题是每个 helper
   执行约 792 条指令，而 kernel 约 140 条。也就是说它慢在执行了更多软件路径，
   不是 ARM 核心停顿得更严重。

因此当前最准确的表述是：

> ARM64 上较大的 BPFtime/kernel per-CPU 差距，首先是 kernel 原生 per-CPU
> 路径非常轻；BPFtime array 额外承担用户态 CPU 查询和索引成本，BPFtime hash
> 又使用了远重于普通 hash 的 Boost.Interprocess 通用容器路径。现有数据不能
> 把该现象归因于 ARM ISA 本身。

## 同一 Jetson 上的时间与指令证据

测试平台为 Jetson Orin Nano（Cortex-A78AE），CPU 3 固定运行，观测频率
1.728 GHz。每个 BPF benchmark 程序在一次触发中调用目标 helper 1,000 次；
下表已除以 1,000，单位为单次 helper。hash lookup 在测量前显式 update，
保证比较的是命中路径。

| 操作 | Kernel ns/helper | BPFtime ns/helper | B/K | Kernel instructions/helper | BPFtime instructions/helper | 指令倍率 |
|---|---:|---:|---:|---:|---:|---:|
| array lookup | 3.815 | 14.353 | 3.76× | 23.19 | 121.76 | 5.25× |
| per-CPU array lookup | 4.402 | 19.080 | 4.33× | 32.21 | 158.76 | 4.93× |
| array update | 13.087 | 18.093 | 1.38× | 93.20 | 168.76 | 1.81× |
| per-CPU array update | 16.578 | 22.573 | 1.36× | 94.22 | 208.76 | 2.22× |
| hash lookup（hit） | 28.414 | 58.687 | 2.07× | 134.68 | 341.00 | 2.53× |
| per-CPU hash lookup（hit） | 29.055 | 135.662 | 4.67× | 139.75 | 791.56 | 5.66× |
| hash update（已有 key） | 93.941 | 55.768 | 0.59× | 415.13 | 376.52 | 0.91× |
| per-CPU hash update（已有 key） | 68.051 | 211.572 | 3.11× | 287.81 | 1,241.19 | 4.31× |

这里最关键的对照不是单独看 `B/K`，而是看 per-CPU 相对普通 map 增加了什么：

- kernel hash lookup：`134.68 → 139.75`，只增加约 **5.1 条指令/helper**；
- BPFtime hash lookup：`341.00 → 791.56`，增加约 **450.6 条指令/helper**；
- BPFtime hash update：`376.52 → 1,241.19`，增加约
  **864.7 条指令/helper**。

kernel 的 per-CPU hash update 甚至比普通 hash update 更快。命中更新时，
kernel 可以直接修改当前 CPU 的 value slot，避免普通共享 hash value 更新路径
中的部分同步成本。BPFtime 当前实现没有得到这种收益，反而进入更通用的共享内存
vector/hash 路径。

## per-CPU array：CPU 查询是主要组成，但不是全部

`per_cpu_array_map_impl::elem_lookup/update()` 每次调用都经过：

```text
my_sched_getcpu()
  -> sched_getcpu()
  -> key 范围检查
  -> key × ncpu × value_size + cpu × value_size
  -> lookup 返回地址；update 再复制 value
```

Jetson 上直接循环调用 `sched_getcpu()` 1 亿次，5 轮平均约
**3.59 ns/call**，`perf stat` 约为 **32 条指令/call**。这与 BPFtime
per-CPU array lookup 相比普通 array 多出的约 37 条指令/helper 在数量级上吻合。

为了避免只靠相减推断，测试使用 `taskset -c 3` 固定 victim，并通过诊断用
`LD_PRELOAD` shim 让 `sched_getcpu()` 直接返回 3。5 轮均值为：

| 操作 | 原路径 ns/1000 helpers | 固定 CPU ns/1000 helpers | 降幅 | CV（原 / 固定） |
|---|---:|---:|---:|---:|
| per-CPU array lookup | 18,173.72 | 16,428.11 | 9.61% | 0.327% / 0.118% |
| per-CPU array update | 22,521.85 | 20,356.07 | 9.62% | 0.138% / 0.262% |

固定 CPU 后仍比普通 array lookup 的约 14.35 ns/helper 重约 2.08 ns/helper。
这部分来自 CPU 维度的边界、乘加索引、共享内存 `offset_ptr` 地址解析，以及
per-CPU map 类型分派。因而不能把 array 的全部差距都归到 `sched_getcpu()`。

这个 shim 只用于路径归因，不是生产修复方案。直接缓存 CPU 编号会涉及线程迁移
时的语义正确性，不能仅凭本测试删除查询。

## per-CPU hash：主要成本是通用共享内存容器

普通 BPFtime hash 已经使用专用的定长实现：

```text
连续 bytes buffer
  -> 固定大小 key/value
  -> 开放寻址和线性探测
  -> memcmp / memcpy
```

per-CPU hash 则采用：

```text
sched_getcpu()
  -> key_templates[cpu]
  -> 将 4-byte key assign 到共享内存 vector
  -> Boost.Interprocess unordered_map 查找
     -> offset_ptr 换算
     -> vector hasher
     -> vector operator== / 逐字节比较
  -> value vector 中选择 cpu × value_size 的区域
```

update 还会访问 `single_value_templates[cpu]`、复制 value，并在插入新 key
时构造覆盖所有 CPU 的完整 value vector。

`perf record` 的 per-CPU hash lookup 热点与该代码结构一致，集中在：

- `boost::interprocess::offset_ptr` 地址换算；
- `bytes_vec_hasher::operator()`；
- `boost::container::vector::operator==`；
- Boost unordered-map 的 `find_node`；
- update 路径中的 vector iterator copy。

固定 `sched_getcpu()` 后，per-CPU hash lookup 5 轮均值只从
135,987.08 降到 132,851.31 ns/1000 helpers，下降 **2.31%**。相对于普通
hash lookup 约 58.69 ns/helper 的差距，CPU 查询只解释约 **4.1%**，剩余约
96% 属于 per-CPU hash 的容器、共享内存指针和复制路径。

## kernel 为什么在 ARM64 上尤其轻

kernel `perf record` 显示：

- 普通 array lookup 的主要执行体是 BPF JIT 程序本身；
- per-CPU array lookup 进入 `percpu_array_map_lookup_elem`；
- 普通 hash lookup 进入 `lookup_nulls_elem_raw` /
  `__htab_map_lookup_elem`；
- per-CPU hash lookup 仍复用同一 hash 查找主体，只在返回 value 时进入
  `htab_percpu_map_lookup_elem` 选择当前 CPU 的 slot。

kernel 在 BPF 程序上下文中已经具有稳定的当前 CPU 语义，可以使用内核
per-CPU 基址直接得到当前 CPU 的 value 地址；它不需要从用户态调用
`sched_getcpu()`，也不需要 Boost 共享内存对象、通用 vector key 或
`offset_ptr`。

这解释了为什么 Jetson 上 kernel 普通 hash 与 per-CPU hash lookup 几乎一样：
28.41 与 29.06 ns/helper。BPFtime 则不是在普通 hash 上增加一个轻量的 value
slot 选择，而是换成了另一套更重的数据结构。

## 跨架构结果应解释为“kernel 分母更快”

此前 GitHub Actions 同一 workflow 的 10 轮均值显示：

| 操作 | ARM Kernel | ARM BPFtime | ARM B/K | x64 Kernel | x64 BPFtime | x64 B/K |
|---|---:|---:|---:|---:|---:|---:|
| per-CPU hash lookup | 14.44 µs | 61.89 µs | 4.29× | 42.00 µs | 73.88 µs | 1.76× |
| per-CPU array lookup | 1.74 µs | 11.31 µs | 6.51× | 3.11 µs | 13.72 µs | 4.41× |
| per-CPU hash update | 36.75 µs | 109.10 µs | 2.97× | 101.79 µs | 116.93 µs | 1.15× |

BPFtime ARM64 的绝对时间在这些代表项中低于 x64，但 ARM kernel 快得更多。
因此“ARM 的 B/K 倍率更大”与“BPFtime ARM 绝对更慢”不是同一个结论。

本轮 Jetson `perf` 给出的进一步解释是：kernel per-CPU 路径的新增指令极少，
而 BPFtime userspace per-CPU hash 为同一语义执行了数百条额外指令。这个代码
路径差异足以解释当前平台上的主要现象，无需假设 ARM cache 或分支预测异常。

## 与 event-output affinity 删除无关

本次 map 路径调用的是 `sched_getcpu()`，没有调用：

```text
sched_getaffinity()
sched_setaffinity()
```

此前从 `bpf_perf_event_output()` 删除的临时 pin/restore 发生在 perf-event
输出 helper 中；`benchmark/uprobe` 的这些 per-CPU map 项不执行该 helper。
两者名称里都涉及 CPU，但不是同一条实现路径。

## 测量范围、稳健性与限制

- Jetson 聚焦测试使用 `ghcr.io/plsy1/bpftime:original`，镜像源 revision 为
  `52ddee18951210f353979999f5c011b1ee5769ae`。
- 镜像和当前 `codex/official-no-btf` 工作树中的
  `per_cpu_array_map.cpp`、`per_cpu_hash_map.cpp` SHA-256 完全一致，因此后续
  perf record 对齐、event-output affinity、SIGSEGV handler 修改不影响本次
  map 归因。
- 聚焦 timing 基线各跑 5 轮；`sched_getcpu()` 固定 CPU 变体各跑 5 轮；
  hardware counter 使用更长单轮，以降低 agent 初始化的占比。
- BPFtime 与 kernel 的 hash lookup 都在测量前显式 update，保证为命中路径。
- `perf stat` 统计整个 victim 进程；表中会包含少量进程启动和一次 uprobe
  调度成本。两种 map 在相同方式下测量，而且一次触发包含 1,000 次 helper，
  因此这些固定成本对 map 间差值较小。
- 本文不使用 delete 项做性能归因：per-CPU array delete 本来就不支持；
  BPFtime per-CPU hash delete 当前也没有实现正常的整条 entry 删除语义。
- Actions ARM64 与 x64 是不同云 runner、微架构和 BPF JIT 后端。现有数据能
  说明当前 runner 的结果与代码路径，不能单独证明普适的 ARM-vs-x64 ISA
  因果。

## 下一步

如果后续目标从“查明”转为“优化”，优先级应为：

1. 先做不合入代码的 per-CPU hash 诊断变体：复用普通 fixed-size hash，
   将 value 扩展为 `value_size × ncpu`，只在命中后选择当前 CPU 的 slot。
   这能直接验证 Boost.Interprocess 通用容器占当前差距的比例。
2. 在同一个 x64 runner 上运行本轮 focused victim + `perf stat`，获得
   instructions/helper、cycles/helper 和 IPC，区分代码量差异与微架构差异。
3. 单独修正并测试 per-CPU hash delete 语义；正确性问题不应与性能优化混在
   一个结论中。
4. 若要减少 array 的 `sched_getcpu()` 成本，需要先定义 BPFtime 在一次 BPF
   程序执行期间的 CPU 稳定语义，再讨论缓存 CPU 编号，不能直接删除查询。

当前阶段已经可以回答“为什么 ARM64 上相对 kernel 慢”：**kernel 的 per-CPU
地址选择近似增量成本；BPFtime array 每次查询 CPU，BPFtime hash 还换用了重量级
共享内存通用容器。ARM64 更大的倍率主要是 kernel 分母特别快，而不是 BPFtime
ARM64 绝对执行普遍更差。**
