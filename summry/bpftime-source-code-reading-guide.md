# bpftime 源码阅读指南

针对 `~/src/bpftime-offical-no-btf`（分支 codex/official-no-btf，行号基于 2026-07-27
commit `076e3e4` 之后的代码）。读者定位：已深入研究过 ssl-nginx benchmark 热路径
（uprobe → `bpf_perf_event_output` → software perf buffer per-thread shard → 消费者），
熟悉 `bpf_helper.cpp` 与 `perf_event_handler.cpp` 的相关段落。本指南以这条热路径为
锚点向外扩展：**先纵向跟完一个事件的完整生命周期，再横向逐子系统展开**。

## 阅读策略

1. 你手里有全套可运行的 benchmark 环境——**验证式阅读**远快于干读：每读一段就用
   `bpftimetool export` 看 shm、加一行 SPDLOG_INFO 重编跑一轮 smoke、或写个最小
   复现。本文每章末尾的"验证性问题"就是为此设计的：读完答不上来 = 漏了关键代码。
2. 全局主线一天可以走完（第一部分）；六个子系统按依赖顺序每个半天到一天。
3. 行号会漂移，用函数名 grep 定位；`.hpp` 先于 `.cpp`。

---

## 第一部分：一个事件的完整生命周期（主线，必读）

以 benchmark 的真实场景串起全部核心代码。你已经从第 11 步开始很熟了——这次从头走。

**控制面（sslsniff loader 进程，LD_PRELOAD syscall-server）**

1. **符号拦截**：`runtime/syscall-server/syscall_server_main.cpp:214` 的 `syscall()`
   ——拦截只是 LD_PRELOAD 符号覆盖 + `dlsym(RTLD_NEXT)` 兜底，不是 seccomp/ptrace。
   分流 `__NR_bpf` / `__NR_perf_event_open` 等。
2. **shm 创建**：`syscall_server_utils.cpp:42` `start_up()` 以 `SHM_CREATE_OR_OPEN`
   建全局 shm（默认名 `bpftime_maps_shm`，50MB，boost::interprocess）。
3. **对象诞生**：`syscall_context.cpp:373` `handle_sysbpf()`——`BPF_MAP_CREATE` →
   `bpftime_maps_create` → `handler_manager::set_handler` → `map_init`（**返回的
   "fd" 是 shm 中 handler vector 的槽下标，假 fd**）；`BPF_PROG_LOAD` →（可选
   PREVAIL verifier，`syscall_context.cpp:549`）→ shm 里只存指令字节。
4. **perf 对象**：`syscall_context.cpp:697` `handle_perfevent()`——
   `PERF_TYPE_SOFTWARE` → `bpftime_add_software_perf_event`（你修过对齐的那个
   buffer 就在这里诞生）；uprobe 类型 → uprobe perf handler。
5. **连线**：`BPF_LINK_CREATE`/`ioctl(PERF_EVENT_IOC_ENABLE)` → shm 里写 link
   handler（prog_id ↔ attach_target_id 两个假 fd 的连线）。
6. **消费者就位**：sslsniff 里 libbpf `mmap(perf fd)` → `handle_mmap64`
   (`syscall_context.cpp:840`) → 直接返回 shm 中 perf buffer 页；`epoll_*` 系列
   同样被拦，支撑 `perf_buffer__poll`。

**数据面（nginx 进程，LD_PRELOAD agent）**

7. **agent 启动**：`runtime/agent/agent.cpp:457` `__libc_start_main` 劫持 →
   `bpftime_agent_main` (L738) → shm `SHM_OPEN_ONLY`（重试 60 次）→
   `register_attach_impl`×3（frida/syscall_trace/cuda）。
8. **实例化**：`runtime/src/attach/bpf_attach_ctx.cpp:59`
   `init_attach_ctx_from_handlers` 遍历 shm handler 表 →
   `instantiate_prog_handler_at`：从 shm 指令字节**本地**构造 `bpftime_prog`（VM
   不在 shm 里！），注册三组 helper（`bpf_attach_ctx.cpp:44-54` →
   `bpf_helper.cpp:1129`）。
9. **JIT**：`bpftime_prog_load` (`bpftime_prog.cpp:169`) → `ebpf_compile` → LLVM
   OrcJIT（`vm/llvm-jit/`）。**helper 分发没有运行期查表**：`call 25` 在编译期直接
   变成符号 `_bpf_helper_ext_0025`，由 absoluteSymbols 绑到 C++ 函数指针——从 JIT
   代码到 `bpf_perf_event_output` 是零间接层。
10. **挂钩**：`attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:173`
    `create_attach_with_ebpf_callback` → `attach_at` → `gum_interceptor_attach`
    ——Frida 改写 `SSL_read`/`SSL_write` 的函数序言指向 trampoline。

**热路径（每个请求 2 次）**

11. **触发**：`SSL_read` 返回 → trampoline 保存 `GumCpuContext` →
    `frida_internal_attach_entry.cpp:315` `uprobe_listener_on_leave`（uretprobe）→
    `frida_register_conversion.cpp` 转成**栈上 `pt_regs` 副本**（单向拷贝，BPF 改
    ctx 不回写）→ `run_callback` → `bpftime_prog_exec` (`bpftime_prog.cpp:231`) →
    JIT 出的 `bpf_main(mem, size)` 直接跑本机码。
12. **输出**：`bpf_helper.cpp:494` `bpf_perf_event_output`（`my_sched_getcpu` 快照
    → perf_event_array_map lookup: cpu→perf handler fd → `bpftime_perf_event_output`
    → `perf_event_handler.cpp:601` per-(pid,tid) shard 写入，8 字节对齐 record）。
13. **消费**：sslsniff `perf_buffer__poll` → 被拦截的 `epoll_wait` → drain：
    `copy_next_record_to` 把 shard 记录搬进 consumer buffer → libbpf 按
    `header.size` 推进 → `print_event`。

走完这 13 步，架构骨架即完整。下面横向展开。

---

## 第二部分：六个子系统

### 1. runtime 核心：共享内存与 handler 注册表（"内核替身"）

一切的中心是 shm 里一个定长（默认 6144 槽）的
`boost vector<handler_variant>`——**下标即假 fd，variant 类型即 fd 种类**。

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `runtime/src/handler/handler_manager.hpp` | 133 | handler_variant 的 7 个成员、5 个 shm 命名对象常量（先读，全读） |
| `runtime/src/bpftime_shm_internal.cpp` | 1159 | **L640 构造函数是全子系统最密的 100 行**：三种 shm_open_type 分支 = server 建 / agent 开的协作 |
| `runtime/src/bpftime_shm_internal.hpp` | 306 | `class bpftime_shm` 当 API 目录读；`is_*_fd` 系列是 fd 判型层 |
| `runtime/src/bpftime_shm.cpp` | 714 | 纯 C ABI 转发层，扫一遍即可 |
| `handler/{prog,link,map}_handler.hpp` | ~420 | "shm 只放数据、VM 在各进程本地"的分界 |
| `runtime/src/bpftime_config.cpp` | 172 | BPFTIME_* 环境变量 → agent_config |

坑：
- shm 里只有可跨进程的数据（boost 容器 + offset_ptr）；JIT 代码、`bpftime_prog`
  都是各进程从指令字节私有重建的——**改 shm 里的 prog 不影响已实例化的 agent**。
- `shm_holder` 是 union 包裹 + placement new，为绕过静态析构顺序；
  `bpftime_destroy_global_shm` 只析构本进程对象，**删系统 shm 的是
  `bpftime_remove_global_shm`**（即 `bpftimetool remove`——你的清理协议的原理）。
- server 用 `SHM_CREATE_OR_OPEN` 而非删了重建，是为了让已注入的 agent 保持映射；
  配套 epoch seqlock（奇数=server 正在 reset）。**这正是你踩过的"shm 残留"双峰
  假象的结构性根源。**
- `clear_id_at` 会级联删除引用该 perf/prog 的 link（O(n) 线性扫全表）。
- handler 注册表本身**没有全局锁**：假定写方基本只有 server。

验证问题：sslsniff 传给 helper 的 map fd 如何从整数变成 shm 缓冲区指针？server
重启会话后 agent 为何不用重新 mmap？agent 退出时 `__destruct_shm` 为何要检查
`SHM_OPEN_ONLY`？

### 2. agent 与 syscall-server（两个 .so 的入口薄层）

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `runtime/syscall-server/syscall_server_main.cpp` | 276 | 全读：syscall() 分流表 |
| `runtime/syscall-server/syscall_context.cpp` | 1097 | `handle_sysbpf`(L373)、`handle_perfevent`(L697)、`handle_mmap64`(L840) |
| `runtime/syscall-server/syscall_server_utils.cpp` | 228 | `start_up`；uprobe perf type 的 mock |
| `runtime/agent/agent.cpp` | 1030 | 只看主线：shm 打开→register_attach_impl→init_attach_ctx；IPC/refresh 可略 |
| 两个 `CMakeLists.txt` | — | 确认 ".so = 薄入口 + 整个 runtime 静态库" |

坑：
- **两个 .so 各链一份完整 runtime 静态库**——同一段 helper/map 代码在两个进程各有
  一份，真正共享的只有 shm 数据。（这就是为什么你的消融只需换 agent .so。）
- agent 有三种进入方式（LD_PRELOAD / Frida 注入 / text-transformer），且有一套防
  重入 IPC（abstract unix socket `bpftime-agent-<pid>`）——读主流程时容易被绕晕，先跳过。
- `init_original_functions` 里 `unsetenv("LD_PRELOAD")`：**子进程不继承注入**——
  调试 fork/exec 场景必记。
- 每个 `handle_*` 都有 `run_with_kernel`（BPFTIME_RUN_WITH_KERNEL）分支——读主线时
  一律先忽略。
- 返回给 libbpf 的假 fd 会用 `open_fake_fd()` dup 占位，让号码在进程 fd 表里也被占住。

验证问题：loader `mmap(perf fd)` 拿到的内存来自哪里？agent 为何要重试 60 次开 shm？
SIGUSR1 的 detach 为什么要经过 pipe 而不在信号处理函数里直接做？

### 3. attach/：事件挂载层（Frida uprobe）

runtime 与所有 attach 实现之间唯一的契约：`ebpf_run_callback` 三参签名
`(memory, size, ret)`（`base_attach_impl.hpp:29`，110 行先读完）。

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `frida_uprobe_attach_impl.hpp` | 111 | 两层实体：`frida_attach_entry`（一个 attach id 一条）与 `frida_internal_attach_entry`（一个函数地址一条，多对一） |
| `frida_internal_attach_entry.cpp` | 344 | **热路径核心**：`uprobe_listener_on_enter`(L294)/`on_leave`(L315)/override_handler |
| `frida_uprobe_attach_impl.cpp` | 331 | attach 建立：module 校验、entry 复用、UREPLACE 降级为 OVERRIDE+wrapper |
| `frida_register_conversion.cpp` | 110 | GumCpuContext↔pt_regs，aarch64 分支（你的平台） |
| `syscall_trace_attach_impl.cpp` | 201 | 对比读：同一契约、不同 ctx（依赖 text_segment_transformer 而非 Frida） |

坑：
- **`frida_uprobe_attach_impl.hpp:20-25` 的 UPROBE/URETPROBE 注释写反了**，以代码
  为准（on_enter 跑 UPROBE、on_leave 跑 URETPROBE）。
- BPF 拿到的 ctx 是栈上 pt_regs **副本**：从不回写，BPF 改寄存器不影响被 hook 函数；
  普通 uprobe 的 BPF 返回值也被丢弃。
- uprobe（`gum_interceptor_attach`，listener）与 override/replace
  （`gum_interceptor_replace`，换函数入口）是 Frida 两种互斥机制。
- `current_thread_gum_cpu_context` 只在 on_enter/override 设置——**uretprobe 里取栈
  会失败**。
- nv_attach_impl（CUDA）占了目录近半代码量，与 uprobe 无关，先跳过。

验证问题：sslsniff 的 `pt_regs *ctx` 在哪行构造、生命周期？同一地址挂 3 uprobe +
1 uretprobe 时三种实体各几个？`bpf_override_return` 的"不执行原函数"经过哪几步？

### 4. vm/：执行引擎（LLVM JIT）

三层：vm-core C API（薄壳）→ compat 抽象基类+工厂（`"llvm"`/`"ubpf"` 按名注册）→
llvm-jit（独立库 llvmbpf 内嵌版）。

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `vm/compat/include/bpftime_vm_compat.hpp` | 265 | **枢纽，先读**：虚接口+工厂表+`struct ebpf_vm` 真实定义 |
| `vm/vm-core/{ebpf-vm.h,ebpf-vm.cpp}` | 340 | 确认 C API 只是转发；`ebpf_jit_fn(mem, len)` 即 ctx 传递方式 |
| `vm/llvm-jit/include/llvmbpf.hpp` + `src/vm.cpp` | 276 | `ext_funcs` 表、`jitted_function` 缓存、"没编译先编译"语义 |
| `vm/llvm-jit/src/compiler_utils.cpp` | 482 | `emitExtFuncCall`(L309)：helper 编号→符号 `_bpf_helper_ext_%04d` |
| `vm/llvm-jit/src/llvm_jit_context.cpp` | 814 | L505-610 符号绑定段：absoluteSymbols 绑 helper 函数指针 |
| `vm/llvm-jit/src/compiler.cpp` | 1376 | 按需：block 划分、寄存器 alloca、512B 栈、lddw 伪指令 |

坑：
- **helper 必须在 compile 前全部 register**：编号在编译期固化为符号，之后改
  `ext_funcs` 不生效。
- llvm 后端**没有真解释器**：`exec()` 是惰性 JIT + 缓存；真解释器只有 ubpf。
- helper 统一按 5×i64 原型发射（依赖调用约定容忍多传参）；`bpf_tail_call`
  (imm==12) 是硬编码特例，call 后直接 br 到 exit。
- bpf-to-bpf 子函数调用不走本机栈：自建 callStack alloca，整个程序 JIT 成单个
  `bpf_main`。

验证问题：`call 25` 到 `bpf_perf_event_output` 中间有几层间接？（答：零层）
compile 后再 register 新 helper 有效吗？JIT `bpf_main` 的两个参数在 uprobe 场景
对应什么？

### 5. bpf_map/ 与 bpf_helper.cpp（map 实现与 helper 注册）

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `bpf_map/map_common_def.hpp` | 103 | 先读：`bytes_vec`、`ensure_on_current_cpu`（快照，便宜）vs `ensure_on_certain_cpu`（真迁移，昂贵） |
| `userspace/per_cpu_array_map.cpp` + `per_cpu_hash_map.cpp` | ~360 | per-CPU 用户态模拟的两种范式；`elem_*` vs `elem_*_userspace` 双接口 |
| `bpf_helper.cpp` | 1400 | 注册骨架 L1095-1369（helper_group 三大组）；你的锚点 `bpf_perf_event_output` L494 |
| `userspace/perf_event_array_map.cpp` | 83 | key=cpu → value=perf handler fd |
| `shared/perf_event_array_kernel_user.cpp` | 525 | 内核共享路径：user_ringbuffer + 动态生成的内核"搬运"程序 |
| `bpftime_probe_read`（bpf_helper.cpp L126-299） | — | SIGSEGV+改写 ucontext PC 的容错——**③段成本的来源候选** |

坑（多条与你的 benchmark 结论直接相关）：
- **per-CPU map 并不真正 per-CPU 隔离**：只是 getcpu 快照选 slot，迁移后两线程可写
  同一 slot；"正确性"依赖内核关抢占假设在用户态本就不成立——这正是删绑核序列的
  正确性论证背景。
- `elem_*`（helper 视角，单 CPU 切片）与 `elem_*_userspace`（syscall 视角，一次全部
  ncpu 份）语义不同，混淆会误读 value 大小。
- `per_cpu_hash_map::elem_delete` 只清零本 cpu 切片不删条目，**且清零范围疑似有
  bug**（fill 的是 `[begin, begin+cpu*value_size)`）；per_cpu_array 完全不支持 delete。
- `bpftime_tail_call` 每次调用都重新 new prog + 重注册 helper + 重 load（可能重
  JIT）——与内核 O(1) 跳转成本完全不同，若哪天 benchmark 带 tail call 要先看这里。
- `ENABLE_PROBE_READ_CHECK` 打开时 probe_read 每次 1-2 个 sigaction syscall——
  **这是 ③段 +3.7pp 的头号嫌疑，值得做下一轮消融**。
- `bpf_probe_read_str` 只是 strncpy，不返回内核语义的长度，已知偏差。

验证问题：`call 25` 从 VM 到 perf_event_handler 经过哪几层、map 类型在哪步分流
userspace/kernel-shared？关掉 `enable_shm_maps_helper_group` sslsniff 会死在哪个
helper？probe_read 的 SIGSEGV 恢复在 aarch64 改写哪个寄存器？

### 6. 外围：daemon / verifier / tools / 单测

| 部件 | 关键文件 | 一句话 |
|---|---|---|
| daemon | `daemon/kernel/bpf_tracer.bpf.c`(732L)、`daemon/user/{bpf_tracer,handle_bpf_event,bpftime_driver}.cpp` | 内核 eBPF tracer 监控目标进程的 bpf()/perf_event_open()，用 `bpf_probe_write_user` 改写 uprobe 路径实现"透明劫持内核 eBPF 到用户态"；`relocate_bpf_prog_insns` 做 map fd→shm id 重定位 |
| verifier | `bpftime-verifier/src/{bpftime-verifier,platform-impl}.cpp` | PREVAIL 薄封装；**配置全是 thread_local**，跨线程校验前必须重新 set_*；只支持 uprobe/tracepoint + hash/array/ringbuf |
| tools | `tools/cli/main.cpp`(1275L) vs `tools/bpftimetool/main.cpp`(380L) | **cli 是注入器**（fork+LD_PRELOAD 或 Frida 注入），**bpftimetool 是 shm 直操作器**（不注入）——你天天用的 `remove` 就是它调 `bpftime_remove_global_shm` |
| 单测 | `runtime/unit-test/CMakeLists.txt` | Catch2 v3；`bpftime_runtime_tests` 可按 tag 过滤（如 `"[software_perf_event]"`）；attach 类测试要先 `make -C runtime/test/bpf` |

坑：daemon 有**两层 eBPF**（自己的内核 tracer vs 被劫持的目标程序），别混淆；
`daemon/test/test_daemon.cpp` 的 TEST_CASE 是空的，真测试在 test_daemon_driver.cpp。

---

## 第三部分：建议的整体节奏与实验

**节奏**（按依赖序）：
第 1 天主线 13 步 → 子系统 1（shm，半天）→ 子系统 2（入口层，半天）→
子系统 3（attach，1 天）→ 子系统 4（vm，1 天）→ 子系统 5（maps/helpers，半天，
大半你已熟）→ 子系统 6（按需）。

**边读边做的实验**（利用现有环境）：
1. 读 shm 章时：跑一次 benchmark 不 remove，`bpftimetool export` dump 出 handler
   表对照 handler_variant 逐槽认类型。
2. 读 JIT 章时：`bpftime_prog_load` 后 dump JIT 符号表，确认
   `_bpf_helper_ext_0025` 的绑定地址就是 `bpf_perf_event_output`。
3. 读 attach 章时：给 `uprobe_listener_on_enter/on_leave` 各加一个计数器重编 agent，
   跑 16B 短测核对 2.001 事件/请求的另一半（enter 侧计数）。
4. 读 probe_read 时：确认 `ENABLE_PROBE_READ_CHECK` 在你的构建里是否打开
   （`grep -r PROBE_READ_CHECK build-alignment-test/CMakeCache.txt`），
   若开着，做一个关掉它的变体消融——直接回答 ③段 +3.7pp 的来源。

**已知的代码-注释不一致清单**（阅读时以代码为准）：
- `frida_uprobe_attach_impl.hpp:20-25`：UPROBE/URETPROBE 注释写反。
- `llvmbpf_vm::exec` 自称 "interpreter mode"，实为惰性 JIT。
- `per_cpu_hash_map` 注释称"sharable lock 保证线程安全"，实际 `should_lock=false`。

## 更新记录

- 2026-07-27：首版。基于 6 个并行子系统测绘（workflow `wf_d1a3151d-5b1`），
  行号对应 commit `076e3e4`。
