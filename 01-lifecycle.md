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
[← 返回目录](README.md)
