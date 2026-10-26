# 7. 外围：daemon / verifier / tools / 单测

> 本章是"地图"性质：不深挖每个部件的内部实现，只给你四张导航图 + 每个部件里**最容易踩坑或最反直觉**的那一处，让你日后需要时能一头扎对地方。行号以**官方上游 `177a1dc`**（2026-10 的 upstream master）为准；与 7 月底稿（`ead56c9`）相比，daemon、tools、单测都有上游改动，文中用"上游近况"标出。

前五章都在讲 bpftime 的**主干**：syscall-server 拦截 `bpf()`、把程序和 map 放进共享内存、agent 注入目标进程、attach 层用 Frida 挂 uprobe、VM 跑字节码。本章讲的是围绕主干的四个"外围"部件——它们不在 ssl-nginx 热路径上，但决定了 bpftime 好不好用、能不能兼容内核 eBPF 生态、以及你怎么调试它。

| 部件 | 关键文件 | 一句话 |
|---|---|---|
| daemon | `daemon/kernel/bpf_tracer.bpf.c`(735L)、`daemon/user/{bpf_tracer,handle_bpf_event,bpftime_driver}.cpp`、`daemon/user/bpf_prog_insns.hpp` | 内核 eBPF tracer 监控目标进程的 `bpf()`/`perf_event_open()`，用 `bpf_probe_write_user` **改写 uprobe 路径**实现"透明劫持内核 eBPF 到用户态"；`relocate_bpf_prog_insns` 做 map fd→kernel map id 重定位 |
| verifier | `bpftime-verifier/src/{bpftime-verifier,platform-impl}.cpp` | PREVAIL 薄封装；**配置全是 `thread_local`**，跨线程校验前必须重新 `set_*`；只支持 uprobe/tracepoint + hash/array/ringbuf。7 月以来上游**没有改动** |
| tools | `tools/cli/main.cpp`(1290L) vs `tools/bpftimetool/main.cpp`(399L) | **cli 是注入器**（`fork`+`LD_PRELOAD` 或 Frida 注入），**bpftimetool 是 shm 直操作器**（不注入）——你天天用的 `bpftimetool remove` 就是它调 `bpftime_remove_global_shm` |
| 单测 | `runtime/unit-test/CMakeLists.txt`、`daemon/test/CMakeLists.txt` | Catch2 v3.4.0；`bpftime_runtime_tests` 可按 tag 过滤（如 `"[software_perf_event]"`）；Frida attach 类测试只在 `BPFTIME_ENABLE_FRIDA` 时编译 |

**两个一进门就要建立的心智**：

1. daemon 有**两层 eBPF**，千万别混：一层是 daemon 自己 attach 到内核 tracepoint 的**内核 tracer**（`bpf_tracer.bpf.c`，真跑在内核里）；另一层是**被劫持的目标程序**想加载的 eBPF（daemon 把它抢过来搬进用户态 shm）。本章说"劫持链"时，主角是前者在监视后者。
2. daemon 这条路和你在 ssl-nginx 里走的路**不是同一条**。你的 benchmark 用的是 `bpftime start`（agent 注入 + syscall-server 拦截，程序一开始就走用户态）。daemon 是给"**不改启动方式、已经在用内核 eBPF 的程序**"准备的透明劫持通道——理解它能让你看清 bpftime "两种接入模式"的全貌。

---

## 7.1 daemon：内核 eBPF 的"透明劫持"

### 它解决什么问题

`bpftime start` 需要你主动用它启动程序。但很多场景下，目标程序（比如某个 profiling agent）已经在用**内核** eBPF——它自己调 `bpf(BPF_PROG_LOAD)`、`perf_event_open()` 挂 uprobe。daemon 的野心是：**在不改目标程序一行代码的前提下**，把这些本该进内核的 eBPF 程序，偷偷搬到 bpftime 的用户态 runtime 里执行。

怎么做到"偷梁换柱"？答案是 daemon 在内核里挂一圈 tracepoint，盯着目标进程进出 `bpf()`/`perf_event_open()`/`ioctl()` 这些 syscall，然后在**目标进程还没真正进内核之前**，用 `bpf_probe_write_user` 篡改它传给内核的参数，让内核"什么都没干成"，同时把原始意图捞出来交给用户态 runtime 重演。

"盯谁"由 `filter_target()`（`daemon/kernel/bpf_kernel_config.h:28-40`）决定：官方现在按 **tgid**（`bpf_get_current_pid_tgid() >> 32`，`:30`）比较 `target_pid`，7 月版本取的是低 32 位（线程 id），多线程目标的非主线程会被漏掉——这是上游 #651 修的。

### 那条"魔法链"：uprobe 路径改写

最精彩的是 uprobe 劫持。目标程序调 `perf_event_open()` 想在 `/lib/libssl.so` 的某个 offset 挂 uprobe。daemon 的内核 tracer 在 `sys_enter_perf_event_open` 里拦下，做两件事：把 `probe_offset` 清零、把 `uprobe_path` 字符串从真实 so 路径改写成一个**占位文件 `/a`**。于是内核在 `/a` 上挂了个 offset 0 的 uprobe——目标程序以为成功了，实际什么都没挂到它关心的函数上。真实的 `(路径, offset)` 则通过 ringbuf 事件送到 daemon 用户态，由它调 `bpftime_uprobe_create_server` 在 bpftime runtime 里挂**真的**用户态 uprobe。

```mermaid
sequenceDiagram
    participant T as 目标进程
    participant K as 内核 tracer(bpf_tracer.bpf.c)
    participant KE as 内核 perf 子系统
    participant D as daemon 用户态(handle_bpf_event/driver)
    participant R as bpftime runtime(shm)

    T->>K: perf_event_open(attr: path=/lib/libssl.so, off=0x1234)
    Note over K: sys_enter_perf_event_open<br/>process_perf_event_open_enter()
    K->>K: 备份原始 path/offset 到 map
    K->>T: bpf_probe_write_user 改写目标内存:<br/>path→"/a", offset→0
    T->>KE: syscall 带着"假参数"进内核
    KE-->>T: 在 /a 上挂了个 offset 0 的 uprobe, 返回 fd
    Note over K: sys_exit_perf_event_open
    K->>D: ringbuf 提交 SYS_PERF_EVENT_OPEN 事件<br/>(带真实 path + offset)
    K->>K: bpf_probe_write_user 把 path/offset 改回原值<br/>(还原目标进程内存, 不留痕迹)
    D->>R: bpftime_uprobe_create_server(server_pid, fd, target_pid, name, offset, ...)
    Note over D,R: 之后 IOCTL(SET_BPF)/LINK_CREATE 再把<br/>内核里的 bpf prog 重定位搬进 shm 并 attach
```

（顺序以代码为准：exit 处理里先 `bpf_ringbuf_submit`（`bpf_tracer.bpf.c:537`），再还原参数（`:541-545`）。）

关键点：**enter 时改成假参数骗内核，exit 时又原样改回**，所以目标进程从自己内存里读回来看，参数毫发无损——它完全不知道被劫持了。这正是"透明"二字的来源。

### 注释版代码选段（1）：uprobe 改写的核心

`daemon/kernel/bpf_tracer.bpf.c:407-437`（`...` 处省略了 `bpf_printk` 调试输出；行尾中文注释为本指南所加）

```c
if (new_attr_pointer->type == uprobe_perf_type) {      // 是 uprobe 类型的 perf event
	// found uprobe
	if (enable_replace_uprobe) {
		if (can_hook_uprobe_at(
			    new_attr_pointer->probe_offset)) {
			u64 old_offset = new_attr_pointer->probe_offset;
			new_attr_pointer->probe_offset = 0;      // ① offset 清零
			long size = bpf_probe_read_user_str(     // 读出真实 so 路径
				old_uprobe_path,
				sizeof(old_uprobe_path),
				(void *)new_attr_pointer->uprobe_path);
			if (size <= 0) {
				// no uprobe path
				return 0;
			}
			if (size > PATH_LENTH) {
				size = PATH_LENTH;
			}
			...
			bpf_probe_write_user(                    // ② 路径改写成 "/a"
				(void *)new_attr_pointer->uprobe_path,
				&new_uprobe_path, (size_t)size);
			bpf_probe_write_user(attr, new_attr_pointer, // ③ 整个 attr 写回目标内存
					     sizeof(*new_attr_pointer));
			...
			return 1;   // 返回 1 = "这个请求归 bpftime 管，送 daemon"
		}
```

- **它在做什么**：把目标进程即将传给内核的 `perf_event_attr`，在原地（用户态内存）改成一个无害版本。
- **为什么这样写**：`bpf_probe_write_user` 是唯一能在内核 eBPF 里反向写目标进程用户态内存的 helper——这是整套劫持的物理基础。offset 清零 + 路径换成占位 `/a`，保证内核那次挂载"合法但无效"。
- **坑在哪**：`new_uprobe_path` 被硬编码成 `/a`（`daemon/user/bpf_tracer.cpp:202`），上方 TODO 注释（`:198-201`）直言原因——路径字符串太长会破坏目标进程用户态内存，只好用最短的 `/a`。这是个"能用但丑"的权衡，也解释了为什么 daemon 启动时若 `/a` 不存在，就用 `std::ofstream` 现场写出一个占位文件（`bpf_tracer.cpp:203-216`）。注意它写的**不是空文件**，而是嵌入的 `placeholder`（构建时由 `daemon/CMakeLists.txt:11-16` 从 `daemon/assets/placeholder` 生成；仓库里这个文件是一个 x86-64 ELF 可执行文件）。

### 注释版代码选段（2）：exit 时还原现场

`daemon/kernel/bpf_tracer.bpf.c:541-545`

```c
		// Revert changes
		bpf_probe_write_user(ap->path_buf_user, ap->name_or_path,   // 路径写回原值
				     sizeof(ap->name_or_path));
		bpf_probe_write_user(&ap->attr_user->probe_offset,          // offset 写回原值
				     &ap->orig_offset, sizeof(ap->orig_offset));
```

- **它在做什么**：syscall 返回前，把 enter 时篡改过的两个字段改回去。
- **为什么这样写**：目标进程 syscall 返回后可能还会读自己的 `attr`（比如 libbpf 会核对）。如果留着 `/a` 和 offset 0，目标程序自检就会发现异常。所以必须"作案后打扫现场"。`orig_offset`/`name_or_path` 都是 enter 时存进 `perf_event_open_param_start` map 的备份（`:488-490`，写入 map 在 `:498`）。

📌 **案例连接（probe_read/probe_write 的 SIGSEGV 处理）**：你 7 月修的第三个 bug 是用户态 `probe_read`/`probe_write_user` 每次调用重装 SIGSEGV handler。那个修复没有进上游，而是被上游 #642（`f311de23`，"use no-fault probe memory copies"）**整体取代**：官方现在的用户态 `bpftime_probe_write_user`（`runtime/src/bpf_helper.cpp:162-177`）在 `ENABLE_PROBE_WRITE_CHECK` 下走 `probe_write_memory`（`:139-159`），Linux 上就是对自己 pid 调 `process_vm_writev`，**根本不再装信号 handler**。而 daemon 这里的 `bpf_probe_write_user` 是**内核 helper**，跑在内核态，由内核自己做用户内存访问的安全处理——和用户态那份是**完全不同的两份实现**，同名不同物。这个"同名两实现"的对照，正好帮你把两层 eBPF 的边界钉死在脑子里。

### daemon 用户态：driver 的重定位

内核 tracer 只负责"抓意图 + 送事件"。真正把内核 eBPF 程序搬进 shm 的是用户态 `bpftime_driver`。最烧脑的一步是 **map fd → map id 重定位**（`relocate_bpf_prog_insns`，`daemon/user/bpftime_driver.cpp:73-125`）：

内核 eBPF 程序里，`LDDW` 指令的 `imm` 存的是**内核 map fd**（进程私有的小整数）。搬到 bpftime runtime 后，这个 fd 毫无意义，必须换成全局唯一的 id。`relocate_bpf_prog_insns` 遍历每条指令，遇到 `EBPF_OP_LDDW` 且 `src_reg` 是 1 或 2（map-by-fd / map-by-idx），就用 `(pid, fd)` 去查内核 map id（`get_bpf_obj_id_from_pid_fd`），再改写 `insns[i].imm`（`:101-112`）。随后 `check_and_create_prog_related_maps`（`:127-161`）用同一个内核 id 在 shm 里建对应 map。

```mermaid
graph TD
    A["内核 eBPF prog<br/>LDDW imm=map_fd 3"] -->|relocate_bpf_prog_insns| B["查 bpf_fd_map[pid,3]<br/>→ kernel_id"]
    B --> C["insns[i].imm = kernel_id"]
    C --> D["bpftime_progs_create<br/>存入全局 shm"]
    D --> E["check_and_create_prog_related_maps<br/>把依赖的 map 也搬进 shm"]
```

这解释了为什么 daemon 需要那么多张辅助 map（`bpf_fd_map`，`bpf_pid_fd_map.h:22`；`bpf_prog_insns_map`，`bpf_obj_id_fd_map.h:38`；`bpf_progs_new_fd_args_map`，`:48`）：它要在内核里就把"哪个 fd 对应哪个 id、哪段指令属于哪个 prog"全记下来，用户态才有据可查。指令本身是靠 `kprobe/bpf_prog_kallsyms_add`（`bpf_obj_id_fd_map.h:50`）在内核加载程序时抓的快照。

### 上游近况（6bd5319d → 177a1dc）

daemon 是 7 月以来外围部件里改动最多的，四个修复都值得知道它们改了哪：

| PR | 改了什么 | 位置 |
|---|---|---|
| #651 | pid 过滤改用 tgid（见上文） | `bpf_kernel_config.h:30-38` |
| #662 | 拒绝被截断的程序快照：用户态先检查 `code_len` 是否超过快照缓冲容量，超了返回 `-E2BIG` 而不是越界拷贝；内核侧 `process_bpf_prog_load_events` 改用 `bpf_probe_read_user` 读 attr，读失败就不替换程序 | `bpf_prog_insns.hpp:20-36`、`bpftime_driver.cpp:89-95`、`bpf_tracer.bpf.c:157-158, 192-193` |
| #687 | 临时内核对象 fd 在每条返回路径上关闭（`BOOST_SCOPE_EXIT_ALL`，并保存/恢复 `errno`） | `bpftime_driver.cpp:171-176, 221-226` |
| #586 | `BPF_LINK_CREATE` 镜像到 bpftime 时保留 perf-link 的 `bpf_cookie` | `handle_bpf_event.cpp:250-265`、`bpftime_driver.cpp:263-312` |

---

## 7.2 verifier：一层 PREVAIL 薄封装，一个 thread_local 大坑

### 定位

`bpftime-verifier/` 把学术界的 PREVAIL 验证器（`ebpf-verifier/` 子模块，即上游 vbpf/ebpf-verifier）包了薄薄一层，供 syscall-server 在加载程序时做**用户态验证**。它只在编译时打开 `ENABLE_EBPF_VERIFIER`（`cmake/StandardSettings.cmake:95`，默认 OFF）才存在，此时 `runtime/CMakeLists.txt:205-209` 给 runtime 定义 `ENABLE_BPFTIME_VERIFIER`。

对外就三类接口（`bpftime-verifier.cpp`）：
- `verify_ebpf_program(raw_inst, num_inst, section_name)` → 返回 `optional<string>`，空 = 通过，有值 = 错误信息。
- `set_available_helpers` / `set_non_kernel_helpers` / `set_map_descriptors` → 验证**之前**必须先喂进去的上下文。

只支持三类程序（`platform-impl.cpp:21-22`：`uprobe`/`uretprobe`/`tracepoint`）和三类 map（`platform-impl.cpp:70-72`：`HASH`/`ARRAY`/`RINGBUF`），其它一律 `throw runtime_error`。

### 那个 thread_local 坑

`platform-impl.cpp:100-103`：

```cpp
// Thread independent
thread_local std::set<int32_t> usable_helpers;
thread_local std::map<int, EbpfMapDescriptor> map_descriptors;
thread_local std::map<int32_t, EbpfHelperPrototype> non_kernel_helpers;
```

这三个全局状态**都是 `thread_local`**。含义：你在**线程 A** 里调 `set_available_helpers(...)` 配好了可用 helper 集合，然后在**线程 B** 里调 `verify_ebpf_program(...)`——线程 B 看到的 `usable_helpers` 是**空的**，任何 helper 调用都会被判"Unusable helper"（`platform-impl.cpp:42-43` 抛异常），验证平白失败。

为什么这样设计？因为底层 PREVAIL 本身塞满了 `thread_local` 全局状态（`ebpf-verifier/src/crab_verifier.cpp:29-30` 的 `global_program_info`、`thread_local_options`……），它假设"配置和验证在同一线程内闭环"。bpftime 的封装层只能跟着用 `thread_local`，否则会和底层状态错位。

**syscall-server 实际是怎么配的**（以官方代码为准，比 7 月底稿写的更微妙）：
- `set_available_helpers`/`set_non_kernel_helpers` 在 `start_up()` 里（`runtime/syscall-server/syscall_server_utils.cpp:212-214`），而 `start_up` 用 `std::call_once` 包着（`:147-149`），由**第一个**被拦截的 syscall 经 `try_startup()`（`syscall_context.cpp:237-252`）触发——也就是说，它跑在"第一个进 syscall-server 的线程"上，不一定是之后加载程序的线程。
- `set_map_descriptors` 在建 map 时调用（`runtime/src/bpftime_shm_internal.cpp:1068`），跑在建 map 的线程上。
- `verify_ebpf_program` 在 `BPF_PROG_LOAD` 分支里调用（`syscall_context.cpp:658`），跑在调 `bpf()` 加载程序的线程上。

对常见的单线程 loader，三者是同一线程，所以没事。多线程 loader 若首个被拦截的 syscall 与 `BPF_PROG_LOAD` 不在同一线程，按上面的代码推断 helper 集合会是空的（这一点是读代码得出的，未实测）。你若哪天想在后台线程池里做批量验证，这里就是第一个爆点。

### 注释版代码选段（3）：验证入口

`bpftime-verifier/src/bpftime-verifier.cpp:77-97`（`...` 省略 stats 初始化）

```cpp
	prog.info = {
		.platform = &bpftime_platform_spec,            // ① 挂上 bpftime 定制平台
		.map_descriptors = get_all_map_descriptors(),  // ② 从 thread_local 取 map 上下文
		.type = bpftime_platform_spec.get_program_type(section_name,
							       ""),
	};
	global_program_info = prog.info;                   // ③ 灌进 PREVAIL 的 thread_local
	std::vector<std::vector<std::string> > notes;
	auto unmarshal_result = unmarshal(prog, notes);    // ④ 字节码→内部 IR, 可能直接报错
	if (std::holds_alternative<std::string>(unmarshal_result)) {
		return std::get<std::string>(unmarshal_result);
	}
	auto inst_seq = std::get<InstructionSeq>(unmarshal_result);
	...
	std::string verifier_message;
	string_capture_streambuf capture_buffer(verifier_message);
	std::ostream message_stream(&capture_buffer);
	auto result = ebpf_verify_program(message_stream, inst_seq, prog.info,
					  &verifier_options, &stats);
```

- **它在做什么**：把原始 8 字节一条的字节码组装成 `raw_program`，喂进 PREVAIL 两阶段——先 `unmarshal`（结构合法性），再 `ebpf_verify_program`（抽象解释做安全性证明）。
- **为什么这样写**：`get_all_map_descriptors()`（②）和 `global_program_info`（③）都读/写 `thread_local`，正是上面那个坑的现场——`prog.info` 里的 map 描述符来自当前线程的 `map_descriptors`，跨线程就是空的。
- **坑在哪**：`verifier_options`（`:19-30`）里 `no_simplify=true`、`check_termination=true` 是硬编码的。验证失败时错误信息通过一个自定义 `string_capture_streambuf`（`:37-61`）从 `ostream` 里捞出来（`:93-101`）——因为 PREVAIL 只会往流里打印、不返回结构化错误。

📌 **案例连接（对齐修复）**：verifier 关心的是"程序**逻辑**安全"（越界、未初始化寄存器、死循环），它管不到**运行时内存布局**。你 7 月修的 perf record 8 字节对齐 bug（已由上游以 #612 合入，见第 8 章），是 record 落在环 buffer 边界的物理布局问题——PREVAIL 验证器对此**完全无感**，它验证时程序连跑都没跑。这提醒你：verifier 通过 ≠ 运行正确，一大类"对齐/并发/生命周期"bug 天然在验证器射程之外，只能靠"读汇编 + 环边界回归测试"的路子抓。

### 验证模式（怎么关掉它）

验证不是死的：`BPFTIME_VERIFIER_LEVEL` 环境变量（`runtime/src/bpftime_config.cpp:137-155`）控制三档——`STRICT`（失败即 `errno = EINVAL` 拒绝加载，`syscall_context.cpp:670-680`）、`WARNING`（默认值，`runtime/include/bpftime_config.hpp:64`；打 warning 但仍加载，`syscall_context.cpp:681-691`）、`NO_VERIFY`（完全跳过，`:654-655`）。另外只有 `BPF_PROG_TYPE_TRACEPOINT` 和 `BPF_PROG_TYPE_SOCKET_FILTER`（被当作 uprobe）两种 prog_type 会被送去验证（`:643-649`）。调试自己写的程序被验证器挡住时，`BPFTIME_VERIFIER_LEVEL=NO_VERIFY` 是最快的绕行开关。

---

## 7.3 tools：注入器 vs shm 直操作器

两个都叫 `main.cpp`，职责却正交，别搞混。

| | `tools/cli` → `bpftime` 命令 | `tools/bpftimetool` → `bpftimetool` 命令 |
|---|---|---|
| 本质 | **注入器**：把 .so 塞进目标进程 | **shm 直操作器**：直接读写全局共享内存，不碰任何进程 |
| 子命令 | `load` / `start` / `attach` / `detach` / `trace`（`main.cpp:977-981`） | `load` / `export` / `import` / `remove` / `run`(+`run-on-cuda`) |
| 怎么干活 | `load`：`fork`+`LD_PRELOAD` syscall-server（`run_command`, `main.cpp:1002`）<br/>`start`：`LD_PRELOAD` agent（`:1020`/`:1025`）<br/>`attach`：Frida 注入到已运行 pid（`inject_by_frida`, `:1043`/`:1046`）<br/>`detach`：给 agent 发 `SIGUSR1`（`:1277`） | `remove`：`bpftime_remove_global_shm()`（`main.cpp:262`）<br/>`export`/`import`：shm ↔ JSON 文件<br/>`load <fd> <JSON>`：把**一段 JSON 文本**描述的 handler 放进指定 fd 槽位<br/>`run`：从 shm 取出 prog 直接 JIT/AOT/解释跑并测时 |
| 需不需要目标进程 | 需要（它就是启动/挂靠目标进程的） | 不需要（纯离线检修 shm） |

**记忆锚**：你天天清环境用的 `remove` 是 **bpftimetool** 的子命令（`bpftimetool remove` → `bpftime_remove_global_shm`）；`bpftime` cli 本身没有 `remove`。cli 的 `detach` 是"通知 agent 优雅退出"，bpftimetool 的 `remove` 是"直接把 `/dev/shm` 里那块内存删掉"——一个温柔一个粗暴，卡死时你要的往往是后者。

### cli 的两种注入机制

- **`load`/`start`（新进程）**：`run_command`（`main.cpp:107`）本质是 `fork` 出子进程，在 `execve` 前把 `LD_PRELOAD` 设成对应 .so，让动态链接器在程序 main 之前就加载 bpftime 库。`load` 预载 syscall-server（拦 `bpf()`），`start` 预载 agent（跑用户态 uprobe）。这就是你 ssl-nginx benchmark 走的路。
- **`attach`/`trace`（已运行进程）**：`inject_by_frida`（`main.cpp:276`）用 Frida 的 `frida_injector_inject_library_file_sync` 把 .so 注射进一个**已经在跑**的 pid——无需重启目标。`trace` 更复杂，还叠了 IPC refresh（`:1170-1184`）、CUDA late-PTX 参数、Ctrl+C 时先 detach agent 再停 loader 的优雅退出编排（`:1188-1255`）。

📌 **案例连接（绑核修复）**：你 7 月提出删掉 `bpf_perf_event_output` 的每事件 affinity 绑核，论据之一是"错误路径漏恢复 mask → 线程被永久钉死单核"。这一半已被上游独立修掉：#678（`3a1d2ecf`）把两条错误路径改成 `goto restore_affinity`（`runtime/src/bpf_helper.cpp:515, 526, 546-552`），但**绑核本身仍在**，删除绑核的提案仍未进上游（见第 8 章）。cli 的 `trace` 退出编排（`tools/cli/main.cpp:1139-1255`）是同一类问题的正面教材——它用 `sigaction` 存旧 handler（`:1146-1153`），退出时 `sigaction(SIGINT, &old_int, ...)` 严格还原（`:1254-1255`），保证信号处理不残留副作用。同样是"改了全局/线程状态必须配对还原"，可以对照记。

### 上游近况（tools）

- **#682 让 `bpftimetool load` 真正可用**：7 月版本检查的是 `argc != 3`，却读 `argv[3]`，于是合法调用总是打印用法后退出；官方改为 `argc != 4`，并用 `strtol` 严格解析 fd，拒绝 `typo`/`-1`/`7x` 等（`tools/bpftimetool/main.cpp:179-202`），配套 `tools/bpftimetool/test_load.py`，CI 在 `.github/workflows/test-tools.yml:60-62` 跑它。
- **#694 两个工具都尊重 `BPFTIME_LOG_OUTPUT`**：在 `main` 一开头就调 `bpftime_set_logger`（bpftimetool `main.cpp:151-157`，cli `main.cpp:879-885`），否则在这之前的 `SPDLOG_*` 会打到 spdlog 默认 logger 的 stdout。
- **#594 shm 访问错误的友好提示**：已在 7 月基线 `6bd5319d` 里（就是那个提交本身），`export` 打开 shm 失败时捕获 `interprocess_exception` 并给出权限/损坏的处理建议（`main.cpp:224-240`）；`export` 还拒绝把输出写到 `/dev/shm/<shm 名>` 本身（`:213-222`）。
- cli 新增 `--fuzzy-backtracer` 选项（`main.cpp:852-854`，对应 `BPFTIME_FRIDA_FUZZY_BACKTRACER`）。

---

## 7.4 单测：组织与怎么跑单个

### 两套测试二进制

| 二进制 | 源码目录 | 跑法 | 覆盖 |
|---|---|---|---|
| `bpftime_runtime_tests` | `runtime/unit-test/` | `./build/runtime/unit-test/bpftime_runtime_tests` | maps、perf event、ringbuf、epoll、probe、get_stack、shm 分配失败、attach+ebpf（Frida / trap 两种后端）、tailcall |
| `bpftime_daemon_tests` | `daemon/test/` | `./build/daemon/test/bpftime_daemon_tests` | daemon driver（cookie、快照容量）、临时 fd 释放 |

用的都是 **Catch2 v3.4.0**：打开 `ENABLE_EBPF_VERIFIER` 时由 `runtime/unit-test/CMakeLists.txt:1-9` FetchContent 拉 `v3.4.0`，否则用子模块 `third_party/Catch2`（根 `CMakeLists.txt:149-152` 添加，子模块同样在 v3.4.0）。`make unit-test` = `unit-test-daemon` + `unit-test-runtime`（`Makefile:52`）；`make build-unit-test`（`Makefile:35-37`）以 Debug 构建并打开两个 probe check。

### 怎么跑单个测试 / 按 tag 过滤

Catch2 支持位置参数选 `TEST_CASE`，也支持 `[tag]` 过滤：

```bash
# 只跑软件 perf event 相关的三个 case（对齐回归测试就在这）
./build/runtime/unit-test/bpftime_runtime_tests "[software_perf_event]"

# 按名字精确跑一个
./build/runtime/unit-test/bpftime_runtime_tests \
  "Software perf event records wrap on aligned boundaries after resize"

# #678 加的绑核恢复测试
./build/runtime/unit-test/bpftime_runtime_tests "[affinity]"

# 列出所有 case / tag
./build/runtime/unit-test/bpftime_runtime_tests --list-tests
./build/runtime/unit-test/bpftime_runtime_tests --list-tags
```

官方的环边界对齐回归测试是 `runtime/unit-test/test_software_perf_event.cpp:266` 的 "Software perf event records wrap on aligned boundaries after resize"，tag 是 `[perf_event][software_perf_event]`（`:267`）；你 7 月写的那个 "...remain aligned at the ring boundary" 没有进上游，#612 带的是这个更短的版本。同文件 `:250-263` 还测了 `E2BIG` 上限，`:23` 是 #678 的 `[perf_event][affinity]` 用例。另一个与对齐同类的新测试是 `test_ringbuf_tail_wrap.cpp:21`（#706，ringbuf 的 record 跨环尾仍连续）。

### 两个必知的坑

1. **attach 类测试要先编 eBPF 程序，而且看后端**。`test_attach_uprobe_with_ebpf.cpp:20` 通过编译期宏 `EBPF_PROGRAM_PATH_UPROBE` 拿到 .bpf.o 路径，这些 .o 由 `runtime/unit-test/CMakeLists.txt:142-158` 的循环调 `add_ebpf_program_target`（定义在 `cmake/libbpf.cmake:110`）从 `assets/*.bpf.c` 现编。`unit-test-runtime` target 还额外 `make -C runtime/test/bpf`（`Makefile:47`）产另一批测试用 .o。忘了编 eBPF，attach 测试会因找不到 .o 而失败。另外，4 个 Frida attach 测试文件只在 `BPFTIME_ENABLE_FRIDA` 时才加入（`CMakeLists.txt:52-61`）；trap 后端测试 `test_attach_uprobe_with_ebpf_trap.cpp`（#665）总会编译，但内容被 `#if BPFTIME_HAVE_TRAP_UPROBE_ATTACH` 包住，而 trap 后端只在 riscv 上默认打开（`cmake/StandardSettings.cmake:76-82`），CI 在 `.github/workflows/test-riscv64.yml` 用 qemu-user 跑（#674）。

2. **daemon 测试比 7 月多了，但 `test_daemon.cpp` 仍是空壳**。`TEST_CASE("Test daemon")`（`test_daemon.cpp:13`）函数体仍为空；7 月时 `test_daemon_driver.cpp` 的断言全被注释掉，现在它有三个真用例：`:18` cookie 保留（#586）、`:51`/`:63` 快照容量上下界（#662）；新增的 `test_daemon_fd.cpp:123`（tag `[daemon-fd]`，#687）靠 `-Wl,--wrap=` 链接选项（`daemon/test/CMakeLists.txt:31-37`）替换 libbpf 调用，检查每条返回路径都关了 fd。覆盖面仍只是 driver 的几个函数，**内核 tracer 的劫持链本身没有单测**，别指望 `bpftime_daemon_tests` 绿灯能证明劫持逻辑正确。顺带纠正 7 月底稿的一个说法：测试文件顶部的 `#if !defined(__x86_64__) && defined(_M_X64)` / `#error Only supports x86_64`（`test_daemon.cpp:8-10`）要求"不是 x86_64 且定义了 MSVC 的 `_M_X64`"，在 GCC/Clang 下永远不成立，所以它**不会**阻止在 aarch64 上编译这些测试。

📌 **案例连接（对齐修复）**：你 7 月修对齐 bug 的做法——先加一个"record 落在环边界"的回归 `TEST_CASE`，再改代码让它变绿——正是这套 Catch2 框架的正确用法；上游合入 #612 时也带了同类测试（上面的 `:266`）。用 `"[software_perf_event]"` tag 过滤只跑这三个 case，单测反馈环能压到秒级。这也反衬出坑 2：daemon 的核心劫持链没有测试，任何改动都得靠手工验证，风险高得多。

---

## 自测题

**Q1（原题保留）：daemon 里的"两层 eBPF"分别指什么？为什么容易混淆？**

<details><summary>答案</summary>

第一层是 daemon **自己**的内核 tracer（`bpf_tracer.bpf.c`），真跑在内核里，attach 到 `sys_enter/exit_bpf`、`perf_event_open`、`ioctl` 等 tracepoint，负责监视目标进程（按 tgid 过滤，`bpf_kernel_config.h:28-40`）。第二层是**被劫持的目标程序**想加载的 eBPF——daemon 把它从内核抢过来搬进用户态 shm。混淆点：两者都是"eBPF 程序"，但一个是监视者（内核态、daemon 主动挂）、一个是被监视者（本想进内核、被搬到用户态）。`bpf_probe_write_user` 这类内核 helper 属于第一层。

</details>

**Q2（原题保留）：为什么 verifier 的配置跨线程会失效？**

<details><summary>答案</summary>

`usable_helpers` / `map_descriptors` / `non_kernel_helpers` 三个全局状态都是 `thread_local`（`platform-impl.cpp:100-103`），因为底层 PREVAIL 本身充满 thread_local 状态、假设配置与验证在同一线程闭环。在线程 A `set_available_helpers`、线程 B `verify_ebpf_program`，线程 B 看到的是空配置，helper 会被判 Unusable 而抛异常。官方 syscall-server 在 `start_up()`（`call_once`，`syscall_server_utils.cpp:147-149, 212-214`）里配 helper，由第一个被拦截的 syscall 所在线程执行；验证在 `BPF_PROG_LOAD` 所在线程（`syscall_context.cpp:658`）。单线程 loader 两者相同所以没事；多线程 loader 有错位风险（读代码推断，未实测）。

</details>

**Q3（新）：uprobe 劫持时，为什么 enter 改参数、exit 又要把参数改回去？如果省掉 exit 的还原会怎样？**

<details><summary>答案</summary>

enter 时把 `probe_offset` 清零、`uprobe_path` 改成 `/a`，是为了骗内核挂一个无效 uprobe（`bpf_tracer.bpf.c:407-437`）。exit 时用备份还原（`:541-545`），因为 syscall 返回后目标进程/libbpf 可能会读回自己的 `attr` 做自检。若省掉还原，目标程序会看到路径变成 `/a`、offset 变 0，与它自己设的值不符，轻则报错重则行为异常——"透明"就破功了。备份存在 `perf_event_open_param_start` map 里（enter 的 `:488-490`，`:498` 写入 map）。

</details>

**Q4（新）：你想验证一个自己写的 uprobe eBPF 程序，但它一直被 verifier 拒绝加载，怎么快速定位/绕过？**

<details><summary>答案</summary>

三步：(1) 确认当前档位——默认就是 `WARNING`（`bpftime_config.hpp:64`），只有设了 `STRICT` 才会真的拒绝；直接 `BPFTIME_VERIFIER_LEVEL=NO_VERIFY`（`bpftime_config.cpp:137-155`）确认是不是验证器的锅——`NO_VERIFY` 下能跑就说明只是没过静态验证。(2) 看错误信息：验证失败的报告经 `string_capture_streambuf` 从 PREVAIL 的 ostream 捞出（`bpftime-verifier.cpp:93-101`），syscall-server 还会附上程序反汇编（`syscall_context.cpp:663-669`），会指出哪条指令、什么问题。(3) 注意 bpftime 只支持 uprobe/uretprobe/tracepoint + hash/array/ringbuf（`platform-impl.cpp:21-22, 70-72`），用了别的 section 或 map 类型会直接 `throw`，不是验证失败而是"不支持"，换类型即可。

</details>

**Q5（新）：`bpftime detach` 和 `bpftimetool remove` 有什么本质区别？卡死时该用哪个？**

<details><summary>答案</summary>

`detach` 属于 **cli**（`tools/cli/main.cpp:1260` 起），它遍历 alive agent 集合、给每个 agent 进程发 `SIGUSR1`（`:1277`）通知它优雅卸载——依赖 agent 还活着且能响应信号。`remove` 属于 **bpftimetool**（`tools/bpftimetool/main.cpp:255-263`），直接调 `bpftime_remove_global_shm()`（`:262`）把 `/dev/shm` 里那块共享内存删除，不理会任何进程。目标进程已崩溃/卡死、agent 无法响应信号时，`detach` 无效，要用 `remove` 强制清干净 shm 再重来。

</details>

---
[← 返回目录](README.md)
