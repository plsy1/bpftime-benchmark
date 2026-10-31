# 附录 A 研究笔记

> **这部分面向参与 bpftime 性能研究的人**：七月我们在上游 `6bd5319d` 上做的三个修复、它们在
> 官方 `177a1dc` 里的去向、向上游提 PR 时的辩护论据、研究用实验的环境和数据，以及读代码时
> 发现的代码与注释不一致之处（这部分已并入[附录 B](B-known-issues.md)）。**学习 bpftime 本身可以跳过整篇附录**，正文各章和
> [第 12 章](12-practice.md) 不依赖这里的任何内容。
>
> 这里是研究记录，信息密度高，保留了行号和 PR 号。行号基于官方 `177a1dc`（不含我们的任何修改）。

**快速查找**

| 想查什么 | 去哪一节 |
|---|---|
| 三个修复各自的上游结局，一张表 | [A.1](#a1) |
| 七月读过的代码，官方版本里哪几处变了 | [A.2](#a2) |
| 三个修复各钉在热路径的哪一环 | [A.3](#a3) |
| 对齐修复（已合入 #612）的官方实现与差异 | [A.4.1](#a41-修复-1software-perf-record-8-字节对齐0fcdb0e--上游-612) |
| 去绑核提案（仍开放）的审稿人问答 | [A.4.2](#a42-修复-2删除每事件-affinity-绑核076e3e4仍开放) |
| SIGSEGV 修复为什么被 #642 取代 | [A.4.3](#a43-修复-3sigsegv-handler-每线程装一次ead56c9--被上游-642-取代) |
| Jetson 构建路径、benchmark 脚本、消融数据 | [A.5](#a5) |
| 代码与注释不一致的九处（已并入附录 B） | [A.6](#a6) |
| 研究向自测题 | [A.7](#a7) |

---

<a id="a1"></a>

## A.1 背景：七月的三个修复与上游结局

7 月我们在 `6bd5319d` 上做了三个修复，准备向上游提 PR / issue：

- perf record 8 字节对齐（`0fcdb0e`）
- 删除每事件绑核（`076e3e4`）
- SIGSEGV handler 每线程只装一次（`ead56c9`）

到官方 `177a1dc`（2026-10 的 upstream master）为止，三者的结局各不相同：

| 7 月修复 | 上游状态 | 官方代码现在怎么做 |
|---|---|---|
| 对齐 `0fcdb0e` | **已合入**：#612（`417f7c61`，"fix: align software perf event records"） | `output_data` 内联做 8 字节向上取整，超 u16 返回 `E2BIG`；细节与我们的版本有差异（见 [A.4.1](#a41-修复-1software-perf-record-8-字节对齐0fcdb0e--上游-612)） |
| 删绑核 `076e3e4` | **未合入**；#678（`3a1d2ecf`）只修了错误路径漏恢复 mask | 每事件仍 getcpu + getaffinity + 2×setaffinity；提案仍开放（见 [A.4.2](#a42-修复-2删除每事件-affinity-绑核076e3e4仍开放)） |
| sigaction `ead56c9` | **被取代**：#642（`f311de23`，"use no-fault probe memory copies"） | 不再装 SIGSEGV handler，probe 读写改走 `process_vm_readv` / `process_vm_writev`（见 [A.4.3](#a43-修复-3sigsegv-handler-每线程装一次ead56c9--被上游-642-取代)） |

所以本附录不再为已不在官方代码里的实现做逐行辩护：A.4.1 讲读懂已合入版本，A.4.3 讲读懂
取代方案，只有 A.4.2 保留完整的辩护清单——它是唯一还需要说服审稿人的提案。

> 之后的 dev 分支工作（2026-10）又用 7 个真实 uprobe 工具审计 bpftime，找到并修了 18 个问题，
> 记录在 `summry/jetson` 分支的 `realprog/bpftime-realprog-compatibility-20261026.md`。
> 那些是我们 dev 分支的修改，不是官方代码，本附录不展开。

---

<a id="a2"></a>

## A.2 重读提醒：官方代码里已经变了的三处

原阅读节奏把 maps/helpers 放在最后，理由之一是我们在 benchmark 工作里已经把 `bpf_helper.cpp`
的热路径段读得很熟。但**熟的是 7 月的版本**。官方 `177a1dc` 里这几处已经变了，读[第 7 章](07-helpers.md)时专门重读：

- `runtime/src/bpf_helper.cpp:97-177`：probe 读写（#642 后改为 `process_vm_readv` / `writev`）；
- `runtime/src/bpf_helper.cpp:487-553`：`bpf_perf_event_output`（#678 加了 `goto restore_affinity`）；
- `runtime/src/handler/perf_event_handler.cpp:318-388`：`append_record_parts` / `output_data`（#612 的对齐）。

---

<a id="a3"></a>

## A.3 全景：三个修复在热路径上的位置

先把这张图刻进脑子：每个修复各钉在链路的哪一环，以及官方现在那一环长什么样。
能一句话说清"这行处于数据流的什么位置"，是和审稿人讨论的开场白。

```mermaid
sequenceDiagram
    participant F as frida uprobe<br/>(on_enter)
    participant P as eBPF prog (JIT)
    participant H as bpf_perf_event_output<br/>bpf_helper.cpp:487
    participant S as producer shard<br/>per-(pid,tid)
    participant C as consumer_buffer
    participant L as libbpf perf_buffer<br/>(消费者进程)

    F->>P: bpftime_prog_exec(ctx)
    Note over P: 修复3 → 被 #642 取代: bpf_probe_read<br/>现为每次一个 process_vm_readv<br/>(bpf_helper.cpp:98-136)
    P->>H: helper 25(ctx, map, flags, data, size)
    Note over H: 修复2 → 仍开放: 每事件 getaffinity<br/>+ 2×setaffinity (bpf_helper.cpp:496-506, 546-552)
    H->>S: output_data(buf, size)
    Note over S: 修复1 → 已合入 #612: record 8B 对齐<br/>(perf_event_handler.cpp:367-388)
    L->>C: epoll_wait → has_data()<br/>(bpftime_shm.cpp:473)
    C->>S: drain: copy_next_record_to<br/>(perf_event_handler.cpp:550-558)
    L->>C: mmap 直读 records
```

三个修复分属两类：

- 修复 1 是**生产者-消费者契约**问题（跨进程、跨代码库）。
- 修复 2、3 是**热路径冗余 syscall** 问题（单进程、纯浪费）。

上游对两类的处理也不同：契约问题直接合入了；修复 3 上游选择了换一套机制（把"冗余 syscall"
变成"每次一个 syscall、但不碰信号"）；修复 2 仍在等论证——"删掉的东西保护不了任何东西"。

---

<a id="a4"></a>

## A.4 PR 状态与辩护清单

总原则不变：一个修复的论证 = **(a) 改了什么**（diff 本身）+ **(b) 周边代码为什么允许这么改**
（不变量论证）+ **(c) 诚实的边界**（行为差异主动声明）。

- 对已合入 / 已取代的两个，现在要做的是读懂官方版本，知道它和我们 7 月版本的差别；
- 对仍开放的绑核删除，(b)(c) 都要按官方 `177a1dc` 的代码重新准备。

```mermaid
graph TD
    A[修复1 对齐<br/>0fcdb0e] -->|已合入 #612| B[perf_event_handler.cpp<br/>318-365 append_record_parts<br/>367-388 output_data<br/>421-462 copy_next_record_to]
    A --> C[test_software_perf_event.cpp<br/>266-321 环回绕对齐测试<br/>250-263 E2BIG 边界]
    D[修复2 绑核<br/>076e3e4] -->|未合入, 仍需辩护| E[bpf_helper.cpp<br/>487-553 helper 本体]
    D --> F[perf_event_handler.cpp<br/>491-548 shard 查找<br/>601-604 output_data 入口]
    G[修复3 sigaction<br/>ead56c9] -->|被 #642 取代| H[bpf_helper.cpp<br/>97-136 probe_read<br/>138-177 probe_write_user]
    B --> I[共同底座:<br/>34-77 内存序原语<br/>550-593 drain/has_data]
    F --> I
```

### A.4.1 修复 1：software perf record 8 字节对齐（`0fcdb0e` → 上游 #612）

#### 状态

已合入（`417f7c61`）。官方的实现比我们的短：没有单独的 `perf_event_record_alignment` 常量和
`align_perf_event_record_size` 函数，取整直接写在 `output_data` 里。

#### 与我们 7 月版本的四处差别

| 方面 | 我们的 `0fcdb0e` | 官方 #612（`177a1dc`） |
|---|---|---|
| 对齐计算 | 独立函数，带溢出检查 | `output_data` 内联（`perf_event_handler.cpp:370-378`），先用 u16 上限挡掉超大 size |
| `perf_sample_raw.size` | 裸 payload 长度 | **含 padding**：`header.size = record_size - sizeof(header)`（`:383`）。#612 的提交记录里"保持未填充"与"填充"来回改过几次，最终版提交说明是 "match perf raw padding semantics" |
| padding 字节 | 显式写零 | **不写**（"runtime: skip perf record padding writes"）：`append_record_parts` 只写两段数据，head 却按含 padding 的 `record_size` 推进（`:352-357`） |
| 消费侧 | `copy_next_record_to` 拒绝非 8 倍数的 record 并丢弃 shard | 只检查 `size` 的上下界（`:442-448`），**不检查对齐** |

#### 必读清单（约 150 行 + 测试）

| 代码段 | 位置 | 为什么必读 |
|---|---|---|
| `append_record_parts` | `runtime/src/handler/perf_event_handler.cpp:318-365` | 新增 `padding_size` 参与溢出检查、容量判断和 head 推进 |
| `output_data` | 同文件 `367-388` | 对齐与 `E2BIG`；`header.size` 与 `sample->size` 的分工 |
| `write_wrapped` / `read_wrapped` | 同文件 `390-419` | 环回绕拷贝——理解"header 跨界"故障的几何 |
| `copy_next_record_to` | 同文件 `421-462` | 消费侧只做大小检查 |
| 环回绕对齐测试 | `runtime/unit-test/test_software_perf_event.cpp:266-321` | 64 字节小环 + 4 条 record（各 20 字节，对齐后 24 字节） |
| `E2BIG` 边界 | 同文件 `250-263` | 最大合法 payload 成功、再大 1 字节失败 |

#### 注释版代码 ①：官方 `output_data`

`perf_event_handler.cpp:367-388`，行尾注释为本指南所加：

```cpp
int software_perf_event_buffer::output_data(const void *buf, size_t size)
{
	SPDLOG_DEBUG("Handling perf event output data with size {}", size);
	constexpr size_t alignment = sizeof(uint64_t);
	if (size > std::numeric_limits<uint16_t>::max() -
			   sizeof(perf_sample_raw) - (alignment - 1)) {
		errno = E2BIG;               // header.size 是 u16，对齐后也必须装得下
		return -1;
	}
	const size_t record_size =
		(sizeof(perf_sample_raw) + size + alignment - 1) &
		~(alignment - 1);            // 向上取整到 8B
	perf_sample_raw header;
	header.header.type = PERF_RECORD_SAMPLE;
	header.header.size = record_size;            // 消费者用它推进 tail
	header.header.misc = 0;
	header.size = record_size - sizeof(header);  // raw size 也含 padding

	append_record_parts(&header, sizeof(header), buf, size,
			    header.size - size);         // padding 只占位，不写
	return 0;                        // append 失败（环满）照样返回 0，见附录 B.6.2
}
```

要点：`data_head` 初值为 0（`:254`），每次按 8 的倍数推进（`:356-357`），归纳可得
**每条 record 的起点恒 8B 对齐**——这仍是整个修复的不变量，官方版本同样成立。

#### 为什么这个契约重要（讨论时仍用得上）

libbpf 的 `perf_event_read_simple` 在 wrap-copy 之前直接读环尾的 `ehdr->size`
（`third_party/bpftool/libbpf/src/libbpf.c:13601-13602`；sslsniff 实际链接的是
`third_party/libbpf/src/libbpf.c`，这两处代码和行号完全相同）。header 若跨环尾，读到的就是
半截 / 残留数据。

7 月的实测后果：header 落在环最后字节 → 读到 `size=0` → tail 不推进 → 消费者空转
（修复后消费者 task-clock −95.5%、指令 −96.1%）。

#### 官方版本值得留意的两点（观察，不是已确认的 bug）

- 由于 raw size 含 padding，libbpf 把 `s->size` 交给用户回调（`libbpf.c:13999`），回调看到的
  数据长度会比真实 payload 多 0～7 字节，而这些 padding 字节没有被写过，内容是环上的残留。
  像 sslsniff 这样在事件结构里自带长度字段的工具不受影响；直接用回调 size 的工具可能受影响（未验证）。
- 官方消费侧没有对齐检查。7 月我们加这道检查是为了防"旧版本 producer 留在持久 shm 里的非对齐
  record"；官方没加，意味着升级前后若复用同一块 shm，仍要先 `bpftimetool remove`。

### A.4.2 修复 2：删除每事件 affinity 绑核（`076e3e4`，仍开放）

#### 状态

未进上游。官方 `bpf_perf_event_output` 仍在每个事件上绑核再恢复。

外部贡献者的 #678（`3a1d2ecf`，"restore CPU affinity on perf output errors"）修掉了我们 7 月指出的
第一个子 bug（错误路径直接 return、不恢复 mask），并加了测试 `test_software_perf_event.cpp:23-124`
（tag `[perf_event][affinity]`）。绑核本身、以及第二个子 bug（恢复时用的是进入时保存的旧 mask）都还在。

#### 必读清单（约 130 行）

| 代码段 | 位置 | 为什么必读 |
|---|---|---|
| `bpf_perf_event_output` helper 本体 | `runtime/src/bpf_helper.cpp:487-553` | 提案要删的就是 `:496-506` 的绑核和 `:546-552` 的恢复；`current_cpu` 的唯一用途在 `:518-521` |
| `my_sched_getcpu` | `runtime/src/platform_utils.cpp:7-9` | Linux 上就是 `sched_getcpu()` |
| shm 转发层 | `runtime/src/bpftime_shm.cpp:575-597` | fd → `software_perf_event_data::output_data` |
| `output_data` → `get_current_thread_shard` | `runtime/src/handler/perf_event_handler.cpp:601-604, 491-548` | 论证核心：写入按 (pid,tid) 分 shard，与 CPU 无关 |
| shard cache 与三级 generation | 同文件 `110-116, 132-145, 495-511` | 无锁快路径为什么安全 |
| drain 与自旋锁 | 同文件 `550-558, 83-95` | 慢路径的互斥靠锁，不靠绑核 |
| 内存序原语 | 同文件 `34-77` | head/tail 的 acquire/release，绑核提供不了这个 |
| #678 的测试 | `runtime/unit-test/test_software_perf_event.cpp:23-124` | 它断言的不变量（调用前后 affinity 相同），删掉绑核后天然成立 |

#### 注释版代码 ②：官方 `bpf_perf_event_output` 的绑核与恢复

`bpf_helper.cpp:490-552` 节选：

```cpp
	int32_t current_cpu = my_sched_getcpu();   // 快照：后面只当 map key 用
	...
	cpu_set_t mask, orig;
	CPU_ZERO(&mask);
	CPU_SET(current_cpu, &mask);
	if (sched_getaffinity(0, sizeof(orig), &orig) < 0)   // syscall 1：存旧 mask
		return (uint64_t)-1;
	// Bind to the current cpu
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0) { // syscall 2：钉到当前核
		...
	}
	...
	if (map_ty == bpftime::bpf_map_type::BPF_MAP_TYPE_PERF_EVENT_ARRAY) {
		const int32_t *val_ptr =
			(int32_t *)(uintptr_t)bpftime::shm_holder
				.global_shared_memory.bpf_map_lookup_elem(
					fd, &current_cpu, false);   // ← cpu 的唯一用途：map key
	...
restore_affinity:
	// Cleanup must not overwrite an error from the output operation.
	const int saved_errno = errno;
	if (sched_setaffinity(0, sizeof(orig), &orig) < 0 && ret >= 0) // syscall 3：恢复
		return (uint64_t)-1;
	errno = saved_errno;
	return (uint64_t)ret;
```

要点：`current_cpu` 在绑核**之前**就快照好了（`:490`）；绑核既不影响它的取值，也不被后续任何
代码依赖。提案的 diff 就是删掉 `:496-506` 和 `:546-552`，并把两处 `goto restore_affinity` 改回直接返回。

#### 审稿人可能问什么

**Q1：绑核难道不是为了保证"写到当前 CPU 的 buffer"？**
写入根本不进 per-CPU 结构。`output_data`（`perf_event_handler.cpp:601-604`）转给
`get_current_thread_shard()`，按 `(getpid(), gettid())` 选 shard（`:493-494, :520-521`）——
**per-thread，不是 per-CPU**。`current_cpu` 的唯一用途是 PERF_EVENT_ARRAY 的 lookup key
（`bpf_helper.cpp:518-521`），而它在绑核前已快照，原实现从未消除"快照后迁移"窗口，只是略微缩短。

**Q2：那并发安全靠什么？**
shard 查找 / 创建 / drain 全走 `shard_lock` 自旋锁（`:513, :552`），无锁快路径靠 thread_local cache
+ 三级 generation 校验（`:497-511`），环指针用 acquire/release 原语（`:34-77`）。
绑核本来就不提供互斥——同核照样可被抢占。

**Q3：#678 已经修了错误路径，为什么还要删？**
#678 修的是"出错时漏恢复"，没回答"为什么要绑"。剩下的问题有两个：

1. 成本：每事件 getaffinity + 2×setaffinity。7 月在 Jetson Orin 上实测这一序列无竞争约 3.1 µs/事件
   （单次 setaffinity 约 1.4 µs，是普通 syscall 的 5 倍）。
2. 第二个子 bug：恢复用的是进入时保存的 `orig`（`:499, :549`），若应用在这期间自己改了 affinity，
   会被旧 mask 覆盖。

删除绑核同时消除这两点。

**Q4：删除后事件的 cpu 归属会不会错？**（诚实边界，主动写进 PR）
存在快照-写入间迁移窗口，但与删除前**同类同源**。7 月实测 16B 审计 10 秒约 20 万事件，
nginx worker `nr_migrations = 0`。cpu 值只影响 perf_buffer 回调的 cpu 参数，无正确性影响。

**Q5：`BPF_MAP_TYPE_KERNEL_USER_PERF_EVENT_ARRAY` 共享路径（`:533-539`）呢？**
用户态绑核影响不了内核侧的 per-CPU buffer 选择（内核用自己的 `smp_processor_id()`），对该路径同样零保护。

**Q6：#678 的测试会不会因此失败？**
按代码推断不会（未实测）：它的三个 SECTION 断言的是返回值、errno（`ENOENT` / `EINVAL` 来自 map
查询失败）和"调用前后 affinity 相同"（`test_software_perf_event.cpp:113-123`）；去掉绑核后
affinity 根本不被改动。实测方法见 [A.5](#a5) 的"去绑核后跑 `[affinity]`"。

**Q7：性能数字可信吗？**
要先声明测量顺序——对齐 bug 不修，吞吐数据被隐藏丢事件污染（这是修复 1 必须先合入的原因，
现在它已在官方里）。

- 7 月数据（基线 `6bd5319d` + 我们的修复）：Jetson 上 bpftime 原本比 kernel eBPF 慢 3.8%～12.6%，
  删除绑核后反超（fair 口径 3 轮 × 2 payload 六腿全同向，16B +7.58% / 256KB +9.81%），
  事件守恒 2.001/请求零丢失；x64 同 VM A/B 吞吐 +37%～41%，kernel 腿 Δ≤0.7% 证明控制干净。
- 10 月在官方 `177a1dc` 上只加这一处改动复测（`benchmark-results/jetson` 分支
  `uprobe/ssl-nginx-2x2-20261023/README.md`）：16B 下官方 −12.0% vs kernel，去绑核后 +7.0%，
  即比官方 +21.6%。256KB 的对比不等价（所有 bpftime 构建都只交付约一半事件，见 [A.6](#a6) 的实测数据和[附录 B.6.2](B-known-issues.md#b-helpers)），
  PR 里只引 16B。

### A.4.3 修复 3：SIGSEGV handler 每线程装一次（`ead56c9` → 被上游 #642 取代）

#### 状态

我们的修复没有进上游。上游 #642（`f311de23`，同时并入 #643 的 `probe_read_str` 语义修复）把整套
"装 SIGSEGV handler + 改 PC 跳回"的机制**删掉了**。所以 7 月准备的 UB 论证、状态机分析、
"read/write 两个 handler 互相覆盖"等既有问题清单，都随旧代码一起失去对象，不再需要辩护。

#### 官方现在怎么做

| 代码段 | 位置 | 要点 |
|---|---|---|
| 编译开关 | `cmake/StandardSettings.cmake:110, 112`；根 `CMakeLists.txt:189-195` | `ENABLE_PROBE_READ_CHECK` / `ENABLE_PROBE_WRITE_CHECK` 默认 ON |
| pid 缓存 | `runtime/src/bpf_helper.cpp:68-73` | 缓存 `getpid()`，`pthread_atfork` 在子进程里刷新 |
| `probe_read_memory` | 同文件 `97-119` | Linux：对自己 pid 调 `process_vm_readv`；macOS：`mach_vm_read_overwrite` |
| `bpftime_probe_read` | 同文件 `121-136` | 开检查 → 失败返回 `-EFAULT`；关检查 → 直接 `memcpy` |
| `probe_write_memory` / `bpftime_probe_write_user` | 同文件 `138-177` | 写方向的镜像：`process_vm_writev` |
| `bpf_probe_read_str` | 同文件 `343-418` | 开检查时按页（且每块 ≤256 字节）分块读，读失败把目标清零并返回 `-EFAULT` |
| 测试 | `runtime/unit-test/test_probe.cpp:57, 125, 227` | 重复读、保留应用 SIGSEGV handler、保留默认 handler |

`bpf_probe_read_user`（112 号，sslsniff 用的就是它）、`bpf_probe_read_kernel` 和 `bpf_probe_read`
（4 号）注册的都是同一个 `bpftime_probe_read`。

#### 读懂取代方案要抓住的三点

1. **没有信号，就没有 7 月那一串问题**：不再碰进程级的 SIGSEGV 处置，应用自己的 handler 不会被覆盖
   （`test_probe.cpp:125` 专门测这个），也不存在"第二个线程把我们自己的 handler 当成 origin 存下来"
   的自我转发递归。
2. **成本模型变了**：开检查时每次 `bpf_probe_read` 恰好一个 `process_vm_readv` syscall。
   对比：7 月原始代码每次 2 个 `rt_sigaction` + memcpy，我们的修复在首次之后 0 个 syscall。
   官方方案在 Jetson ssl-nginx 热路径上的成本我们没有单独测过（TODO：如需引用，按 A.5 的
   "probe check 消融"测；[第 12 章里对应第 7 章的实验](12-practice.md#exp-7)给了计数 syscall 的方法）。
3. **错误语义更接近内核**：读失败返回 `-EFAULT`（`:129-131`），而不是靠 fault 恢复。

#### 如果有人问"你们当时的方案为什么没进"

如实说：上游选择了不依赖信号的方案，它同时解决了我们 PR 里打算声明为 out of scope 的三个既有问题；
我们的修复只解决了"每次重装"，不再有提交价值。

---

<a id="a5"></a>

## A.5 研究用实验与环境备注

[第 12 章](12-practice.md)的验证实验是通用写法。这里保留旧版实践章实验部分里和研究环境、研究数据相关的内容。

### 源码与构建（Jetson）

- 源码以官方树为准：`/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/official`（只读）。
- Jetson 上已有一份干净的官方 `177a1dc` 构建：
  `/home/jetson/src/bpftime-official-177a1dc-llvm16-20261021/build-jit`（RelWithDebInfo、LLVM JIT）。
  注意它的 CMakeCache 里 `ENABLE_PROBE_READ_CHECK` / `WRITE_CHECK` 是 **OFF**，与默认值相反，
  且没有编出 `bpftime_runtime_tests`。
- 需要单测或 probe check 时，在官方树的副本里 `make build-unit-test`（`Makefile:35-37`，Debug，两个 check 都开）。
- 7 月用的 `bpftime-offical-no-btf/build-alignment-test` 已不存在，且它是 `ead56c9`，不是官方代码。
- x86 NUC 上对应的现成构建见第 12 章的"深入参考"。

### 实验：dump handler 表

原写法是"跑一次 benchmark 后不执行 `bpftimetool remove`，直接 `bpftimetool export`"：

```bash
B=/home/jetson/src/bpftime-official-177a1dc-llvm16-20261021/build-jit
sudo $B/tools/bpftimetool/bpftimetool export /tmp/shm.json
python3 -m json.tool /tmp/shm.json | less
```

**更正**：官方 `177a1dc` 的 loader 正常退出时，若共享内存由它创建且已无活着的 agent，会在
`atexit` 里删掉共享内存（`runtime/syscall-server/syscall_server_utils.cpp` 的
`remove_syscall_server_global_shm`）。所以 benchmark 正常结束后通常已经没有共享内存可导出，
要在 loader 运行期间导出（[第 12 章里对应第 2 章的实验](12-practice.md#exp-2)已按此改写并实测）。
只有 loader 被 `kill -9` 或崩溃时，残留的共享内存才能事后导出。

JSON 的类型字符串见 `runtime/src/bpftime_shm_json.cpp:276-368`，`handler_variant` 的 7 个成员见
`runtime/src/handler/handler_manager.hpp:87-90`。共享内存不存在或无权限时，#594 的提示会告诉你原因
（`tools/bpftimetool/main.cpp:224-240`）。

### 实验：确认 helper 25 的绑定

helper 编号在 `runtime/src/bpf_helper.cpp:939`（`BPF_FUNC_perf_event_output = 25`），注册在
`:1345-1348`；JIT 侧符号名由 `ext_func_sym` 生成（`vm/llvm-jit/src/compiler_utils.hpp:39-44`，
格式 `_bpf_helper_ext_%04u` → `_bpf_helper_ext_0025`）。原命令：

```bash
SPDLOG_LEVEL=debug BPFTIME_LOG_OUTPUT=console \
  LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so <victim> \
  2>&1 | grep -i "ext_0025\|perf_event_output"
```

运行时日志级别在 `bpftime_set_logger` 里经 `spdlog::cfg::load_env_levels()` 读取
（`runtime/include/bpftime_logger.hpp:89`）；#694 之后 `bpftime` / `bpftimetool` 两个工具本身也尊重
`BPFTIME_LOG_OUTPUT`。

**两处更正**（按源码核实）：

- agent 侧的 `BPFTIME_LOG_OUTPUT` 只管打开共享内存之前的引导日志。之后 agent 改用共享内存里
  loader 写入的日志配置（`runtime/agent/agent.cpp:956-958`），所以 `console` 要设在 loader 一侧。
- DEBUG 日志是否存在取决于编译期 `SPDLOG_ACTIVE_LEVEL`（不传时默认 TRACE，`CMakeLists.txt:159-164`），
  与 Release / Debug 构建类型无关。`make benchmark` 和 installation.md 的示例都传了 INFO，
  这样的构建看不到 DEBUG 日志，此时改用 AOT 产物 `llvm-objdump -t` 看符号表。

### 实验：核对 enter 侧事件计数（事件守恒审计）

在 `attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp:293`（`uprobe_listener_on_enter`）
与 `:310`（`uprobe_listener_on_leave`）各加一个原子计数器，重编 agent，然后跑短测和事件守恒审计，
核对 2.001 事件/请求的另一半：enter 计数应与消费者收到的 SSL_read + SSL_write 事件数之和对得上。

**更正**：严格说 enter 计数应**大于等于**事件数。sslsniff 的程序在返回值 `len <= 0` 时不发事件
（`example/tracing/sslsniff/sslsniff.bpf.c:94`），nginx 的非阻塞 `SSL_read` 会产生这类调用。
做守恒审计时应只统计返回值大于 0 的调用。

TODO：7 月用的 `v4-ablation/short-test.sh`、`event-audit.sh` 在当前机器和 `benchmark-results/jetson`
分支里都没找到（`/home/jetson/src/results/v4-ablation/` 下只剩日志）；10 月的等价脚本是
`uprobe/ssl-nginx-2x2-20261023/bench/run_fair.py`，审计方式见同目录 README，用前需确认参数。

### 实验：probe check 消融

官方代码里开关的含义已变：ON = 每次 `process_vm_readv`，OFF = 纯 `memcpy`（`bpf_helper.cpp:128-135`）。

```bash
grep PROBE <build>/CMakeCache.txt
# 默认 ENABLE_PROBE_READ_CHECK:BOOL=ON / ENABLE_PROBE_WRITE_CHECK:BOOL=ON
```

做 ON / OFF 两个构建跑短测，差值就是官方"no-fault 拷贝"在热路径上的成本。
7 月消融报告 ③ 段的 +3.7pp 是旧的信号机制下测的，不能直接套到官方代码上。

### 实验：perf buffer 单测的两个研究向练习

```bash
./build/runtime/unit-test/bpftime_runtime_tests "[software_perf_event]"
./build/runtime/unit-test/bpftime_runtime_tests "[affinity]"
```

`[software_perf_event]` 的三个 TEST_CASE（`test_software_perf_event.cpp:157/266/323`）分别验证
多线程 shard 隔离（含 `E2BIG` 边界）、mmap resize 后的对齐回绕、shm 耗尽时 `ENOMEM`；
`[affinity]`（`:23`）是 #678 的绑核恢复测试。两个练习：

1. 改一处 `output_data` 的 `alignment`，观察哪些断言炸（[第 12 章里对应第 7 章的实验](12-practice.md#exp-7)有通用写法）。
2. **去绑核后跑 `[affinity]`**：把 A.4.2 的去绑核改动打上去，确认 `[affinity]` 仍然通过
   （A.4.2 Q6 里"按代码推断不会失败"的实测版）。

---

<a id="a6"></a>

## A.6 代码与注释不一致清单（已并入附录 B）

7 月清单的 7 条加上 10 月新发现的 2 条，已在官方 `177a1dc` 上重新核对，**没有一条被上游修掉**。
这 9 条现在统一放在[附录 B 已知问题清单](B-known-issues.md)里，按模块归类，条目里注明"原附录 A.6"：

| 原编号 | 一句话 | 现在的位置 |
|---|---|---|
| A.6.1 | UPROBE / URETPROBE 注释写反 | [B.3.8](B-known-issues.md#b-attach) |
| A.6.2 | `ebpf-vm.h` 自称 interpreter，实为惰性 JIT | [B.5.16](B-known-issues.md#b-vm) |
| A.6.3 | `per_cpu_hash_map` 一条注释两处失实 | [B.7.9](B-known-issues.md#b-maps) |
| A.6.4 | per-CPU hash `elem_delete` 清零范围与意图相反 | [B.7.7](B-known-issues.md#b-maps) |
| A.6.5 | `output_data` 把丢弃当成功 | [B.6.2](B-known-issues.md#b-helpers) |
| A.6.6 | `bpf_perf_event_output` 的 `flags` 被忽略 | [B.6.1](B-known-issues.md#b-helpers) |
| A.6.7 | mocked mmap 兜底路径连调两次 | [B.2.5](B-known-issues.md#b-entry) |
| A.6.8 | `bpftimetool load` 用法说"文件"，实际要 JSON 文本 | [B.9.12](B-known-issues.md#b-periphery) |
| A.6.9 | `#error Only supports x86_64` 永不触发 | [B.10.9](B-known-issues.md#b-build-test) |

研究相关的实测数据留在这里：A.6.5 的后果 10 月在 `177a1dc` 上实测到了——256KB 响应时 kernel 每请求
17.0 个 SSL_write 事件，官方 bpftime 只有约 8.6 个（`benchmark-results/jetson` 分支
`uprobe/ssl-nginx-2x2-20261023/README.md` Finding 3）。环满丢弃对 BPF 程序和消费者双向不可见，
所以 256KB 的吞吐对比不等价（见 A.4.2 Q7）。

---

<a id="a7"></a>

## A.7 研究向自测题

读完本附录（以及跑完实验）后自测。前三道对应三个修复在官方代码里的现状，答不上来说明
还没读透官方版本。

**1. 官方 #612 之后，一条 payload 为 5 字节的 record 在环上占几个字节？`header.size` 和 `sample->size` 各是多少？padding 字节是什么内容？**

<details><summary>答案</summary>

`perf_sample_raw` = `perf_event_header`(8) + `uint32_t size`(4) = 12 字节，加 payload 5 = 17，
向上对齐到 **24 字节**（`perf_event_handler.cpp:376-378`）。

- `header.size = 24`（消费者用它推进 tail）；
- `sample->size = 24 - 12 = 12`（`:383`），**含** 7 字节 padding——这点和我们 7 月的版本不同，我们写的是裸 payload 5。

第 17～23 字节是 padding，官方**不写**它们（`append_record_parts` 只写 header 和 payload，`:352-355`），
内容是环上残留。单测 `:226-229` 断言了 `sample.size == header.size - sizeof(perf_sample_raw)`。
</details>

**2. 为什么删除绑核后不需要任何替代同步措施？写入路径上每一层的安全靠什么？**

<details><summary>答案</summary>

因为绑核从未承担同步职责：

- (a) `current_cpu` 只是 map lookup key 的快照（`bpf_helper.cpp:490, :518-521`）；
- (b) ring 写入按 `(pid, tid)` 选 producer shard（`perf_event_handler.cpp:493-494, :520-521`），单写者；
- (c) shard 表的增删查受 `shard_lock` 自旋锁（`:513, :552`）；
- (d) shard cache 快路径靠三级 generation 校验（`:497-511`）；
- (e) head/tail 跨进程可见性靠 acquire/release 原语（`:34-77`）。

每层各有其锁或其单写者性质，无一依赖"线程停在某个 CPU 上"。#678 只让恢复更可靠，没有改变这个结论。
</details>

**3. 官方 #642 之后，开着 `ENABLE_PROBE_READ_CHECK` 时，一次 `bpf_probe_read` 读到坏指针会发生什么？每次调用有几个 syscall？和 7 月的两个版本比呢？**

<details><summary>答案</summary>

`bpftime_probe_read` 调 `probe_read_memory`，Linux 上对自己的 pid 发一次 `process_vm_readv`
（`bpf_helper.cpp:110-114`）；坏指针时内核返回错误、拷贝长度不符，helper 返回 `-EFAULT`
（`:129-131`），进程里不产生 SIGSEGV，也不碰任何信号处置。每次调用 1 个 syscall。

- 7 月原始代码：每次 2 个 `rt_sigaction`（因读未初始化的 `original_sa` 导致每次重装）+ memcpy；
- 我们的 `ead56c9`：首次之后 0 个 syscall，坏指针靠 SIGSEGV handler 改 PC 恢复。

关掉检查时官方就是裸 `memcpy`（`:133-134`），坏指针直接崩。
</details>

**4. `get_current_thread_shard` 的三个 generation 各防什么？**

<details><summary>答案</summary>

- `event_generation`（`perf_event_handler.cpp:110-116`，`pid<<32 ^ 计数器`）：防 shm 中同地址析构重建的
  另一个 event 被旧 cache 命中（`:500`）；
- `buffer_generation`：消费者 `ensure_mmap_buffer` 扩容后递增（`:619-622`），迫使所有 producer 换新 shard
  （新 shard 以新容量创建，`:535-538`），防止新旧容量混写；
- `shard.generation`：防 list 节点被 erase（包括 `reclaim_inactive_producer_shards_locked` 回收已退出线程的
  空 shard，`:575-587`）后内存重用——cache 里存的裸指针必须与记录的 `shard_generation` 一致才可信（`:503-506`）。
</details>

**5. producer 环满时，BPF 程序和消费者分别观察到什么？这是 bug 吗？**

<details><summary>答案</summary>

BPF 程序看到 helper 返回 0（成功）——`output_data` 忽略 `append_record_parts` 的 false 返回值
（`perf_event_handler.cpp:385-387`）；消费者什么也看不到——全 runtime 不产生 `PERF_RECORD_LOST`。
是 bug（静默丢弃），官方 `177a1dc` 仍未修。kernel 语义是 helper 返回 `-ENOSPC` 且可统计丢失。

10 月在官方代码上实测：256KB 响应时 bpftime 只交付约一半事件，吞吐对比因此不等价。这也是对齐 bug
期间"高吞吐假象"能长期隐身的原因：丢事件无任何可见信号。
</details>

**6. 若应用在 agent attach 之后自己调用 `sigaction(SIGSEGV, ...)`，官方 `177a1dc` 的行为是什么？和 7 月两个版本比呢？**

<details><summary>答案</summary>

官方：probe 读写不再安装任何 SIGSEGV handler，应用的 handler 原样生效，probe 的坏指针也不经过信号
（返回 `-EFAULT`）。`test_probe.cpp:125`（"preserves application SIGSEGV handlers"）和 `:227`
（"default SIGSEGV handler is preserved"）测的就是这个。

- 7 月原始代码：下一次 `probe_read` 会重装我们的 handler，把应用的顶掉；
- 我们的修复：不再重装，应用 handler 生效，但此后 probe 踩坏指针时 fault 会落到应用 handler 上。

两种都不理想，官方的不依赖信号的方案同时避开了这两种情况。
</details>

---
[← 返回目录](README.md)
