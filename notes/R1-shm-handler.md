# R1 shm-handler 源码笔记

> 基线 `177a1dc465ff`，仓库根 `/home/y/src/bpftime-wt-official-177a1dc`。所有 `path:line` 相对仓库根。
> 「推断」= 只靠读代码得出、没跑；「未验证」= 没有办法或没有去验证；「已实跑」= 在 `build-jit/` 产物或
> scratchpad 实验程序上跑过（实验代码在 `/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R1/`）。

## 0. 一句话职责 + 范围

**职责**：在一段 boost::interprocess 共享内存里实现"假内核对象表"——一个定长 `handler_variant` 数组（下标=假 fd），
外加运行配置、会话计数器（epoch seqlock）、存活 pid 集合等命名对象；提供创建/打开/删除 shm、增删 handler、
map/perf/epoll/ringbuf 访问、JSON 导出导入、MPK 开关等 C/C++ 接口，供 syscall-server、agent、helper、daemon、工具使用。

**读了哪些**：
- `runtime/src/handler/` 全部（13 个文件）
- `runtime/src/bpftime_shm_internal.{cpp,hpp}`、`bpftime_shm.cpp`、`bpftime_shm_json.{cpp,hpp}`、`bpftime_config.cpp`、
  `bpftime_internal.h`、`bpftime_handler.hpp`、`platform_utils.cpp`
- `runtime/include/` 全部（14 个文件）
- `runtime/object/`（3 个）、`runtime/extension/`（2 个）
- `runtime/CMakeLists.txt`（以及为确认开关读了 `cmake/StandardSettings.cmake`）
- 相关单测（见 §1 末尾），以及为理解生命周期而读的跨模块片段：`runtime/syscall-server/syscall_server_utils.cpp:1-220`、
  `runtime/agent/agent.cpp` 中 shm/epoch 相关段、`runtime/src/attach/bpf_attach_ctx.cpp:55-175`、`runtime/src/bpftime_prog.cpp:120-285`（片段）

**总行数**：范围内源码 9980 行（handler+include+object+extension 6508 行；shm/config/internal/CMakeLists 等 3472 行）。
其中 `runtime/include/bpftime_epoll.h` 1299 行是 macOS 兼容用的 Linux uapi 结构拷贝，只做了结构性浏览。
相关单测另读约 2400 行。

## 1. 文件清单

| 文件 | 行数 | 做什么（一句话） | 必读? |
|---|---|---|---|
| `runtime/src/handler/handler_manager.hpp` | 136 | 命名对象常量、`get_global_shm_name`、`shm_remove`、`handler_variant`、`handler_manager` 声明 | 必读 |
| `runtime/src/handler/handler_manager.cpp` | 169 | 表的 get/set/clear/级联删除/find_minimal | 必读 |
| `runtime/src/handler/map_handler.hpp` | 303 | `bpf_map_handler`：attr、offset_ptr 容器指针、引用计数、spinlock | 必读 |
| `runtime/src/handler/map_handler.cpp` | 1478 | 按 map 类型 switch 分派 lookup/update/delete/next_key/init/free/push/pop/peek；外部 map ops 表 | 必读（选读各 case） |
| `runtime/src/handler/perf_event_handler.hpp` | 244 | perf handler、perf 环、per-线程 shard、data variant | 必读 |
| `runtime/src/handler/perf_event_handler.cpp` | 654 | perf 环写入/drain/回收、各 perf handler 构造函数 | 必读 |
| `runtime/src/handler/prog_handler.hpp` | 67 | `bpf_prog_handler`：字节码 vector、aot 字节、name | 必读 |
| `runtime/src/handler/prog_handler.cpp` | 20 | prog handler 构造（不设 type） | 必读（短） |
| `runtime/src/handler/link_handler.hpp` | 46 | `bpf_link_handler`：prog_id/attach_target_id/cookie | 必读 |
| `runtime/src/handler/epoll_handler.hpp` | 37 | epoll handler：weak_ptr 文件列表 | 选读 |
| `runtime/src/handler/epoll_handler.cpp` | 30 | epoll handler 构造 | 跳过 |
| `runtime/src/handler/memfd_handler.hpp` | 24 | memfd 占位 handler | 选读 |
| `runtime/src/handler/memfd_handler.cpp` | 11 | memfd 构造 | 跳过 |
| `runtime/src/handler/btf_handler.hpp` | 21 | `bpf_btf_handler{int id}`，TODO，未进 variant | 跳过 |
| `runtime/src/bpftime_shm_internal.hpp` | 343 | `class bpftime_shm`、epoch 结构、pid set 类型、`shm_lifecycle_lock`、`shm_holder` union | 必读 |
| `runtime/src/bpftime_shm_internal.cpp` | 1429 | shm 构造（5 种打开方式）、假 fd、各 add_*、epoch、pid 集合、MPK、全局 init/destroy/remove | 必读 |
| `runtime/src/bpftime_shm.cpp` | 710 | C 风格包装 `bpftime_*`、`bpftime_epoll_wait`、`map_ptr_by_fd`/`map_val` | 必读 |
| `runtime/src/bpftime_shm_json.hpp` | 48 | hex 编解码、导入导出声明 | 选读 |
| `runtime/src/bpftime_shm_json.cpp` | 368 | handler 表 ⇄ JSON | 选读 |
| `runtime/src/bpftime_config.cpp` | 176 | 环境变量 → `runtime_config` | 必读 |
| `runtime/src/bpftime_internal.h` | 52 | ufunc ctx、`map_ptr_by_fd`/`map_val` 声明、likely 宏 | 选读 |
| `runtime/src/bpftime_handler.hpp` | 18 | 把所有 handler 头一次 include | 跳过 |
| `runtime/src/platform_utils.cpp` | 37 | `my_sched_getcpu`；macOS 上伪造 sched_*affinity | 跳过 |
| `runtime/include/bpftime_shm.hpp` | 438 | 公共 API：`bpf_map_attr`、`bpf_event_type`、`bpf_map_type`、`shm_open_type`、所有 `bpftime_*` C 函数 | 必读 |
| `runtime/include/bpftime_config.hpp` | 126 | `runtime_config` 结构与默认值、fd 上下限 | 必读 |
| `runtime/include/bpf_attach_ctx.hpp` | 265 | agent 侧装配上下文（含 `last_epoch_seq_seen`、CUDA CommSharedMem 布局） | 选读（属 R 其他块） |
| `runtime/include/bpftime_prog.hpp` | 109 | `bpftime_prog` 类声明（执行体） | 选读 |
| `runtime/include/bpftime_helper_group.hpp` | 63 | helper group 类、`helper_is_async_signal_safe` | 选读 |
| `runtime/include/bpftime_logger.hpp` | 108 | `bpftime_set_logger`（console/文件/null sink） | 选读 |
| `runtime/include/bpftime_ufunc.hpp` | 78 | ufunc 类型与注册 API | 跳过 |
| `runtime/include/bpftime.hpp` | 38 | 汇总头 + tracepoint 原始结构 | 跳过 |
| `runtime/include/platform_utils.hpp` | 30 | macOS cpu_set_t 兼容 | 跳过 |
| `runtime/include/spinlock.hpp` | 55 | macOS 自旋锁（`yield` 指令） | 跳过 |
| `runtime/include/spinlock_wrapper.hpp` | 74 | 无 POSIX spinlock 时用 Spinlock 冒充 pthread_spinlock_t | 跳过 |
| `runtime/include/syscall_table.hpp` | 28 | syscall tracepoint 表声明 | 跳过 |
| `runtime/include/bpftime_epoll.h` | 1299 | macOS 下的 epoll/perf/bpf uapi 结构拷贝 + libbpf 桩 | 跳过 |
| `runtime/object/bpftime_object.hpp` | 34 | 用 libbpf 打开 .o 得到 `bpftime_prog` 列表的 API | 选读 |
| `runtime/object/bpf_object.cpp` | 237 | 上述实现 | 选读 |
| `runtime/object/CMakeLists.txt` | 69 | OBJECT 库 `bpftime-object` | 跳过 |
| `runtime/extension/extension_helper.cpp` | 198 | ufunc 分发 helper + 可选 io_uring/fs helper 组 | 选读 |
| `runtime/extension/userspace_xdp.h` | 19 | `xdp_md_userspace` 结构 | 跳过 |
| `runtime/CMakeLists.txt` | 291 | runtime 静态库源文件清单与开关 | 必读 |

**相关单测（已全部读完）**（`grep -l "handler_manager|bpftime_shm|shm_holder|runtime_config|import_shm|export_shm|epoch"` 筛出后按主题挑选）：

| 文件 | 行数 | 测什么 |
|---|---|---|
| `runtime/unit-test/common_def.hpp` | 24 | 测试用 `shm_remove`（与 `bpftime::shm_remove` 同名不同实现） |
| `runtime/unit-test/test_bpftime_shm_json.cpp` | 107 | hex 编解码；导出/导入 prog+tracepoint+link+array+percpu_array |
| `runtime/unit-test/test_config.cpp` | 60 | logger 路径 setter；`BPFTIME_ALLOW_EXTERNAL_MAPS`、`BPFTIME_FRIDA_FUZZY_BACKTRACER` |
| `runtime/unit-test/test_epoll.cpp` | 161 | `bpftime_epoll_wait` 对 ringbuf/软件 perf 的就绪、容量、超时、信号掩码恢复 |
| `runtime/unit-test/test_shm_allocation_failure.cpp` | 512 | shm 太小时 fail-open、日志不污染宿主 stdio、所有权删段、活 pid 保留段 |
| `runtime/unit-test/assets/shm_allocation_test_helper.c` | 233 | 上面测试的子进程 helper |
| `runtime/unit-test/assets/runtime_options_test_helper.c` | 22 | 只发一次 BPF_PROG_LOAD 的 helper（本组测试未直接用到） |
| `runtime/unit-test/test_software_perf_event.cpp` | 344 | perf 输出不改亲和性；多线程 shard；8 字节对齐与回绕；扩容 ENOMEM |
| `runtime/unit-test/maps/test_shm_hash_maps.cpp` | 231 | 跨 fork 读同一 handler 表；同名 map 互不干扰；dup 共享容器 + 引用计数 |
| `runtime/unit-test/maps/test_external_map_ops.cpp` | 94 | 外部 map ops 表 |
| `runtime/unit-test/attach_with_ebpf/test_shm_progs_attach.cpp` | 270 | 手工往 handler 表放 prog/perf/link，fork 子进程 `init_attach_ctx_from_handlers` |
| `runtime/unit-test/maps/test_map_in_map_bounds.cpp` | 38 | 只测 map-in-map 越界（与 handler 无关，读过，归 maps 读者） |
| `runtime/unit-test/CMakeLists.txt` | 158 | 测试清单（`test_shm_progs_attach` 只在 Frida 构建时编，:54-61） |

**没有任何单测覆盖 epoch/session 协议**（`grep -rn "epoch\|begin_new_session" runtime/unit-test` 无结果）。

## 2. 核心数据结构

### 2.1 `bpftime_shm`（每进程一个，住在进程的全局 union 里）
定义 `runtime/src/bpftime_shm_internal.hpp:78-325`。本身**不在 shm 里**，是进程本地"入口指针集合"：
- `open_type`（:81）：打开方式。`__destruct_shm` 据此判断是不是 agent。
- `segment`（:83）：`managed_shared_memory`，即映射本身。
- `manager`（:86，`= nullptr`）→ shm 中命名对象 `"bpftime_handler"`。
- `syscall_installed_pids`（:89）→ `"bpftime_syscall_pid_set"`。
- `runtime_config`（:92）→ `"bpftime_runtime_config"`。
- `injected_pids`（:95）→ `"bpftime_alive_agent_pids"`。**注意：没有 `= nullptr` 初始化器**（其余指针都有），见 §7。
- `alive_syscall_server_pids`（:98）→ `"bpftime_alive_syscall_server_pids"`。
- `pid_set_lock`（:101）→ `"bpftime_pid_set_lock"`（`interprocess_mutex`）。
- `epoch_state`（:103）→ 字面量 `"bpftime_global_epoch_state"`（不在常量表里，`bpftime_shm_internal.cpp:827,879,929`）。
- `local_runtime_config`（:106）：没有 shm 时的进程本地配置（`std::optional`）。
- `mock_setter`（:79）：syscall-server 注入的回调，CUDA map 初始化时临时关 mock（`map_handler.cpp:952,973` 等）。
- CUDA：`cuda_comm_shared_mem`（:108，shm 里 `"cuda_comm_shared_mem"`）、`cuda_host_memory_registered`（:109）。
- MPK：`is_mpk_init`、`pkey`（:113-117，仅 `BPFTIME_ENABLE_MPK`）。

生命周期：`shm_holder` 是 `union bpftime_shm_holder`（`bpftime_shm_internal.hpp:329-337`，定义 `bpftime_shm_internal.cpp:117`），
空构造/空析构；`bpftime_initialize_global_shm` 用 placement new 构造（`bpftime_shm_internal.cpp:49-57`），
`bpftime_destroy_global_shm` 显式析构并清 `global_shm_initialized`（:59-79）；进程退出时 `__destruct_shm`（:90-112，
`destructor(65535)`）兜底。也可以直接栈上构造 `bpftime_shm shm(name, type)`（单测这么做，`test_bpftime_shm_json.cpp:85`）。

### 2.2 shm 段布局（命名对象 + 匿名对象）
构造函数 `bpftime_shm_internal.cpp:790-995` 决定。名字常量在 `runtime/src/handler/handler_manager.hpp:51-59`：

| 名字 | 类型 | 创建方式 |
|---|---|---|
| `bpftime_handler` | `handler_manager`（内含 `vector<handler_variant>`，长度 = `max_fd_count`） | `find_or_construct`/`construct` |
| `bpftime_syscall_pid_set` | `boost::interprocess::set<int>` | 同上 |
| `bpftime_runtime_config` | `runtime_config`（1200 字节，纯 POD + char 数组，已实跑 sizeof） | 用**创建者进程的环境变量**构造 |
| `bpftime_alive_agent_pids` | `set<int>` | 同上 |
| `bpftime_alive_syscall_server_pids` | `set<int>` | 同上 |
| `bpftime_pid_set_lock` | `interprocess_mutex` | 同上 |
| `bpftime_global_epoch_state` | `{uint64_t epoch_seq = 0}` | 同上 |
| `cuda_comm_shared_mem` | `cuda::CommSharedMem`（`bpf_attach_ctx.hpp:93-102`） | 仅 CUDA，server 侧 construct+memset（:961-975），agent 侧最多等 1s find（:941-959） |
| 匿名 | 每个 map 的 impl、每个 map 的 `atomic_uint32_t` 引用计数、`software_perf_event_data`、shard list 节点等 | `anonymous_instance` |

段大小 = `shm_memory_size << 20`（MB，默认 50，`bpftime_config.hpp:79`；`bpftime_shm_internal.cpp:838,842,892`）。
handler 表占 `max_fd_count × sizeof(handler_variant)`：本机 x86-64 `sizeof(handler_variant)=168`（已实跑），
默认 6144 格 ≈ 1.03 MB；最大 1M 格 ≈ 168 MB（超过默认 50MB，构造时 `bad_alloc`，正是
`test_shm_allocation_failure.cpp:343-352` 用 `BPFTIME_MAX_FD_COUNT=1048576`+64MB 触发 fail-open 的原理）。
其余实测 sizeof：map 160、prog 104、perf 72、link 72、epoll 32、memfd 40（已实跑）。

### 2.3 `handler_manager`（在 shm 里）
`runtime/src/handler/handler_manager.hpp:100-132`；唯一成员 `handlers`（:131），`handler_variant_vector` 用 shm 分配器（:92-96）。
构造：`handlers(max_fd_count, allocator)` 一次分配全部格子，全部是 `unused_handler`（`handler_manager.cpp:19-24`）。
定长、**无任何锁**（头文件 include 了 sharable_mutex 但没用，:25-27）。析构只打 trace 日志（:26-33）。
- `get_handler`/`operator[]`：越界返回静态 `unused_handler` 哨兵（`handler_manager.cpp:35-47`）。
- `set_handler`：越界 `-ENOSPC`（:66-69）、已占 `-EEXIST`（:70-73）、放 unused `-ENOTSUP`（:74-78）；move 赋值后，若是 map 且无 impl
  就 `map_init(memory)`，**忽略返回值**（:80-86）。
- `set_handler_at_empty_slot`：取 `find_minimal_unused_idx`（:53-62），**生产代码没人用**，只有 `test_shm_progs_attach.cpp:114,144`。
- `find_minimal_unused_idx`：线性扫（:150-158）；唯一生产调用者是 daemon（`daemon/user/bpftime_driver.cpp:26` 经 `bpftime_find_minimal_unused_fd`）。
- `clear_id_at`（:98-148）：map → `map_free`；perf → 扫全表删 `attach_target_id==fd` 的 link；prog → 扫全表删 `prog_id==fd` 的 link；
  link → 先置空自己再 `clear_id_at(attach_target_id)`（:139-146，**不检查 target 类型**）；最后置空（:147）。
- `clear_all`：对所有已占格 `clear_id_at`（:160-167）。

### 2.4 `handler_variant`（`handler_manager.hpp:87-90`）
`variant<unused_handler(0), bpf_map_handler(1), bpf_link_handler(2), bpf_prog_handler(3), bpf_perf_event_handler(4), epoll_handler(5), memfd_handler(6)>`。
`bpf_btf_handler`（`btf_handler.hpp:14-17`）存在但**不在 variant 里**，只被 `bpftime_handler.hpp` include。

#### `bpf_map_handler`（`runtime/src/handler/map_handler.hpp:67-300`）
- 公有：`attr`（:69，完整 `bpf_map_attr`）、`type`（:73）、`name`（:74，shm string，只作展示——容器匿名）。
- 私有：`id`（:277）、`map_lock`（:278，`pthread_spinlock_t`，`pthread_spin_init(&map_lock, 0)` 即 PROCESS_PRIVATE，:95,108,121）、
  `map_impl_ptr`（:280，`offset_ptr<void>`，mutable）、`map_refcnt_ptr`（:281，`offset_ptr<atomic_uint32_t>`）、
  `max_entries/flags/key_size/value_size`（:282-285）。
- 构造（:75-98）只记参数，不建容器；移动构造/赋值转移指针并把源置空（:100-131）；拷贝被删。
- 析构不释放容器，若 `map_impl_ptr` 非空打 CRITICAL（:132-142）。
- 容器在 `map_init`（`map_handler.cpp:762-1144`）里按类型 `memory.construct<T>(anonymous_instance)`，并建/置 1 引用计数（:773-782）。
  未知类型：`allow_non_buildin_map_types` 为真则调外部 `alloc_map`，`map_impl_ptr=nullptr`（:1127-1137），否则返回 -1。
- `map_free`（:1146-1270）：引用计数 `fetch_sub`，>1 只解绑（:1153-1163），否则按类型 `destroy_ptr`。
- `can_share_map_impl`/`share_map_impl_from`/`inc_map_refcount`（hpp:246-262）给 `dup_bpf_map` 用。
- `get_userspace_value_size`：PERCPU_ARRAY/HASH 乘在线 CPU 数，GPU per-thread 乘 `gpu_thread_count`（cpp:69-84）。
- 每个操作（lookup/update/delete/next_key/push/pop/peek）都是一个大 switch，`impl->should_lock` 为真时持 `map_lock`（cpp:109-748, 1386-1476）。
  PERCPU 类型在 `from_syscall=true` 时走 `*_userspace` 版本（:147-158, 308-319, 623-634）。
  `ARRAY_OF_MAPS` 的 update/delete 只允许 from_syscall（:365-373, 680-688）。
- 外部 ops 表 `global_map_ops_table[2048]`（:60-62），进程本地，`bpftime_register_map_ops` 写入（:1375-1383）。

#### `bpf_prog_handler`（`prog_handler.hpp:26-63`）
`type`（:36）、`insns`（shm vector\<ebpf_inst\>，:59）、`aot_insns`（:60）、`name`（:62）。
构造（`prog_handler.cpp:9-19`）只 assign 字节码和名字，**`prog_type` 参数被丢弃，`type` 未初始化**（已实跑：存进去的是随机值 `-1388388152`）。
`aot_insns` 在本范围内无人写入。

#### `bpf_link_handler`（`link_handler.hpp:16-43`）
`args`（完整 `bpf_link_create_args`）、`prog_id`、`attach_target_id`、`attach_cookie`（optional）。
三个构造：从 args（只有 `attach_type==41` 才取 `perf_event.bpf_cookie`，:21-32）；(prog,target)（:33-36，**`args` 未初始化**）；
(prog,target,cookie)（:37-42）。纯 POD，不分配 shm。

#### `bpf_perf_event_handler`（`perf_event_handler.hpp:190-240`）
- `type`（int，可存自定义类型）、`enabled`（mutable bool，`enable()/disable()` 只改标志，:193-207）、`data`（:209）。
- `data` = `variant<uprobe_perf_event_data, tracepoint_perf_event_data, software_perf_event_shared_ptr, custom_perf_event_data, kprobe_perf_event_data>`（:184-187）。
  - uprobe：`offset, pid, ref_ctr_off, _module_name`（:161-166）；uprobe/uretprobe/override/ureplace 共用。
  - kprobe：`func_name, addr, ref_ctr_off`（:168-172）。
  - tracepoint：`pid, tracepoint_id`（:173-178）。
  - custom：`attach_argument` 字符串（:180-182）。
  - software：`managed_shared_ptr<software_perf_event_data>`（:154-156）。
- 6 个构造函数（cpp:148-235），分别对应 custom / override+ureplace（可 `default_enabled`）/ uprobe / kprobe / tracepoint / software。
- **`enabled` 标志在 agent 装配路径上没人读**（`grep enabled runtime/src/attach attach/base_attach_impl` 只有 CUDA 无关命中），
  即 `PERF_EVENT_IOC_DISABLE` 对 agent 无效果（推断，基于 grep）。JSON 导出会带上它。

#### `software_perf_event_data` / buffer / shard（`perf_event_handler.hpp:71-152`，实现 cpp:247-629）
- `software_perf_event_buffer`：`pagesize`、`mmap_buffer`（第一页是 `perf_event_mmap_page` 头，其后是 2^n 数据区）、`copy_buffer`（drain 中转）。
  写入保留 1 字节空隙（cpp:344-350），记录按 8 字节对齐（cpp:370-378），单记录上限 `65535 - sizeof(perf_sample_raw) - 7`（cpp:371-375，超出 E2BIG）。
  **满了或尚未 mmap 时静默丢弃并返回 0**（`append_record_parts` 返回 false 被 `output_data` 忽略，cpp:385-387；测试显式断言此行为 `test_software_perf_event.cpp:282-285`）。
- `software_perf_event_shard`：`pid, tid, generation, buffer_generation, buffer`（hpp:101-113）。
- `software_perf_event_data`：`cpu, config, sample_type, pagesize, event_generation, producer_buffer_generation, consumer_buffer, shard_lock(PROCESS_SHARED spinlock), next_shard_generation, drains_since_reclaim, producer_shards(list)`（hpp:122-152）。
  - 生产者 `output_data` → `get_current_thread_shard()`：先查 **thread_local** 缓存（cpp:143-145，最多 64 项），未命中再持 `shard_lock` 扫链表或新建 shard（cpp:491-548）。
    shard 的数据区大小 = 当前 consumer 缓冲大小。
  - 消费者 `has_data()` 先 `drain_producer_shards()`（持锁把每个 shard 的记录逐条搬进 consumer 环，cpp:550-558, 589-593），每 64 次 drain 或 shard≥64 时
    回收"无数据且线程已死（`tgkill(pid,tid,0)`）"的 shard（cpp:560-587）。
  - consumer 扩容（mmap 拦截时）`ensure_mmap_buffer` 持锁、成功改变大小就 `producer_buffer_generation++`，旧 shard 不再被新写入命中（cpp:611-624）。
  - 生命周期：perf handler 构造时 `mem.construct<...>(anonymous_instance)` 并包进 managed_shared_ptr（cpp:224-235）；handler 被清时 variant 析构释放引用；epoll 里只存 weak_ptr。

#### `epoll_handler`（`epoll_handler.hpp:43-56`）
`mutable file_vector files`，元素 `epoll_file{variant<software_perf_event_weak_ptr, ringbuf_weak_ptr> file; epoll_data_t data}`。
构造函数里有一个无用局部变量 `epoll_event ev;`（`epoll_handler.cpp:18`）。

#### `memfd_handler`（`memfd_handler.hpp:16-21`）
`flags`、`name`，纯占位。

### 2.5 `runtime_config`（`runtime/include/bpftime_config.hpp:52-118`）
字段：`debug`（:53，**全仓库无人读写**）、`jit_enabled`、三个 helper group 开关、`verifier_mode`、`allow_non_buildin_map_types`、
`enable_frida_fuzzy_backtracer`、`shm_memory_size`(MB)、`max_fd_count`、`logger_output_path[1024]`、`vm_name[128]`、`enable_gpu_gdrcopy`、
`gpu_gdrcopy_max_per_key_bytes`。全部定长，适合放进 shm。setter 用 `strcpy`（:106-113），无长度检查。
注意结构体默认 `enable_ufunc_helper_group=false`（:59），但 `construct_runtime_config_from_env()` 在没设 `BPFTIME_HELPER_GROUPS` 时
把三组**全部置 true**（`bpftime_config.cpp:100-104`）。

### 2.6 epoch 状态与 pid 集合
- `bpftime_global_epoch_state{uint64_t epoch_seq=0}`（`bpftime_shm_internal.hpp:53-55`）；特殊值 `UNSTABLE=UINT64_MAX`、`MISSING=UINT64_MAX-1`（:46-47）。
- pid 集合：三个 `boost::interprocess::set<int>`（:57-75），由 `pid_set_lock`（`interprocess_mutex`）保护，访问统一走
  `pid_set_scoped_lock`：250ms 内每 1ms `try_lock`，超时/异常放弃（`bpftime_shm_internal.cpp:121-175`）。之所以不用阻塞 lock，推断是因为持锁进程可能已死（非 robust mutex）。

### 2.7 `shm_lifecycle_lock`（`bpftime_shm_internal.hpp:34-44`，实现 `.cpp:178-229`）
进程本地 RAII：把 shm 名里非 `[A-Za-z0-9_.-]` 字符换成 `_`，打开 `/tmp/bpftime-shm-<名>.lock`（O_CREAT, 0666，再 fchmod 0666），`flock(LOCK_EX)` 阻塞等待。
打开失败/异常时**静默不加锁**（fail-open）。析构只 `LOCK_UN`+close，**从不 unlink**（/tmp 里已观察到 30 个残留 lock 文件，已实跑 `ls /tmp/bpftime-shm-*.lock | wc -l`）。

### 2.8 公共枚举/结构（`runtime/include/bpftime_shm.hpp`）
- `bpf_map_attr`（:27-46）：内核字段 + `kernel_bpf_map_id` + `gpu_thread_count=1024`。
- `bpf_event_type`（:48-64）：0-5 对应内核 PERF_TYPE_*，6-9 为 u/uret/k/kret probe，1008 override，1009 ureplace。
- `bpf_map_type`（:70-135）：内核类型 + `KERNEL_USER_*`（+1000）+ GPU 类型（+1500…）；`BPF_MAP_TYPE_MAX=2048`。
- `shm_open_type`（:137-143）：`REMOVE_AND_CREATE, OPEN_ONLY, NO_CREATE, CREATE_OR_OPEN, CREATE_ONLY`。
- `bpf_link_create_args`（:251-298）：拷贝自内核 `bpf_attr.link_create`；`BPFTIME_BPF_PERF_EVENT_ATTACH_TYPE=41`（:248）。
- `bpftime_map_ops`（:196-211）：外部 map 回调表。
- `extern const shm_open_type global_shm_open_type;`（:181）**只有声明没有定义**（全仓 grep 无定义）。

## 3. 主要流程与调用链

### 3.1 shm 创建/打开（构造函数五条路）
`bpftime_initialize_global_shm(type)`（`bpftime_shm_internal.cpp:49`）→ `bpftime_shm(type)`（:1033，取 `get_global_shm_name()`，`handler_manager.hpp:60-67`）
→ `bpftime_shm(name,type)`（:790）：
1. 先 `construct_runtime_config_from_env()`（:794）拿 `shm_memory_size`/`max_fd_count`——**每种打开方式都读环境变量**，但只有建段时用得上。
2. `SHM_OPEN_ONLY`（:797-829）：`open_only` 映射，逐个 `find`；缺失就是 nullptr。不存在的段抛 `interprocess_exception`。
3. `SHM_CREATE_OR_OPEN`/`SHM_CREATE_ONLY`（:830-880）：`open_or_create`/`create_only` 映射，逐个 `find_or_construct`。复用旧段时拿到**旧表、旧配置、旧大小、旧 max_fd_count**。
4. `SHM_REMOVE_AND_CREATE`（:881-930）：`shared_memory_object::remove` 后 `create_only`，逐个 `construct`。
5. `SHM_NO_CREATE`（:931-936）：打日志后直接 return（测试/工具用），之后所有 `manager` 判空路径返回失败。
6. CUDA（:937-976）、MPK（:979-994）收尾。
Boost 1.83 的 `truncate` 用 `posix_fallocate`（`/usr/include/boost/interprocess/shared_memory_object.hpp:462`），所以建段即实占 tmpfs。

### 3.2 假 fd 分配与 handler 放置（syscall-server 主路径）
`syscall_context` 拦截 `bpf(BPF_MAP_CREATE)`（`runtime/syscall-server/syscall_context.cpp:393`）→ `bpftime_maps_create(-1,...)`（`bpftime_shm.cpp:81-84`）
→ `bpftime_shm::add_bpf_map`（`bpftime_shm_internal.cpp:1048-1077`）
→ `open_fake_fd()`（:674-688）：`open("/dev/null")`，得到 ≤2 时 dup（最多 4 次，**被 dup 的低号 fd 不关**），号码 ≥ 表长则 close 并 `ENOSPC`
→ （verifier 构建时登记 map 描述，:1058-1069）→ 目标格非空则 `clear_id_at`（:1070-1074）
→ `handler_manager::set_handler`（`handler_manager.cpp:63-88`）→ `bpf_map_handler::map_init`（`map_handler.cpp:762`）匿名构造容器。
要点：假 fd 号由**调用进程的内核 fd 表**决定（借来的 `/dev/null`），与 `find_minimal_unused_idx`（表内最小空格）是两套不同的分配器——
实跑显示表空时 `find_minimal_unused_fd()=0`，而 `open_fake_fd()` 返回 3（已实跑 exp.cpp）。prog/uprobe/tracepoint/link 同理（`add_bpf_prog` :691-706 等，fd<0 才分配）。
`add_bpf_link`（:709-750）是唯一"先校验再分配"的：prog 必须是 prog，perf 型 link 的 target 必须是 perf handler，失败不开 fd。

### 3.3 关闭与级联删除
`close()` 拦截（`syscall_context.cpp:273`）→ `bpftime_close`（`bpftime_shm.cpp:282-285`）→ `bpftime_shm::close_fd`（`bpftime_shm_internal.cpp:752-757`）
→ `handler_manager::clear_id_at`（`handler_manager.cpp:98-148`）。真 `/dev/null` fd 由调用方关（`bpftime_shm_internal.hpp:286-288` 注释）。
级联（已实跑）：同一 perf 上挂两个 link，关掉 link1 后 **perf 和 link2 都没了**，prog 还在：
`[cascade] after close_fd(link1): is_perf(perf)=0 exist(link2)=0 is_prog(prog)=1`。

### 3.4 会话（epoch seqlock）
写侧：`start_up`（`runtime/syscall-server/syscall_server_utils.cpp:147-172`）→ `begin_new_session`（`bpftime_shm_internal.cpp:1017-1031`）：
`epoch_seq += 1`（变奇）→ `reset_server_state`（:1381-1397：`manager->clear_all` + 持 pid 锁清 `syscall_installed_pids`）→ `epoch_seq += 1`（变偶）。
初值 0，第一次会话后为 2，第二次为 4（已实跑：`begin_new_session -> 2`、`-> 4`）。
读侧：`read_stable_epoch_seq`（:997-1015：奇数 sleep 1ms 重试，连读两次相等才返回，最多 200 次）←
`bpf_attach_ctx::init_attach_ctx_from_handlers`（`runtime/src/attach/bpf_attach_ctx.cpp:73-162`）：最多 3 轮；扫表前读到的值与 `last_epoch_seq_seen` 不同就拆所有 link 并清实例化状态（:91-104），
扫两遍表（第二遍给 CUDA link，:106-136），扫后再读，不同就拆掉重来（:137-159），3 轮都不成功返回 `-EAGAIN`。MISSING 当 0 处理（:79-83, 139-141）。
另一个读者：agent IPC `status` 命令回显 `epoch_seq=`（`runtime/agent/agent.cpp:370-386`）。
**这个 seqlock 只保护"换会话"，不保护会话内的普通增删**：`set_handler`/`clear_id_at` 不碰 epoch_seq。

### 3.5 shm 所有权与删除（跨模块，生命周期视角）
syscall-server：`start_up` 持 `shm_lifecycle_lock`（`syscall_server_utils.cpp:157-158`）→ `initialize_global_shm_with_ownership`（:126-145：先 `SHM_CREATE_ONLY`，
`already_exists_error` 时退回 `SHM_CREATE_OR_OPEN`，其他异常先删段再抛）→ `atexit(remove_syscall_server_global_shm)`（:161）→ 登记 alive server pid（:162-168，失败则放弃所有权）
→ `begin_new_session`（:172）→ `bpftime_set_runtime_config`（:216，用本进程环境变量覆盖 shm 配置）。
退出：`remove_syscall_server_global_shm`（:56-124）：非 owner pid（fork 出的子进程）或非 owner 只摘 pid；owner 持锁摘自己、
在 alive server 集合和 alive agent 集合里都找不到活 pid（`kill(pid,0)==0||EPERM`，:51-54）才 `shared_memory_object::remove`。
agent：持锁重试 60×50ms `SHM_OPEN_ONLY`（`agent.cpp:931-948`），初始化成功后登记 alive agent pid 再放锁（:1138-1145）。
进程退出兜底 `__destruct_shm`：OPEN_ONLY 进程摘 alive agent pid，然后析构（`bpftime_shm_internal.cpp:90-112`）。
无条件删段：`bpftime_remove_global_shm`（:81-88）← `bpftimetool remove`（`tools/bpftimetool/main.cpp:262`）。

### 3.6 helper 访问 map（数据面）
JIT/解释器加载时 `ebpf_set_lddw_helpers(vm, map_ptr_by_fd, nullptr, map_val, ...)`（`runtime/src/bpftime_prog.cpp:128,156,160`）
→ `map_ptr_by_fd`（`bpftime_shm.cpp:635-648`）校验是 map 后**返回 fd 本身**；`map_val`（:650-672）取第一个 key 的值地址（用于 `.data/.rodata` 之类的 map value 重定位）。
运行时 helper `bpf_map_lookup_elem` → `shm_holder.global_shared_memory.bpf_map_lookup_elem(fd,key,false)`（`bpf_helper.cpp:303`）
→ `try_get_map_handler`（`bpftime_shm_internal.cpp:266-273`，判型失败 `ENOENT`）→ `bpf_map_handler::map_lookup_elem`（`map_handler.cpp:109`）→ impl。
syscall 侧则走 C 包装 `bpftime_map_lookup_elem`（`bpftime_shm.cpp:115-126`，`from_syscall=true`，捕获异常返回 nullptr）。

### 3.7 perf 输出与 epoll_wait（数据面 → 控制面）
`bpf_perf_event_output` helper（`bpf_helper.cpp:487-551`）→ `bpftime_perf_event_output`（`bpftime_shm.cpp:575-596`）→ `software_perf_event_data::output_data`（`perf_event_handler.cpp:601-604`）
→ 本线程 shard（:491-548）→ `software_perf_event_buffer::output_data`（:367-388）。
loader 侧 `epoll_wait` 拦截（`syscall_context.cpp:1119`）→ `bpftime_epoll_wait`（`bpftime_shm.cpp:418-539`）：轮询 `epoll_inst.files`，
weak_ptr lock 成功且 `has_data()`（软件 perf 会顺带 drain）就填事件；没数据时临时解除 SIGINT/SIGTERM 阻塞、`sigtimedwait` 1ms，
收到则手动调用原 handler 并返回 `EINTR`；最后恢复原 sigmask。注意 `has_data()` 不消费数据，同一源下次还会报就绪（`test_epoll.cpp:122`）。

### 3.8 JSON 导出/导入
导出 `bpftime_export_shm_to_json`（`bpftime_shm_json.cpp:276-368`）：遍历已占格，按类型写 `{"type":..., "attr":..., "name"?}`，key 为 fd 字符串。
导入 `bpftime_import_shm_from_json`（:237-268）：读文件 → 收集 (fd, json) → `stable_sort` 把 link 放最后 → 逐个 `import_shm_handler_from_json`（:106-221）调用
`add_bpf_prog/add_bpf_map/add_uprobe/add_tracepoint/add_uprobe_override/add_software_perf_event/add_bpf_link`（显式 fd）。
单个 handler：`bpftime_import_shm_handler_from_json(fd, json_text)`（:223-235）← `bpftimetool load`。
不对称之处见 §7（实跑：含 epoll 的导出无法再导入；软件 perf 导入抛 json 异常；cookie、prog type 丢失/错乱）。

## 4. 跨模块接口

### 4.1 本块对外提供（及调用点）
| 接口 | 主要调用者 |
|---|---|
| `bpftime_initialize_global_shm` / `bpftime_destroy_global_shm` / `bpftime_remove_global_shm` | syscall-server `syscall_server_utils.cpp:129,143`；agent `agent.cpp:937`；daemon `daemon/user/bpftime_driver.cpp:402,409`；`tools/bpftimetool/main.cpp:87,197,225,249,262,328`；`tools/aot/main.cpp:109,160,218`；`tools/cli/main.cpp:1263`；示例/基准若干 |
| `bpftime_maps_create` | `syscall_context.cpp:393,533`；`bpftime_driver.cpp:254` |
| `bpftime_progs_create` | `syscall_context.cpp:475,695`；`bpftime_driver.cpp:197` |
| `bpftime_link_create` | `syscall_context.cpp:718` |
| `bpftime_uprobe_create` / `kprobe_create` / `tracepoint_create` | `syscall_context.cpp:837/886/900`；uprobe 也被 `bpftime_driver.cpp:325` |
| `bpftime_add_software_perf_event` / `bpftime_add_ureplace_or_override` | `syscall_context.cpp:907` / `:915,925` |
| `bpftime_attach_perf_to_bpf(_with_cookie)` | `syscall_context.cpp:729,796,1059`；`bpftime_driver.cpp:304` |
| `bpftime_perf_event_enable/disable` | `syscall_context.cpp:1032` 附近；`bpftime_driver.cpp:345` |
| `bpftime_close` | `syscall_context.cpp:273`；`bpftime_driver.cpp:381` |
| `bpftime_maps_dup` / `bpftime_add_memfd_handler` | `syscall_context.cpp:1210` / `:1227` |
| `bpftime_epoll_create/_wait`、`add_ringbuf_fd_to_epoll`、`add_software_perf_event_fd_to_epoll` | `syscall_context.cpp:1077/1119/1089/1095` |
| `bpftime_get_ringbuf_consumer_page`、`bpftime_get_array_map_raw_data`、`bpftime_get_software_perf_event_raw_buffer` | `syscall_context.cpp:961/982/991`（mmap 拦截） |
| `bpftime_map_lookup/update/delete/get_next_key/lookup_and_delete`、`bpftime_map_value_size_from_syscall`、`bpftime_map_get_info`、`bpftime_translate_shared_map_type_to_kernel_map_type`、`bpftime_is_map_fd` | `syscall_context.cpp:569,588,624,613,576,754,496,387`；`bpf_helper.cpp:512`（get_info）；`bpf_attach_ctx_cuda.cpp:140,202` |
| `bpftime_perf_event_output` / `bpftime_shared_perf_event_output` / `bpftime_ringbuf_reserve/submit` / `bpftime_is_prog_array` / `bpftime_get_current_thread_cookie` | `bpf_helper.cpp:529/536/440,458/558/668` |
| `shm_holder.global_shared_memory.*`（C++ 直接用） | `bpf_helper.cpp:303,520`（map 访问）、`:819`（stack trace）；`bpf_attach_ctx.cpp:65,78,138`；agent、syscall-server 多处 |
| `bpftime_get_runtime_config` / `bpftime_set_runtime_config` | get：`agent.cpp:699,749,956,1127`、`syscall_context.cpp:652`、`map_handler.cpp:1128`；set：`syscall_server_utils.cpp:216`、**`bpftime_prog.cpp:140`**、`bpftime_driver.cpp:404`、`tools/aot/main.cpp:219`、`tools/bpftimetool/main.cpp:252` |
| `construct_runtime_config_from_env` | `syscall_context.cpp:206`、`syscall_server_utils.cpp:152`、`bpftime_driver.cpp:403`、shm 构造函数 |
| `map_ptr_by_fd` / `map_val` | `bpftime_prog.cpp:128,156,160`（作为 lddw 回调传给 vm compat 层） |
| `begin_new_session` / `read_stable_epoch_seq` / pid 集合 / `shm_lifecycle_lock` / `set_mock_setter` | 见 §3.4、§3.5 |
| `bpftime_find_minimal_unused_fd` | 仅 `bpftime_driver.cpp:26`（daemon） |
| `bpftime_import_*_json` / `bpftime_export_global_shm_to_json` | `tools/bpftimetool/main.cpp:201,226,254`；`example/libbpftime_example/main.cpp:11,13` |
| `bpftime_register_map_ops` | 仅单测与外部使用者 |
| `bpftime_protect_enable/disable` | `bpftime_prog.cpp:255,274`（每次执行前后） |
| `bpftime_set_logger` / `bpftime_logger_flush`（logger.hpp） | `syscall_context.cpp:201`、`agent.cpp:134,957`、`tools/cli/main.cpp:883`、`tools/aot/main.cpp:247` |
| `get_extension_helper_group`（extension） | `bpf_helper.cpp:1181-1185`：ufunc helper group **就是**它 |
| `bpftime_object_open` 等（object/） | 单测（`test_shm_progs_attach.cpp:213` 等）、工具 |
| `get_gpu_map_extra_buffer` / `get_gpu_map_max_thread_count` | `bpf_attach_ctx_cuda.cpp:343` 等 |

### 4.2 本块依赖外部
- map 实现：`runtime/src/bpf_map/userspace/*`、`bpf_map/shared/*`（libbpf 构建）、`bpf_map/gpu/*`（CUDA）——`map_handler.cpp:7-44` include，按类型构造。
- `ebpf_inst` 定义（`vm/include/ebpf-vm.h`）：`prog_handler.hpp:6`；verifier：`verifier::get_map_descriptors/set_map_descriptors`（`bpftime_shm_internal.cpp:1058-1069`）。
- `bpf_attach_ctx.hpp` 引 `attach_private_data.hpp`、`base_attach_impl.hpp`（attach 模块）。
- CUDA：`stop_cuda_watcher_before_shm_unmap`（`bpftime_shm_internal.cpp:37,64`，实现在 `bpf_attach_ctx_cuda.cpp`）、cuda runtime API。
- `current_thread_bpf_cookie`（`bpftime_prog.hpp:18`，定义在 prog 模块）被 `bpftime_get_current_thread_cookie` 用（`bpftime_shm.cpp:262-269`）。
- libbpf（object/：`bpf_object__open` 等）、Boost.Interprocess、spdlog、nlohmann json（`third_party/json.hpp`）、liburing（可选）。

## 5. 配置项、环境变量、编译开关

### 5.1 环境变量
| 名字 | 默认 | 读取位置 | 影响 |
|---|---|---|---|
| `BPFTIME_GLOBAL_SHM_NAME` | `bpftime_maps_shm` | `handler_manager.hpp:60-67`（每次调用都 getenv） | shm 名；lock 文件名 |
| `BPFTIME_SHM_MEMORY_MB` | 50（夹到 1–10240） | `bpftime_config.cpp:119` | 只在建段时生效 |
| `BPFTIME_MAX_FD_COUNT` | 6144（夹到 128–1M） | `bpftime_config.cpp:124-128` | 只在建 handler 表时生效；复用旧段时 shm 里配置的 `max_fd_count` 会被覆盖成新值、与真实表长不一致（推断） |
| `BPFTIME_HELPER_GROUPS` | 未设=三组全开 | `bpftime_config.cpp:92-104` | 逗号分隔 `ufunc/kernel/shm_map`；一旦设置先全关再按 token 开 |
| `BPFTIME_DISABLE_JIT` | 未设=JIT | :106-108 | `jit_enabled=false` |
| `BPFTIME_ALLOW_EXTERNAL_MAPS` | 未设 | :110-112 | 允许未知 map 类型 |
| `BPFTIME_FRIDA_FUZZY_BACKTRACER` | 未设 | :114-116 | Frida 模糊回溯 |
| `BPFTIME_VM_NAME` | 编译期 `DEFAULT_VM_NAME`（llvm；无 LLVM JIT 时 ubpf） | :130-135 | `strcpy` 进 128 字节数组 |
| `BPFTIME_VERIFIER_LEVEL` | WARNING | :137-156 | STRICT/WARNING/NO_VERIFY |
| `BPFTIME_LOG_OUTPUT` | `~/.bpftime/runtime.log` | :158-161 | `strcpy` 进 1024 字节数组；`console`=stderr |
| `BPFTIME_GPU_ARRAY_GDRCOPY` | 未设 | :165-168 | 首字符 '1' 启用 |
| `BPFTIME_GPU_ARRAY_GDRCOPY_MAX_PER_KEY_BYTES` | 4096（0–1GiB） | :169-174 | GDRCopy 阈值 |
| `BPFTIME_MAP_GPU_THREAD_COUNT` | `attr.gpu_thread_count`=1024 | `map_handler.cpp:750-760` | GPU per-thread/ringbuf map 尺寸；`stoull` 无异常保护 |
| `SPDLOG_LEVEL` | — | `bpftime_logger.hpp`（`spdlog::cfg::load_env_levels`） | 日志级别 |

**重要**：shm 里的 `runtime_config` 来自**建段/最后一次 set 的进程**的环境变量；agent 读的是 shm 里那份（`agent.cpp:956`），
agent 自己进程的这些环境变量除了 shm 名之外基本不起作用（推断）。

### 5.2 编译开关
| 开关 | 定义处 | 作用 |
|---|---|---|
| `BPFTIME_ENABLE_MPK` | `cmake/StandardSettings.cmake:84-87`（`add_definitions(-DBPFTIME_ENABLE_MPK)`），默认 OFF | shm 构造时 `pkey_alloc`+`pkey_mprotect`；`bpftime_protect_*` 生效 |
| `BPFTIME_ENABLE_IOURING_EXT` | `StandardSettings.cmake:89-92`，默认 OFF；`runtime/CMakeLists.txt:198-203` 链 uring | extension 里 io_uring helper |
| `BPFTIME_ENABLE_FS_HELPER` | **任何 CMake 都不定义** | `extension_helper.cpp:28-45,148-161` 的 fs helper 实际永不编译 |
| `BPFTIME_USE_VAR_HASH_MAP` | **任何 CMake 都不定义** | HASH map 用 `fix_size_hash_map_impl`（`map_handler.cpp:54-58`） |
| `BPFTIME_BUILD_WITH_LIBBPF` | 默认 ON | 决定 kernel-user map、PROG_ARRAY、STACK_TRACE 的 init/free 分支（`map_handler.cpp:886-933,1194-1217`）；object/ 子目录与单测是否构建（`runtime/CMakeLists.txt:271-286`） |
| `BPFTIME_ENABLE_CUDA_ATTACH` | 默认 OFF | GPU map、CommSharedMem、cudaHostRegister |
| `BPFTIME_LLVM_JIT` | 默认 ON | 否则 `DEFAULT_VM_NAME="ubpf"`（`runtime/CMakeLists.txt:127-131`） |
| `ENABLE_EBPF_VERIFIER` | 默认 OFF | 给 runtime 定义 `ENABLE_EBPF_VERIFIER ENABLE_BPFTIME_VERIFIER`（PRIVATE，:205-210）；`add_bpf_map` 登记 map 描述 |
| `BPFTIME_ENABLE_UNIT_TESTING` | 默认 OFF（build-jit 里 ON） | 且需 libbpf 才建单测（:281-286） |
| `DEBUG` | — | `bpftime_destroy_global_shm` 打 fprintf（`bpftime_shm_internal.cpp:74-77`） |

runtime 库组成（`runtime/CMakeLists.txt:54-125`）：`add_library(runtime ${sources})` 未指定 STATIC/SHARED（默认静态，build-jit 产物为 `libruntime.a`）；
源文件 = attach ctx + 6 个 handler + shm 三件套 + prog/config/ufunc/bpf_helper/platform_utils + 14 个 userspace map + extension_helper；
CUDA 再加 9 个，Linux+libbpf 再加 4 个 kernel-user map。PUBLIC 链 `bpftime_vm spdlog bpftime_base_attach_impl`，有 trap 后端时链之并定义
`BPFTIME_HAVE_TRAP_UPROBE_ATTACH`（:167-170）。`syscall_id_table` 自定义目标生成 `syscall_id_list.h`（:135-143）。
`bpftime-object` 是 OBJECT 库，只在 libbpf 构建时加（`runtime/object/CMakeLists.txt:2-4`，`runtime/CMakeLists.txt:271-273`）。
build-jit 的 CMakeCache：LIBBPF=ON、CUDA=OFF、MPK=OFF、UNIT_TESTING=ON、LLVM_JIT=ON、ENABLE_EBPF_VERIFIER=OFF（已实跑 grep）。

## 6. 并发、生命周期与错误处理

- **handler 表无锁**：`handler_manager` 不加任何锁；读者（agent）与写者（server）并发时，`std::variant` 赋值非原子，存在撕裂读（推断）。
  epoch seqlock 只覆盖 `begin_new_session`，会话内普通 `set_handler`/`clear_id_at` 不会改变 epoch（`bpftime_shm_internal.cpp:1017-1031` 是唯一修改 epoch 的地方）。
  同一 server 进程内多线程同时 `bpf()` 也无保护（推断）。
- **map 锁**：每个 map 一个 `pthread_spinlock_t`，PROCESS_PRIVATE 初始化（`map_handler.hpp:95`），glibc 下跨进程可用但依赖实现；
  `should_lock` 由 impl 决定。lookup 返回的值指针在解锁后才被使用（任何 map 都是这样，推断）。
- **perf shard**：每 (pid,tid) 单生产者、consumer 单消费者，用 release/acquire 宏（`perf_event_handler.cpp:37-77`，x86 只是编译器屏障）；
  shard 链表由 PROCESS_SHARED spinlock 保护；生产者热路径通过 thread_local 缓存避免加锁。
- **segment manager 锁**：所有 shm 分配（hash 插入、新建 shard、perf 扩容）都经 boost segment manager 的进程间互斥量。若持锁进程被 kill，
  其他进程分配时会永久阻塞（非 robust mutex；推断，未验证）。
- **pid 集合锁**：`pid_set_scoped_lock` 250ms 超时放弃（`bpftime_shm_internal.cpp:121-175`），各 add/remove/iterate 失败时返回 false；
  `remove_syscall_server_global_shm` 在快照失败时选择不删段（`syscall_server_utils.cpp:96-98,114-116`）。
- **跨进程生命周期锁**：`shm_lifecycle_lock`（flock）串行化 server 启动/退出删段 与 agent 打开+登记；打开 lock 文件失败则不加锁继续。
- **fork**：子进程继承 `shm_holder` 映射与 `g_syscall_server_shm_owner_pid`，退出时 `getpid()!=owner` 只摘 pid 不删段（`syscall_server_utils.cpp:61-69`）；
  perf shard 缓存 key 用数据指针、校验 pid/tid，fork 后 tid 不同会重建 shard（`perf_event_handler.cpp:497-511`）。
- **进程退出顺序**：`__destruct_shm` 是 `.fini_array` 析构（`destructor(65535)`），在 `atexit`/`__cxa_atexit` 注册的回调（包括 spdlog 全局对象析构、
  `remove_syscall_server_global_shm`）之后运行；所以 `bpftime_destroy_global_shm` 里避开 spdlog（:72-73 注释）。但 `__destruct_shm` 先调的
  `remove_pid_from_alive_agent_set` 在超时时会 `SPDLOG_WARN`（:153），理论上可能碰到已析构的 spdlog（推断）。
- **错误约定混杂**：`set_handler` 直接返回负 errno（`-ENOSPC/-EEXIST/-ENOTSUP`），多数 `bpftime_shm` 方法返回 -1 并设 `errno`，
  `add_bpf_prog_attach_target` 返回 `-ENOSPC`（:444-448），`bpftime_poll_from_ringbuf` 返回 `-EINVAL`。C 包装里只有 map 数据操作 try/catch，
  `bpftime_maps_create` 等**不捕获** `bad_alloc`（由 syscall-server 的 `handle_exceptions` 兜底，跨模块）。
  `set_handler` 失败时，`open_fake_fd` 打开的真 `/dev/null` fd 不会被关闭（各 add_* 都没有清理路径，推断泄漏）。
- **fail-open**：`set_handler` 忽略 `map_init` 失败（`handler_manager.cpp:80-86`）——实跑：`max_entries=3` 的 ringbuf 被接受，返回 fd 7，`has_map_impl=0`；
  之后对它的 lookup 会在 `static_cast<ringbuf_map_impl*>(nullptr)->...` 上空指针解引用（推断，未跑以免崩溃）。
- **MPK**（仅 `BPFTIME_ENABLE_MPK`）：每个进程在构造 shm 时 `pkey_alloc(0, PKEY_DISABLE_WRITE)` + `pkey_mprotect(整段, RW, pkey)`（`bpftime_shm_internal.cpp:979-994`）；
  `bpftime_prog_exec` 执行前 `pkey_set(pkey,0)`、执行后 `pkey_set(pkey, PKEY_DISABLE_WRITE)`（`bpftime_prog.cpp:255,274` → `bpftime_shm_internal.cpp:760-778`）。
  即保护目标是"宿主代码在 eBPF 执行之外误写 shm"。PKRU 是**每线程**的，见 §7 的问题。

## 7. 不直观的行为、疑似 bug、代码与注释不一致

1. **`bpftime::shm_remove` 析构删错了段**（确认）。`runtime/src/handler/handler_manager.hpp:69-85`：带名字的构造函数删 `name`，析构却删
   `get_global_shm_name()`。用 `bpftime::shm_remove remover(SHM_NAME)` 的单测（`test_external_map_ops.cpp:50`、`test_shm_hash_maps.cpp:103`、
   `test_shm_progs_attach.cpp:202`、`tailcall/*.cpp`）跑完会**留下自己的段**，并且**删掉全局默认段 `bpftime_maps_shm`**（若未设环境变量）——
   在有真实会话的机器上跑单测会把正在用的 shm 删掉。证据：实跑 exp.cpp `[shm_remove] after dtor exists(n3)=1`；`/dev/shm` 里长期残留
   `BPFTIME_TEST_ENTERNAL_MAP_SHM`、`my_shm_maps_test`（与上述测试名一致）。`runtime/unit-test/common_def.hpp:6-19` 的同名结构是正确的。
2. **关闭一个 link 会删掉它的 perf event 以及挂在同一 perf event 上的所有其他 link**（确认，已实跑）。`handler_manager.cpp:139-146` + :105-121。
   与内核语义不同：内核关 link 不会让用户手里的 perf fd 失效，也不会影响别的 link。用户之后对该 perf fd 的 ioctl 会因 `is_perf_fd` 失败而退回原始 ioctl。
   另外非 perf 型 link（如 XDP 的 `target_ifindex`）也会 `clear_id_at(target)`，可能误删同号的其他 handler（推断，现有指南已提）。
3. **`bpf_prog_handler::type` 从不初始化**（确认，已实跑）。`prog_handler.cpp:9-19` 不使用 `prog_type` 参数；JSON 导出写出垃圾值（`bpftime_shm_json.cpp:302`），
   实跑导出 `"type": -1388388152`。
4. **JSON 导出/导入不对称**（确认，已实跑）：
   - 导出 `epoll_handler`、`memfd_handler`（`bpftime_shm_json.cpp:336-357`），导入不认（:216-219），整份导入返回 -1 并**中途停止**（已导入的保留）。
   - 软件 perf 导出时没有 `pid`（:87-95），导入无条件读 `value["attr"]["pid"]`（:148）→ nlohmann 抛 `type_error.302`，C 接口 `bpftime_import_*` 不捕获，异常穿过 `extern "C"`。
   - kprobe/custom 型 perf 导出无数据字段；导入的 switch 不支持 KPROBE/KRETPROBE/UREPLACE(1009)/custom（:154-195），只支持 UPROBE_OVERRIDE。
   - link 只导出 prog/target（:342-347），导入时 `attach_type=0`（:208-211）→ cookie 丢失（实跑：原 cookie 0x1234 不在 JSON 中），且绕过 perf target 校验。
   - prog 导入读 `attach_fds`，导出从不写（:130 vs :302-311），该循环永远空转。
   - map 只导出结构不导出内容；`gpu_thread_count` 不导出。`offset`/`ref_ctr_off` 以 `int` 读回（:156-157），>2^31 截断。
   - 导入按 nlohmann 对象 key 的**字典序**进行（"10" 在 "3" 前），只有"link 最后"这一层依赖排序（:248-258）。
5. **`map_init` 失败被吞**（确认，已实跑）：见 §6。
6. **假 fd 有两套分配器**（确认，已实跑）：syscall-server 用调用者进程的内核 fd 号（`open_fake_fd`），daemon 用表内最小空格（`find_minimal_unused_idx`）。
   两套号码空间混用同一张表时可能撞号：`bpftimetool import`/daemon/第二个 syscall-server 写入的 fd 号与本进程 `open_fake_fd` 的结果冲突时，
   `set_handler` 返回 `-EEXIST`；`add_bpf_map` 更糟——目标格非空时**直接清掉原 handler**（`bpftime_shm_internal.cpp:1070-1074`，可能是别人的 prog/map）（推断）。
7. **第二个 syscall-server 会清空第一个的会话**（推断）。每次 `start_up` 都 `begin_new_session`（`syscall_server_utils.cpp:172`），包括走 `SHM_CREATE_OR_OPEN` 复用段时；
   两个 loader 同时用同一个 shm 名，后启动者会清掉先启动者的所有 handler，并用自己的环境变量覆盖 `runtime_config`（:216）。头文件注释（`bpftime_shm_internal.hpp:162-169`）表明设计目标就是"一次只有一个 trace 会话"。
8. **epoch seqlock 不保护会话内写入**（确认，读代码）：`set_handler`/`clear_id_at` 不碰 `epoch_seq`。agent auto-refresh 扫表时 server 正在加 handler，读者看到的是半写的 variant 也不会重试（推断）。
9. **`read_stable_epoch_seq` 注释与实现不符**（确认）：`bpftime_shm_internal.hpp:171` 说缺失时返回 0，实现返回 `BPFTIME_EPOCH_SEQ_MISSING`（`.cpp:999-1000`）。另 `.cpp:891` 注释 "Allocate 20M" 已过时（默认 50MB）。
10. **MPK 模式大概率不可用**（推断，pkey 语义已实跑；bpftime MPK 构建未验证）。实跑 `pkey.c`：`pkey_alloc(0,PKEY_DISABLE_WRITE)` 后调用线程**只读**；
    该线程之后新建的线程与之前已有的线程对该 pkey **读写都 SEGV**（本机内核 6.12，CPU 有 pku）。对照代码：
    (a) 构造 shm 的线程随后就写 shm（syscall-server `add_pid_into_alive_syscall_server_set`、`begin_new_session`；agent `add_pid_into_alive_agent_set`，`agent.cpp:1138`）→ 应 SEGV；
    (b) 其他线程（loader 的 epoll 线程、业务线程在 `bpftime_prog_exec` 之外读 shm）不可读；
    (c) `bpftime_prog_exec` 结束总是 `enable`，嵌套执行（tail call 外的嵌套触发）会提前恢复保护。
    另：trap 后端的信号执行路径不切 MPK（`bpftime_prog.cpp:241-247`，且 `prepare_for_signal_execution` 拒绝 MPK，见 04 章）。
11. **`injected_pids` 缺少 `= nullptr`**（确认，读代码；影响推断）。`bpftime_shm_internal.hpp:95`。`SHM_NO_CREATE` 路径不赋值，placement new 不清零，
    之后若对 NO_CREATE 实例调 `add/remove_pid_from_alive_agent_set` 会用到不确定值（单测 cleanup 里常 destroy→`initialize(SHM_NO_CREATE)`）。
12. **`SHM_NO_CREATE`/未初始化时 `get_runtime_config` 解引用空 optional**（推断）。`bpftime_shm_internal.cpp:1165-1172`：`runtime_config==nullptr` 时返回 `*local_runtime_config`，从未 set 过则 UB。
    `test_bpftime_shm_json.cpp:129` 在没初始化全局 shm 的情况下调用 `bpftime_get_runtime_config()`，依赖了这种 UB（或依赖同一二进制里先跑的测试）。
13. **`strcpy` 进定长数组**（确认，读代码）：`bpftime_config.hpp:106-113`；`BPFTIME_LOG_OUTPUT` >1023 字节或 `BPFTIME_VM_NAME` >127 字节即栈/shm 溢出（`construct_runtime_config_from_env` 标了 `noexcept`）。
14. **`open_fake_fd` 的 dup 循环泄漏低号 fd**（确认，读代码）：`bpftime_shm_internal.cpp:676-680`，dup 后不 close 原 fd，0/1/2 若被关过会被 `/dev/null` 永久占住（这对宿主可能反而是好事，但不是"借一个号"）。
15. **`bpftime_prog` 带 config 的构造函数会写全局 runtime_config**（确认，跨模块）：`runtime/src/bpftime_prog.cpp:140` `bpftime_set_runtime_config(std::move(config))`。
    若进程已打开全局 shm，这会**覆盖 shm 里所有进程共享的配置**（`bpftime_object_open(path, cfg)` 走这条，`bpf_object.cpp:109`）。需 prog/VM 读者确认影响面。
16. **`PERF_EVENT_IOC_SET_BPF` 路径**（跨模块，推断）：`bpftime_attach_perf_to_bpf` 会新开一个假 fd 作 link 并返回其号码（`bpftime_shm_internal.cpp:444-451`），
    syscall-server 直接把这个正数当 ioctl 返回值（`syscall_context.cpp:1059-1061`），内核语义是返回 0；且该 link 的 `/dev/null` fd 没人关。需 syscall-server 读者确认。
17. **`enabled` 标志无消费者**（推断，grep）：见 §2.4。
18. **`bpftime_map_lookup_and_delete_elem` 只等于 pop**（确认）：`bpftime_shm.cpp:155-166` 调 `bpf_map_pop_elem`，hash map 的 `BPF_MAP_LOOKUP_AND_DELETE_ELEM` 会得到 `-ENOTSUP`（`map_handler.cpp:1439-1443`）。
19. **外部 map 的 `map_free` 永远不会被调用**（确认，读代码）：外部类型 `map_init` 把 `map_impl_ptr` 置空（`map_handler.cpp:1130`），`map_free` 开头见空即 return（:1148-1150），走不到 default 分支（:1258-1265）。
20. **`BUILD_WITH_LIBBPF=OFF` 时 STACK_TRACE 不一致**（推断）：lookup/update/delete 分支无条件（`map_handler.cpp:159-163` 等），init/free 分支在 `#ifdef BPFTIME_BUILD_WITH_LIBBPF` 里（:926-932, 1195-1197）→ init 失败被吞后访问空指针。PROG_ARRAY 同理但 lookup 也在 ifdef 内，退化为"不支持"。
21. **`bpftime_epoll_wait` 的信号处理（已实跑，非 bug）**：怀疑 `SIG_IGN` 会被当函数指针调用（`bpftime_shm.cpp:516-524`）。实跑 exp2：SIG_IGN 下 tgkill SIGINT 后正常超时返回 0，SIG_DFL 下进程被 SIGINT 杀死。
    原因：`sigtimedwait` 期间内核会临时放开等待集合，`real_blocked` 不含 SIGINT，忽略的信号在生成时就被丢弃，致命默认信号直接杀进程；只有安装了真实 handler 的信号会被 sigtimedwait 取到并手动调用。
22. **死声明/死代码**（确认）：`global_shm_open_type`（`bpftime_shm.hpp:181`）无定义；`bpftime_object_load_relocate_btf`（`bpftime_object.hpp:14`）无定义；
    `libbpf_print_fn`（`bpf_object.cpp:198-205`）未使用；`runtime_config::debug` 无人读写；`bpf_btf_handler` 不在 variant；`handler_manager.hpp:25-27` 的锁头文件未用；
    `~bpftime_shm` 在非 CUDA 构建里只有一个未用局部变量（`bpftime_shm_internal.cpp:1263`）。
23. **`get_software_perf_event_raw_buffer` 失败返回"有值的 nullptr"**（确认）：`bpftime_shm_internal.cpp:1190` `return nullptr;` 构造出 engaged optional；调用方 `.value_or(nullptr)` 恰好无害。
24. **object/ 小问题**（确认）：`find_program_by_secname` 在 `progs` 为空时 `progs.front()` 是 UB（`bpf_object.cpp:119`）；`create_programs(cfg)` 循环里 `std::move(cfg)` 多次（:109-110），第二个 prog 起拿到的是被移动过的 config（runtime_config 是 POD，实际是拷贝，无害）；
    `name || "<NULL>"` 传给 `{}` 打印的是 bool（:84,106）；`bpftime_object_open` 失败也返回非空对象（:153-157, 176-180）。
25. **`spinlock.hpp` 只适合 ARM**（确认）：`__asm__("yield")`（:37）在 x86 不是合法指令；只在没有 POSIX spinlock 的平台（macOS）启用（`spinlock_wrapper.hpp:12`）。
26. **`__destruct_shm` 的 "65535" 不等于"最后"**（推断，基于 GCC/glibc 语义，未验证）：在带优先级的 destructor 中 65535 反而最先跑；它之所以晚于 C++ 静态对象析构，是因为 `.fini_array` 在 `__cxa_atexit` 回调之后执行。

## 8. 对照现有指南

对照文件：`/home/y/src/bpftime-wt-reading-guide/00-architecture.md`、`02-runtime-shm.md`。

### 8.1 错误
- **00:418 术语表"假 fd：应用拿到的'fd'其实是 handler 数组下标，不是内核 fd"** → 在 syscall-server 路径上它**同时是**一个真实内核 fd（`/dev/null`），
  `open_fake_fd`（`bpftime_shm_internal.cpp:674-688`）；02 章 §2.6 自己也说"真 fd、假语义"，00 与 02 互相矛盾。
- **00:372-373 "set_handler_at_empty_slot：找最小空槽位放入，返回下标 = 假 fd"、00:641 "vector 槽位会被复用（find_minimal_unused_idx，总是取最小空槽）"**
  → 生产路径不用它们；syscall-server 用 `open_fake_fd`，`find_minimal_unused_idx` 只有 daemon（`daemon/user/bpftime_driver.cpp:26`）用，`set_handler_at_empty_slot` 只有单测用。实跑表空时两者返回 0 与 3。
- **00:671 "ufunc helper group 默认关闭（enable_ufunc_helper_group = false）"** → 实际进 shm 的配置来自 `construct_runtime_config_from_env()`，未设 `BPFTIME_HELPER_GROUPS` 时三组**全开**（`bpftime_config.cpp:100-104`）。结构体默认值只对手写 `runtime_config{}` 生效。
- **02 自测题 6 / 深入参考 "agent 是无锁读者，靠 epoch 前后比对发现不一致"、"定长数组……无锁读才安全"** → epoch 只在 `begin_new_session` 时变（`bpftime_shm_internal.cpp:1017-1031`），会话内的增删不会被 epoch 发现；定长只保证"地址不变"，不保证格子读写原子。属于过度承诺。
- **02 §2.6④ "删 link → 删它的目标 perf event。这是为了模拟内核行为：内核里关掉 link fd 后，perf event 的引用随之释放"** → 内核里用户的 perf fd 仍有效、同一 perf 上的其他 link 不受影响；bpftime 会删掉 perf 格子并连带删掉其他 link（已实跑）。说"模拟内核"不准确，应说"为防止 perf 格子泄漏而采取的近似"。
- **02 §2.4 "构造函数按一个打开方式参数分三条路"** → 实际是四个分支 + `SHM_NO_CREATE` 直接返回（`bpftime_shm_internal.cpp:797-936`），表里也漏了 `SHM_NO_CREATE`（只在深入参考提到）。
- **02 §2.4 "agent 全用 find：它永远不动 shm 的骨架"、小结"agent 只打开、只读结构"** → 大体对，但 agent 会在 shm 里分配：登记 alive pid（set 插入）、hash map 插入、为每个线程新建 perf shard（`perf_event_handler.cpp:534-538`），都会走 segment manager 的分配与锁。"只读"容易让人以为 agent 崩溃不会影响 shm。
- **02 深入参考 "`__destruct_shm` 标了 destructor(65535)，保证最后跑"** → 理由不对，见 §7-26（推断）。
- **02 §2.1 / 00:204 "不做任何 RPC / 从不直接通信"** → 控制面另有 agent IPC（abstract unix socket，`status`/`detach`/refresh，`agent.cpp:213,333-413`），只是热路径不用。属表述过绝对（这部分归 agent 读者确认）。
- 00:415/269 "loader 退出且无存活进程时删除" → 只有**拥有者**（`SHM_CREATE_ONLY` 建成段的那个 pid）会删（`syscall_server_utils.cpp:61-78`）。02 章写对了，00 章省略了条件。

### 8.2 遗漏（学习者应该知道）
- `bpftime::shm_remove` 析构删错段（§7-1），以及单测会删掉默认全局段——做实验时必须设 `BPFTIME_GLOBAL_SHM_NAME`。
- `bpf_prog_handler::type` 未初始化（§7-3）。
- JSON 导出/导入的不对称与不能 round-trip（§7-4），`bpftimetool export` 的 JSON 不能当备份用。
- lock 文件 `/tmp/bpftime-shm-*.lock` 永不删除；打开失败时静默不加锁。
- 两套假 fd 分配器与撞号风险（§7-6）；第二个 loader 会清空第一个 loader 的会话（§7-7）。
- `map_init` 失败被吞导致"有 fd 无容器"的 map（§7-5，实跑）。
- `PERF_EVENT_IOC_ENABLE/DISABLE` 只改一个无人读的标志（§7-17）。
- 软件 perf 缓冲满/未 mmap 时静默丢弃且返回 0（`perf_event_handler.cpp:385-387`，与内核 `-ENOSPC`+lost 记录不同，推断）。
- `BPFTIME_MAX_FD_COUNT` 与 `BPFTIME_SHM_MEMORY_MB` 一样只在建段时生效；`sizeof(handler_variant)=168`，可据此估算表占用。
- MPK 的真实语义（保护的是"宿主在 eBPF 执行外误写 shm"）以及每线程 PKRU 带来的问题（§7-10）。02 章完全没提 MPK。
- `bpftime_prog(…, config)` 构造会覆盖全局/shm 配置（§7-15）。
- 环境变量表漏了 `BPFTIME_GPU_ARRAY_GDRCOPY`、`BPFTIME_GPU_ARRAY_GDRCOPY_MAX_PER_KEY_BYTES`、`BPFTIME_MAP_GPU_THREAD_COUNT`、`SPDLOG_LEVEL`。
- 编译开关 `BPFTIME_ENABLE_FS_HELPER`、`BPFTIME_USE_VAR_HASH_MAP` 没有 CMake 入口；HASH map 实际是 `fix_size_hash_map_impl`。
- 外部 map ops 表是进程本地的，`map_free` 回调永远不会被调用（§7-19）。
- 没有任何单测覆盖 epoch/session。

### 8.3 讲得好、可以保留
- 02 §2.1 储物柜比喻、§2.2 命名/匿名对象与 `offset_ptr` 指针纪律、"为什么容器匿名"（与 `test_shm_hash_maps.cpp:151-231` 一致）。
- 02 §2.5 seqlock 的奇偶语义与读侧三轮重试描述准确（行号核对一致）；`read_stable_epoch_seq` 注释错误这一条是对的。
- 02 §2.6 `open_fake_fd` 的动机（号码被内核真实占住）与 `close_fd` 由调用方关真 fd 的约定正确；级联删除的推演（含"非 perf link 也会清 target"）正确。
- 02 §2.7 union 手动生命周期 + 三种"销毁"的区分表很清楚。
- 02 深入参考里的大部分行号（`handler_manager.cpp:35-47/63-88/98-148/160-167`、`link_handler.hpp:16-43`、`prog_handler.hpp:36,59-62`、`epoll_handler.hpp:20-35`、`memfd_handler.hpp:16-21`、`bpftime_shm_internal.cpp` 各段）抽查均正确；
  `posix_fallocate` 一条已核对（Boost 1.83，`shared_memory_object.hpp:462`）；`map_lock` pshared=0、`set_handler` 不看 `map_init` 返回值都是对的。
- 00 章"shm 懒创建、无论新旧都开新会话"（00:229-230）正确。

## 9. 学习建议

### 9.1 推荐阅读顺序
1. `runtime/include/bpftime_shm.hpp:27-143`（attr/枚举/打开方式）→ `:221-436`（C API 全貌）。
2. `runtime/src/handler/handler_manager.hpp:51-132` → `handler_manager.cpp` 全文（169 行，表与级联）。
3. 五种 handler：`link_handler.hpp` → `prog_handler.hpp` → `map_handler.hpp:67-300` → `perf_event_handler.hpp:161-240` → `epoll_handler.hpp`。
4. `bpftime_shm_internal.hpp:34-339` → `bpftime_shm_internal.cpp:790-995`（构造）→ `:674-757`（假 fd、add_*、close）→ `:997-1031`、`:1381-1397`（epoch）→ `:49-229`（全局 init/destroy、两种锁）。
5. `runtime/src/attach/bpf_attach_ctx.cpp:73-162`（epoch 读侧）与 `runtime/syscall-server/syscall_server_utils.cpp:56-172`（所有权/删段）。
6. `bpftime_shm.cpp:418-539`（epoll_wait）、`:635-672`（map_ptr_by_fd/map_val）。
7. 选读：`map_handler.cpp:762-1144`（map_init）、`perf_event_handler.cpp:312-624`（perf 环与 shard）、`bpftime_shm_json.cpp`、`bpftime_config.cpp`。

### 9.2 动手实验
所有实验都先 `export BPFTIME_GLOBAL_SHM_NAME=notes_$RANDOM`，结束后 `rm /dev/shm/$BPFTIME_GLOBAL_SHM_NAME`。

1. **跑本块单测**（已实跑，全部通过）：
   ```bash
   cd build-jit/runtime/unit-test
   ./bpftime_runtime_tests "[epoll]"                 # All tests passed (6558 assertions in 1 test case)
   ./bpftime_runtime_tests "[software_perf_event]"   # 8307 assertions in 3 test cases
   ./bpftime_runtime_tests "Test bpftime runtime_config"
   ./bpftime_runtime_tests "[allocation]"            # 87 assertions in 13 test cases
   ```
   注意不要随手跑 `"Test shm hash maps with sub process"` / external map ops / json 测试——它们用固定 shm 名并会留下残留（§7-1）。
2. **观察 epoch 与假 fd、级联删除、JSON 不对称**（已实跑，`scratchpad/R1/exp.cpp`，链接 `build-jit/runtime/libruntime.a`，链接参数取自 `build.ninja` 中 `bpftime_runtime_tests` 的 `LINK_LIBRARIES`）。关键输出：
   ```
   sizeof(handler_variant)=168 map=160 prog=104 perf=72 link=72 epoll=32 memfd=40 runtime_config=1200
   [epoch] initial=0 / begin_new_session -> 2 / -> 4
   [fd] find_minimal_unused_fd=0 ; prog fd=3 (real fd -> /dev/null)
   [prog] stored type field=-1388388152 (passed 2)
   [cascade] after close_fd(link1): is_perf(perf)=0 exist(link2)=0 is_prog(prog)=1
   [map_init] bad ringbuf add_bpf_map ret=7 has_map_impl=0
   [json] import full ret=-1        (日志: Unsupported handler type epoll_handler)
   [json] import swpe threw: [json.exception.type_error.302] type must be number, but is null
   [shm_remove] after dtor exists(n3)=1
   ```
3. **看 lock 文件残留**：`ls /tmp/bpftime-shm-*.lock`（本机已有 30 个，均为 0 字节，从未被删除）。
4. **pkey 语义**（已实跑，`scratchpad/R1/pkey.c`）：预期 `alloc thread ... read=ok write=SEGV`，新线程/旧线程 `read=SEGV write=SEGV`。用来理解为什么 MPK 版 bpftime 很难工作。
5. 现有 02 章"动手验证"的 1–3 条仍可用（`ls -l /proc/<loader>/fd` 看 `/dev/null` 假 fd 很直观），但 export 出的 JSON 不要拿去 import。

### 9.3 学习者最容易误解的点
1. "假 fd 就是数组下标、不是真 fd"——在 syscall-server 里它是真 `/dev/null` fd 的号码；在 daemon 里才是"表内最小空格"。
2. "epoch 让 agent 无锁读表是安全的"——epoch 只标记**换会话**；会话内的写入与读取之间没有任何同步。
3. "关 link ≈ 内核行为"——bpftime 会把 perf event 和同一 perf 上的所有 link 一起删掉。
4. "`ufunc` helper 默认关"——从环境变量构造的配置里默认是开的。
5. "`bpftimetool export` 可以备份/迁移 shm"——导出既不含 map 内容，也无法被 import 完整还原（epoll/memfd/软件 perf/kprobe/cookie/prog type）。

## 10. 未读完/未确定的地方
- `runtime/include/bpftime_epoll.h`（1299 行）只做结构性浏览，未逐个核对 uapi 结构与 Linux 头一致性（macOS 专用）。
- `map_handler.cpp` 的 CUDA/GPU 分支只读未验证（本机构建无 CUDA）；`bpftime_shm_internal.cpp` CUDA 段同样。
- MPK 结论：pkey 语义已实跑，但没有（也不允许）重编 MPK 版 bpftime 去验证 §7-10 的崩溃。
- §6 "segment manager 锁非 robust、进程被杀后其他进程会永久阻塞"未验证（取决于 Boost 配置）。
- §7-7（多 loader 互相清会话）、§7-6（撞号误删）、§7-16（SET_BPF 返回值）是读代码推断，未构造多进程场景实跑。
- 跨模块需要其他读者确认的点：
  - syscall-server 读者：`PERF_EVENT_IOC_SET_BPF` 返回正数 link 号且 link fd 不关（§7-16）；`bpftime_maps_create` 等返回的负 errno 是否被正确转换为 `-1+errno`；`handle_exceptions` 是否捕获 json 导入异常（`bpftimetool` 侧）。
  - agent / attach 读者：perf handler 的 `enabled` 是否真的无人消费；`init_attach_ctx_from_handlers` 在会话内并发写入时的行为。
  - prog/VM 读者：`bpftime_prog(…, config)` 覆盖全局 `runtime_config`（`bpftime_prog.cpp:140`）的调用面；MPK 与嵌套执行。
  - maps 读者：各 impl 的 `should_lock` 取值，`map_init` 失败后各类型访问空 impl 的具体后果。
  - 链接实验时出现 `struct ebpf_inst` 的 ODR 警告（两个不同定义，`opcode` vs `code` 字段），归 VM 读者看。
- 02 章"上游变更"一节列出的 PR 编号未逐个核对（未验证）。
