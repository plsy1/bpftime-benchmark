# 为什么 `bpf_perf_event_output` 的 CPU 绑核序列是冗余的（附两个既有 bug）

本文解释 2026-07-27 短测判定"affinity 绑核序列功能冗余"的代码依据与实验证据。
代码引用基于 `bpftime-offical-no-btf`（分支 codex/official-no-btf，含对齐修复）。
配套数据见 `bpftime-official-no-btf-ssl-nginx-path-ablation-20260726.md` 的
"④ 段内部消融：短测结果"一节。

## 1. 这段代码在做什么

`runtime/src/bpf_helper.cpp:494-554`，每个 perf 事件执行一次：

```c
uint64_t bpf_perf_event_output(uint64_t ctx, uint64_t map, uint64_t flags,
			       uint64_t data, uint64_t size)
{
	int32_t current_cpu = my_sched_getcpu();      // :497 拍下 cpu 快照
	...
	cpu_set_t mask, orig;
	CPU_ZERO(&mask);
	CPU_SET(current_cpu, &mask);
	sched_getaffinity(0, sizeof(orig), &orig);    // :506 保存原 mask
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {  // :508 绑到 current_cpu
		...
	}
	...   // map 类型查询 + per-CPU lookup + ring 写入
	sched_setaffinity(0, sizeof(orig), &orig);    // :552 恢复原 mask
	return (uint64_t)ret;
}
```

意图：kernel 的 `bpf_perf_event_output` 运行在**关抢占**上下文，helper 期间
"当前 CPU"不变，per-CPU 逻辑天然安全；bpftime 在用户态无法关抢占，
于是用"绑核到当前 CPU"模拟。

成本（本机 Jetson Orin 实测）：`sched_setaffinity` 单次 ~1.4 µs（普通 syscall
284 ns 的 5 倍），整个序列无竞争 3.1 µs/事件、负载下 6.7 µs；占 helper 总耗时
约 72%（消融 12.59 pp / 17.51 pp，与 07-22 插桩的 70.9% 吻合）。

## 2. 为什么它保护不了任何东西

绑核可能想保护三件事，逐一检查：

### (a) per-CPU map lookup 的 key —— 不需要

```c
int32_t current_cpu = my_sched_getcpu();       // :497 快照在绑核【之前】
...
sched_setaffinity(0, sizeof(mask), &mask);     // :508 绑核
...
bpf_map_lookup_elem(fd, &current_cpu, false);  // :527 用的是快照值
```

lookup 的 key 是**局部变量里的快照**，不是"lookup 时刻所在的 CPU"。
线程中途迁移与否，lookup 行为完全相同——绑核没有改变任何归属判定。
归属早在 `:497` 就由快照定死了。

### (b) ring 写入的并发安全 —— 数据结构已经保证

写入链：`bpftime_perf_event_output`（`bpftime_shm.cpp:579`）→
`software_perf_event_data::output_data`（`perf_event_handler.cpp:601-604`）：

```c
int software_perf_event_data::output_data(const void *buf, size_t size)
{
	return get_current_thread_shard().buffer.output_data(buf, size);
}
```

写入目标是**按 (pid, tid) 选择的 per-thread producer shard**，不是 per-CPU
缓冲：同一线程无论跑在哪个核，写的都是自己的 shard；不同线程永远写不同
shard。"防止两个线程并发写同一 per-CPU 缓冲"的经典绑核理由在此架构下不存在。
shard 列表管理另有 `pthread_spinlock`（PTHREAD_PROCESS_SHARED）保护。

### (c) 互斥 —— 绑核本来就不提供

绑核不是锁：两个线程可以同时各自绑核，甚至绑到同一个核。若真存在共享数据
竞争，绑核完全挡不住。

### (d) KERNEL_USER 共享路径 —— 用户态绑核无效

`BPF_MAP_TYPE_KERNEL_USER_PERF_EVENT_ARRAY` 分支（`bpf_helper.cpp:540`）的
cpu 归属由 kernel 侧 transporter 程序以 `BPF_F_CURRENT_CPU` 在其触发 CPU 上
决定（`perf_event_array_kernel_user.cpp:390` 附近），用户态 pin 影响不到。

### 实验验证

no-affinity 变体（删除 :503-512 与 :552）短测：事件守恒 **2.001 事件/请求、
零丢失**（16B 审计：248,082 收 / 247,884 期望，多出部分为 handshake），
消费内容正常。去掉绑核，什么都没坏；吞吐 16B/256KB 均反超 kernel ~10%。

## 3. 两个既有 bug

### Bug 1：错误路径不恢复 affinity → 线程被永久钉死单核

绑核之后、恢复之前有两个提前 return：

```c
sched_setaffinity(0, ..., &mask);              // 已绑到单核
if (int err = bpftime_map_get_info(...); err < 0) {
	...
	return -1;                             // :520 ← 未恢复 orig！
}
...
if (val_ptr == nullptr) {
	...
	return (uint64_t)(-1);                 // :532 ← 未恢复 orig！
}
...
sched_setaffinity(0, ..., &orig);              // :552 仅正常路径可达
```

任一错误路径触发后，被追踪应用的该线程（如 nginx worker）**从此只能运行在
事件发生时所在的那一个核上**，没有任何代码会再恢复它。正常 benchmark 中
map 不出错所以不触发，但这是潜伏的持久性能事故。

### Bug 2：恢复使用陈旧 mask，与应用自身的 setaffinity 竞态

`:552` 恢复用的是本次事件开始时保存的 `orig`。若应用在这几微秒窗口内
自行修改了线程 affinity（nginx `worker_cpu_affinity`、运维 `taskset` 等），
恢复动作会用陈旧 mask 把应用的设置悄悄覆盖。热路径每秒数万事件，窗口常开。

这两个 bug 都随"删除绑核序列"自然消失——因此修复应定性为 **bugfix + perf**，
而非单纯性能优化。它们也与 aligned V4 的高 CV（~5%）和离群低值样本高度相关：
no-affinity 变体 CV 收敛至 1–2.7%，离群消失。

## 4. 诚实的边界：删除绑核后唯一的行为差异

**迁移窗口**：线程在拍快照（:497）之后、ring 写入之前被迁移，事件会记在
"旧 CPU"名下（影响 perf_buffer 回调的 cpu 参数与 per-CPU 顺序观感）。但：

1. **原实现同样存在该窗口**——快照先于绑核，绑核前就可能已迁移；绑核只是
   略微缩短窗口，从未消除它。删除绑核不引入新的语义类别。
2. 实测发生率≈0：16B 审计 10 秒约 20 万事件，worker `nr_migrations` = **0**。
3. 对 sslsniff 类工具，cpu 归属只影响回调参数，无正确性影响；且 shard+drain
   架构本身已经改变了跨 CPU 的事件顺序语义（`get_smp_processor_id` 也早是
   纯快照实现），删除绑核与现状一致。

另一个相关但独立的问题（不属于本修复）：`flags` 参数被整个忽略——显式
`BPF_F_INDEX_MASK` 索引输出会被错当 current-cpu 处理，应单独立项。

## 5. 结论

绑核序列在此路径上：**付出 helper 总成本的 72%（Jetson 上每请求 6–13 µs，
压在单 nginx worker 的关键路径上），换到的保护为零，并附带两个 bug**。
修复方式：无条件删除 :503-512 与 :552（cpu 快照保留现有 `my_sched_getcpu()`，
本机 rseq 路径仅 3.5 ns），上游 PR 论证结构见消融分析文档"下一步"节。
