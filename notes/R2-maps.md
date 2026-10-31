# R2 map 实现（bpf_map 根目录 / userspace / shared + map_handler 分发）源码笔记

基线：官方 `177a1dc465ff`，仓库根 `/home/y/src/bpftime-wt-official-177a1dc`。下文所有 `路径:行号` 都相对仓库根。
「推断」= 读代码得出、未运行验证；「未验证」= 行为没跑过；「已实跑」= 在 `build-jit/` 或 scratchpad 小程序上实际跑过。

## 0. 一句话职责 + 范围

本块是 bpftime 所有 **CPU 侧 BPF map 的数据结构实现**，以及把"按 fd 找到的 map handler"上的操作**按类型分发**到具体实现的那一层（`bpf_map_handler`）。所有实现对象都放在 boost::interprocess 共享内存里，供 syscall-server（loader 侧）与 agent（目标进程侧）共同访问；另有一组 `shared/` 实现把操作转发给**真实内核 map**。

读过的目录/文件：
- `runtime/src/bpf_map/` 根目录 3 个文件（`README.md`、`map_common_def.hpp`、`bpftime_hash_map.hpp`）；
- `runtime/src/bpf_map/userspace/` 全部 29 个文件；
- `runtime/src/bpf_map/shared/` 全部 8 个文件；
- `runtime/src/handler/map_handler.hpp`、`map_handler.cpp`（全文读完，分发部分重点；handler 生命周期归 R1）；
- 不读：`runtime/src/bpf_map/gpu*`（R6）。map_handler.cpp 里 `#if defined(BPFTIME_ENABLE_CUDA_ATTACH)` 的 GPU 分支只扫过，不展开。

范围内源文件总行数：**7741 行**（bpf_map 根 351 + userspace 4471 + shared 1138 + map_handler 1781；`wc -l` 实测）。
相关单测：`runtime/unit-test/maps/` 15 个文件共 5349 行（全部读完），外加 6 个 maps 相关的非 maps 目录测试（见 §1 末表）。

## 1. 文件清单

### 1.1 源码

| 文件 | 行数 | 做什么（一句话） | 必读/选读/跳过 |
|---|---:|---|---|
| `runtime/src/bpf_map/README.md` | 4 | 说明 `shared/` 是内核-用户共享 map、`userspace/` 是纯用户态 map | 选读 |
| `runtime/src/bpf_map/map_common_def.hpp` | 103 | shm 里的字节向量类型 `bytes_vec`/`uint64_vec`、哈希器、`ensure_on_current_cpu`、`check_update_flags` | 必读 |
| `runtime/src/bpf_map/bpftime_hash_map.hpp` | 244 | 默认 HASH 的底层：shm 里一块连续 `bytes_vec` 上的开放寻址+线性探测定长哈希表 | 必读 |
| `userspace/array_map.hpp/.cpp` | 37+82 | ARRAY：一块 `value_size*max_entries` 的连续 `bytes_vec`，无锁 | 必读 |
| `userspace/fix_hash_map.hpp/.cpp` | 44+86 | 默认 HASH 实现 `fix_size_hash_map_impl`，包一层 `bpftime_hash_map`（忽略 flags） | 必读 |
| `userspace/var_hash_map.hpp/.cpp` | 62+150 | 备选 HASH `var_size_hash_map_impl`：shm 里的 `boost::unordered_map<bytes_vec,bytes_vec>`；默认编译**不使用** | 选读 |
| `userspace/map_in_maps.hpp` | 36 | ARRAY_OF_MAPS：继承 array，value 存 int（内层 map 的 fd），lookup 把 fd 当指针返回 | 选读 |
| `userspace/per_cpu_array_map.hpp/.cpp` | 57+147 | PERCPU_ARRAY：`[entry][cpu]` 连续布局，程序/loader 两视角 | 必读 |
| `userspace/per_cpu_hash_map.hpp/.cpp` | 86+216 | PERCPU_HASH：一个 key 对应 `ncpu*value_size` 拼接的 value；无锁 | 必读 |
| `userspace/lru_var_hash_map.hpp/.cpp` | 91+221 | LRU_HASH：unordered_map + shm 里 `managed_shared_ptr` 双向链表 | 选读 |
| `userspace/lpm_trie_map.hpp/.cpp` | 154+633 | LPM_TRIE：shm 里的二叉前缀树，`unique_ptr` 子节点，内部再加一把进程共享自旋锁 | 选读 |
| `userspace/bloom_filter.hpp/.cpp` | 162+409 | BLOOM_FILTER：shm 位数组，jhash/djb2 两种哈希，随机种子 | 选读 |
| `userspace/queue.hpp/.cpp` | 168+226 | QUEUE：定长环形数组 head/tail/count | 选读 |
| `userspace/stack.hpp/.cpp` | 170+215 | STACK：shm vector 尾部 push/pop | 选读 |
| `userspace/ringbuf_map.hpp/.cpp` | 99+347 | RINGBUF：仿内核/libbpf 内存布局的 MPSC 环；reserve/submit/fetch | 必读 |
| `userspace/perf_event_array_map.hpp/.cpp` | 37+83 | PERF_EVENT_ARRAY：`int32` 数组（CPU→perf 对象 fd），默认 -1 | 必读 |
| `userspace/prog_array.hpp/.cpp` | 39+211 | PROG_ARRAY：存 bpftime prog fd（编码成负数）或内核 prog id | 选读 |
| `userspace/stack_trace_map.hpp/.cpp` | 66+137 | STACK_TRACE：`unordered_map<uint32 id, uint64_vec>`，由 `bpf_get_stackid` 填 | 选读 |
| `shared/array_map_kernel_user.hpp/.cpp` | 45+236 | KERNEL_USER_ARRAY：mmap 内核 array map（失败则走 bpf syscall） | 选读 |
| `shared/hash_map_kernel_user.hpp/.cpp` | 49+103 | KERNEL_USER_HASH：每次操作直接 libbpf `bpf_map_*_elem` 到内核 map | 选读 |
| `shared/percpu_array_map_kernel_user.hpp/.cpp` | 40+74 | KERNEL_USER_PERCPU_ARRAY：同上思路，但 `init_map_fd` 从未被调用（坏的） | 跳过（只需知道坏了） |
| `shared/perf_event_array_kernel_user.hpp/.cpp` | 66+525 | KERNEL_USER_PERF_EVENT_ARRAY：用户态写内核 USER_RINGBUF，内核里手写 BPF 程序定时搬运到真 perf array | 选读 |
| `runtime/src/handler/map_handler.hpp` | 303 | `bpf_map_handler` 定义：type/attr/name/锁/`offset_ptr<void>` 实现指针/引用计数 | 必读 |
| `runtime/src/handler/map_handler.cpp` | 1478 | 分发层：lookup/update/delete/get_next_key/push/pop/peek 的 `switch(type)`；`map_init`/`map_free` | 必读（GPU 分支跳过） |

### 1.2 单测（全部读完）

| 测试文件 | 行数 | 覆盖对象 | 备注 |
|---|---:|---|---|
| `runtime/unit-test/maps/test_bpftime_hash_map.cpp` | 188 | `bpftime_hash_map` 原语 | 含冲突链删除后再插入的回归 |
| `maps/test_fix_hash_map.cpp` | 53 | `fix_size_hash_map_impl::map_get_next_key` 扫描质数桶 | 只 1 个用例 |
| `maps/test_shm_hash_maps.cpp` | 231 | handler 层 HASH/ARRAY，fork 子进程读，同名 map 独立、dup 共享 impl+refcount | 唯一测 handler 分发+跨进程的 |
| `maps/kernel_unit_tests.cpp` | 805 | 移植内核 `test_maps.c`：**var_size_hash**、array、per_cpu_array、per_cpu_hash（userspace 视角）、并行、RDONLY/WRONLY | 测的是 var_hash 而非默认 HASH |
| `maps/test_per_cpu_array.cpp` | 80 | 绑核写、loader 读，和反向 | 用 `ensure_on_certain_cpu` |
| `maps/test_per_cpu_hash.cpp` | 99 | 同上 for per-CPU hash | 不测 delete |
| `maps/test_external_map_ops.cpp` | 94 | `bpftime_register_map_ops` 自定义类型 2000 | 需 `allow_non_buildin_map_types` |
| `maps/test_map_in_map_bounds.cpp` | 38 | ARRAY_OF_MAPS 越界 lookup → ENOENT | |
| `maps/test_stack_trace_map.cpp` | 101 | `fill_stack_trace` 插入/哈希碰撞/重复 | 没覆盖"同长不同内容"碰撞 |
| `maps/test_queue_map.cpp` | 539 | queue push/pop/peek/BPF_EXIST 覆盖/标准接口 | 并发测试自己外加锁 |
| `maps/test_stack_map.cpp` | 567 | stack 同上，LIFO | 同上 |
| `maps/test_bloom_filter_map.cpp` | 758 | 构造校验、push/peek、假阳性率、两种哈希 | |
| `maps/test_lpm_trie_map.cpp` | 1355 | 与 TLPM 朴素实现对拍、IP、删除、flags、满、多线程、字符串 | 不测 `get_next_key`；对若干内核差异"两种结果都接受" |
| `maps/test_lru_hash_map.cpp` | 341 | LRU 插入/淘汰/flags/容量/迭代 | |
| `maps/test_lru_var_hash_map.cpp` | 100 | LRU 基本与淘汰 | |
| `runtime/unit-test/test_ringbuf_tail_wrap.cpp` | 92 | ringbuf 记录在环尾保持连续（#706 回归） | 走 `bpftime_ringbuf_reserve/submit/poll` |
| `runtime/unit-test/test_epoll.cpp` | 161 | ringbuf/软件 perf fd 加入 bpftime epoll | 只读了 ringbuf 相关部分 |
| `runtime/unit-test/test_software_perf_event.cpp` | 344 | `bpf_perf_event_output` + PERF_EVENT_ARRAY（23-127 行） | 其余部分属 perf handler（不在本范围） |
| `runtime/unit-test/tailcall/test_user_to_user_tailcall.cpp` | 79 | PROG_ARRAY 存 bpftime prog fd，关闭后 lookup 为空 | |
| `runtime/unit-test/tailcall/test_user_to_kernel_tailcall.cpp` | 126 | PROG_ARRAY 存内核 prog（需 root） | 未实跑 |
| `runtime/unit-test/test_bpftime_shm_json.cpp` | 107 | 仅 `add_bpf_map` ARRAY/PERCPU_ARRAY 后导出导入 | 只扫了 map 相关部分 |

所有 maps 测试都编进同一个 `bpftime_runtime_tests`（`runtime/unit-test/CMakeLists.txt:16-51`）。

## 2. 核心数据结构

### 2.1 `bpf_map_handler`（分发层，`runtime/src/handler/map_handler.hpp:67-300`）
- 生命周期：由 `bpftime_shm::add_bpf_map` 构造临时对象（`runtime/src/bpftime_shm_internal.cpp:1075-1076`），`std::move` 进 shm 里的 `handler_manager::handlers[fd]`（`runtime/src/handler/handler_manager.cpp:80`），**handler 本体住在 shm**（handler_variant 向量里）。
- 字段：
  - `attr`（`bpf_map_attr`，`map_handler.hpp:69`，定义见 `runtime/include/bpftime_shm.hpp:27-46`）：原始创建参数 + bpftime 扩展字段 `kernel_bpf_map_id`、`gpu_thread_count`(默认 1024)。
  - `type`（`bpf_map_type`，`:73`）：分发标签。内核类型原值 + `KERNEL_USER_MAP_OFFSET=1000`（`bpftime_shm.hpp:66,113-120`）+ `GPU_MAP_OFFSET=1500`（`:68,122-132`），`BPF_MAP_TYPE_MAX=2048`（`:134`）。
  - `name`（shm 里的 `boost_shm_string`，`:74`）。
  - `map_lock`（`mutable pthread_spinlock_t`，`:278`）：每个 handler 一把，`pthread_spin_init(&map_lock, 0)`（`:95,108,121`）——pshared 参数为 0（PROCESS_PRIVATE），见 §7。
  - `map_impl_ptr`（`offset_ptr<void>`，`:70,280`）：指向 shm 中的具体实现对象。
  - `map_refcnt_ptr`（`offset_ptr<std::atomic_uint32_t>`，`:71-72,281`）：dup 共享 impl 时的引用计数，shm 中 `anonymous_instance` 构造（`map_handler.cpp:773-782`）。
  - `max_entries/flags/key_size/value_size`（`:282-285`）。
- 拷贝禁用；移动构造/赋值把对端 impl/refcnt 置空并重新 `pthread_spin_init`（`:100-131`）。析构只在 impl 未释放时打 CRITICAL（`:132-142`），不释放任何东西。
- 释放：`handler_manager::clear_id_at` → `map_free(memory)`（`handler_manager.cpp:103-104`，`map_handler.cpp:1146-1270`）：先 `fetch_sub` 引用计数，>1 只清本 handler 的指针；最后一个才 `memory.destroy_ptr(static_cast<具体类型*>(impl))`，**会跑具体 impl 的析构函数**。

### 2.2 公共类型（`runtime/src/bpf_map/map_common_def.hpp`）
- `bytes_vec` = `boost::interprocess::vector<uint8_t, shm allocator>`（`:20-22`）；`uint64_vec`（`:23-25`）。所有 impl 的变长数据都是它，元素分配在 shm 段。
- `bytes_vec_hasher`（boost::hash_combine，`:64-74`）、`uint32_hasher`（恒等，`:76-81`）、`int_hasher`（`:95-100`）。
- `check_update_flags`：只校验低 32 位 ∈ {0,1,2}，高 32 位"留给 bpftime 自定义操作"（`:83-94`）。被 array、per-CPU array/hash 用；var_hash/lru/lpm/queue 各自有更严格的检查。
- `ensure_on_current_cpu`：就是 `func(my_sched_getcpu())`（`:27-31`），不绑核。`ensure_on_certain_cpu` 真正 `sched_setaffinity`（`:33-62`），只有单测用（`runtime/unit-test/maps/test_per_cpu_array.cpp:28`、`test_per_cpu_hash.cpp:43`）。`my_sched_getcpu` 在 Linux 上即 `sched_getcpu()`（`runtime/src/platform_utils.cpp:7-9`）。

### 2.3 各 map 实现（全部经 `memory.construct<T>(anonymous_instance)` 在 shm 中构造，`map_handler.cpp:762-1144`）

| 实现类 | 关键字段与内存布局 | 锁 |
|---|---|---|
| `bpftime_hash_map`（`bpftime_hash_map.hpp:52-240`） | `data_buffer`：`bytes_vec`，长度 `next_prime(max_entries)*(4+key+value)`（`:79-91`）；每槽 `[u32 state][key][value]`，state=EMPTY0/FILLED1/DELETED2（`:54-56,70-74`）；`_count`/`_max_element_count=max_entries`（原值，非质数，`:84-85`）。哈希 `h=h*31+byte`（`:40-47`），线性探测（`:142-158`），删除留墓碑（`:215-234`），插入优先复用第一个墓碑（`:160-213`）。 | 无自带锁 |
| `fix_size_hash_map_impl`（`fix_hash_map.hpp:23-41`） | 内嵌一个 `bpftime_hash_map`；`_num_buckets` 重新算质数（`fix_hash_map.cpp:22`）用于遍历 | `should_lock=true`（`.hpp:30`） |
| `var_size_hash_map_impl`（`var_hash_map.hpp:22-58`） | `boost::unordered_map<bytes_vec,bytes_vec>` 放 shm；`key_vec/value_vec` 是**共享临时缓冲**（`:36-37`），靠外层锁保护；`flags` 支持 `BPF_F_RDONLY/WRONLY`（`var_hash_map.cpp:39,82,115`） | true（`:40`） |
| `array_map_impl`（`array_map.hpp:14-34`） | `data`：`value_size*max_entries` 连续字节（`array_map.cpp:22`），无对齐填充；`get_raw_data()` 供 mmap（`:15-18`） | false（`:20`） |
| `array_map_of_maps_impl`（`map_in_maps.hpp:16-32`） | 继承 array，value_size 固定 `sizeof(int)`（`:21`）；lookup 返回 `(void*)(uint64)map_id`（`:24-31`） | 继承 false |
| `per_cpu_array_map_impl`（`per_cpu_array_map.hpp:19-54`） | `data` 长度 `max_entries*ncpu*value_size`（`per_cpu_array_map.cpp:23`），`data_at(idx,cpu)=idx*value_size*ncpu+cpu*value_size`（`.hpp:25-28`）；`ncpu=sysconf(_SC_NPROCESSORS_ONLN)`（`.cpp:30`） | false（`:31`） |
| `per_cpu_hash_map_impl`（`per_cpu_hash_map.hpp:20-83`） | `impl`：`unordered_map<bytes_vec,bytes_vec>`，value 长 `ncpu*value_size`；`value_template`（全零 ncpu 份）、按 CPU 预分配的 `key_templates`/`single_value_templates`（`per_cpu_hash_map.cpp:31-46`） | false（`.hpp:52`） |
| `lru_var_hash_map_impl`（`lru_var_hash_map.hpp:46-88`） | `unordered_map<bytes_vec, hash_map_value{value, shared_ptr 链表节点}>`；`lru_link_list_head/tail`（`managed_shared_ptr`，前向 shared_ptr、后向 weak_ptr，`:25-39`）；节点用 `segment_manager->construct` 分配（`lru_var_hash_map.cpp:168-175`） | true（`:77`） |
| `lpm_trie_map_impl`（`lpm_trie_map.hpp:80-151`） | `root`：`boost::interprocess::unique_ptr<lpm_trie_node>`；节点 `child[2]`、`prefixlen`、`is_intermediate`、`data`=[前缀 data_size][value]（`:39-78`）；`n_entries`；**`managed_shared_memory &memory` 引用成员**（`:98`）；内部 `pthread_spinlock_t mapSpin`，以 `PTHREAD_PROCESS_SHARED` 初始化（`lpm_trie_map.cpp:71`） | true（`:118`）且内部再锁一次 |
| `bloom_filter_map_impl`（`bloom_filter.hpp:38-159`） | `bit_array`：`bytes`，位数 = `max(64, max_entries*nr_hashes*7/5)` 向上取 2 的幂（`bloom_filter.cpp:74-89,103-122`）；`_hash_seed` 构造时随机（`:92`）；`nr_hashes ∈[1,15]`（`:66-71`） | true（`:67`） |
| `queue_map_impl`（`queue.hpp:33-165`） | `data` 预先 `resize(max_entries*value_size)`（`queue.cpp:26`）；`head/tail/count` 环形下标（`.hpp:48-50`） | true（`:59`） |
| `stack_map_impl`（`stack.hpp:38-168`） | `data` 只 `reserve`（`stack.cpp:25`），push = `insert` 到尾，pop = `erase` 尾，BPF_EXIST 满时 `erase(begin, begin+value_size)`（`:81,141`），O(n) 搬移 | true（`.hpp:60`） |
| `ringbuf_map_impl` / `ringbuf`（`ringbuf_map.hpp:34-96`） | `ringbuf_map_impl` 只持一个 `managed_shared_ptr<ringbuf>`（`:66-68,75`），方便 epoll 持 weak_ptr；`ringbuf` 里 `raw_buffer` 大小 `2*page + 2*max_ent`（`ringbuf_map.cpp:178-183`）：第 0 页 consumer_pos、第 1 页 producer_pos、第 2 页起 data（`:191-194`）；`reserve_spin_lock` pshared=1（`:195`） | handler 层 false（`.hpp:78`）；reserve 内部自旋锁 |
| `perf_event_array_map_impl`（`perf_event_array_map.hpp:18-35`） | `i32_vec data(max_entries, -1)`（`perf_event_array_map.cpp:18`）；key/value 必须 4 字节否则抛异常（`:20-25`） | true（`.hpp:23`） |
| `prog_array_map_impl`（`prog_array.hpp:18-37`） | `i32_vec data(max_entries, -1)`（`prog_array.cpp:105`）；bpftime prog fd 编码为 `-fd-2`（`:89-97`），内核 prog 存 `prog id`（`:162-171`） | false（`.hpp:25`） |
| `stack_trace_map_impl`（`stack_trace_map.hpp:40-63`） | `unordered_map<uint32, uint64_vec>`（`:24-26,43`）；`key_buf` 共享临时缓冲（`:45`）；`max_stack_entries=value_size/8`（`stack_trace_map.cpp:33`） | true（`.hpp:48`） |
| `array_map_kernel_user_impl`（`shared/array_map_kernel_user.hpp:13-41`） | `kernel_map_id`、**`map_fd`、`mmap_ptr`（进程私有值）存在 shm 对象里**（`:17-21`）；`has_kernel_map=km_id>0`，否则退化为本地 `value_data`（`array_map_kernel_user.cpp:110-116`） | false（`:26`） |
| `hash_map_kernel_user_impl`（`shared/hash_map_kernel_user.hpp:21-46`） | `key_vec/value_vec`（shm 共享缓冲）、`kernel_map_id`、`map_fd`（进程私有值在 shm） | false（`:35`） |
| `percpu_array_map_kernel_user_impl`（`shared/percpu_array_map_kernel_user.hpp:16-38`） | `kernel_map_fd=-1`、`value_data(1)`、`ncpu`；`_value_size/_max_entries` 未初始化 | false（`:26`） |
| `perf_event_array_kernel_user_impl`（`shared/perf_event_array_kernel_user.hpp:29-55`） | `dummy=0xffffffff`、`max_ent`、`kernel_perf_id`、`user_rb_id`、`pfd`、`transporter_prog_fd`；有 **virtual 析构**（`:42`），但从不经基类指针多态使用（推断：只是 vptr 指向创建进程的 vtable，`destroy_ptr` 用静态类型调用，不经 vtable 分派） | false（`:37`） |

### 2.4 进程私有（不在 shm）的结构
- `global_map_ops_table[BPF_MAP_TYPE_MAX]`（`map_handler.cpp:60-62`）：外部 map 类型的函数表，**每进程一份**，注释明确说明（`runtime/include/bpftime_shm.hpp:191-195`）。
- `user_rb_map`：`std::map<kernel_perf_id, unique_ptr<user_ringbuffer_wrapper>>`，进程堆全局指针（`perf_event_array_kernel_user.cpp:39-47`）。
- `prog_array.cpp:100` 的 `thread_local current_thread_lookup_val`、`lpm_trie_map.cpp:255` 的 `thread_local std::vector tl_value`：lookup 返回值所在的 TLS 缓冲。
- `prog_array.cpp:31-33` 的 `libc_handle/libc_syscall`：绕过 syscall-server 钩子的 libc `syscall` 指针。

## 3. 主要流程与调用链

### 3.1 BPF 程序调用 `bpf_map_lookup_elem`（helper 视角，from_syscall=false）
`bpftime_map_lookup_elem_helper` (`runtime/src/bpf_helper.cpp:299-304`) → `bpftime_shm::bpf_map_lookup_elem(fd,key,false)` (`runtime/src/bpftime_shm_internal.cpp:281-286`) → `try_get_map_handler` (`:266-273`) → `bpf_map_handler::map_lookup_elem` (`runtime/src/handler/map_handler.cpp:109-268`) → `switch(type)` → `static_cast<具体impl*>(map_impl_ptr.get())` → `do_lookup` lambda（`:112-119`）按 `impl->should_lock` 决定是否 `bpftime_lock_guard guard(map_lock)` → `impl->elem_lookup(key)`。
- map 参数就是 fd：BPF 字节码里的 map 引用经 `map_ptr_by_fd` 直接返回 fd（`runtime/src/bpftime_shm.cpp:635-648`），所以 helper 把 `uint64 map` 强转 `int`。
- 未知类型走 `default`：查 `global_map_ops_table[type].elem_lookup`（`map_handler.cpp:257-265`）。
- 返回值是指向 shm 内部的指针（array/hash/percpu 等），程序写它就是改 map；例外见 §7（LPM、prog_array、kernel-user hash/percpu 返回的是缓冲区副本）。

### 3.2 loader 经 syscall-server 调 `bpf(BPF_MAP_LOOKUP_ELEM)`（from_syscall=true）
`syscall_context::handle_sysbpf` case `BPF_MAP_LOOKUP_ELEM` (`runtime/syscall-server/syscall_context.cpp:554-578`) → `bpftime_map_lookup_elem(fd,key)` (`runtime/src/bpftime_shm.cpp:115-126`，传 `true`，捕获 C++ 异常返回 nullptr) → 同 3.1 的 handler 分发 → 拿到指针后 `memcpy(attr->value, value_ptr, bpftime_map_value_size_from_syscall(fd))`（`syscall_context.cpp:575-576`）→ `bpftime_shm::bpf_map_value_size` (`bpftime_shm_internal.cpp:275-279`) → `get_userspace_value_size` (`map_handler.cpp:69-84`：PERCPU_ARRAY/PERCPU_HASH ×`sysconf(_SC_NPROCESSORS_ONLN)`，GPU per-thread ×`gpu_thread_count`)。
- update/delete/get_next_key/lookup_and_delete 同理（`syscall_context.cpp:579-627` → `bpftime_shm.cpp:128-153,155-166,207-…`）。`BPF_MAP_LOOKUP_AND_DELETE_ELEM` 只映射到 **pop**（`bpftime_shm.cpp:155-166`），所以 HASH/PERCPU_HASH 的 lookup_and_delete 经 syscall 会得到 `-ENOTSUP`（`map_handler.cpp:1439-1443`）。

### 3.3 map 创建：`map_init`
syscall-server `BPF_MAP_CREATE`（非 run_with_kernel）(`syscall_context.cpp:529-552`) → `bpftime_maps_create(-1,name,attr)` (`bpftime_shm.cpp:81-84`) → `bpftime_shm::add_bpf_map` (`bpftime_shm_internal.cpp:1048-1077`：分配假 fd、可选写 verifier map 描述、清旧 handler) → `handler_manager::set_handler` (`handler_manager.cpp:63-88`) → 若 `!has_map_impl()` 调 `map_init(memory)` (`:83-85`，**返回值被丢弃**) → `map_init` (`map_handler.cpp:762-1144`) 按 type `memory.construct<impl>(anonymous_instance)(...)` 并 `init_refcnt()`。
- 特殊校验：RINGBUF 要求 `max_entries` 是 2 的幂，否则 `return -1`（`:806-818`）；BLOOM 要求 `key_size==0`，`nr_hashes=map_extra&0xF`，0 → 5，固定 JHASH（`:860-884`）；KERNEL_USER_PERF_EVENT_ARRAY 固定 `key=4,value=4,max_ent=ncpu`（`:910-917`）。
- 构造函数可能抛异常（lpm/bloom/queue/stack/perf_event_array/prog_array/stack_trace 的参数校验；ringbuf 页大小检查；shm 不足的 `bad_alloc`），`map_init` 不捕获（推断：由调用方 `bpftime_maps_create` 链上的 try/catch 或进程崩溃兜底，归 R1 确认）。
- 未内建类型：`allow_non_buildin_map_types` 为真时调用 `global_map_ops_table[type].alloc_map`，否则 `-1`（`:1127-1141`；配置定义 `runtime/include/bpftime_config.hpp:70`）。

### 3.4 dup：两个 fd 共享一份 impl
`bpftime_maps_dup` (`bpftime_shm.cpp:92-95`) → `bpftime_shm::dup_bpf_map` (`bpftime_shm_internal.cpp:1079-1125`) → `can_share_map_impl()`（impl 与 refcnt 都非空，`map_handler.hpp:246-250`）→ 新建 handler + `share_map_impl_from`（只拷两个 offset_ptr，`:251-255`）+ `inc_map_refcount`（`:256-262`）→ `set_handler`（新 handler 已有 impl，不再 `map_init`）。不能共享时（旧 shm 对象）退化为建一个**独立的新 map**（`bpftime_shm_internal.cpp:1118-1124`）。单测：`runtime/unit-test/maps/test_shm_hash_maps.cpp:203-225`。

### 3.5 ringbuf 生产/消费协议
生产（目标进程 BPF 程序）：
`bpf_ringbuf_reserve` helper (`bpf_helper.cpp:450-459`) → `bpftime_ringbuf_reserve(fd,size)` (`bpftime_shm.cpp:388-399`) → `ringbuf_map_impl::reserve` (`ringbuf_map.cpp:166-169`) → `ringbuf::reserve` (`:280-329`)：
1. `size` 不能带 BUSY/DISCARD 位（`:282-288`）；
2. 加 `reserve_spin_lock`（多生产者互斥，`:289`）；
3. `avail = max_ent - (prod - cons)`，`total = round_up(size+8, 8)`；`total>max_ent`→E2BIG，不够→ENOSPC（`:290-301`）；
4. 若 `off+total > max_ent`（会跨环尾）：在 off 处写一条 `len = (max_ent-off-8)|DISCARD` 的**填充记录**，producer 跳到下一圈起点，`off=0`（`:302-320`，#706 修复，回归测试 `runtime/unit-test/test_ringbuf_tail_wrap.cpp:21-90`）；
5. 写 8 字节头 `{len = size|BUSY, fd = self_fd}`（`ringbuf_hdr`，`:258-261,321-323`）——内核这里存 `pg_off`，bpftime 存 **map fd**；
6. `smp_store_release(producer_pos, prod+total)` 发布（`:324`），返回 `data+off+8`。
`bpf_ringbuf_submit/discard` (`bpf_helper.cpp:461-485`) 用 `ptr[-1]`（即头里的 fd 字段）找回 map fd → `bpftime_ringbuf_submit` (`bpftime_shm.cpp:401-411`) → `ringbuf::submit` (`ringbuf_map.cpp:331-342`)：由 sample 指针反推头，清 BUSY（discard 再置 DISCARD），`__atomic_exchange_n(...ACQ_REL)`，**不加锁**。`bpf_ringbuf_output` = reserve+memcpy+submit（`bpf_helper.cpp:432-448`）。flags 均忽略，只打 WARN。

消费（loader，libbpf `ring_buffer__new/poll`）：
- libbpf 对 ringbuf fd 做两次 mmap，syscall-server 截获：`PROT_READ|PROT_WRITE` → consumer 页，`PROT_READ` → producer 页（`syscall_context.cpp:958-979` → `bpftime_get_ringbuf_consumer_page/producer_page`，`bpftime_shm.cpp:360-386` → `ringbuf_map.cpp:157-164`）。返回的就是 shm 里 `raw_buffer` 的地址；producer 页后面紧跟 `2*max_ent` 的 data，正好匹配 libbpf 映射 `page + 2*max_entries` 的预期（推断：`*2` 的分配就是为兼容 libbpf 的映射长度；bpftime 实际只写前 `max_ent` 字节，因为跨尾用填充记录而不是内核的双重映射）。
- libbpf 自己按 `len/BUSY/DISCARD/roundup` 协议读 data 并写 consumer_pos——bpftime 侧不参与。等待通过 bpftime 的 epoll：`bpftime_add_ringbuf_fd_to_epoll` 存 `ringbuf_weak_ptr`（`bpftime_shm_internal.cpp:533-536`），`bpftime_epoll_wait` 每 1ms 轮询 `ringbuf::has_data()`（`bpftime_shm.cpp:483-498,455`，`ringbuf_map.cpp:243-256`）。
- bpftime 自带的消费函数 `ringbuf::fetch_data`（`ringbuf_map.cpp:198-242`，几乎逐行照搬 libbpf `ringbuf_process_ring`）只被 `bpftime_poll_from_ringbuf`（`bpftime_shm.cpp:680-692`）使用，主要给单测/工具用。
- 内存序：x86 只有编译器屏障（TSO），arm64 用 `stlr/ldar`，riscv 用 fence，其他架构 `#error`（`ringbuf_map.cpp:39-102`）。

### 3.6 perf_event_array（纯用户态）生产/消费
`bpf_perf_event_output` (`bpf_helper.cpp:487-553`)：快照 `my_sched_getcpu()` → 保存并临时绑核 → `bpftime_map_get_info` 取类型（`bpftime_shm.cpp:287-306`）→ PERF_EVENT_ARRAY 时以**当前 CPU 号为 key** `bpf_map_lookup_elem(fd,&cpu,false)`（`:517-521`，handler 层加 map_lock）→ `perf_event_array_map_impl::elem_lookup` 返回 `&data[cpu]`（`perf_event_array_map.cpp:28-36`）→ 取出 perf handler fd → `bpftime_perf_event_output(perf_fd, data, size)`（写入 per-thread 分片；属 perf handler，不在本范围）→ 恢复亲和性。`flags`（BPF_F_CURRENT_CPU/index）完全忽略。
- 表项谁填：loader 的 libbpf `perf_buffer__new` 对每个 CPU `perf_event_open`（syscall-server 变成软件 perf 对象）后 `bpf_map_update_elem(map, &cpu, &pfd)`，走 `perf_event_array_map_impl::elem_update`（`:38-49`），value 原样存 int（推断：对应 guide 06 已有描述，本次未在 syscall-server 内追踪）。
- 回归测试：`runtime/unit-test/test_software_perf_event.cpp:23-127`（未知 map、CPU 越界 EINVAL、成功路径、亲和性恢复）。

### 3.7 KERNEL_USER_PERF_EVENT_ARRAY（用户态 → 内核 perf buffer）
创建方（`map_init` 所在进程，通常 syscall-server/daemon）在构造函数里（`perf_event_array_kernel_user.cpp:95-158`）：
1. `bpf_map_create(USER_RINGBUF, "ku_perf_id_<id>", 1MB)`（`:107-111`），记下 `user_rb_id`；
2. `create_intervally_triggered_perf_event(10)`：`PERF_TYPE_SOFTWARE/CPU_CLOCK`、`sample_period=10*1000`、`pid=0,cpu=-1`（`:446-458`）；
3. `create_transporter_prog`：手搓 BTF + 手写字节码（`:324-444,461-525`）：`bpf_user_ringbuf_drain` 取 1 条（回调恒返回 1）→ 前 8 字节是长度，长度上限 0x1c0=448 → `bpf_perf_event_output(ctx, kernel_perf_array, 0xffffffff=BPF_F_CURRENT_CPU, buf, size)`；
4. `ioctl(PERF_EVENT_IOC_SET_BPF)` + `ENABLE`（`:145-155`）。
生产方（agent 里 BPF 程序）：`bpf_perf_event_output` 分支 (`bpf_helper.cpp:533-538`) → `bpftime_shared_perf_event_output` (`bpftime_shm.cpp:599-625`) → `output_data_into_kernel` (`perf_event_array_kernel_user.cpp:66-91`)：size ≤ 448，否则 E2BIG → `ensure_current_map_user_ringbuf`（每进程懒建 `user_ring_buffer__new(bpf_map_get_fd_by_id(user_rb_id))`，`:229-270`）→ `reserve(size+8)` → 写 u64 长度 + payload → `submit`。
消费方：内核真实 perf buffer 的消费者（例如 libbpf perf_buffer），bpftime 不参与。

### 3.8 KERNEL_USER_ARRAY/HASH：与内核 map 交互
- 创建：syscall-server `run_with_kernel` 模式下 `BPF_MAP_CREATE` 先真建内核 map，再 `create_kernel_bpf_map`（`syscall_context.cpp:494-527,363-395`）用 `info.id` 作为 **bpftime 侧 fd**、`type+1000`、`kernel_bpf_map_id=info.id` 调 `bpftime_maps_create`；daemon 同理（`daemon/user/bpftime_driver.cpp:227-254`）。
- 访问（agent 里 BPF 程序）：
  - ARRAY：首次操作 `init_map_fd()`（`array_map_kernel_user.cpp:38-101`）：`bpf_map_get_fd_by_id` → `bpf_obj_get_info_by_fd` 刷新 value_size/max_entries → 先匿名 mmap 预留，再 `MAP_SHARED|MAP_FIXED` mmap 内核 map fd（仿 libbpf 两步法；`BPF_F_RDONLY_PROG` 时只读）。成功后 lookup/update/delete 直接读写 mmap 区（`:133-145,169-178,196-205`）；mmap 失败则回退到 libbpf `bpf_map_lookup_elem` 拷到 shm 里的 `value_data`（`:147-151,180,207`）。
  - HASH：每次操作都是 libbpf syscall（`hash_map_kernel_user.cpp:46-94`），lookup 拷进 shm 里的单份 `value_vec` 并返回它。
  - PERCPU_ARRAY：设计同 HASH，但 `init_map_fd` 从未被调用，见 §7。

## 4. 跨模块接口

### 4.1 本块对外提供
| 接口 | 定义 | 调用点 |
|---|---|---|
| `bpf_map_handler::map_lookup_elem/map_update_elem/map_delete_elem/bpf_map_get_next_key` | `map_handler.hpp:160-227`，`.cpp:109-748` | `bpftime_shm_internal.cpp:281-331`；`bpftime_shm.cpp:671`（`map_val` 直接调 handler）；`test_shm_hash_maps.cpp` 等 |
| `map_push_elem/map_pop_elem/map_peek_elem` | `map_handler.hpp:265-270`，`.cpp:1386-1476` | `bpftime_shm_internal.cpp:305-323` |
| `map_init/map_free` | `map_handler.hpp:228-229` | `handler_manager.cpp:84,104` |
| `get_userspace_value_size` | `map_handler.cpp:69-84` | `bpftime_shm_internal.cpp:278` |
| `try_get_ringbuf_map_impl/try_get_array_map_impl/try_get_stack_trace_map_impl/try_get_shared_perf_event_array_map_impl` | `map_handler.cpp:86-99,1271-1285` | `bpftime_shm_internal.cpp:533,591,641,652`；`bpftime_shm.cpp:610` |
| `get_raw_spin_lock()` | `map_handler.hpp:238-241` | `bpf_helper.cpp:818`（`bpf_get_stackid` 手工加锁再调 `fill_stack_trace`） |
| `has_map_impl/can_share_map_impl/share_map_impl_from/inc_map_refcount` | `map_handler.hpp:242-262` | `handler_manager.cpp:83`；`bpftime_shm_internal.cpp:1109-1113` |
| `bpftime_lock_guard` | `map_handler.hpp:45-62` | `map_handler.cpp` 各 do_xxx；`bpf_helper.cpp:818` |
| `get_gpu_map_extra_buffer/get_gpu_map_max_thread_count` | `map_handler.cpp:1286-1374` | GPU 侧（R6） |
| `bpftime_register_map_ops` | `map_handler.cpp:1375-1383`，声明 `runtime/include/bpftime_shm.hpp:217` | 仅单测 `maps/test_external_map_ops.cpp:78`（仓库内无其他使用者） |
| `ringbuf_map_impl::reserve/submit/get_consumer_page/get_producer_page/create_impl_weak_ptr/create_impl_shared_ptr`；`ringbuf::fetch_data/has_data` | `ringbuf_map.hpp:56-95` | `bpftime_shm.cpp:365,379,393,406,686`；`bpftime_shm_internal.cpp:535`；`bpftime_shm.cpp:489` |
| `array_map_impl::get_raw_data` | `array_map.cpp:15-18` | `bpftime_shm.cpp:352` → syscall-server mmap（`syscall_context.cpp:980-986`） |
| `stack_trace_map_impl::fill_stack_trace` | `stack_trace_map.cpp:96-137` | `bpf_helper.cpp:821` |
| `perf_event_array_kernel_user_impl::output_data_into_kernel/get_user_ringbuf_fd` | `perf_event_array_kernel_user.cpp:62-91` | `bpftime_shm.cpp:612`；`get_user_ringbuf_fd` 仓库内无调用者（推断：grep 未见） |
| `create_transporting_kernel_ebpf_program/create_intervally_triggered_perf_event` | `perf_event_array_kernel_user.hpp:59-63` | 仅本文件内部 |
| `prog_array_map_impl` 的 lookup 返回 fd | `prog_array.cpp:115-145` | `bpftime_tail_call`（`bpf_helper.cpp:564`，走 `bpftime_map_lookup_elem` 即 from_syscall=true） |
| `my_bpf_prog_get_fd_by_id`（非 static 的全局函数） | `prog_array.cpp:63-74` | 仅本文件（推断：全局可见只是没加 static） |
| 类型别名 `bytes_vec` 等 / `ensure_on_*` | `map_common_def.hpp` | GPU map、单测、`test_bpftime_shm_json.cpp:12` |

### 4.2 本块依赖外部
| 依赖 | 位置 |
|---|---|
| `bpf_map_attr`、`bpf_map_type`、`bpftime_map_ops` 定义 | `runtime/include/bpftime_shm.hpp:27-46,66-134,196-217` |
| `bpftime_get_runtime_config().allow_non_buildin_map_types` | `map_handler.cpp:1128`；`runtime/include/bpftime_config.hpp:70` |
| `shm_holder.global_shared_memory.set_enable_mock`（GPU 分支） | `map_handler.cpp:952` 等 |
| `my_sched_getcpu` | `runtime/src/platform_utils.cpp:7-9`；用于 `map_common_def.hpp:30`、`per_cpu_hash_map.cpp:50,73,98` |
| `bpftime_is_prog_fd` | `prog_array.cpp:129,156`（声明在 `bpftime_shm.hpp`） |
| libbpf：`bpf_map_get_fd_by_id/bpf_obj_get_info_by_fd/bpf_map_*_elem/bpf_map_create/user_ring_buffer__*/btf__*/bpf_btf_load/bpf_prog_load` | `shared/*.cpp` |
| libc `syscall` via dlsym（绕开 syscall-server 钩子） | `prog_array.cpp:31-33` |
| boost::interprocess（allocator、unordered_map、managed_shared_ptr、unique_ptr、offset_ptr） | 全部 impl |

## 5. 配置项、环境变量、编译开关

| 名字 | 默认 | 定义/读取 | 影响 |
|---|---|---|---|
| `BPFTIME_USE_VAR_HASH_MAP`（宏） | **未定义**（全仓库 CMake 都没定义它，grep 只在 `map_handler.cpp:54,786` 出现） | `map_handler.cpp:54-58,786-795` | 未定义 → `BPF_MAP_TYPE_HASH` 用 `fix_size_hash_map_impl`；定义 → `var_size_hash_map_impl`（支持 flags/E2BIG/RDONLY/WRONLY） |
| `BPFTIME_BUILD_WITH_LIBBPF` | ON（`cmake/StandardSettings.cmake:107`），`-DBPFTIME_BUILD_WITH_LIBBPF=1`（`CMakeLists.txt:104-105`） | `map_handler.cpp:37-42,187-213,338-364,492-518,653-679,886-933,1194-1217` | 关掉后：4 种 KERNEL_USER map、**PROG_ARRAY、STACK_TRACE 的 `map_init`/`map_free`** 都不编译（STACK_TRACE 的 lookup/update/delete/get_next_key 分支却不在 ifdef 内）；`shared/*.cpp` 只在 `UNIX AND NOT APPLE AND LIBBPF` 时编入（`runtime/CMakeLists.txt:105-112`） |
| `BPFTIME_ENABLE_CUDA_ATTACH` | OFF | `map_handler.cpp:15-24,219-256,…` | GPU map 类型（R6） |
| `BPFTIME_MAP_GPU_THREAD_COUNT`（环境变量） | 无（用 attr.gpu_thread_count=1024） | `map_handler.cpp:750-760` | 仅 GPU map |
| `runtime_config::allow_non_buildin_map_types` | false（`bpftime_config.hpp:70`） | `map_handler.cpp:1128` | 允许未内建 map 类型走外部 map ops |
| `attr.map_extra` 低 4 位 | 0 → 5 个哈希 | `map_handler.cpp:868-872` | bloom filter 哈希函数个数 |
| `attr.flags` 中 `BPF_F_RDONLY/WRONLY` | — | 只有 var_hash 认（`var_hash_map.cpp:39,82,115`） | 默认 HASH 不支持 |
| `check_update_flags` 高 32 位 | — | `map_common_def.hpp:83-94` | 注释说"留给 bpftime 自定义 op"，本范围内无使用者 |
| 硬编码：perf 搬运周期 `10`（实际 10µs，见 §7）、USER_RINGBUF 1MB、单条 448 字节 | — | `perf_event_array_kernel_user.cpp:132,110,69` | |

## 6. 并发、生命周期与错误处理

### 6.1 锁策略
- **handler 层统一锁**：`should_lock` 是每个 impl 的 `static const bool`，在 `do_xxx` lambda 里编译期决定是否拿 `map_lock`（`map_handler.cpp:112-127,273-289,438-445,588-603,1389-1396,1421-1428,1448-1455`）。
  - true：fix_hash、var_hash、lru、lpm（内部再锁一次 `mapSpin`）、bloom、queue、stack、stack_trace、perf_event_array。
  - false：array、array_of_maps、per_cpu_array、per_cpu_hash、ringbuf、prog_array、4 种 kernel_user。
- `map_lock` 是 `pthread_spinlock_t`，在 shm 里，跨进程共用；初始化 pshared=0（推断：glibc 的 spinlock 实现忽略 pshared，实践中跨进程可用，但不符合 POSIX 约定）。
- **dup 后锁不共享**：每个 handler 有自己的 `map_lock`，`share_map_impl_from` 只共享 impl 指针（`map_handler.hpp:251-255,278`），所以经两个 fd 操作同一 impl 时互不排斥（推断，见 §7）。
- ringbuf：reserve 用 impl 内的进程共享自旋锁（`ringbuf_map.cpp:195,289`），submit 用原子交换不加锁，消费无锁（单消费者假设）。
- per-CPU 两种 map 完全不加锁，靠"当前 CPU 号快照"分片，无独占保证；per_cpu_hash 的新 key 插入会并发改 `unordered_map`（推断：数据竞争）。
- `bpf_get_stackid` 绕过分发层，自己拿 `get_raw_spin_lock()`（`bpf_helper.cpp:818`）。

### 6.2 跨进程/生命周期
- 所有 impl 的容器都是 shm 分配器 + `offset_ptr`，跨进程可读。**例外**（含进程私有值的 shm 对象）：
  - `lpm_trie_map_impl::memory`（引用成员，存的是创建进程里 `managed_shared_memory` 对象的地址，`lpm_trie_map.hpp:98`），在 `create_node` 里使用（`lpm_trie_map.cpp:119-129`）；
  - kernel_user 系列的 `map_fd`/`mmap_ptr`/`kernel_map_fd`/`pfd`/`transporter_prog_fd`（`array_map_kernel_user.hpp:17-21`、`hash_map_kernel_user.hpp:29-30`、`percpu_array_map_kernel_user.hpp:19`、`perf_event_array_kernel_user.hpp:34-35`）；
  - `lru_var_hash_map_impl::memory_allocator/memory_deleter`（推断：boost 的 allocator/deleter 内部存 `offset_ptr<segment_manager>`，跨进程安全）。
- 释放：`map_free` 在**调用 `clear_id_at` 的那个进程**里 `destroy_ptr`，于是 kernel_user 的析构会在那个进程里 `close(map_fd)` / `ioctl(pfd)` / `close(pfd)`（`array_map_kernel_user.cpp:229-234`、`hash_map_kernel_user.cpp:96-101`、`perf_event_array_kernel_user.cpp:159-164`），关的可能是别的进程的 fd 号（推断）。
- fork：本块没有任何 atfork 处理。per-CPU / perf 输出只依赖 `sched_getcpu()`；`user_rb_map`（进程堆）在 fork 后被子进程继承（推断：子进程里的 `user_ring_buffer` mmap 仍有效但与父进程共享生产位置，未验证）。

### 6.3 错误返回约定（不统一）
- 大多数 impl：失败返回 `-1` 并设 `errno`（array、var_hash、lru、lpm、queue、stack、bloom、per_cpu_*、perf_event_array、prog_array）。
- 例外：`stack_trace_map_impl::elem_update` 返回 `-ENOTSUP`（`stack_trace_map.cpp:63-68`）；ARRAY_OF_MAPS 在 `!from_syscall` 时 update/delete 返回 `-EINVAL`（`map_handler.cpp:366-369,681-684`）；push/pop/peek 对不支持类型返回 `-ENOTSUP`（`:1412-1415,1439-1443,1471-1474`）；kernel_user 系列返回 libbpf 的返回值（libbpf ≥1.0 是 `-errno`）；`output_data_into_kernel` reserve 失败返回**正** errno（`perf_event_array_kernel_user.cpp:81-84`）。
- helper 把 impl 的 long 原样转 `uint64_t` 返给 BPF 程序（`bpf_helper.cpp:306-341`），所以 BPF 程序大多只看到 `-1`，看不到内核那种 `-EEXIST/-E2BIG/-ENOENT`（推断）。
- 分发层 `default` 未知类型：lookup 返回 nullptr，update/delete/get_next_key 返回 -1，都只打 ERROR 日志（`map_handler.cpp:257-265,421-430,571-581,736-745`）。
- `bpftime_shm.cpp` 的 `bpftime_map_*` 系列（syscall 入口）用 `try/catch(std::exception)` 吞异常返回 -1/nullptr（`bpftime_shm.cpp:102-205`）；helper 入口（`bpf_helper.cpp:299-341`）**没有** try/catch，impl 里抛的异常会穿过 JIT 代码（推断：例如 ringbuf `spin_lock_guard` 抛 `runtime_error`，`ringbuf_map.cpp:267-272`）。

## 7. 不直观的行为、疑似 bug、代码与注释不一致

（与 06 章"深入参考"已记录的条目重复的，在末尾标「指南已有」。）

1. **`map_init` 失败被忽略，留下 impl 为空的 handler。** `set_handler` 调 `map_init` 不看返回值（`runtime/src/handler/handler_manager.cpp:83-85`），仍返回 fd。例：RINGBUF `max_entries` 非 2 的幂 → `return -1` 不构造 impl（`map_handler.cpp:813-818`）；BLOOM `key_size!=0`（`:862-866`）；未知类型（`:1137-1140`）。之后任何操作都会 `static_cast<…>(nullptr)->…`；`try_get_ringbuf_map_impl` 返回含 nullptr 的 optional，`bpftime_ringbuf_reserve` 直接 `impl->reserve` 解引用（`bpftime_shm.cpp:391-393`）。确定程度：代码路径确认，崩溃未实跑。
2. **dup 出来的 fd 与原 fd 共享 impl 但各用各的 `map_lock`**（`map_handler.hpp:251-255,278`；`bpftime_shm_internal.cpp:1109-1115`）。对 should_lock=true 的 map（hash/lru/queue…），经两个 fd 并发操作不互斥。确定程度：推断（读代码确定锁是两把，未构造并发复现）。
3. **默认 HASH（fix_size）不实现 flags/容量/删除语义**：`elem_update` 忽略 flags 且无视底层返回值恒返回 0；`elem_delete` 恒返回 0（`fix_hash_map.cpp:34-45`）；`elem_lookup` 未命中不设 errno（`:26-32` + `bpftime_hash_map.hpp:142-158`）。**已实跑**（`/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R2/exp.cpp`）：`max=2: upd3(full)=0 lookup3=(nil) NOEXIST-on-existing=0 delete-missing=0 EXIST-on-missing=0`。「指南已有」大部分；"EXIST 对不存在的 key 也会插入成功"是补充。
4. **kernel 兼容单测测的不是默认 HASH**：`maps/kernel_unit_tests.cpp` 全部用 `var_size_hash_map_impl`（`:55,157,305,536,715,777,795`），而 `BPF_MAP_TYPE_HASH` 默认走 fix_size（`map_handler.cpp:54-58`）。所以"内核 test_maps 通过"不代表 HASH map 有内核语义。确认。
5. **stack_trace 的内容比较逻辑写反**：`fill_stack_trace` 里 `equals = (size相等); if (!equals) equals = equals && std::equal(...)`（`stack_trace_map.cpp:115-120`），内容永远不比较：同桶同长度的不同栈被当成"相同"返回旧 id；且 `!discard_old_one` 时不同栈也返回同一个 id（内核返回 `-EEXIST`）。**已实跑**：`[stack_trace] id1=637 id2=637 stored==a? 1 stored==b? 0`。现有单测 `test_stack_trace_map.cpp:57-101` 没覆盖这个情形。确认。
6. **stack_trace lookup 只返回实际帧数**：value vector 长度 = 栈帧数（`stack_trace_map.cpp:103-108`），但 syscall 路径按 `value_size` memcpy（`syscall_context.cpp:575-576`），帧数少于 `value_size/8` 时越界读 shm（内核会零填充）。推断。
7. **per-CPU hash 的 helper 视角 delete 清错范围、不删 key**（`per_cpu_hash_map.cpp:96-107`）。**已实跑**（ncpu=4，在 CPU1 上）：`key still present=1 values=[0,22,33,44]`——清掉的是 CPU0 的份，CPU1 自己的 22 还在。「指南已有」，本次补充实跑证据。
8. **per-CPU hash helper 视角 update 不查 flags/max_entries**（`per_cpu_hash_map.cpp:68-94`），新 key 插入在无锁情况下改 `unordered_map`（`should_lock=false`，`.hpp:52`）→ 多线程同时插不同 key 有数据竞争。「指南已有」。
9. **LPM：结构里存 `managed_shared_memory&`**（`lpm_trie_map.hpp:98`），`create_node` 用它分配（`lpm_trie_map.cpp:119-129`）。若 map 由 syscall-server 创建、agent 进程里的 BPF 程序做 update，引用指向别的进程的对象地址 → 未定义行为/崩溃。lookup/delete 不用它。确定程度：推断（高置信）。
10. **LPM lookup 返回 thread_local 副本**（`lpm_trie_map.cpp:254-263`）：BPF 程序通过返回指针写 value 不会落到 map 里（内核里会）。**已实跑**：`wrote 12345 via lookup ptr; re-lookup value=1, same tls buffer=1`。确认。
11. **LPM `get_next_key` 基本没实现**：`key==NULL` 只返回最左一个非中间节点，`key!=NULL` 一律 ENOENT（`lpm_trie_map.cpp:583-612`）。**已实跑**：2 个条目只遍历到 1 个。确认。
12. **LPM 删除是逻辑删除**（标 intermediate，不回收节点，`lpm_trie_map.cpp:553-559`）；之后对同一前缀 `BPF_NOEXIST` 插入返回 EEXIST（`:349-355` 先判 NOEXIST 再判 exists_real）。**已实跑**：`ret=-1 errno=17`。内核此时应成功。确认。
13. **LPM 双重加锁**：handler 层 `should_lock=true` 拿 `map_lock`，impl 内又拿 `mapSpin`（`lpm_trie_map.hpp:118`、`lpm_trie_map.cpp:194,269,512,566`）。无害但多余。确认。
14. **LPM lookup 的 `matchlen == _max_prefixlen` 分支**直接把当前节点当 found，即使它是 intermediate（`lpm_trie_map.cpp:221-225`），随后 `get_node_value_data` 为空返回 ENOENT，而不会回退到更短的已匹配前缀。推断（边界情况）。
15. **BLOOM 经 syscall 的 LOOKUP 不是 peek**：`elem_lookup` 恒返回 EOPNOTSUPP（`bloom_filter.cpp:283-289`），而头文件注释写"BPF_MAP_LOOKUP_ELEM -> Peek"（`bloom_filter.hpp:9`）。内核 syscall LOOKUP 对 bloom 走 peek。另外注释说返回 `-EOPNOTSUPP`/"Negative error code"（`.hpp:99-127`），实际是 `-1`+errno。注释与代码不一致，确认。
16. **QUEUE/STACK `elem_update` 不检查 flags**（`queue.cpp:66-105`，与 `map_push_elem` 的 `:137-142` 不同），注释说非法 flags 返回 -EINVAL、满时返回 -EBUSY（`queue.hpp:100-104`），实际不检查、满时 E2BIG。`elem_delete` 当 pop 用（`queue.cpp:107-120`），而内核 syscall DELETE 对 queue/stack 返回 EINVAL。注释与代码不一致，确认。
17. **ARRAY_OF_MAPS 的 lookup 把 map id 当指针返回**（`map_in_maps.hpp:29-30`）。helper 视角下 BPF 程序把它当"map 句柄"继续用（map 句柄就是 fd，`bpftime_shm.cpp:646-647`），说得通；但 syscall 路径会 `memcpy(value, (void*)id, 4)`（`syscall_context.cpp:575`）→ 解引用小整数地址崩溃。另外存的 id 为 0 时 lookup 返回 nullptr 被当成"不存在"。推断（未实跑）。
18. **prog_array 的内核分支每次 lookup 都 `BPF_PROG_GET_FD_BY_ID` 新开一个 fd**（`prog_array.cpp:136-144`），tail_call 路径会 `close`（`bpf_helper.cpp:645-651`），但 syscall 路径的 lookup 永不关闭 → fd 泄漏。推断。`prog_array.cpp` 的 lookup/update 只在 `__linux__` 下定义（`:113-177`）。
19. **kernel_user 系列把进程私有的 fd/mmap 地址存进 shm 对象**（§6.2）。第一个调用 `init_map_fd` 的进程写入后，其他进程看到 `map_fd>=0`/`mmap_ptr!=nullptr` 就直接用，fd 号/地址在本进程无效。推断（高置信）；只有"始终只有一个 agent 进程访问"时才正确。
20. **KERNEL_USER_ARRAY 下标没有按 8 字节对齐**：mmap 大小按 `roundup(value_sz,8)*max_entries`（`array_map_kernel_user.cpp:27-35`），但访问用 `key*_value_size`（`:142,176,202`）。内核 array 的元素步长是 `round_up(value_size,8)`（内核 `kernel/bpf/arraymap.c` 的 `elem_size`，来自对内核的已有知识），value_size 非 8 倍数时读写错位。推断（未实跑，需要 root）。
21. **KERNEL_USER_ARRAY 的 delete 把元素清零返回 0**（`:183-208`），内核 array 删除返回 EINVAL；mmap 路径 update 忽略 flags（`:169-178`）。确认（代码）。
22. **KERNEL_USER_HASH 的 `value_vec` 是 shm 里的单份共享缓冲且 `should_lock=false`**（`hash_map_kernel_user.hpp:26-27,35`），并发 lookup 互相覆盖；注释"Allocate as a local variable to make it thread safe, since we use sharable lock"（`hash_map_kernel_user.cpp:52-53,69-70,80-81,91-92`）与代码不符（既不是局部变量也没有锁）。`init_map_fd` 失败被忽略（`:16-37,49-51`）。确认（代码）。
23. **KERNEL_USER_PERCPU_ARRAY 完全不可用**：`init_map_fd` 从未被调用（全仓库只有定义，grep 确认），`kernel_map_fd` 恒为 -1，所有操作都对 fd -1 调 libbpf（`percpu_array_map_kernel_user.cpp:34-63`）；`value_data` 只有 1 字节、`_value_size/_max_entries` 未初始化。确认。
24. **KERNEL_USER_PERF_EVENT_ARRAY**：`sample_period = duration_ms*1000` 对 CPU_CLOCK（纳秒）实际是 10µs（`:446-458`，「指南已有」，未实测）；`pid=0,cpu=-1` 的 perf event 只在**创建进程自身占用 CPU 时**计时触发（推断：daemon/syscall-server 空闲时搬运程序可能长时间不跑，未验证）；`reserve_lock` 从不使用（`:52,249,277-284`，「指南已有」）；`output_data_into_kernel` 失败返回正 errno 被当成功（`:81-84` + `bpftime_shm.cpp:612-617`，「指南已有」）；`user_rb_map` 是无锁的进程全局 `std::map`（`:39-47,229-245`），多线程首次输出有竞争（推断）；构造函数里 `kernel_perf_fd`、`btf_fd`、`btf` 对象、`user_rb_fd` 都没关闭/释放（`:109,133,464,486`）；早期失败 `return` 留下半初始化对象（`:112-126`），而后期失败抛异常（`:138-155`）。
25. **`ensure_on_certain_cpu` 主模板**的 `func(currcpu)` 对零参 `std::function<T()>` 传参，实例化即编译失败，`currcpu` 从不赋值（`map_common_def.hpp:33-49`）。「指南已有」。
26. **`bpftime_hash_map` 墓碑不回收**：没有 rehash，`EMPTY` 槽只减不增（`bpftime_hash_map.hpp:186-188,226`）。长期增删后所有槽都是 FILLED/DELETED，未命中查找退化为全表扫描（`:146-157`）。推断（性能）。
27. **`#ifdef` 不对称**：STACK_TRACE 和 PROG_ARRAY 的 `map_init`/`map_free` 在 `BPFTIME_BUILD_WITH_LIBBPF` 里（`map_handler.cpp:919-932,1194-1197,1214-1216`），但 STACK_TRACE 的 lookup/update/delete/get_next_key 分支不在（`:159-163,320-324,474-478,635-639`）。关 libbpf 时 STACK_TRACE 创建失败（走 default）。get_next_key 的 kernel_user 分支条件是 `__linux__ && LIBBPF`（`:492`），其他三个操作只有 `LIBBPF`（`:187,338,653`）。确认（代码）。
28. **LRU lookup 会改链表**（`move_to_head`，`lru_var_hash_map.cpp:33`），所以只读操作也必须拿锁（should_lock=true 正确）。淘汰时先 `map_impl.erase(tail->key)` 再 `evict_entry(tail)`（`:66-71`）。LRU 是"全局"LRU，没有内核 LRU 的 per-CPU 本地列表与批量淘汰；`BPF_F_NO_COMMON_LRU` 等 flag 不认。确认（代码）。
29. **ringbuf 与内核/libbpf 的差异**：头里第二个 u32 存 map fd 而不是 page offset（`ringbuf_map.cpp:258-261,323`），submit 靠 `ptr[-1]` 取 fd（`bpf_helper.cpp:465,478`）；跨环尾用 DISCARD 填充记录而非内核双重映射（`ringbuf_map.cpp:302-320`）；`max_entries` 只要求 2 的幂，不要求页对齐（`map_handler.cpp:806-818`）；flags（`BPF_RB_NO_WAKEUP` 等）忽略（`bpf_helper.cpp:436-438,454-457`）；没有 `bpf_ringbuf_query`、`reserve_dynptr`（推断：helper 表里未见，归 helper 读者确认）。确认（代码）。
30. **queue/stack 测试注释误导**：`test_queue_map.cpp` 并发测试注释说"rely on the internal mutex of queue_map_impl"，实际 impl 没有内部锁，测试自己外加了 `pthread_spinlock`（`test_queue_map.cpp:279-320,336-340`）。确认。
31. **perf_event_array_kernel_user 有 virtual 析构**（`perf_event_array_kernel_user.hpp:42`），shm 中对象带 vptr。`destroy_ptr` 用静态类型调用析构（推断：编译器对已知最终类型做去虚化，或在创建进程以外的进程通过 vptr 调用会跳到错误地址——取决于编译器，未验证）。

## 8. 对照现有指南

对照文件：`/home/y/src/bpftime-wt-reading-guide/06-maps-helpers.md`（map 相关：6.1、6.2、6.4、6.5 的 perf array 部分、6.7、小结、"深入参考"里"map 分发层/per-CPU map/perf 输出/内核共享版 perf array"四段）。

### 错误 / 不准确
- 6.2「实现对象的创建和销毁都由 handler 管理器显式完成（`map_init` / `map_free`），**不靠 C++ 析构函数**」→ 实际 `map_free` 用 `memory.destroy_ptr(static_cast<具体类型*>(impl))`（`map_handler.cpp:1165-1266`），**具体 impl 的析构函数会被调用**（例如 lpm 的 `pthread_spin_destroy`、kernel_user 的 `close(map_fd)`、perf kernel_user 的 `ioctl/close`）。不靠的只是 `~bpf_map_handler`（`map_handler.hpp:132-142`）。建议改成"不靠 handler 的析构函数"。
- 6.2「锁是**每个 map 一把**自旋锁」→ 实际是每个 **handler（fd）** 一把（`map_handler.hpp:278`）；dup 出的 fd 共享 impl 却不共享锁（§7-2）。
- 6.2 与小结「目前**只有两种 per-CPU map** 真正区分这两个视角」→ 不完整：ARRAY_OF_MAPS 的 update/delete 在 `from_syscall=false` 时直接 `-EINVAL`（`map_handler.cpp:366-369,681-684`）；外部 map ops 也收到 `from_syscall`（`:260,424,575,739`）；另外 tail_call helper 查 prog_array 走的是 `bpftime_map_lookup_elem`（`bpf_helper.cpp:564`），即 `from_syscall=true`，"helper 一律传 false"不严格成立。
- 6.2 mermaid 图把"loader 调 bpf(2)"画成直接进分发层：实际 loader 的 lookup 还多一步 `memcpy(value, ptr, get_userspace_value_size)`（`syscall_context.cpp:575-576`），这是"两视角 value 大小不同"真正落地的位置，6.4 末段没指出这一行。属于遗漏而非错误。
- 深入参考 should_lock 列表本身正确（已逐个核对）。

### 遗漏（学习者应知道而指南没讲）
1. **map 类型全景**：指南只讲 sslsniff 用到的 HASH/PERCPU_ARRAY/PERF_EVENT_ARRAY。缺 RINGBUF、LRU_HASH、LPM_TRIE、BLOOM、QUEUE/STACK、STACK_TRACE、PROG_ARRAY、ARRAY_OF_MAPS、4 种 KERNEL_USER、外部 map ops 的总表（本笔记 §9 末给出）。
2. **ringbuf 生产/消费协议**整个没讲（§3.5）：头里存 fd、DISCARD 填充记录、`2*max_ent` 分配与 libbpf mmap 的对应、epoll 1ms 轮询 `has_data`。
3. **默认 HASH 的内存布局**（`bpftime_hash_map`：质数桶、`[state][key][value]`、墓碑、`*31` 哈希）未讲，只提了"语义缺口"。
4. **`BPFTIME_USE_VAR_HASH_MAP` 在任何 CMake 里都没定义**（指南只说"未定义 → fix_size"，没说"也没有开关能打开它"），以及 kernel_unit_tests 测的是 var_hash（§7-4）。
5. **map_init 返回值被忽略**（§7-1）、dup/refcount 机制（§3.4）。
6. **kernel-user 共享 map 的工作方式**（mmap/逐次 syscall、fd 以 kernel map id 注册、进程私有 fd 存在 shm）只讲了 perf array 一种。
7. stack_trace 内容比较 bug（§7-5，已实跑）、LPM 的跨进程引用/TLS 返回/迭代缺失/NOEXIST 误报（§7-9~12，部分已实跑）、ARRAY_OF_MAPS syscall lookup（§7-17）、prog_array fd 泄漏（§7-18）、KERNEL_USER_ARRAY 步长、KERNEL_USER_PERCPU_ARRAY 不可用。
8. BPF 程序看到的 map helper 返回值大多是 `-1` 而不是 `-errno`（§6.3）。
9. `BPF_MAP_LOOKUP_AND_DELETE_ELEM` 经 syscall 只对 queue/stack 有效（`bpftime_shm.cpp:155-166`）。

### 讲得好、可以保留
- 6.2 "为什么不用虚函数 + `offset_ptr<void>` + type switch" 的动机解释，与代码一致（`map_handler.hpp:70`、`.cpp:129-266`）。
- 6.4 两种 per-CPU 布局图与 `data_at` 公式，正确；"CPU 号只是快照，不是独占"的判断正确。
- 深入参考里的 should_lock 列表、fix_hash 语义缺口、per-CPU hash delete bug（本次已实跑复现）、`ensure_on_certain_cpu` 死代码、perf kernel_user 的 448 字节/10µs/`reserve_lock`/正 errno 等条目都与代码一致。
- 6.5 perf_event_array 作为"CPU 号 → perf 对象 fd 路由表"的定位准确（`perf_event_array_map.cpp:18,28-36`）。

## 9. 学习建议

### 推荐阅读顺序
1. `runtime/include/bpftime_shm.hpp:27-46,66-134,191-217`：attr、类型枚举（注意 +1000/+1500 偏移）、外部 map ops。
2. `runtime/src/handler/map_handler.hpp:64-300` → `map_handler.cpp:109-268`（只看 lookup 一个函数就能理解全部分发套路）→ `:762-845`（map_init 前几种）→ `:1146-1270`（map_free 与 refcount）。
3. `runtime/src/bpf_map/map_common_def.hpp` 全文。
4. ARRAY：`userspace/array_map.cpp`（最简单的 impl 模板）。
5. 默认 HASH：`bpftime_hash_map.hpp` → `userspace/fix_hash_map.cpp`。
6. per-CPU：`per_cpu_array_map.cpp`、`per_cpu_hash_map.cpp`，对照 `map_handler.cpp:69-84` 与 `syscall_context.cpp:569-576`。
7. RINGBUF：`ringbuf_map.cpp:175-342` + `bpf_helper.cpp:432-485` + `syscall_context.cpp:958-979` + `bpftime_shm.cpp:455-500`。
8. PERF_EVENT_ARRAY：`perf_event_array_map.cpp` + `bpf_helper.cpp:487-553`。
9. 其余按需：LRU → queue/stack → bloom → lpm → stack_trace/prog_array/map_in_maps → `shared/`。

### 动手实验
1. **跑全部 map 单测**（已实跑）：
   ```bash
   export BPFTIME_GLOBAL_SHM_NAME=notes_R2_$RANDOM
   build-jit/runtime/unit-test/bpftime_runtime_tests "[maps],[kernel],[lru_hash],[lpm_trie],[lpm_trie_map],[bloom_filter],[queue_map],[stack_map],[bpftime_hash_map],[ringbuf]"
   # 无 tag 的几个按名字跑：
   build-jit/runtime/unit-test/bpftime_runtime_tests "Test basic operations of array map"   # per-CPU array
   build-jit/runtime/unit-test/bpftime_runtime_tests "Test stack trace map 2"
   ```
   关键输出：`All tests passed (8438518 assertions in 51 test cases)`；无 tag 的 7 个（per-CPU array/hash、external ops、stack trace 1/2、shm hash maps、user→user tailcall）也全过。注意 tailcall 测试会用 `BPFTIME_GLOBAL_SHM_NAME` 建 shm，跑完要 `rm /dev/shm/notes_R2_*`。
2. **复现 4 个语义问题**（已实跑）：`/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R2/exp.cpp`，直接把 4 个 impl 的 .cpp 与 `platform_utils.cpp` 编进去（`build-jit` 的 `libruntime.a` 是 `-fno-fat-lto-objects`，直接链接麻烦）：
   ```bash
   R=/home/y/src/bpftime-wt-official-177a1dc; S=$R/runtime/src
   g++ -std=gnu++20 -O1 -g -I$R/runtime/include -I$S -I$R/third_party/spdlog/include \
     -I$R/build-jit/libbpf/uapi -I$R/build-jit/libbpf exp.cpp \
     $S/bpf_map/userspace/{stack_trace_map,fix_hash_map,per_cpu_hash_map,lpm_trie_map}.cpp \
     $S/platform_utils.cpp -o exp -lpthread -lrt
   taskset -c 1 ./exp notes_R2_exp_$RANDOM
   ```
   实际输出：
   ```
   [stack_trace] id1=637 id2=637 stored==a? 1 stored==b? 0 (kernel would return -EEXIST for b)
   [fix_hash] max=2: upd1=0 upd2=0 upd3(full)=0 lookup3=(nil) NOEXIST-on-existing=0 delete-missing=0 EXIST-on-missing=0
   [percpu_hash] on cpu 1 after helper elem_delete: key still present=1 values=[0,22,33,44]
   [lpm] entries=2, keys visited via get_next_key=1
   [lpm] NOEXIST re-insert after delete: ret=-1 errno=17 (EEXIST=17)
   [lpm] wrote 12345 via lookup ptr; re-lookup value=1, same tls buffer=1
   ```
3. **建议给学习者的小实验（未实跑）**：用 `bpftool`/libbpf 写一个 loader，在 `bpftime load` 下创建 `max_entries=3000` 的 ringbuf（非 2 的幂），观察 `bpf_map_create` 返回 fd 成功、随后 reserve 段错误（验证 §7-1）。
4. **观察 ringbuf 布局**：在 `test_ringbuf_tail_wrap` 上打断点于 `ringbuf::reserve` 的填充分支（`ringbuf_map.cpp:303`），第 74 条记录会触发（测试注释 `test_ringbuf_tail_wrap.cpp:55-57`）。

### 学习者最容易误解的点
1. "HASH map 有内核语义"——默认实现是 `fix_size_hash_map_impl`，不认 flags、满了也返回成功；kernel 兼容单测测的是另一个没启用的实现。
2. "per-CPU 是真 per-CPU"——只是 `sched_getcpu()` 快照，无绑核无锁；loader 视角 value 大 ncpu 倍，落地点在 syscall-server 的 memcpy。
3. "map 句柄是指针"——在 bpftime 里 map 句柄就是 shm 里的假 fd（`map_ptr_by_fd` 原样返回 fd），ARRAY_OF_MAPS 的 lookup 结果也是 fd 当指针。
4. "lookup 返回的指针都能写回 map"——LPM（TLS 副本）、kernel-user hash/percpu（共享缓冲副本）、prog_array（TLS int）都不是。
5. "ringbuf 和内核一模一样"——内存布局刻意兼容 libbpf 消费者，但头里存 fd、跨尾用填充记录、只有 1ms 轮询的 epoll。

### 总表：map 类型 → 实现文件 → 关键特性 → 单测

| map 类型（值） | 实现类 / 文件 | 布局与分配 | 锁 | from_syscall 差异 | 编译条件 | 单测 |
|---|---|---|---|---|---|---|
| HASH (1) | `fix_size_hash_map_impl` / `userspace/fix_hash_map.cpp` + `bpftime_hash_map.hpp`（宏可切 `var_hash_map.cpp`） | 质数桶开放寻址，单块 `bytes_vec` | handler 锁 | 无 | 总是 | `test_bpftime_hash_map`、`test_fix_hash_map`、`test_shm_hash_maps`；var 版：`kernel_unit_tests` |
| ARRAY (2) | `array_map_impl` / `userspace/array_map.cpp` | 连续 `value*max`，可被 mmap | 无 | 无 | 总是 | `kernel_unit_tests`(test_arraymap)、`test_shm_hash_maps` |
| PROG_ARRAY (3) | `prog_array_map_impl` / `userspace/prog_array.cpp` | `i32_vec`，-1 空；bpftime fd 编码 `-fd-2`，内核存 id | 无 | 无 | LIBBPF（init/free） | `tailcall/test_user_to_user_tailcall`、`test_user_to_kernel_tailcall`（root） |
| PERF_EVENT_ARRAY (4) | `perf_event_array_map_impl` / `userspace/perf_event_array_map.cpp` | `i32_vec`（CPU→perf fd），-1 空 | handler 锁 | 无 | 总是 | `test_software_perf_event`（间接） |
| PERCPU_HASH (5) | `per_cpu_hash_map_impl` / `userspace/per_cpu_hash_map.cpp` | unordered_map，value=ncpu 份拼接 | 无 | 有（`*_userspace`） | 总是 | `test_per_cpu_hash`、`kernel_unit_tests`(hashmap_percpu) |
| PERCPU_ARRAY (6) | `per_cpu_array_map_impl` / `userspace/per_cpu_array_map.cpp` | `[entry][cpu]` 连续 | 无 | 有 | 总是 | `test_per_cpu_array`、`kernel_unit_tests`(arraymap_percpu*) |
| STACK_TRACE (7) | `stack_trace_map_impl` / `userspace/stack_trace_map.cpp` | unordered_map<id, uint64_vec>，由 `bpf_get_stackid` 写 | handler 锁（helper 手动） | 无 | LIBBPF（init/free） | `test_stack_trace_map` |
| LRU_HASH (9) | `lru_var_hash_map_impl` / `userspace/lru_var_hash_map.cpp` | unordered_map + shm shared_ptr 双链表 | handler 锁 | 无 | 总是 | `test_lru_hash_map`、`test_lru_var_hash_map` |
| LPM_TRIE (11) | `lpm_trie_map_impl` / `userspace/lpm_trie_map.cpp` | shm 二叉 trie，逻辑删除 | handler 锁 + 内部锁 | 无 | 总是 | `test_lpm_trie_map` |
| ARRAY_OF_MAPS (12) | `array_map_of_maps_impl` / `userspace/map_in_maps.hpp` | array<int fd> | 无 | 有（非 syscall 不许改） | 总是 | `test_map_in_map_bounds` |
| QUEUE (22) | `queue_map_impl` / `userspace/queue.cpp` | 预分配环形数组 | handler 锁 | 无 | 总是 | `test_queue_map` |
| STACK (23) | `stack_map_impl` / `userspace/stack.cpp` | vector 尾部增删 | handler 锁 | 无 | 总是 | `test_stack_map` |
| RINGBUF (27) | `ringbuf_map_impl`/`ringbuf` / `userspace/ringbuf_map.cpp` | `2*page + 2*max_ent`，consumer/producer 页 + data | reserve 内部锁 | 无（elem_* 全 ENOTSUP） | 总是 | `test_ringbuf_tail_wrap`、`test_epoll` |
| BLOOM_FILTER (30) | `bloom_filter_map_impl` / `userspace/bloom_filter.cpp` | 2 的幂位数组，随机种子 | handler 锁 | 无（syscall lookup 不是 peek） | 总是 | `test_bloom_filter_map` |
| KERNEL_USER_HASH (1001) | `hash_map_kernel_user_impl` / `shared/hash_map_kernel_user.cpp` | 每次 libbpf syscall | 无 | 无 | Linux+LIBBPF | 无 |
| KERNEL_USER_ARRAY (1002) | `array_map_kernel_user_impl` / `shared/array_map_kernel_user.cpp` | mmap 内核 map，失败回退 syscall | 无 | 无 | Linux+LIBBPF | 无 |
| KERNEL_USER_PERF_EVENT_ARRAY (1004) | `perf_event_array_kernel_user_impl` / `shared/perf_event_array_kernel_user.cpp` | USER_RINGBUF + 内核搬运程序 | 无 | 无 | Linux+LIBBPF | 无 |
| KERNEL_USER_PERCPU_ARRAY (1006) | `percpu_array_map_kernel_user_impl` / `shared/percpu_array_map_kernel_user.cpp` | 每次 libbpf syscall（但 fd 从未初始化） | 无 | 无 | Linux+LIBBPF | 无 |
| 其他任意类型 | `global_map_ops_table`（进程私有） | 由注册方决定 | 无 | 透传 | `allow_non_buildin_map_types` | `test_external_map_ops` |
| GPU 系列 (1501–1527) | `bpf_map/gpu*`（R6） | — | — | — | CUDA | R6 |

不支持（走 default 报 Unsupported）：CGROUP_ARRAY、LRU_PERCPU_HASH、HASH_OF_MAPS、DEVMAP/SOCKMAP/CPUMAP/XSKMAP/SOCKHASH、各类 STORAGE、REUSEPORT_SOCKARRAY、STRUCT_OPS、USER_RINGBUF 等（`bpftime_shm.hpp:79-111` 有枚举、`map_init` 无分支）。另：`run_with_kernel` 模式下 syscall-server 会把**任意**内核 map 类型注册成 `type+1000`（`syscall_context.cpp:514-519`），只有 1001/1002/1004/1006 有实现，其余（如 RINGBUF→1027）会落入 §7-1 的"impl 为空"状态（推断）。

## 10. 未读完/未确定的地方

- `map_handler.cpp` 中 `BPFTIME_ENABLE_CUDA_ATTACH` 的 GPU 分支（约 `:219-256,379-415,529-565,694-730,949-1119,1227-1257,1286-1374`）只扫读，未分析（R6 范围）。
- `test_epoll.cpp`、`test_software_perf_event.cpp`、`test_bpftime_shm_json.cpp`、`test_shm_allocation_failure.cpp` 只读了与 map 相关的段落；`test_lpm_trie_map.cpp`、`test_bloom_filter_map.cpp`、`test_stack_map.cpp` 中重复性强的统计/断言段是快速通读，没有逐断言核对。
- 未实跑：§7-1（map_init 失败后崩溃）、§7-2（dup 锁不共享的并发后果）、§7-6（stack_trace syscall 越界读）、§7-9（LPM 跨进程 update）、§7-17（ARRAY_OF_MAPS syscall lookup）、§7-18（prog_array fd 泄漏）、§7-19~24（kernel_user 系列，需要 root 和内核 map）、`tailcall/test_user_to_kernel_tailcall`。
- 需要别的读者确认：
  - R1：`map_init` 里构造函数抛出的异常最终在哪里被捕获（`bpftime_maps_create` → `add_bpf_map` → `set_handler` 链上是否有 try/catch）；`set_handler` 忽略 `map_init` 返回值是否有意。
  - syscall-server 读者：libbpf `perf_buffer__new` 填 PERF_EVENT_ARRAY 的具体路径；`run_with_kernel` 模式下非 1001/1002/1004/1006 类型的行为；ARRAY_OF_MAPS 的 value（内层 fd）在 syscall 层是否做过 fd 翻译。
  - helper 读者：`bpf_ringbuf_query`、`bpf_map_lookup_percpu_elem`、`bpf_for_each_map_elem` 等是否存在；helper 返回值 `-1` vs `-errno` 的整体约定。
  - daemon 读者：daemon 路径下 KERNEL_USER map 被哪个进程 `map_init`（决定 perf kernel_user 的 perf event 绑在哪个进程、fd/mmap 存进 shm 的是谁的）。
- 内核侧对照（array `elem_size` 8 字节对齐、bloom 默认 5 个哈希、kernel ringbuf 头 pg_off）来自对内核源码的已有知识，未在本机内核源码上逐行核对。
