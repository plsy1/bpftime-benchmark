# BPFtime uprobe map 路径调查指南

日期：2026-07-31

## 1. 调查目标

本轮调查围绕 `benchmark/uprobe`，回答三个问题：

1. 一次 map helper 调用的成本分别落在哪一层；
2. `lookup`、`update`、`delete` 的瓶颈是否相同；
3. per-CPU map 相比普通 map 多出的成本是什么，以及 ARM64/x64 的差异来自哪里。

最终需要得到一份可量化的成本账本，而不是只根据某个顶层 benchmark 数字猜测实现热点。

本轮先查明问题，不直接做优化；也不把 `ssl-nginx` 的网络、perf buffer 输出和消费者开销混进来。

## 2. 先建立完整调用链

BPFtime userspace map helper 的主要路径如下：

```text
uprobe 触发
  ↓
Frida hook / BPF 程序调度
  ↓
LLVM JIT 生成的 BPF 程序
  ↓
bpftime_map_{lookup,update,delete}_elem_helper()
  ↓
bpftime_shm::bpf_map_{lookup,update,delete}_elem()
  ↓
try_get_map_handler()
  ├─ is_map_fd()
  ├─ handler_manager::get_handler()
  └─ std::variant 取出 bpf_map_handler
  ↓
bpf_map_handler::map_{lookup,update,delete}_elem()
  ├─ should_lock 判断
  ├─ map type switch
  └─ 转换为具体 map 实现
  ↓
array / hash / per-CPU array / per-CPU hash 实现
```

边界要分清：

- BPF 程序及其 helper 调用指令由 LLVM JIT 生成；
- helper 入口之后的大部分 map 路径是普通 C++ runtime，不属于 JIT；
- runtime 当前使用 LTO 编译，但 LTO 不等于 JIT；
- kernel eBPF 是否启用 JIT 是另一套独立配置。

## 3. 源码阅读顺序

按下面顺序阅读，可以从 benchmark 一直追到具体数据结构。

### 3.1 benchmark 定义

文件：

- `benchmark/uprobe/uprobe.bpf.c`

重点看：

- `DEFINE_MAP_OPERATIONS`
- `bpf_map_lookup_elem`
- `bpf_map_update_elem`
- `bpf_map_delete_elem`
- 每个 BPF 程序内部的 1000 次 helper 循环
- 普通 array/hash 和 per-CPU array/hash 各自使用的 map 定义

先记录每个测试的：

- map 类型；
- key/value 大小；
- helper 调用次数；
- key 的变化方式；
- 执行前 map 是空、已存在还是已删除状态。

### 3.2 helper 注册和入口

文件：

- `runtime/src/bpf_helper.cpp`
- `runtime/src/bpftime_prog.cpp`
- `vm/vm-core/src/ebpf-vm.cpp`
- `vm/compat/llvm-vm/compat_llvm.cpp`
- `vm/llvm-jit/src/vm.cpp`

重点函数：

- `bpftime_map_lookup_elem_helper`
- map update/delete 对应 helper
- `bpftime_prog_register_raw_helper`
- `ebpf_register`
- `register_external_function`

这一段用于确认：

- JIT 程序调用的真实函数地址；
- helper wrapper 是否做了参数转换或额外检查；
- 从 JIT 到 C++ runtime 的 ABI 边界。

### 3.3 fd、共享内存和 handler 分发

文件：

- `runtime/src/bpftime_shm_internal.cpp`
- `runtime/src/handler/handler_manager.hpp`
- `runtime/src/handler/handler_manager.cpp`

重点函数和结构：

- `bpftime_shm::bpf_map_lookup_elem`
- update/delete 对应入口
- `try_get_map_handler`
- `is_map_fd`
- `handler_manager::get_handler`
- `handler_variant`
- `handler_variant_vector`

这里需要检查：

- 一次 helper 是否重复读取同一个 handler；
- fd 校验、边界检查和 `std::variant` 访问的次数；
- Boost.Interprocess 容器和 `offset_ptr` 是否进入热路径；
- 是否发生锁判断或共享内存元数据访问。

### 3.4 map handler

文件：

- `runtime/src/handler/map_handler.cpp`

重点函数：

- `bpf_map_handler::map_lookup_elem`
- `bpf_map_handler::map_update_elem`
- `bpf_map_handler::map_delete_elem`

重点看：

- `should_lock`；
- map type `switch`；
- 到具体 map 实现的 `static_cast`；
- 不同操作的错误处理；
- 普通 map 和 per-CPU map 是否经过相同公共路径。

### 3.5 具体 map 实现

普通 array：

- `runtime/src/bpf_map/userspace/array_map.cpp`

普通 hash：

- `runtime/src/bpf_map/userspace/fix_hash_map.cpp`
- `runtime/src/bpf_map/bpftime_hash_map.hpp`

per-CPU array：

- `runtime/src/bpf_map/userspace/per_cpu_array_map.hpp`
- `runtime/src/bpf_map/userspace/per_cpu_array_map.cpp`

per-CPU hash：

- `runtime/src/bpf_map/userspace/per_cpu_hash_map.hpp`
- `runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp`

分别确认：

- array 的边界检查、地址计算和 value copy；
- hash 的哈希、探测、key 比较、插入和删除；
- per-CPU array 的 `sched_getcpu()`、CPU 下标和地址计算；
- per-CPU hash 的 Boost unordered_map、共享内存 vector、key/value 复制和比较。

## 4. 分层测量设计

不要一开始只测完整 uprobe benchmark。应把同一次操作拆成四层：

| 层级 | 测量入口 | 包含的成本 |
|---|---|---|
| L0 | 直接调用具体 `*_map_impl::elem_*` | 具体数据结构和最底层操作 |
| L1 | `bpf_map_handler::map_*_elem` | L0 + type switch、cast、锁判断 |
| L2 | `bpftime_shm::bpf_map_*_elem` | L1 + fd、manager、variant、共享内存分发 |
| L3 | 现有 BPF JIT helper benchmark | L2 + helper wrapper、JIT ABI、hook/dispatch |

各层差值可近似解释为：

```text
L1 - L0 ≈ map handler 的分发、类型判断和锁判断
L2 - L1 ≈ fd、handler manager、variant 和共享内存访问
L3 - L2 ≈ JIT helper ABI、wrapper 以及剩余调度成本
```

L3 的单 helper 成本不能直接拿一次 uprobe 的总时间。现有 BPF 程序每次触发执行 1000 次 helper，应先减去空 probe：

```text
单次 helper 近似成本
  = (操作测试每次触发成本 - Empty Probe 每次触发成本) / 1000
```

直接测试 L0–L2 时要防止编译器删掉循环：

- 测试入口使用 `noinline` 或函数指针；
- lookup 结果写入 `volatile` sink；
- update/delete 的返回值被实际消费；
- 测试开始前完成内存分配和 map 初始化；
- JIT 编译和首次触发作为 warm-up，不计入正式结果。

## 5. 操作矩阵和执行顺序

### 阶段 A：普通 array

先用普通 array 做公共路径的干净载体，因为它的具体实现最简单。

依次测：

1. lookup；
2. update，固定使用 `BPF_ANY` 和 8 字节 value；
3. delete/error 路径。

array 的 delete 本身不受支持，所以该项只能用于测量公共分发和错误返回，不能解释为真实删除性能。

普通 array 的 L0 很轻。如果 L2/L3 仍明显偏重，公共 helper/handler 路径就是首要目标。

### 阶段 B：普通 hash

必须分开 map 状态：

1. lookup hit；
2. lookup miss；
3. update existing；
4. update insert；
5. delete hit；
6. delete miss。

每种状态都要显式准备，不能依赖上一轮测试遗留的 map 内容。

### 阶段 C：per-CPU array

测：

1. lookup；
2. update。

然后与相同配置的普通 array 相减：

```text
per-CPU array lookup - ordinary array lookup
  ≈ sched_getcpu + CPU 下标和 per-CPU 地址计算

per-CPU array update - ordinary array update
  ≈ 上述 per-CPU 定位成本及其与 copy 路径的交互
```

per-CPU array delete 同样不支持，不纳入有效操作比较。

### 阶段 D：per-CPU hash

测：

1. lookup hit/miss；
2. update existing/insert。

再与普通 hash 的同状态操作相减。该差值不只是 `sched_getcpu()`，还会包含 Boost.Interprocess unordered_map、共享内存 vector、`offset_ptr`、key/value 复制和比较。

当前 per-CPU hash delete 存在语义问题：实现没有正常 erase，并且清零范围也需要单独核对。修复和验证语义之前，不用它得出 delete 性能结论。

### 阶段 E：跨架构复测

只在 Jetson 上完成分层定位后，把最小化、聚焦后的测试搬到 x64：

- 相同源码提交；
- 相同编译开关；
- 相同 map 参数和循环次数；
- 相同 hit/miss/insert/delete 状态；
- 分别报告绝对 `ns/helper` 与相对 kernel 倍率。

不能只比较 “BPFtime 比 kernel 快/慢百分之多少”，因为 x64 与 ARM64 的 kernel 基线成本不同，会产生明显的分母效应。

## 6. 必须控制的 map 状态

现有 `uprobe.bpf.c` 的 BPF 程序内部会遍历 key `0..999`，这会影响 hash 测试语义：

- hash update 的第一次触发可能以插入为主；
- 后续触发会变成更新已存在元素；
- hash delete 的第一次触发删除已有项后，后续大量操作可能都是 miss。

因此：

- lookup hit：提前填满相同 key；
- lookup miss：使用确定不存在的 key；
- update existing：提前填满；
- update insert：每轮使用空 map，或每轮重建 map；
- delete hit：每轮重新填充待删除 key；
- delete miss：确保 key 从未插入；
- 测试报告必须标明 map 状态，不能只写 “hash update/delete”。

## 7. 构建和运行口径

当前仓库的 `make benchmark` 明确使用：

```text
BPFTIME_LLVM_JIT=1
BPFTIME_ENABLE_LTO=1
ENABLE_PROBE_WRITE_CHECK=0
ENABLE_PROBE_READ_CHECK=0
```

解释：

- BPFtime BPF 程序默认走 LLVM JIT；
- runtime 使用 LTO；
- benchmark 默认关闭 probe read/write 安全检查；
- 关闭检查后，读写 helper 主要走 `memcpy`，SIGSEGV handler 路径不会被这组 benchmark 覆盖。

执行前还要：

- clean build，或检查 `CMakeCache.txt`，避免缓存保留旧开关；
- 记录 commit、镜像 tag、编译器、LLVM 版本和 Boost 版本；
- 固定 victim CPU；
- 固定 CPU/GPU/EMC 频率并记录；
- 使用相同的 value 大小、循环次数和进程身份；
- BPFtime 应统计执行 JIT 程序的 victim/agent 进程，不能只统计 syscall server；
- kernel BPF JIT 状态单独记录，不能由 BPFtime 的构建选项推断。

## 8. 计数器和 profile

每个聚焦测试至少执行 5 轮，先报告：

- `ns/helper`；
- `cycles/helper`；
- `instructions/helper`；
- 标准差或变异系数。

建议的 `perf stat` 事件：

```text
task-clock
cycles
instructions
branches
branch-misses
cache-references
cache-misses
```

IPC 只作为辅助指标。路径是否重，主要看每 helper 的绝对指令和周期。

只有在某一层差值已经明确偏大后，再对该层运行 `perf record/report`。这样热点才能回答“这一层内部什么最重”，而不是得到一个混合了 hook、JIT、map 和测试框架的总火焰图。

## 9. 结果表模板

### 9.1 分层成本

| Map | Operation/state | L0 ns/helper | L1 | L2 | L3 | 主要增量层 |
|---|---|---:|---:|---:|---:|---|
| array | lookup hit | | | | | |
| array | update existing | | | | | |
| hash | lookup hit | | | | | |
| hash | lookup miss | | | | | |
| hash | update insert | | | | | |
| hash | update existing | | | | | |
| hash | delete hit | | | | | |
| hash | delete miss | | | | | |

### 9.2 per-CPU 增量

| Operation/state | Ordinary ns/helper | per-CPU ns/helper | per-CPU 增量 | 增量比例 |
|---|---:|---:|---:|---:|
| array lookup | | | | |
| array update | | | | |
| hash lookup hit | | | | |
| hash lookup miss | | | | |
| hash update existing | | | | |
| hash update insert | | | | |

### 9.3 跨架构

| Architecture | Map/operation | Kernel ns/helper | BPFtime ns/helper | BPFtime / kernel |
|---|---|---:|---:|---:|
| ARM64 | | | | |
| x64 | | | | |

## 10. 当前已有证据和待验证假设

已有结果提示：

- 普通 array 的具体操作很轻，公共 helper/handler 分发可能占较大比例；
- per-CPU array 的额外成本较稳定，主要候选是 `sched_getcpu()` 和 per-CPU 地址计算；
- per-CPU hash 的增量明显更大，主要候选是 Boost.Interprocess unordered_map、共享内存 vector、`offset_ptr`、key/value 复制、哈希和比较；
- x64 上 BPFtime 相对 kernel 的倍率更好，很大程度上是 x64 kernel uprobe/map 路径本身更重，而不一定是 BPFtime 在 ARM64 上的绝对成本更高。

这些内容是已有证据支持的方向，但仍应通过 L0–L3 分层测试确认，不能把候选热点直接当作最终归因。

## 11. 本轮明确排除的内容

以下问题属于其他路径，不混入本轮 map 调查：

- `ssl-nginx` 的 `bpf_perf_event_output`；
- software perf event 的临时 CPU affinity；
- perf record 8 字节对齐；
- `perf_buffer__poll` 和 sslsniff userspace 消费；
- Docker network namespace、netfilter 和 Tailscale；
- payload 输出路径；
- probe read/write 检查开启后的 SIGSEGV handler。

如果后续要研究上述问题，应分别建立独立配置和对照组。

## 12. 推荐执行清单

```text
0. 固定源码、构建开关、频率、CPU affinity 和测试参数
1. 按调用链完成源码阅读，记录每层函数和数据结构
2. 建立 L0/L1/L2 的最小 direct harness
3. 先测普通 array 的 lookup/update/delete-error
4. 测普通 hash 的 hit/miss/insert/existing/delete 状态
5. 测 per-CPU array，并与普通 array 做同操作相减
6. 测 per-CPU hash，并与普通 hash 做同状态相减
7. 对最大增量层运行 perf record，定位内部热点
8. 把最小聚焦测试搬到 x64，保持完全相同口径
9. 汇总绝对成本、分层差值、per-CPU 增量和跨架构倍率
```

## 13. 完成标准

满足以下条件后，才能认为本轮调查完成：

- 每个性能数字都标明 map 类型、操作、map 状态、层级和单位；
- lookup/update/delete 均已覆盖，unsupported 操作被明确排除；
- hash 的 hit/miss、insert/existing 没有混在一个平均值中；
- 能用 L0–L3 的差值说明成本主要落在哪一层；
- 能区分普通 map 公共成本与 per-CPU 的额外成本；
- 能分别解释 ARM64/x64 的绝对成本和相对 kernel 倍率；
- 结论不依赖 `ssl-nginx`、网络 namespace 或 perf buffer 路径。
