# BPFtime `bpf_perf_event_output` 临时 CPU affinity 删除说明

**日期：** 2026-07-28
**对应提交：** `076e3e4`（`Remove per-event CPU affinity pinning from bpf_perf_event_output`）
**适用代码：** `bpftime-offical-no-btf`，分支 `codex/official-no-btf`

## 结论

当前 BPFtime 的 software perf-event 路径中，`bpf_perf_event_output()` 原有的
临时 CPU affinity 操作没有保护 map lookup、producer buffer 或并发安全：

```text
sched_getaffinity()
sched_setaffinity()   # 临时固定到 current_cpu
...
sched_setaffinity()   # 恢复原 affinity
```

删除它是合理的。正确性依据来自数据结构和执行路径，而不是 Jetson 特有行为，
因此该结论适用于 ARM、x64 等平台；不同平台只是在性能收益大小上有差异。

对当前 `ssl-nginx/sslsniff`，删除后不会改变事件的 PID/TID、SSL 明文、事件长度、
perf handler 或 producer shard。事件守恒审计约为 `2.001 events/request`，
没有观察到功能回归。

## 删除了什么，保留了什么

原实现每次输出事件都执行：

```cpp
int32_t current_cpu = my_sched_getcpu();

cpu_set_t mask, orig;
CPU_ZERO(&mask);
CPU_SET(current_cpu, &mask);
sched_getaffinity(0, sizeof(orig), &orig);
sched_setaffinity(0, sizeof(mask), &mask);

// 查询 map、选择 perf handler、写入事件

sched_setaffinity(0, sizeof(orig), &orig);
```

修复后保留了 CPU 快照：

```cpp
int32_t current_cpu = my_sched_getcpu();
```

只删除了：

- `sched_getaffinity()`：保存线程原来的 affinity mask；
- 第一次 `sched_setaffinity()`：临时把线程固定到快照 CPU；
- 第二次 `sched_setaffinity()`：恢复旧 affinity mask。

因此，该修复不是删除 CPU 选择逻辑，而是删除选择 CPU 之后的临时 pin/restore。

## 事件输出路径

`sslsniff.bpf.c` 使用：

```c
bpf_perf_event_output(ctx, &perf_SSL_events, BPF_F_CURRENT_CPU,
                      data, EVENT_SIZE(...));
```

BPFtime 中对应的数据路径为：

```text
nginx worker 执行 SSL_read/SSL_write
  -> BPF probe 生成事件
  -> bpf_perf_event_output()
  -> my_sched_getcpu() 取得 CPU 快照
  -> map[current_cpu] 选择 perf_handler_fd
  -> bpftime_perf_event_output()
  -> software_perf_event_data::output_data()
  -> get_current_thread_shard()
  -> software_perf_event_buffer::output_data()
  -> append perf record
```

关键点：

1. `current_cpu` 在原临时绑核之前就已经保存为局部变量；
2. perf-event array lookup 使用 `&current_cpu` 这个快照作为 key；
3. lookup 完成后得到确定的 `perf_handler_fd`；
4. 下层输出路径不再调用 `sched_getcpu()`；
5. producer shard 根据 `(pid, tid)` 选择，而不是根据当前执行 CPU 选择。

所以，无论线程随后是否迁移，map key、perf handler 和 producer shard 都不会改变。

## 什么是 ring 写入

ring buffer 是循环使用的共享内存。BPF probe 产生事件后，producer 把一条 perf
record 写入 ring，`sslsniff` 再从 ring 中读取：

```text
nginx/BPF producer
  -> 写入 perf_event_header
  -> 写入 raw payload 长度
  -> 写入 PID/TID、进程名、SSL 明文等事件数据
  -> 更新 ring 的 data_head

sslsniff consumer
  -> 读取 data_head/data_tail
  -> 取出完整 record
  -> 调用 handle_event()
```

BPFtime 的 software perf-event 实现有两级缓冲：

```text
nginx worker
  -> per-thread producer shard
  -> drain 到 consumer ring
  -> libbpf perf_buffer
  -> sslsniff handle_event()
```

“ring 写入在哪个 CPU 执行”只表示 nginx worker 在执行内存复制和更新 ring head
时运行在哪个 CPU。它不改变事件的 PID/TID 或 SSL payload。

## 为什么临时绑核不提供必要保护

### 1. 它不影响 map lookup

执行顺序是：

```text
current_cpu = my_sched_getcpu()
  -> 临时绑核
  -> map lookup 使用之前保存的 current_cpu
```

map key 在绑核前已经确定。临时绑核不会改变选择哪个 map entry。

### 2. ring 写入按线程分片，不按 CPU 分片

`software_perf_event_data::get_current_thread_shard()` 使用：

```text
getpid() + current_thread_id()
```

同一个线程迁移到其他 CPU 后仍然写入自己的 producer shard；不同线程使用不同
shard。shard 的创建、查找和回收另有 process-shared spinlock 保护。

### 3. affinity 不是锁

把线程固定到某个 CPU 不等于关闭抢占。两个线程即使都固定到同一个 CPU，也仍然
可以被调度器交错执行。因此 affinity 不能替代互斥锁，也不能保护一个多 producer
共享 ring。

当前实现不依赖 affinity，而是通过 per-thread producer shard 避免多 producer
直接写同一个 shard。

### 4. kernel-user shared perf 路径也不依赖这次 pin

`BPF_MAP_TYPE_KERNEL_USER_PERF_EVENT_ARRAY` 路径先写入 user ring buffer，随后由
kernel transporter BPF 程序以 `BPF_F_CURRENT_CPU` 输出到内核 perf-event array。
最终 CPU 归属由 transporter 在内核执行时决定，用户态 helper 的临时 affinity
不能为这条路径提供正确性保证。

## 对 CPU 分析程序有没有影响

需要区分“事件的逻辑 CPU 标签”和“ring memcpy 实际运行的 CPU”。

假设：

```text
线程在 CPU 2 调用 helper
  -> current_cpu 快照为 2
  -> 线程迁移到 CPU 3
  -> 仍使用 map[2] 对应的 perf handler 输出
```

删除 affinity 后，ring memcpy 可能在 CPU 3 执行，但消费者看到的 CPU 归属仍由
快照和所选 perf buffer 决定。对只关心事件内容的程序没有影响。

典型的 CPU 敏感程序包括：

- `sched_switch`/`sched_wakeup` 调度时间线分析；
- IRQ/softirq 的 per-CPU 负载统计；
- XDP/TC 的 per-CPU 或 RX-queue 流量分析；
- CPU 热点、任务迁移和 NUMA 局部性分析；
- 依赖 per-CPU 事件顺序的重建程序。

这些程序确实需要一致的 CPU attribution，但原来的临时绑核仍不能完整保证它：

```text
BPF 程序开始执行
  -> 前面的 helper/map 操作可能已经发生
  -> 线程可能迁移
  -> 到 bpf_perf_event_output() 才取得快照并临时绑核
```

原 affinity 只覆盖最后的 event-output helper，无法保证：

- probe 触发 CPU 与 helper CPU 相同；
- 整个 BPF 程序执行期间不迁移；
- `bpf_get_smp_processor_id()`、per-CPU map 与 event output 使用同一个 CPU。

如果未来需要严格的 CPU 语义，正确方向是：

1. 在 probe/BPF 程序调度入口保存统一的 execution-CPU 快照；
2. 全部 CPU 相关 helper 和 per-CPU map 使用同一快照；
3. 必要时增加 migration detection/retry；
4. 将 CPU 明确写入事件 payload，供消费者交叉校验。

仅在 `bpf_perf_event_output()` 内临时修改 affinity 不能替代上述机制。

### 当前 `sslsniff` 不依赖 CPU attribution

`sslsniff.c` 的：

```c
static void handle_event(void *ctx, int cpu, void *data, __u32 data_size)
```

虽然接收 `cpu` 参数，但处理 SSL 事件时完全没有读取它。CPU 只在 lost-event
日志中用于打印。因此当前 benchmark 不依赖 per-CPU 归属。

## 加回 affinity 会发生什么

对当前 `sslsniff` 的事件结果，加回前后基本相同：

- `current_cpu` 快照相同；
- map entry 和 perf handler 相同；
- producer shard 相同；
- PID/TID 和 SSL payload 相同。

但加回后会重新引入：

1. 每事件一次 `sched_getaffinity()` 和两次 `sched_setaffinity()`；
2. Jetson 上约 `3.1–6.7 μs/event` 的固定成本；
3. 吞吐下降和更高方差；
4. map 查询或 lookup 失败时没有恢复 affinity、线程可能被永久钉核的错误路径；
5. 使用旧 mask 恢复 affinity、覆盖应用自身 affinity 修改的竞态；
6. 对调度和 CPU 分析程序的额外观测扰动。

所以，加回旧代码不会获得完整的 CPU 一致性，却会恢复已确认的性能和行为问题。

## 平台无关性

删除 affinity 的正确性依据是：

```text
CPU snapshot map key
+ per-thread producer shard
+ 下层路径不读取当前 CPU
```

这些条件来自 BPFtime 的数据结构，与 ARM 或 x64 指令集无关。

- **Jetson/ARM：** affinity syscall 成本高，删除后收益明显；
- **x64 hosted VM：**同 VM A/B 中，删除后 BPFtime 吞吐提高约 37%～41%；
- **macOS：**项目中的 `sched_getaffinity()`/`sched_setaffinity()` 是兼容性空实现，
  删除基本没有运行时影响。

因此，“是否需要”是平台无关的代码结论，“能提高多少性能”是平台相关的测量结果。

## 验证证据

16 B 事件守恒审计：

```text
original:
  events       200162
  wrk requests 100033
  ≈ 2.001 events/request

no-affinity:
  events       248082
  wrk requests 123942
  ≈ 2.001 events/request
```

两组 nginx worker 的 `nr_migrations` 均为 0。删除 affinity 后事件比例保持一致，
吞吐提高，消费者内容正常。

短测消融中，affinity 序列占完整 event-output 段成本约 72%；删除后 16 B 和
256 KB 均由落后 kernel 转为领先。fair 口径三轮验证中：

| Payload | BPFtime vs kernel-fair |
|---|---:|
| 16 B | +7.58% |
| 256 KB | +9.81% |

## 独立的已知边界

当前 `bpf_perf_event_output()` 忽略传入的 `flags`，总是使用
`my_sched_getcpu()` 的结果查 map。这意味着显式 `BPF_F_INDEX_MASK` 还没有正确
实现。`sslsniff` 使用的是 `BPF_F_CURRENT_CPU`，不受显式索引问题影响。

这是独立的 helper 语义问题，不能通过恢复临时 affinity 解决。

## 推荐表述

> BPFtime 原先在每次 `bpf_perf_event_output` 中保存线程 affinity、临时固定到
> 当前 CPU，再恢复旧 affinity。代码走读确认，CPU 号在绑核前已经快照并作为
> perf-event array 的 lookup key，而事件写入使用按 `(pid, tid)` 隔离的
> per-thread producer shard，后续路径不依赖线程继续运行在该 CPU；affinity
> 也不提供互斥。因此该序列没有保护实际数据结构。删除后事件守恒和功能保持，
> 同时消除了显著的跨平台性能成本以及两个 affinity 状态错误。
