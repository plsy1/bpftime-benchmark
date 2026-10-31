# X 跨模块交叉核对（汇总者亲自查源码裁定）

基线 `177a1dc465ff`。每条：问题来源 → 结论 → 证据。

## 已裁定

1. **bpf_attach_ctx 的 attach/detach 有没有锁**（R5 问）
   有。`ctx_mutex`（`runtime/include/bpf_attach_ctx.hpp:200`）在
   `init_attach_ctx_from_handlers`（`bpf_attach_ctx.cpp:76`）、`destroy_instantiated_attach_link`（`:513`）、
   `destroy_all_attach_links`（`:544`）、`reset_instantiated_state`（`:568`）里加锁。
   所以经 runtime 走的 attach/detach 是串行的；Frida / syscall_trace 后端自身无锁，只在绕过 runtime
   直接调用 attach impl 时才会暴露。热路径（回调执行）不持这把锁。

2. **`BPFTIME_BUILD_WITH_LIBBPF` 默认值与作用**（R5 问）
   默认 ON（`cmake/StandardSettings.cmake:107`），定义宏于 `CMakeLists.txt:104-105`。
   agent 里 syscall_trace impl 的 include、注册、析构都包在 `#if __linux__ && BPFTIME_BUILD_WITH_LIBBPF`
   里（`runtime/agent/agent.cpp:57,140,964,1058`）。

3. **`current_thread_bpf_cookie` 是否复位**（R5 问）
   不复位，但每次 runtime 包装的回调执行前都会重新赋值（`bpf_attach_ctx.cpp:398-402`；
   信号上下文里写到 `current_signal_callback->cookie`）。读取点 `bpftime_shm.cpp:264`。
   结论：runtime 管理的 link 不会读到别的 link 的 cookie；只有不经该包装的回调才可能读到残留值。

4. **CUDA attach type 8/9 与 KPROBE/KRETPROBE 同号**（R5 问）
   是同号且**有意复用**：`ATTACH_CUDA_PROBE = 8 / ATTACH_CUDA_RETPROBE = 9`
   （`attach/nv_attach_impl/nv_attach_impl.hpp:43-44`）对应 `BPF_TYPE_KPROBE = 8 / KRETPROBE = 9`
   （`runtime/include/bpftime_shm.hpp:59-60`）。kprobe 类型的 perf handler 按 `func_name` 生成 private data
   （`bpf_attach_ctx.cpp:473-486`），交给注册在 type 8/9 上的 attach impl——也就是 nv_attach_impl。
   即：在 bpftime 里"kprobe"实际表示"CUDA kernel 探针"，CPU 侧没有 kprobe 实现。写作时要讲清。

5. **map_init 失败 / 构造函数抛异常在哪被接住**（R2 问）
   - `set_handler` 忽略 `map_init` 返回值（`handler_manager.cpp:81-86`），返回 -1 的情形（参数非法）
     会留下"有 fd 无实现"的 map（R1 已实跑）。
   - `map_init`（`map_handler.cpp:762` 起）内部除一处 `bad_alloc` 的 catch-rethrow 外不捕获异常；
     实现类构造函数抛的异常会穿过 `set_handler`，在 syscall-server 由 `handle_exceptions` 接住并
     **回退到真内核的 bpf(2)**（fail-open，见 R3）。所以同是"参数非法"，有的 map 类型得到一个坏的假 fd，
     有的会被转去真内核创建。

6. **perf handler 的 `enabled` 标志有没有人读**（R1 问）
   没有。只在 `perf_event_handler.hpp:193-205` 设置、在 json 导入导出里读写（`bpftime_shm_json.cpp:200,333`）。
   agent / attach 路径不读。`PERF_EVENT_IOC_DISABLE` 对已挂上的程序无效。（CUDA 里的 `enabled` 是另一个字段。）

7. **`should_lock` 在 handler 层的用法**（R6 问）
   `map_handler.cpp` 每个操作分发前 `if (impl->should_lock)` 才加该 handler 的 `map_lock`
   （`:113,121,274,283,439,589,597,1390,1422,1449` 等）。should_lock=false 的实现（GPU hash 等）完全无锁。
   结合 R2：锁在 handler（每个 fd）上，dup 出的 fd 各有一把。

8. **queue/stack 有没有内部锁**（R8 问）
   没有。`queue.hpp:58-59`、`stack.hpp:59-60` 声明 `should_lock = true`，"Map handler should lock externally"。
   单测直接调 impl 并在外面自己加锁，所以测不到 handler 层的并发路径。

9. **PERF_EVENT_IOC_SET_BPF 的返回值**（R1 问）
   确认返回 `bpftime_attach_perf_to_bpf` 的结果（新建 link 的 fd），内核语义是返回 0
   （`runtime/syscall-server/syscall_context.cpp:1051-1061`）。libbpf 只检查 `<0`，所以功能上无碍，
   但这个 link fd 没有人关闭。

10. **普通 LD_PRELOAD agent 里的 syscall trace link 会不会触发**（R3 问）
    不会。`_bpftime__setup_syscall_trace_callback`（`agent.cpp:1200`）只被 text_segment_transformer 调用
    （`attach/text_segment_transformer/agent-transformer.cpp:90`）。不经 `bpftime start -s`（transformer）启动时，
    syscall trace 类型的 link 能实例化成功但永不触发。

11. **agent 日志何时切到 loader 配置的路径**（R8 问）
    agent 打开 shm、拿到 runtime_config 后立即 `bpftime_set_logger(runtime_config.get_logger_output_path())`
    （`agent.cpp:956-957`），在 `ctx_holder.init()` 之前。所以 agent 启动最初几行日志按 agent 自己的环境变量，
    之后全部按 loader 的配置。

12. **ufunc helper group 默认开还是关**（R1 与第 0 章改写稿冲突）
    两层都对：结构体默认值 ufunc=false（`bpftime_config.hpp`），但 `construct_runtime_config_from_env`
    在未设置 `BPFTIME_HELPER_GROUPS` 时三组全开（`bpftime_config.cpp:100-104`，R1/R4 均确认）。
    实际经 syscall-server 启动时走后者 → 三组全开。

13. **syscall-server 给 verifier 的 helper 列表里 ufunc 分支加错组**（R3、R7 都报）
    两人独立确认同一处（`syscall_server_utils.cpp:200-206`），只影响开启 verifier 的构建。

14. **`bpftime_prog` 第二个构造函数改写全局 runtime config**（R1 问、R4 确认）
    确认（`bpftime_prog.cpp:140`）。

## 交给阶段四（核查者）或无法在本机确认

- R5：transformer 在 aarch64 上能否构建（需 Jetson）；系统调用跳板的寄存器/red zone/rt_sigreturn 疑点（需 root 设 mmap_min_addr=0）。
- R6：所有标「推断」的 GPU 行为（需 GPU，可交 Jetson）。
- R7：daemon 整链（需 root）。
- R8：单测默认顺序 SIGSEGV 在 CI 的 Debug 构建上是否复现。
- R2：ARRAY_OF_MAPS 在 syscall 层 lookup 的 fd 翻译、run_with_kernel 的 type+1000 映射——写作时按 R2 笔记标「推断」。
