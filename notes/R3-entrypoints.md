# R3 入口层（syscall-server / agent / bpf_attach_ctx / CLI）源码笔记

基线 `177a1dc465ff`，仓库根 = `/home/y/src/bpftime-wt-official-177a1dc`。下文路径均相对仓库根。
实验程序与日志在 `/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R3/`。
`build-jit` 配置：`BPFTIME_LLVM_JIT=ON`、`BPFTIME_ENABLE_FRIDA=ON`、`BPFTIME_BUILD_WITH_LIBBPF=ON`、
`BPFTIME_ENABLE_CUDA_ATTACH=OFF`、`ENABLE_EBPF_VERIFIER=OFF`、`CMAKE_BUILD_TYPE=RelWithDebInfo`、
`SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO`（所以 `SPDLOG_DEBUG` 宏在此构建里被编译掉）。CLI 二进制 `bpftime`
在 build-jit 中**没有构建**（`build-jit/tools/cli/` 只有 CMake 文件），CLI 相关行为全部只读代码、未实跑。

## 0. 一句话职责 + 范围

本块是 bpftime 的"入口层"：**syscall-server**（LD_PRELOAD 进 loader，把 libbpf 的 bpf/perf_event_open/
mmap/epoll/ioctl/若干文件操作改写成对 shm handler 表的读写）、**agent**（LD_PRELOAD 或 Frida 注入进目标进程，
打开 shm、按 handler 表在本进程实例化 prog/perf event/link 并交给 attach impl 挂钩，提供 SIGUSR1/IPC 卸载与刷新）、
**bpf_attach_ctx**（agent 侧"按表实例化 + 装配 attach"的核心类）、**CLI**（`bpftime load/start/attach/detach/trace`，
本质是拼环境变量 + fork/exec 或 Frida 注入 + IPC/信号）。

读了的目录/文件：
- `runtime/syscall-server/` 全部（9 个文件，2490 行）
- `runtime/agent/` 全部（3 个文件，1313 行）
- `runtime/src/attach/` 全部（3 个文件，1632 行）+ 其头文件 `runtime/include/bpf_attach_ctx.hpp`（265 行）
- `tools/cli/` 全部（2 个文件，1317 行）
- 根目录 `CLAUDE.md`（179 行，契约在 :147-163）、`usage.md`（258 行）
- 相关测试（见 §1 末尾表）
范围内源码合计 6752 + 265（头文件）= 7017 行；文档 437 行。

为对照/核实而额外读过的范围外代码（只读相关函数）：`runtime/src/bpftime_shm_internal.{hpp,cpp}`（shm_lifecycle_lock、
epoch、pid 集合、open_fake_fd、reset_server_state）、`runtime/src/bpftime_config.cpp`（env 解析）、
`runtime/src/bpf_helper.cpp:58-65,595,784`（`get_global_attach_ctx` 弱符号及使用点）、
`runtime/src/handler/map_handler.cpp:950-1051`（`set_enable_mock` 调用点）、
`attach/text_segment_transformer/agent-transformer.cpp`（`start -s` 路径）、
`attach/frida_uprobe_attach_impl/src/frida_attach_private_data.cpp:10-40`、
`attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp:129-166`（throw 点）、
`cmake/StandardSettings.cmake:76-82`、`.github/workflows/test-runtime.yml:235-252`。

## 1. 文件清单

| 文件 | 行数 | 做什么（一句话） | 必读? |
|---|---|---|---|
| `runtime/syscall-server/syscall_server_main.cpp` | 551 | 所有被 LD_PRELOAD 覆盖的 libc 符号 + ctx 懒初始化状态机 + 异常兜底包装 + `syscall()` 按号分流 | 必读 |
| `runtime/syscall-server/syscall_context.hpp` | 198 | `syscall_context` 类：原始函数指针、3 个 mock 开关、mock 文件表、`init_original_functions` | 必读 |
| `runtime/syscall-server/syscall_context.cpp` | 1238 | 各 `handle_*`：bpf cmd 大 switch、perf_event_open 分流、mmap/ioctl/epoll/open/read/fopen/dup3/memfd mock、kernel 混合模式 | 必读 |
| `runtime/syscall-server/syscall_server_utils.cpp` | 352 | `start_up`（建/开 shm、会话、atexit 清理）、/sys 4 个 mock 文件、perf type 探测 | 必读 |
| `runtime/syscall-server/syscall_server_utils.hpp` | 30 | 上述工具函数声明 + `PERF_UPROBE_REF_CTR_OFFSET_SHIFT=32` | 选读 |
| `runtime/syscall-server/CMakeLists.txt` | 66 | 共享库 `bpftime-syscall-server`，链 runtime 静态库、version script、可选 verifier | 选读 |
| `runtime/syscall-server/syscall-server.version` | 4 | 导出符号白名单（其余全 local） | 必读（短） |
| `runtime/syscall-server/README.md` | 44 | 旧用法说明（LD_PRELOAD、RUN_WITH_KERNEL、NOT_LOAD_PATTERN） | 选读 |
| `runtime/syscall-server/.gitignore` | 7 | 忽略规则 | 跳过 |
| `runtime/agent/agent.cpp` | 1213 | agent 全部：`__libc_start_main` 劫持、`bpftime_agent_main`、防重入、IPC server、SIGUSR1→管道→worker、auto-refresh、syscall trace 回调 | 必读 |
| `runtime/agent/agent.version` | 4 | 导出符号白名单 | 必读（短） |
| `runtime/agent/CMakeLists.txt` | 96 | 共享库 `bpftime-agent`，按开关链 Frida/trap/syscall_trace/nv attach impl | 选读 |
| `runtime/include/bpf_attach_ctx.hpp` | 265 | `bpf_attach_ctx` 类定义 + CUDA 通信结构（`CommSharedMem` 等） | 必读 |
| `runtime/src/attach/bpf_attach_ctx.cpp` | 581 | 扫表（epoch 校验）、递归实例化 prog/perf/link、注册 attach impl、detach/reset | 必读 |
| `runtime/src/attach/bpf_attach_ctx_cuda.cpp` | 429 | 仅 CUDA 构建：CUDA watcher 线程（GPU→host helper 代理）、map_basic_info、CUDAContext | 选读（CUDA 时必读） |
| `runtime/src/attach/demo_ptx_prog.hpp` | 622 | 一段硬编码 PTX 字符串，**仓库中无任何引用**（死代码） | 跳过 |
| `tools/cli/main.cpp` | 1290 | `bpftime` CLI：load/start/attach/detach/trace 五个子命令 | 必读 |
| `tools/cli/CMakeLists.txt` | 27 | 可执行 `bpftime`，链 frida-core、runtime；install 到 `~/.bpftime` | 选读 |
| `CLAUDE.md` | 179 | 契约在 :147-163（与 `AGENTS.md` 对应段逐字相同，已 diff） | 必读契约段 |
| `usage.md` | 258 | 用户手册：load/start/attach、`start -s`、env 配置 | 选读 |

相关测试（grep `syscall_server|syscall-server|bpftime-agent|bpf_attach_ctx|agent_main|SYSCALL_SERVER_LIBRARY` 得到）：

| 测试文件 | 行数 | 测了什么 | 编进测试? |
|---|---|---|---|
| `runtime/unit-test/test_shm_allocation_failure.cpp` | 512 | 13 个用例：syscall-server shm 不够时 fail-open、perf mmap 报 ENOMEM、默认日志不碰 stdio、文件 logger 早于 startup、owner 退出删段/非 owner 不删/有活 agent 或活 server 时不删、mock 文件不留 tmp；agent 在无 shm 时不碰 stdio 且 SIGUSR1 保持 SIG_DFL | 是（`runtime/unit-test/CMakeLists.txt:39`） |
| `runtime/unit-test/assets/shm_allocation_test_helper.c` | 233 | 上面用例的被测宿主程序（startup/startup-ok/startup-wait/mock-tmp/perf-mmap/check-sigusr1 六种模式） | 是（CMake :84） |
| `runtime/unit-test/assets/runtime_options_test_helper.c` | 22 | 发一个非法 SOCKET_FILTER prog（名 `bypass_me`），供 CI 测 `BPFTIME_RUN_WITH_KERNEL`+`BPFTIME_NOT_LOAD_PATTERN`（`.github/workflows/test-runtime.yml:235-252`，需 root） | 是（CMake :86），但不在 Catch2 用例里 |
| `runtime/unit-test/attach_with_ebpf/test_shm_progs_attach.cpp` | 270 | 手工在 shm 建 prog/perf/link handler，`bpf_attach_ctx::init_attach_ctx_from_handlers(manager, config)` 后验证 replace/uprobe 生效，并在 fork 子进程里再实例化一遍 | 是（仅 FRIDA，CMake :54-60） |
| `runtime/unit-test/attach_with_ebpf/test_attach_uprobe_with_ebpf_trap.cpp` | 252 | :169-225 用 `bpf_attach_ctx`+trap impl 验证不安全 helper 的 link 被拒（不改代码字节）；:227-251 未 prepare 的 prog 在 trap 回调里返回 -ENOTSUP | 是（trap 仅 riscv 构建） |
| `runtime/unit-test/attach_with_ebpf/test_attach_replace.cpp` | 122 | 只构造 `bpf_attach_ctx` 用来注册 ufunc，直接用 frida impl 挂 override；不测 ctx 的扫表 | 是（仅 FRIDA） |
| `runtime/unit-test/test_bpftime_shm_json.cpp` | 107 | :104-105 json 导入后构造 `bpf_attach_ctx` 调 `init_attach_ctx_from_handlers(config)`（无断言，只测不崩） | 是 |
| `runtime/unit-test/test_get_stack.cpp` | 146 | :41-57 提供强符号 `get_global_attach_ctx()` 覆盖 runtime 弱符号，注册 mock attach impl（type 6） | 是 |
| `runtime/unit-test/cuda/test_cuda_compile.cpp` | 98 | :84-98 CUDA watcher 线程响应 flag1 并在 shm 销毁前停 | 仅 CUDA |
| `runtime/test/src/test_shm_client.cpp` | 41 | 旧式：打开 shm 后 `init_attach_ctx_from_handlers` | **否**：`runtime/test/CMakeLists.txt` 的 `test_sources` 全部注释掉 |
| `runtime/test/src/test_ufunc.cpp` | 46 | 旧 API `bpftime_probe_create_ctx()`（已不存在） | 否（同上） |

`build-jit/runtime/unit-test/bpftime_runtime_tests "[syscall_server],[agent]"` 已实跑：`All tests passed (87 assertions in 13 test cases)`。

## 2. 核心数据结构

### 2.1 syscall-server 侧

- **`ctx_initialized` / `tls_initializing`**（`runtime/syscall-server/syscall_server_main.cpp:35-36`）：进程级状态
  0 未初始化 / 1 构造中 / 2 完成 / 3 禁用；`__thread` 标志表示"本线程正在构造 ctx"。静态存储，永不重置。
- **`cached_next_symbol<Fn>`**（`syscall_server_main.cpp:91-109`）：每个被覆盖符号一份 `{atomic<Fn> value; atomic<bool> initialized}`，
  在 ctx 不可用时用 `dlsym(RTLD_NEXT, name)` 懒解析原函数（`:122-133`）；解析失败时 `call_next_symbol` 置 `errno=ENOSYS`
  返回失败值（`:135-146`）。静态存储。
- **`syscall_server_ctx_union context`**（`syscall_server_main.cpp:192-205`）：union 包 `syscall_context`，空构造/空析构；
  由 `initialize_ctx()` placement-new（`:220`），**永不析构**。
- **`class syscall_context`**（`runtime/syscall-server/syscall_context.hpp:64-196`），进程堆外的全局静态对象（见上）：
  - 原始函数指针 `orig_*_fn`（`:79-90`, `orig_syscall_fn` 在 `:164` 是 public）。由 `init_original_functions()`（`:96-133`）
    以 `dlsym(RTLD_NEXT)` 解析，缺任何一个（fopen 除外，可退到 fopen64）就抛 `std::runtime_error`；`orig_mmap64_fn` 直接取
    `"mmap"`（`:117-118`）；最后 `unsetenv("LD_PRELOAD")`（`:132`）。
  - `mocked_mmap_values`（`:92`）：`unordered_set<uintptr_t>`，记录"mmap 时直接返回的 shm 指针"，供 munmap 认出并吞掉。**无锁**。
  - `mocked_file_lock`（spinlock，`:93`）+ `mocked_files`（`:94-95`，fd → `unique_ptr<mocked_file_provider>`）：open/openat 打开 /sys
    mock 文件时登记，read 从内存 buf 供数据，close 摘除。
  - `run_with_kernel`（`:144`）、`by_pass_kernel_verifier_pattern`（`:148`）：由 `load_config_from_env()` 填（`syscall_context.cpp:177-194`）。
  - 三个开关（`:154-158`）：`enable_mock`（默认 true，构造期间和**每一次** `try_startup` 期间置 false）、`initializing_cuda`、
    `enable_mock_after_initialized`（默认 true；startup 失败置 false；也可被 shm 的 `mock_setter` 回调翻转）。
- **`mocked_file_provider`**（`syscall_context.hpp:40-62`）：`cursor`、`buf`、`access_lock`（spinlock）；`replacement_file` 字段
  全仓无人读写（死字段）。
- **常量**：`MOCKED_UPROBE_TYPE_VALUE=9`、`MOCKED_URETPROBE_BIT=0`、`MOCKED_KPROBE_TYPE_VALUE=8`、`MOCKED_KRETPROBE_BIT=0`
  （`syscall_context.hpp:34-38`）；4 个 /sys 路径（`runtime/syscall-server/syscall_server_utils.cpp:42-49`）。
- **startup 全局**（`syscall_server_utils.cpp:35-39`）：`g_startup_once`（`std::once_flag`）、`g_startup_exception`
  （首次失败的异常，之后每次 `start_up` 都重抛）、`g_syscall_server_shm_name`、`g_syscall_server_shm_owner_pid`、
  `g_syscall_server_owns_shm`。

### 2.2 agent 侧（`runtime/agent/agent.cpp`）

- `initialized`（`:68-73`）：`AGENT_UNINITIALIZED/INITIALIZING/READY`，`__atomic` 访问。detach 后**仍保持 READY**（`perform_detach`
  不改它，只有"ctx 未构造"分支才置 UNINITIALIZED，`:422-425`）。
- `orig_main_func`（`:66`）、`injected_with_frida`（`:81`，默认 true，`bpftime_hooked_main` 置 false `:538`；agent.cpp 内**无人读取**，
  只供 text-transformer 通过 dlsym 写入 `attach/text_segment_transformer/agent-transformer.cpp:82-88`）。
- auto-refresh：`auto_refresh_epoch`（atomic 代号，任何人 +1 即让当前线程退出）、`auto_refresh_thread_mutex`、
  `auto_refresh_thread`、`auto_refresh_loader_pid`（`:83-86`）。
- IPC：`agent_ipc_once`、`agent_ipc_stop`、`agent_ipc_fd`、`agent_ipc_thread`（`:88-93`）。
- detach：`detach_pipe_fds[2]`、`detach_thread_started`、`detach_mutex`、`global_ctx_constructed`（`:95-98`）。
- `bpf_attach_ctx_holder ctx_holder`（`:100-117`）：union，显式 `init()`/`destroy()`；`get_global_attach_ctx()`（`:119-122`）返回它
  （这是 runtime `bpf_helper.cpp:59` 弱符号的强定义，静态链接期绑定；该 C++ 符号不在 `agent.version` 导出）。
- `orig_hooker`（`:531`）：text-transformer 传进来的"原始 syscall hooker"。

### 2.3 `bpf_attach_ctx`（`runtime/include/bpf_attach_ctx.hpp:156-259`）

生命周期：agent 中是 `ctx_holder` 里的全局对象（`agent.cpp:960` 构造，`init_fail` 时 `destroy()`，正常路径永不析构）；单测里是栈对象。
关键字段：
- `ctx_mutex`（`:200`）：保护扫表/detach/reset（`init_attach_ctx_from_handlers` `bpf_attach_ctx.cpp:76`、`destroy_*` `:513,:544`、`reset` `:568`）。
  **`find_instantiated_prog`（`:190-195`）不加锁**。
- `last_epoch_seq_seen`（`:203`）：上次看到的稳定 epoch（偶数）。
- `current_id`（`:216-217`）：只被赋值（`.cpp:177,:578`），从不读（死字段）。
- `helpers`（`:220`）：attach impl 通过 `register_custom_helpers` 提供的 helper（idx → info）。
- `instantiated_progs`（`:223`）：handler id → `unique_ptr<bpftime_prog>`（进程堆，JIT 产物在这里）。
- `instantiated_attach_links`（`:225-226`）：link handler id → (attach impl 返回的 attach id, impl*)。
- `instantiated_perf_events`（`:228-230`）：perf handler id → (private data, attach type)。
- `attach_impls`（`:232-236`）：attach type（int）→ (impl*, private_data_creator)。**键就是 perf handler 的 `type` 数值**：
  `bpf_event_type` 定义在 `runtime/include/bpftime_shm.hpp:48-64`（UPROBE=6、URETPROBE=7、KPROBE=8、KRETPROBE=9、
  TRACEPOINT=2、OVERRIDE=1008、UREPLACE=1009），与 `ATTACH_UPROBE=6`、`ATTACH_SYSCALL_TRACE=2`
  （`attach/syscall_trace_attach_impl/include/syscall_trace_attach_impl.hpp:39`）、`ATTACH_CUDA_PROBE=8/ATTACH_CUDA_RETPROBE=9`
  （`attach/nv_attach_impl/nv_attach_impl.hpp:43-44`）按数值对上。也就是说 tracepoint perf 事件交给 syscall_trace impl，
  kprobe perf 事件交给 nv(CUDA) impl。
- `attach_impl_holders`（`:238-239`）：impl 的所有权。
- `instantiated_handlers`（`:241`）：已成功实例化的 handler id 集合（决定 auto-refresh 时是否跳过）。

CUDA 部分（`bpf_attach_ctx.hpp:35-144`，仅 `BPFTIME_ENABLE_CUDA_ATTACH`）：`HelperOperation`（1/2/3/4/6/14/501）、
`HelperCallRequest/Response`、`CommSharedMem{flag1, flag2, occupy_flag, request_id, map_id, req, resp, time_sum}`、
`CUDAContext`（`cuda_watcher_should_stop`、`cuda_shared_mem`、`cuda_shared_mem_device_pointer`）。`CommSharedMem` 在 shm 中
（`get_cuda_comm_shared_mem()`，`bpf_attach_ctx_cuda.cpp:386-393`），由 `create_cuda_context` 清零并经
`cudaHostGetDevicePointer` 拿设备指针（`:405-426`）。

### 2.4 CLI（`tools/cli/main.cpp`）

全局：`subprocess_pid`（`:87`）、`g_trace_interrupted/g_trace_target_pid/g_trace_loader_pid`（`:89-91`）。
库名常量 `AGENT_LIBRARY`/`SYSCALL_SERVER_LIBRARY`/`AGENT_TRANSFORMER_LIBRARY`（Linux `:79-82`）。

## 3. 主要流程与调用链

### 3.1 syscall-server：一次被拦截调用（以 `bpf()` 为例）

`syscall()` (`syscall_server_main.cpp:468`) → 读 6 个变参（`:472-480`）→ `initialize_ctx()` (`:206-236`)：
不可用则 `fallback_syscall` (`:180-189`) 走 `dlsym` 缓存的原 `syscall` → 可用则按号分流：`__NR_bpf` → `handle_exceptions(handle_sysbpf, call_original)`
(`:488-498`)；`__NR_perf_event_open` → `handle_perfevent` (`:499-509`)；`__NR_ioctl` 只打 debug 日志后落到末尾原样放行 (`:510-512`)；
`__NR_dup3` → `handle_dup3` (`:513-521`)；`__NR_memfd_create` → `handle_memfd_create` (`:522-530`)；其余 `call_original()` (`:532`)
→ `syscall_context::handle_sysbpf` (`syscall_context.cpp:482`)：先查三开关（`:484-488`，不满足 → 原 syscall），
再 `try_startup()` (`:489`→`:237-254`→`syscall_server_utils.cpp:147 start_up`)，然后按 cmd switch（§3.3 表）。

说明：其它覆盖函数（mmap/ioctl/…）走 `call_with_context(_debug)` (`syscall_server_main.cpp:259-280`)：ctx 不可用 → fallback；
否则 `handle_exceptions` (`:238-257`) 依次捕获 `boost::interprocess::bad_alloc`、`std::exception`、`...`，记日志（仅当 ctx 状态=2 且
logger 存在，`:59-70`）后调 fallback。注意 `syscall()` 在 ctx 可用时的 fallback 用的是 `context->orig_syscall_fn`（`:484-487`），
ctx 不可用时用 `cached_next_symbol`（`:481-483`）。

### 3.2 syscall-server 启动（lazy startup）与会话

触发点：**任何一个**被覆盖函数第一次进入 `handle_*` 且通过开关检查时（每个 `handle_*` 都调 `try_startup()`：
`syscall_context.cpp:263,284,314,337,489,816,944,956,1023,1076,1086,1117,1146,1159,1208,1226`）。不必是 bpf 相关调用。
已实跑：loader 在 `main` 开头 `/dev/shm/<name>` 不存在，执行一次 `close(-1)` 后就存在，`BPFTIME_USED=1`、`LD_PRELOAD` 已被 unset
（`out_loader.txt` [1][2] 行）。

链：`initialize_ctx` → `new syscall_context` (`syscall_server_main.cpp:220`) → ctor (`syscall_context.cpp:196-212`)：
`init_original_functions()`（dlsym + `unsetenv("LD_PRELOAD")`）→ `enable_mock=false` → `bpftime_set_logger(BPFTIME_LOG_OUTPUT 或
~/.bpftime/runtime.log)` → `load_config_from_env()` → `construct_runtime_config_from_env()`（只为打日志）→ `enable_mock=true`。
→ 某个 `handle_*` → `try_startup` (`:237-254`)：`enable_mock=false` + guard（析构时**无条件**置回 true）→ `start_up(*this)`；
异常时 `enable_mock_after_initialized=false` 并重抛。
→ `start_up` (`syscall_server_utils.cpp:147-228`) 在 `std::call_once` 内：
`construct_runtime_config_from_env()` (`:152`) → 记 shm 名/owner pid (`:155-156`) → `shm_lifecycle_lock`（flock `/tmp/bpftime-shm-<名>.lock`，
`runtime/src/bpftime_shm_internal.cpp:178-219`）(`:157-158`) → `initialize_global_shm_with_ownership()` (`:126-145`：先 `SHM_CREATE_ONLY`，
`already_exists_error` 时退 `SHM_CREATE_OR_OPEN` 且 owns=false；其它异常先 `remove` 半成品段再重抛) → `atexit(remove_syscall_server_global_shm)`
(`:161`) → 登记 alive server pid，失败则 owns=false (`:162-168`) → [CUDA] `ctx.initialize_cuda()` (`:169-171`) →
**`begin_new_session()`** (`:172`；`bpftime_shm_internal.cpp:1017-1031`：epoch+1(奇) → `reset_server_state()`（`manager->clear_all` + 清
syscall pid 集合，`:1381-`）→ epoch+1(偶)) → `set_mock_setter` (`:173-180`) → [VERIFIER] 设置可用 helper (`:181-215`) →
`bpftime_set_runtime_config` (`:216`，把 loader 的 env 配置写进 shm) → `setenv("BPFTIME_USED","1",0)` (`:219`)。
任何异常存入 `g_startup_exception` (`:222-224`)，之后每次 `start_up` 都 `rethrow` (`:226-227`)。

退出：`remove_syscall_server_global_shm` (`:56-124`)：非 owner pid（fork 子进程）或非 owner 只把自己从 alive server 集合摘掉；
owner 在 lifecycle lock 下摘自己，若 alive server 集合或 alive agent 集合里还有活进程（`kill(pid,0)==0||errno==EPERM`，`:51-54`）
或快照失败则不删，否则 `shared_memory_object::remove`。

会话含义：**每个**新 syscall-server 进程（哪怕是打开已存在的 shm、非 owner）都会 `begin_new_session` 把整张 handler 表清空。

### 3.3 `handle_sysbpf` 各 cmd（`syscall_context.cpp:482-805`）

总前置：三开关不满足 → 原 syscall（`:484-488`）；`try_startup`（`:489`）；`errno=0`（`:490`）。`run_with_kernel` 由 env
`BPFTIME_RUN_WITH_KERNEL` **存在即真**（`:179-185`）。

| cmd | 用户态模式（默认） | `run_with_kernel` 模式 | 位置 |
|---|---|---|---|
| `BPF_MAP_CREATE` | `bpftime_maps_create(-1, name, bpf_map_attr{type,key,value,max,flags,ifindex,btf_vmlinux_value_type_id,btf_id,btf_key,btf_value,map_extra})`，返回假 fd（`/dev/null`） | 类型翻译（GPU_KERNEL_SHARED_ARRAY→ARRAY）后真建内核 map；再 `create_kernel_bpf_map(fd, type+KERNEL_USER_MAP_OFFSET 或原类型)` 用内核 map **id** 当 bpftime fd 在 shm 建影子 map | `:493-553`；`:363-400` |
| `BPF_MAP_LOOKUP_ELEM` | `bpftime_map_lookup_elem` 拿指针，null → `ENOENT/-1`；否则按 `bpftime_map_value_size_from_syscall` memcpy 到 `attr->value` | 原 syscall | `:554-578` |
| `BPF_MAP_UPDATE_ELEM` | `bpftime_map_update_elem(fd,key,value,flags)` | 原 syscall | `:579-592` |
| `BPF_MAP_DELETE_ELEM` | `bpftime_map_delete_elem` | 原 syscall | `:593-603` |
| `BPF_MAP_LOOKUP_AND_DELETE_ELEM` | `bpftime_map_lookup_and_delete_elem(fd, value)`（queue/stack pop） | 原 syscall | `:604-615` |
| `BPF_MAP_GET_NEXT_KEY` | `bpftime_map_get_next_key(fd,key,next_key)` | 原 syscall | `:616-627` |
| `BPF_PROG_LOAD` | [仅 `ENABLE_BPFTIME_VERIFIER` 编译] TRACEPOINT→"tracepoint"、SOCKET_FILTER→"uprobe" 两类才过 PREVAIL，STRICT 失败 `EINVAL`，WARNING 只告警；然后 `bpftime_progs_create(-1, insns, cnt, name, prog_type)`——**只存字节码** | `create_kernel_bpf_prog_in_userspace`：名字匹配 `BPFTIME_NOT_LOAD_PATTERN` 则改成加载 2 条指令的平凡程序（`r0=0;exit`，清 func/line info）进内核，否则原样进内核；成功后把 LDDW(src 1/2) 的 imm 从 fd 改写为内核 map id，再 `bpftime_progs_create(内核 prog fd, …)` | `:628-703`；`:402-480` |
| `BPF_LINK_CREATE` | 先 `bpftime_link_create(-1, &attr->link_create)`；失败且 prog_fd 是 prog、target 是 perf、`attach_type==BPF_PERF_EVENT` → `bpftime_attach_perf_to_bpf_with_cookie` | target 不是 bpftime perf fd → 原 syscall；否则同左 | `:704-732` |
| `BPF_MAP_FREEZE` | 直接返回 0（未实现） | 原 syscall | `:733-742` |
| `BPF_OBJ_GET_INFO_BY_FD` | map fd：填 `bpf_map_info` 部分字段（type、key/value size、max_entries、flags、btf ids、ifindex、map_extra、`id=fd`、name）；prog fd：只填 `id=fd`；**其它 fd：什么都不填直接返回 0** | 原 syscall | `:743-786` |
| `BPF_PROG_ATTACH` | `bpftime_attach_perf_to_bpf(target_fd, attach_bpf_fd)` | target 不是 bpftime perf fd → 原 syscall | `:787-799` |
| 其它（BTF_LOAD、OBJ_PIN/GET、PROG_TEST_RUN、*_GET_NEXT_ID、*_GET_FD_BY_ID、批量 map 操作、LINK_UPDATE/DETACH、RAW_TRACEPOINT_OPEN 等） | `default`：**原样发给内核** | 同 | `:800-802` |

已实跑（`out_loader.txt`）：MAP_CREATE 返回 fd 4 且 `/proc/self/fd/4 -> /dev/null`；PROG_LOAD/perf_event_open/LINK_CREATE 均成功；
`BPF_BTF_LOAD` 走 default 进内核，非 root 得 `EPERM`；`BPF_OBJ_GET_INFO_BY_FD(fd=0)` 返回 0、errno 0、输出缓冲区原样未动（`info.id=0xabababab`）。

### 3.4 `handle_perfevent`（`syscall_context.cpp:806-935`）

前置：三开关；`try_startup`。按 `attr->type` 分流：

| `attr->type` | 用户态模式 | run_with_kernel | 位置 |
|---|---|---|---|
| == `determine_uprobe_perf_type()` | `retprobe = config & (1<<retprobe_bit)`，`ref_ctr_off = config>>32`，`name=(char*)config1`，`offset=config2` → `bpftime_uprobe_create(-1,pid,name,offset,retprobe,ref_ctr_off)` | 原 perf_event_open | `:817-842` |
| == `determine_kprobe_perf_type()` | 解析同上（name=函数名、addr=config2）→ `bpftime_kprobe_create(-1, name, addr, retprobe, ref_ctr_off)` | 名字以 `cuda_` 开头：用 `do_exit` 改写 attr 真开一个内核 kprobe 拿 fd，再以该 fd + 去前缀名建 bpftime kprobe；否则原 perf_event_open | `:843-889` |
| `PERF_TYPE_TRACEPOINT(2)` | `bpftime_tracepoint_create(-1, pid, config)` | 原 perf_event_open | `:890-904` |
| `PERF_TYPE_SOFTWARE(1)` | `bpftime_add_software_perf_event(cpu, sample_type, config)`（perf buffer 用） | **同左（不看 run_with_kernel）** | `:905-910` |
| `BPF_TYPE_UREPLACE(1009)` | `bpftime_add_ureplace_or_override(-1,pid,config1,config2,true)` | 同左 | `:911-919` |
| `BPF_TYPE_UPROBE_OVERRIDE(1008)` | `…(…, false)` | 同左 | `:920-929` |
| 其它 | 原 perf_event_open | 同 | `:931-934` |

`determine_*_perf_type`（`syscall_server_utils.cpp:260-298`）：文件不存在返回 mock 常量；存在则 `parse_uint_from_file` 用 `fopen(...,"re")`
读——这个 `fopen` 发生在 server 内部、开关为真时，会再被自己拦截并读到 mock 内容（`:240` → `syscall_server_main.cpp:434` →
`handle_fopen`），所以实际值恒为 mock 常量（推断，逻辑上已核对；本机真值恰好也是 uprobe=9、kprobe=8，无法用实验区分）。

### 3.5 mmap / munmap / epoll / ioctl / 文件 mock

**mmap**：`mmap` → `handle_mmap`（`syscall_context.cpp:937-947`，开关含 `run_with_kernel`）→ `handle_mmap64`（`:949-1008`）：
- fd 是 ringbuf map：`prot==RW` → `bpftime_get_ringbuf_consumer_page`；`prot==R` → `bpftime_get_ringbuf_producer_page`；非空则记入
  `mocked_mmap_values` 并返回（`:958-979`）。**offset 被忽略**。为空或 prot 不匹配 → 落到末尾真 mmap。
- fd 是 array map：`bpftime_get_array_map_raw_data` 非空即返回（`:980-986`）。
- fd 是 software perf event：`bpftime_get_software_perf_event_raw_buffer(fd,length)`，非空返回；否则保持 errno（默认 ENOMEM）返回
  `MAP_FAILED`（`:987-1001`）。**不**退回真 mmap。
- 其它：先做一次 `orig_mmap64_fn(addr,len,prot|PROT_WRITE,flags|MAP_ANONYMOUS,-1,0)` 且丢弃结果，再做真正的 mmap（`:1005-1007`）。
  已实跑：1000 次 4KiB 匿名 mmap+munmap 后 VmSize 从 108140 kB 涨到 112140 kB（正好 1000×4KiB 泄漏）。

**munmap**（`:1124-1150`）：先（不看开关）查 `mocked_mmap_values`，命中则摘除并返回 0（不真 unmap）；否则开关检查 → `try_startup` → 再查一次 → 原 munmap。

**epoll**：
- `epoll_create1`（`:1070-1078`）：非 kernel 模式下**无条件**返回 `bpftime_epoll_create()`（`/dev/null` 假 fd + shm 中 `epoll_handler`，
  `bpftime_shm_internal.cpp:547-559`），flags 被忽略。
- `epoll_ctl`（`:1080-1108`，开关**不查** `initializing_cuda`）：只处理 `EPOLL_CTL_ADD` 且 fd 为 ringbuf map / software perf event；
  其余 WARN 后调原 `epoll_ctl(epfd=假fd, …)`。
- `epoll_wait`（`:1110-1122`）：epfd 是 bpftime epoll handler → `bpftime_epoll_wait`；否则原函数。
- 已实跑（`hostside.c`）：在 server 下对 pipe 做 `epoll_ctl` 返回 `-1 EINVAL`，`epoll_wait` 返回 0（无 preload 时分别为 0 / 1）——
  **宿主自己的 epoll 用法会被弄坏**。

**ioctl**（`:1010-1068`）：`PERF_EVENT_IOC_ENABLE` → `bpftime_perf_event_enable`；`DISABLE` → `bpftime_perf_event_disable`；
`SET_BPF` → `bpftime_attach_perf_to_bpf(fd, data)`；kernel 模式且 fd 不是 bpftime perf fd → 原 ioctl；mock 返回 <0 或其它 req →
`SPDLOG_WARN("Calling original ioctl...")` 后原 ioctl（**宿主的每一次普通 ioctl 都会打一条 WARN**）。

**文件 mock**：
- 四个路径（`syscall_server_utils.cpp:42-49`）：`/sys/bus/event_source/devices/{uprobe,kprobe}/type` 与 `.../format/retprobe`；内容为
  `"9"`、`"config:0"`、`"8"`、`"config:0"`（`:299-322`），**与 run_with_kernel 无关、与真文件是否存在无关**。
- `open`/`openat`（`syscall_context.cpp:277-328`）：openat 先把 (dirfd, 相对路径) 解析成全路径（`syscall_server_utils.cpp:324-350`，
  经 `/proc/self/fd/<dirfd>`；`AT_FDCWD` 时直接用相对路径，所以相对路径**不会**匹配 mock）；命中则 `mkostemp`+`unlink` 拿一个空的真实 fd
  （`syscall_context.cpp:84-109`，O_CLOEXEC 跟随调用者），把 provider 挂进 `mocked_files`，之后 `read` 从内存 buf 供数据（`:330-362`）；
  文件本身是空的，所以 `pread`/`mmap`/`fdopen`+stdio 都读不到内容（推断）。
- `fopen`/`fopen64`/`_IO_new_fopen`（`:1152-1198`）：命中则把 buf 真写进 unlinked 临时文件、`lseek` 回 0、`fdopen(fd,"r")`；
  `'e'` 标志 → O_CLOEXEC。任一步失败都关 fd、退回原 `fopen`。
- `close`（`:256-275`）：在 `mocked_files` 中则摘除后真 close；否则 `bpftime_close(fd)`（`clear_id_at`，任何 fd 号都清 handler 表同号槽）再真 close。
- `read`（`syscall_server_main.cpp:423-432`）不打 debug 日志（其它包装都打）。

**dup3 / memfd_create（只经 `syscall()` 进入）**：
- `__NR_dup3`（`syscall_context.cpp:1200-1214`）：oldfd 是 bpftime map fd → `bpftime_maps_dup(oldfd,newfd)`（**不做真 dup3**），否则原 syscall。
  glibc 的 `dup3()` 包装是内联 syscall，不经过这里。
- `__NR_memfd_create`（`:1216-1228`）：开关满足即**无条件** `bpftime_add_memfd_handler`（`/dev/null` 假 fd，`bpftime_shm_internal.cpp:1420-1428`）。
  已实跑：`syscall(__NR_memfd_create,…)` 在 server 下返回指向 `/dev/null` 的 fd，`ftruncate` 失败 EINVAL；glibc `memfd_create()` 不受影响。

**被拦截函数总表**（导出见 `runtime/syscall-server/syscall-server.version:2`；`nm -D` 已实跑：
`close epoll_create1 epoll_ctl epoll_wait fopen fopen64 ioctl _IO_new_fopen mmap mmap64 munmap open openat read syscall`，
CUDA 关闭时无 `bpftime_syscall_server__poll_gpu_ringbuf_map`）：

| 符号 | 何时 mock（除"ctx 可用 + 三开关为真"外） | mock 做什么 | 否则 |
|---|---|---|---|
| `syscall(__NR_bpf)` | 见 §3.3 表 | handler 表读写 | 原 syscall |
| `syscall(__NR_perf_event_open)` | 见 §3.4 表 | 建 perf handler | 原 syscall |
| `syscall(__NR_dup3)` | 非 kernel 模式且 oldfd 是 map | `bpftime_maps_dup` | 原 syscall |
| `syscall(__NR_memfd_create)` | 总是 | `/dev/null` 假 fd + memfd handler | — |
| `syscall(其它号)` | 从不 | — | 原 syscall（`__NR_ioctl` 只记日志） |
| `mmap`/`mmap64` | 非 kernel 模式且 fd 是 ringbuf/array/sw-perf | 返回 shm 内指针 | 原 mmap（外加一次泄漏的匿名 mmap） |
| `munmap` | addr 在 `mocked_mmap_values` | 摘账，返回 0 | 原 munmap |
| `ioctl` | req ∈ {ENABLE, DISABLE, SET_BPF}（kernel 模式另需 fd 是 bpftime perf） | 改 perf handler / 建关系 | 原 ioctl（打 WARN） |
| `epoll_create1` | 非 kernel 模式 | 建 bpftime epoll handler | 原函数 |
| `epoll_ctl` | 非 kernel 模式、`ADD`、fd 是 ringbuf/sw-perf 且添加成功 | 挂进 epoll handler | 原函数（对假 epfd 必失败） |
| `epoll_wait` | 非 kernel 模式且 epfd 是 bpftime epoll | `bpftime_epoll_wait` | 原函数 |
| `open`/`openat` | 绝对路径（或 dirfd 解析后）命中 4 个 /sys 路径 | unlinked 空临时 fd + 内存 provider | 原函数 |
| `read` | fd 在 `mocked_files` | 从 provider buf 拷贝 | 原 read |
| `fopen`/`fopen64`/`_IO_new_fopen` | 路径命中 4 个 /sys 路径（不做 dirfd 解析） | 写入内容的 unlinked 临时文件 + `fdopen` | 原 fopen |
| `close` | 总是（开关满足时） | 摘 mock 文件或 `bpftime_close(fd)`，再真 close | 原 close |

### 3.6 agent 启动（两条注入路径 + syscall-trace 路径）

**LD_PRELOAD（`bpftime start`）**：动态链接器把 agent 的 `__libc_start_main`（`agent.cpp:544-556`）排在 libc 前 → 记下 `main` → 以
`bpftime_hooked_main` 代替 main 调 glibc 原版 → `bpftime_hooked_main`（`:535-542`）：`injected_with_frida=false`，
`bpftime_agent_main("", &stay_resident)`，然后**无论成败**调原 main。
**Frida（`bpftime attach <pid>` / `bpftime trace`）**：CLI `inject_by_frida`（`tools/cli/main.cpp:276-298`）
`frida_injector_inject_library_file_sync(pid, agent.so, "bpftime_agent_main", data)`，Frida 在目标进程里调导出的
`bpftime_agent_main(data, &stay_resident)`；`stay_resident=FALSE` 时 Frida 卸载该库。
**syscall trace（`bpftime start -s` / `attach -s`）**：先注入 `libbpftime-agent-transformer.so`（`attach/text_segment_transformer/agent-transformer.cpp:48-102`），
它 `dlmopen(LM_ID_NEWLM, AGENT_SO 或 data)` 加载 agent 到新命名空间、写 agent 的 `injected_with_frida`，再调 agent 的
`_bpftime__setup_syscall_trace_callback(&hooker)`（`agent.cpp:1199-1212`）：保存原 hooker、换成 `syscall_callback`、调
`bpftime_agent_main("", &val)`。此路径下 agent 的 `__libc_start_main` 不生效（transformer 自己劫持了它，`agent-transformer.cpp:32-46`）。

`bpftime_agent_main`（`agent.cpp:830-1177`）主线：
1. `install_agent_bootstrap_logger()`（`:832`→`:128-136`）：仅 UNINITIALIZED 时按本进程 `BPFTIME_LOG_OUTPUT`（缺省 `~/.bpftime/runtime.log`）设 logger。
2. 防重入三道（`:880-923`）：① `try_forward_to_existing_agent(data)`（`:262-268`）连本 pid 的 abstract socket `\0bpftime-agent-<pid>`，
   发 `"refresh <data>"` 成功即返回（stay_resident=FALSE）；② `find_other_loaded_agent_control()`（`:162-203`）用
   `dlsym(RTLD_NEXT)` 与 `dl_iterate_phdr`+`dlopen(RTLD_NOLOAD)` 找别的已加载副本的 `bpftime_agent_control`，有则调用它并返回；
   ③ CAS `UNINITIALIZED→INITIALIZING`：失败时若状态≠READY 直接返回；READY 且非 `force_reinit` → 仅 `start_agent_ipc_server_once()`；
   READY 且 `force_reinit=1` → `refresh_attach_session(data)`，stay_resident=TRUE。
3. 打开 shm（`:929-955`）：最多 60 次，每次先构造 `shm_lifecycle_lock`，`bpftime_initialize_global_shm(SHM_OPEN_ONLY)`，失败 sleep 50ms；
   全失败 → `init_fail()`。**锁一直持有到 `:1145`**（覆盖整个扫表/JIT/挂钩）。
4. `bpftime_set_logger(shm 中 runtime_config 的 logger 路径)`（`:956-958`）——agent 之后的日志写到 **loader 配置的位置**（已实跑：victim 未设
   `BPFTIME_LOG_OUTPUT`，其 attach 日志出现在 loader 的 `log2.txt`）；`apply_injected_kv_overrides(data)`（`:959`→`:765-800`，
   只认 `cuda_late_ptx_dir`、`cuda_disable_cuobjdump`，写成 env）。
5. `ctx_holder.init()`（`:960-963`）。
6. 注册 attach impl：syscall_trace（`ATTACH_SYSCALL_TRACE`，`set_original_syscall_function(orig_hooker)`，`:964-989`）；uprobe 后端：
   `BPFTIME_HAVE_TRAP_UPROBE_ATTACH` → trap impl（`:991-1012`），否则 `BPFTIME_HAVE_FRIDA_ATTACH` → `frida_attach_impl(runtime_config.enable_frida_fuzzy_backtracer)`
   （`:1013-1032`），二者都注册 {UPROBE, URETPROBE, UPROBE_OVERRIDE, UREPLACE}；都没有 → WARN（`:1033-1035`）；CUDA → nv impl
   {CUDA_PROBE, CUDA_RETPROBE}（`:1037-1057`）；最后 `syscall_trace_impl_ptr->set_to_global()`（`:1058-1060`）。
   （trap 与 frida 在 CMake 层面互斥：riscv 强制 trap ON/frida OFF，其它架构相反，`cmake/StandardSettings.cmake:76-82`。）
7. `init_attach_ctx_from_handlers(runtime_config)`（`:1063-1079`）：返回非 0 或抛 `std::exception` → `init_fail()`。
8. `srand(random_device)`（`:1080`）。
9. `auto_refresh_ms>0` 时起 auto-refresh 线程（`:1082-1136`，见 §3.8）。
10. 登记 alive agent pid，失败 → `init_fail`（`:1138-1144`）；释放 shm 锁（`:1145`）。
11. `ensure_detach_worker_started()`（`:1151-1154`→`:464-529`），失败 → `init_fail`。
12. `start_agent_ipc_server_once()`（`:1157`）。
13. `signal(SIGUSR1, sig_handler_sigusr1_detach)`（`:1158`）。
14. `*stay_resident=TRUE`、`setenv("BPFTIME_USED","1",0)`、`initialized=READY`（`:1160-1163`）。

### 3.7 扫表与装配（`bpf_attach_ctx::init_attach_ctx_from_handlers`）

`init_attach_ctx_from_handlers(config)`（`bpf_attach_ctx.cpp:62-70`，取 `shm_holder` 的 manager，无则 -1）→ 带 manager 版本（`:73-162`）：
持 `ctx_mutex`，最多 3 轮：
- `read_stable_epoch_seq()`（`bpftime_shm_internal.cpp:997-1015`，最多 200×1ms 自旋）：MISSING → 当作 0 并 WARN；UNSTABLE → sleep 50ms 下一轮。
- seq ≠ `last_epoch_seq_seen` → `destroy_all_attach_links_unlocked` + `reset_instantiated_state_unlocked`，更新 last（`:91-104`）。
- 第一遍扫：对每个已分配槽 `instantiate_handler_at(manager,i,stk,config,false)`，负返回值只记 debug（`:106-120`）。
- 第二遍扫（`handle_nv_attach_impl=true`）：CUDA link 在此时才挂（需要 map_basic_info）（`:125-136`）。
- 再读 epoch：MISSING → return 0；UNSTABLE → 拆光、sleep 重来；等于扫前 → return 0；否则拆光、记新值、重来（`:137-159`）。3 轮都不成 → `-EAGAIN`。

`instantiate_handler_at`（`:197-267`）：已在 `instantiated_handlers` → 0；`stk` 防环（`:207-212`）；
- prog → `instantiate_prog_handler_at`（`:294-315`）：`new bpftime_prog(insns,cnt,name)` 放进 `instantiated_progs`（**先放后 load**），
  `load_prog_and_helpers`（`:43-60`：按 config 注册 kernel_utils / ufunc / shm_maps 三组 helper，然后 `bpftime_prog_load(config.jit_enabled)`），
  再注册 attach impl 提供的 `helpers`（`:310-312`，此时已 load/JIT 过——第 5 章相关）；成功才进 `instantiated_handlers`。
- perf event → `instantiate_perf_event_handler_at`（`:420-510`）：SOFTWARE 直接返回 0（**不进** `instantiated_perf_events`/`instantiated_handlers`）；
  按 type 找 `attach_impls`（找不到 `-ENOENT`）；uprobe 族拼 `"<module>:<offset>"`、tracepoint 用 id 字符串、kprobe 用函数名、其它用
  `custom_data.attach_argument`，交给 `private_data_creator`；Frida 的 creator 会在本进程 `/proc/self/maps` 中解析模块，模块未映射返回 `-ENOENT`
  （`attach/frida_uprobe_attach_impl/src/frida_attach_private_data.cpp:29-34`）。
- link → 先递归 prog（`:234-242`）、再递归 target perf（`:243-251`）、再 `instantiate_bpf_link_handler_at`（`:316-419`）：
  `instantiated_perf_events[target]`（operator[]，没有就造默认项 type=0）→ 按 attach type 找 impl（无 → `-ENOTSUP`）→
  CUDA prog 走 nv 分支（仅第二遍，填 `nv_attach_private_data`，`:339-377`）；普通 prog：nv 私有数据配普通 prog 报错（`:380-389`），
  trap impl 先 `prog->prepare_for_signal_execution()`（`:391-396`），然后 `create_attach_with_ebpf_callback(lambda, *priv_data, attach_type)`，
  lambda 设置 cookie（trap 时写 `attach::current_signal_callback->cookie`，否则写 TLS `current_thread_bpf_cookie`）后
  `prog->bpftime_prog_exec(mem, mem_size, ret)`（`:397-407`）；attach_id<0 只记 debug 返回（`:409-415`）；成功记入
  `instantiated_attach_links` 与 `instantiated_handlers`（`:416-417`）。
- 其它 handler（map、epoll、memfd…）什么也不做（`:260-262`）。

**异常不被扫表循环吞掉**：`create_attach_with_ebpf_callback` 内 Frida 挂钩失败会 `throw std::runtime_error`
（`attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp:140,166`），一路抛出 `init_attach_ctx_from_handlers`。
已实跑：对 4 字节函数（`lea eax,[rdi+1]; ret`）挂 uprobe 时 Frida 报 `GUM_ATTACH_WRONG_SIGNATURE`，agent 日志
`Unable to instantiate handlers: …` → `init_fail` → **整个 agent 放弃**（同表里其它 link 也一个都不挂），宿主照常运行（log_loader.txt）。

register：`register_attach_impl`（`:269-293`）先让 impl 通过回调登记自定义 helper 到 `helpers`，再把 impl 所有权放进 `attach_impl_holders`，
为每个 type 写 `attach_impls[ty] = (impl*, creator)`（后注册覆盖先注册）。

detach：`destroy_instantiated_attach_link_unlocked`（`:517-541`）调 `impl->detach_by_id(attach_id)`；`destroy_all_attach_links_unlocked`
（`:548-564`）逐个拆，**遇到第一个错误就返回**；`reset_instantiated_state_unlocked`（`:572-579`）清四张表。

### 3.8 detach / refresh / auto-refresh / IPC

- **SIGUSR1**：`sig_handler_sigusr1_detach`（`agent.cpp:557-568`）只做 `auto_refresh_epoch++` 与向 `detach_pipe_fds[1]` 写 1 字节；
  worker 线程（`:508-523`，`std::thread(...).detach()`）阻塞 `read`，读到且 `initialized==READY` 才 `perform_detach()`。
- **`perform_detach`**（`:419-454`）：持 `detach_mutex`；ctx 未构造 → 置 UNINITIALIZED 返回；否则 epoch++ 并 join auto-refresh →
  `destroy_all_attach_links()` → **无论成败**都 `reset_instantiated_state()` → 只有 detach 成功才摘 alive pid → `bpftime_logger_flush()`。
  已实跑：victim 运行中 `kill -USR1`，计数器从 31 停住（out3_loader.txt），日志 `Detaching..` 来自 worker tid。
- **IPC server**（`:271-417`）：`call_once`，abstract `AF_UNIX` 流 socket `\0bpftime-agent-<pid>`，`listen(8)`，单线程 accept 循环；
  `SO_PEERCRED` 只放行 uid 0 或同 euid；读请求（≤1MiB，去尾 `\n\r\0`）：`refresh <kv>` → `refresh_attach_session` 回 `ok`/`err refresh <rc>`；
  `detach` → `perform_detach` 回 `ok`/`err detach <rc>`；`status` → `epoch_seq=<n>|missing|unstable`；其它 → `unknown`。
  `atexit` 里停循环、close fd、detach 线程（`:399-407`）。
  已实跑（python 客户端）：`status`→`epoch_seq=2`，`bogus`→`unknown`，`detach`→`ok`（计数停），`refresh auto_refresh_ms=300`→`ok`（计数恢复，
  日志 `Starting auto-refresh thread (300 ms, loader_pid=-1)`）。
- **`refresh_attach_session`**（`:676-763`）：要求 READY 且 ctx 已构造；持 `detach_mutex`；kv 覆盖；epoch++ 并 join 旧 auto-refresh；
  `destroy_all_attach_links` + `reset_instantiated_state`（全拆）→ `init_attach_ctx_from_handlers`（**无 try/catch**）→ 登记 alive pid →
  按 data 可能起新 auto-refresh → `start_agent_ipc_server_once()`。
- **auto-refresh 线程**（`:1082-1136`，refresh 版 `:714-758`）：循环：epoch 变了就退；`loader_pid>0` 且 `kill(lp,0)` 为 ESRCH 就退；
  sleep `auto_refresh_ms`；`init_attach_ctx_from_handlers` 并 `catch (const std::exception&)` 记 debug。只会**增量**实例化新 handler
  （已在 `instantiated_handlers` 的跳过）；表里被删掉的 link 只有在 epoch 变化时才会被拆。
- `bpftime_agent_control`（`:148-158`）：给"另一个副本"调用的入口，`refresh_attach_session` 包在 `try/catch(...)` 里。

### 3.9 CLI 子命令（`tools/cli/main.cpp`）

`main`（`:877-1290`）：先按 `BPFTIME_LOG_OUTPUT` 设 logger（`:882-885`）、`load_env_levels`；`signal(SIGINT/SIGTSTP, signal_handler)`
（`:889-890`，handler 把信号转发给 `subprocess_pid`，`:870-875`）；`-i/--install-location` 默认 `$HOME/.bpftime`（`:893-908`）；
`-d/--dry-run` 定义了但**从未使用**（`:910-912`）。库路径 `resolve_library_path_or_exit`（`:757-775`）：若 CLI 自身位于 build 树
（向上 0-5 层找同时存在 `runtime/agent/<agent>` 与 `runtime/syscall-server/<server>` 的目录，`:509-530`）优先用 build 树，否则
`<install>/<lib>`，都没有 `std::exit(1)`。

| 子命令 | 做什么 | 位置 |
|---|---|---|
| `load [opts] CMD...` | 解析 runtime 选项转成 env（`--no-jit`→`BPFTIME_DISABLE_JIT=true`、`--run-with-kernel-verifier`→`BPFTIME_RUN_WITH_KERNEL=true`、`--bpftime-not-load-pattern`、`--spdlog-level`、`--bpftime-log-output`、`--allow-external-maps`、`--fuzzy-backtracer`、`--memory-size`→`BPFTIME_SHM_MEMORY_MB`）；`run_command`：fork，子进程把 `LD_PRELOAD=<server.so>`（**整体替换**原 LD_PRELOAD）与 `AGENT_SO=`（空）及上述 env 合并进 environ，`execvpe`；父进程 waitpid，返回子进程退出码（信号退出返回 128+sig） | `:994-1003`；`:107-197`；`:777-868` |
| `start [-s] CMD...` | `-s`：`LD_PRELOAD=<agent-transformer.so>`、`AGENT_SO=<agent.so>`；否则 `LD_PRELOAD=<agent.so>`。无 runtime 选项 | `:1004-1028` |
| `attach [-s] PID` | Frida 注入：`-s` 时注入 transformer 并以 agent 路径作 data；否则注入 agent，data=`""`（无 auto-refresh、无 force_reinit） | `:1029-1047` |
| `detach` | `SHM_OPEN_ONLY` 打开 shm（失败 → WARN 并返回 0）；遍历 alive agent 集合，对每个 pid 先 IPC `detach`（不等回复），再 `kill(pid,SIGUSR1)` | `:1260-1288` |
| `trace (--pid N | --pidof COMM) [--auto-refresh-ms 500] CMD...` | 解析目标 pid（`--pidof` 读 `/proc/*/comm` 精确且唯一匹配）；若 euid=0 读目标 `/proc/pid/status` 的 Uid/Gid，loader 将降权到目标 uid/gid（`setgroups(0)`/`setgid`/`setuid`，`:206-226`），并尝试 chown/chmod 已存在的 `/dev/shm/<名>` 为目标用户 0600（`:448-492`）；`prepare_cuda_late_ptx_dir`（**总是**尝试：mkdtemp + 对目标 exe 及 cmdline 中存在的文件跑 `cuobjdump --extract-ptx all`，`:603-755`）；`spawn_command` 以 `LD_PRELOAD=<server.so>` 启动 loader（不 wait，`:199-274`）；拼 agent 参数 `loader_pid=<loader>;force_reinit=1[;auto_refresh_ms=N][;cuda_late_ptx_dir=…|;cuda_disable_cuobjdump=1]`（`:1156-1166`）；先尝试 IPC `refresh <arg>`，失败再 Frida 注入（`:1168-1185`）；等待 loader：SIGINT/SIGTERM（`sigaction` 无 SA_RESTART）时先对目标 IPC `detach`（失败则 SIGUSR1 + 200ms），再 `kill(loader,SIGINT)`；loader 退出后再发一次 detach（同样兜底 SIGUSR1）；返回值是**注入的 rc**，不是 loader 的退出码 | `:1048-1259` |

## 4. 跨模块接口

### 4.1 本块对外提供

- syscall-server 导出（`runtime/syscall-server/syscall-server.version:2`）：`epoll_wait epoll_ctl epoll_create1 ioctl mmap64 mmap close syscall munmap open openat read fopen fopen64 _IO_new_fopen bpftime_syscall_server__poll_gpu_ringbuf_map`。
  后者只在 CUDA 构建存在（`syscall_server_main.cpp:536-551`），使用者：`example/gpu/kernel_trace/kernel_trace.c:92` 等经 dlsym 调用。
- agent 导出（`runtime/agent/agent.version:2`，`nm -D` 已实跑 7 个，CUDA 关时无 `__cudaRegisterFatBinary`）：
  - `__libc_start_main`、`bpftime_hooked_main`：被动态链接器/glibc 调用。
  - `bpftime_agent_main`：Frida 入口（`tools/cli/main.cpp:282-285`）；transformer 经 `_bpftime__setup_syscall_trace_callback` 间接调用。
  - `bpftime_agent_control`：被另一个已加载 agent 副本经 dlsym 调（`agent.cpp:162-203`）。
  - `injected_with_frida`：被 `attach/text_segment_transformer/agent-transformer.cpp:82-88` 写。
  - `_bpftime__setup_syscall_trace_callback`、`syscall_callback`：被 transformer 调（`agent-transformer.cpp:89-100`），`syscall_callback`
    转给 `global_syscall_trace_attach_impl->dispatch_syscall`（`agent.cpp:1182-1197`）。
  - `__cudaRegisterFatBinary`（CUDA）：劫持 CUDA runtime 的 fatbin 注册，先 `register_llvm_vm_factory()` 再调原函数（`agent.cpp:812-828`）。
- `get_global_attach_ctx()`（`agent.cpp:119-122`）：强定义，覆盖 `runtime/src/bpf_helper.cpp:59-65` 的弱定义（弱定义抛异常）；使用点
  `bpf_helper.cpp:595`（tail call 找已实例化 prog）、`:784`、`:851`（get_stack/stackid 取 uprobe attach impl type 6）。单测
  `runtime/unit-test/test_get_stack.cpp:45` 也提供强定义。
- `bpf_attach_ctx` 公共 API（`bpf_attach_ctx.hpp:158-197`）：`init_attach_ctx_from_handlers(manager?, config)`、`register_attach_impl`、
  `destroy_instantiated_attach_link`、`destroy_all_attach_links`、`reset_instantiated_state`、`get_attach_impl_by_attach_type`、
  `find_instantiated_prog`、`find_nv_attach_impl`（CUDA）；自由函数 `load_prog_and_helpers`（`:261`）。使用者：agent（上文）、
  `bpf_helper.cpp`、`runtime/unit-test/*`（§1 表）。`find_nv_attach_impl` 的调用点不在本范围（推断在 nv attach / map 代码，需 R?（CUDA）确认）。
- `bpftime::stop_cuda_watcher_before_shm_unmap()`（`bpf_attach_ctx_cuda.cpp:95-98`）：供 shm 析构前停 watcher（调用点在范围外，未核实）。
- `cuda::set/get_cuda_shared_mem_device_pointer`（`bpf_attach_ctx.hpp:125-141`）：inline，供 nv attach 使用（未核实调用点）。
- CLI 与 agent 的 IPC 协议（abstract socket 名、`refresh/detach/status` 文本协议）：两边各自实现一份相同的 `make_agent_ipc_addr`/`try_send_agent_ipc`
  （`agent.cpp:206-260`、`tools/cli/main.cpp:301-361`）。
- 环境变量 `BPFTIME_USED=1`：给宿主程序检测（`example/bpf_features/tailcall_minimal/tailcall_minimal.c:154`，测试 helper `:90`）。

### 4.2 本块依赖外部

- shm / handler 表（R2）：`bpftime_initialize_global_shm`、`shm_open_type`、`shm_holder.global_shared_memory.{get_manager, begin_new_session,
  set_mock_setter, add/remove/iterate_*_alive_*_set, read_stable_epoch_seq, get_cuda_comm_shared_mem}`、`shm_lifecycle_lock`、
  `get_global_shm_name()`、`bpftime_set_runtime_config/bpftime_get_runtime_config`、`construct_runtime_config_from_env`
  （`runtime/src/bpftime_config.cpp:92-176`）。
- `bpftime_shm.hpp` 的 C API（R2/R5）：`bpftime_maps_create/progs_create/link_create/uprobe_create/kprobe_create/tracepoint_create/
  add_software_perf_event/add_ureplace_or_override/attach_perf_to_bpf(_with_cookie)/perf_event_enable/disable/epoll_create/epoll_wait/
  add_ringbuf_fd_to_epoll/add_software_perf_event_fd_to_epoll/get_ringbuf_consumer_page/producer_page/get_array_map_raw_data/
  get_software_perf_event_raw_buffer/maps_dup/add_memfd_handler/close/map_*_elem/map_get_info/is_*_fd/
  translate_shared_map_type_to_kernel_map_type/poll_gpu_ringbuf_map`（调用点见 §3.3-3.5 行号）。
- logger：`bpftime_set_logger`、`bpftime_logger_flush`、`DEFAULT_LOGGER_OUTPUT_PATH`（`runtime/include/bpftime_config.hpp:13`）。
- VM / prog（R6 等）：`bpftime_prog` 构造、`bpftime_prog_load`、`bpftime_prog_exec`、`bpftime_prog_register_raw_helper`、
  `prepare_for_signal_execution`、`is_cuda`、`get_insns`、`current_thread_bpf_cookie`；helper 组 `bpftime_helper_group::get_*_helper_group().add_helper_group_to_prog`。
- attach impl（R4）：`base_attach_impl::{register_custom_helpers, create_attach_with_ebpf_callback, detach_by_id}`、`attach_private_data::initialize_from_string`、
  `frida_attach_impl`、`trap::trap_attach_impl`、`syscall_trace_attach_impl::{set_original_syscall_function, set_to_global, dispatch_syscall}`、
  `global_syscall_trace_attach_impl`、`nv_attach_impl`/`nv_attach_private_data`、`attach::current_signal_callback`。
- verifier（R?）：`verifier::verify_ebpf_program/set_available_helpers/set_non_kernel_helpers`、`get_ufunc_helper_protos()`（仅 `ENABLE_EBPF_VERIFIER`）。
- 第三方：frida-core（CLI 注入）、frida-gum（agent 内）、argparse、spdlog、boost.interprocess、libbpf（`bpf_obj_get_info_by_fd`，`syscall_context.cpp:141-142`）、CUDA driver/runtime。

## 5. 配置项、环境变量、编译开关

| 名字 | 默认 | 定义/读取位置 | 影响 |
|---|---|---|---|
| `LD_PRELOAD` | — | server ctor `unsetenv`（`syscall_context.hpp:132`）；CLI 整体替换（`tools/cli/main.cpp:136-145,249-258`） | 注入；server 初始化后 exec 的子进程不再被注入；**agent 不 unset**，exec 子进程会继承 agent |
| `BPFTIME_GLOBAL_SHM_NAME` | `bpftime_maps_shm` | `runtime/src/handler/handler_manager.hpp:51,62`；CLI `:439-445` | shm 名，也决定 `/tmp/bpftime-shm-<名>.lock` |
| `BPFTIME_LOG_OUTPUT` | `~/.bpftime/runtime.log` | server ctor `syscall_context.cpp:200-202`；agent bootstrap `agent.cpp:133-135`；config `bpftime_config.cpp:158-161`；CLI `:882-885` | 日志位置；agent 初始化后改用 shm 中 loader 的值 |
| `BPFTIME_RUN_WITH_KERNEL` | 未设 | `syscall_context.cpp:179-185`（**存在即真**，值无关） | 内核 map/verifier 混合模式 |
| `BPFTIME_NOT_LOAD_PATTERN` | 空 | `syscall_context.cpp:186-193`，使用 `:406-430` | 仅 kernel 模式：匹配的 prog 名以平凡程序代替进内核 |
| `BPFTIME_DISABLE_JIT` | 未设 | `bpftime_config.cpp:106-108`（存在即真） | `runtime_config.jit_enabled=false`，agent 的 `load_prog_and_helpers` 用它 |
| `BPFTIME_ALLOW_EXTERNAL_MAPS`、`BPFTIME_FRIDA_FUZZY_BACKTRACER` | 未设 | `bpftime_config.cpp:110-116` | 外部 map；agent 构造 frida impl 的参数（`agent.cpp:1017-1018`） |
| `BPFTIME_SHM_MEMORY_MB`、`BPFTIME_MAX_FD_COUNT` | 50 / 6144 | `bpftime_config.cpp:119-128` | shm 大小、handler 表长度（server 建段时用） |
| `BPFTIME_VERIFIER_LEVEL` | WARNING | `bpftime_config.cpp:137-156` | 仅 `ENABLE_EBPF_VERIFIER` 构建的 PROG_LOAD 校验 |
| `SPDLOG_LEVEL` | — | CLI `load_env_levels`（`:886`）；logger 内部 | 日志级别 |
| `BPFTIME_USED` | 写入 "1"（不覆盖） | server `syscall_server_utils.cpp:219`；agent `agent.cpp:1161` | 宿主可据此判断被 bpftime 控制 |
| `AGENT_SO` | CLI 总会设（`load`/`start` 非 -s 时为空串） | CLI `:118-147`；transformer 读 `agent-transformer.cpp:56` | `start -s` 时 agent 路径 |
| `BPFTIME_CUDA_LATE_PTX_DIR`、`BPFTIME_CUDA_DISABLE_CUOBJDUMP` | — | agent 由注入 kv 写入（`agent.cpp:787-797`） | CUDA late attach |
| `BPFTIME_CUOBJDUMP`、`BPFTIME_CUDA_ROOT`/`CUDA_HOME`/`CUDA_PATH`/`LLVMBPF_CUDA_PATH`/`CUDAToolkit_ROOT` | — | CLI `:547-601` | `trace` 找 cuobjdump |
| `HOME` | — | CLI `:893` | 默认 install 位置 |
| agent 注入参数 kv（`;`/空白分隔） | — | `auto_refresh_ms`（`:570-606`）、`loader_pid`（`:608-645`）、`force_reinit`（`1/true/yes`，`:647-674`）、`cuda_late_ptx_dir`、`cuda_disable_cuobjdump`（`:765-800`） | 刷新/重入行为 |

编译开关：`BPFTIME_ENABLE_CUDA_ATTACH`（server CUDA 初始化、poll 导出；agent nv impl；`bpf_attach_ctx_cuda.cpp` 是否编译 `runtime/CMakeLists.txt:91-92`）；
`ENABLE_EBPF_VERIFIER`→`ENABLE_BPFTIME_VERIFIER`（`runtime/syscall-server/CMakeLists.txt:56-61`）；`BPFTIME_BUILD_WITH_LIBBPF`（syscall_trace impl 与回调、
`bpf_obj_get_info_by_fd`）；`BPFTIME_HAVE_FRIDA_ATTACH`（`runtime/agent/CMakeLists.txt:84`）/`BPFTIME_HAVE_TRAP_UPROBE_ATTACH`（`:94`）；
`BPFTIME_UBPF_JIT`/`BPFTIME_LLVM_JIT`（whole-archive 链 VM，保证构造函数注册工厂，`syscall-server/CMakeLists.txt:13-25`、`agent/CMakeLists.txt:25-37`）；
`TEST_LCOV`；`BPFTIME_GPU_HELPER_MAX_BUF`（`bpf_attach_ctx.hpp:39-45`，改了要重生成 trampoline PTX）。

## 6. 并发、生命周期与错误处理

### 6.1 锁 / 原子 / TLS
- server：`ctx_initialized`（原子 CAS + `sched_yield` 自旋）、`tls_initializing`（`__thread`）（`syscall_server_main.cpp:206-236`）；
  `cached_next_symbol` 原子（多个线程可能并发 dlsym 同一符号，结果相同，无害）；`mocked_file_lock`/`access_lock` spinlock；
  `g_startup_once`；三开关 `std::atomic<bool>`（多用 relaxed）。`mocked_mmap_values` 无锁。
- agent：`initialized` 原子；`detach_mutex`（perform_detach / refresh_attach_session 串行）；`auto_refresh_thread_mutex`；
  `auto_refresh_epoch` 作取消令牌；`agent_ipc_once`；`detach_thread_started` CAS。
- `bpf_attach_ctx::ctx_mutex` 保护扫表/拆除；`find_instantiated_prog` 不加锁（被 helper 在探针线程调用）。
- 跨进程：`shm_lifecycle_lock`（flock，server start_up 全程、server 退出清理、agent 从开 shm 到登记 pid 全程）；shm 里 `pid_set_lock`
  （`interprocess_mutex`，250ms 超时 try_lock，`bpftime_shm_internal.cpp:121-175`）；epoch seqlock。

### 6.2 fork / exec
- server：exec 子进程因 `unsetenv("LD_PRELOAD")` 不再注入；fork 子进程带着 .so 与已初始化的 ctx（`g_startup_once` 已执行，不会再 `begin_new_session`），
  退出清理只摘自己（`syscall_server_utils.cpp:61-69`）。
- agent：不 unset LD_PRELOAD → `bpftime start` 下目标 exec 的子进程也会加载 agent 并各自初始化（推断）；fork 子进程继承 READY 状态与
  已改写的指令，但 IPC/worker/auto-refresh 线程不随 fork 复制，alive 集合里只有父 pid（推断，未验证）。

### 6.3 fail-open 落点（全部）
syscall-server：
1. `initialize_ctx` 失败（ctx 构造抛异常 → 状态 3）→ 所有包装走 `cached_next_symbol` 原函数（`syscall_server_main.cpp:219-224,263-264,481-483`）。
2. 构造期重入（`tls_initializing`）→ 原函数（`:208-209`）。
3. `dlsym` 失败 → `errno=ENOSYS` 返回失败值（`:135-146,161-165,172-176,183-187`）。
4. `handle_exceptions`：bad_alloc / std::exception / ... → 原操作（`:238-257`）。`bpf()` 落到真内核。
5. `start_up` 失败 → `g_startup_exception` 门闩 + `enable_mock_after_initialized=false`，此后所有 `handle_*` 在开关处放行（`syscall_server_utils.cpp:222-227`、`syscall_context.cpp:247-253`）。单测 "falls back when startup shared memory is too small" 覆盖。
6. 建段失败先删半成品（`syscall_server_utils.cpp:131-142`）。
7. alive pid 登记失败 → 放弃自动删段（`:162-168`）。
8. openat 路径解析失败 / mock fd 建不出 / 写入失败 / lseek / fdopen 失败 → 原 open/openat/fopen（`syscall_context.cpp:285-298,317-322,1163-1190`）。
9. ioctl mock 返回负值 → 原 ioctl（`:1033-1066`）；epoll_ctl mock 失败 → 原 epoll_ctl（`:1087-1107`）。
10. ringbuf/array mmap 拿不到指针 → 落到原 mmap（`:958-986` → `:1005-1007`）。（反例：software perf mmap 失败**不**回退，返回 `MAP_FAILED`，`:997-1000`。）
11. `default` cmd 原样进内核（`:800-802`）；LINK_CREATE 失败时的 perf-attach 兜底（`:722-730`）。
12. CUDA poll：ctx 不可用 `ENOSYS`，异常 `EIO`（`syscall_server_main.cpp:540-549`）。
13. `safe_spdlog_*` 在 logger 未就绪时不打日志（`:47-70`）。
agent：
14. 整个 `bpftime_agent_main` 体在 `try{...}catch(std::exception&){init_fail}catch(...){init_fail}`（`agent.cpp:879,1172-1176`）。
15. `init_fail`（`:837-878`）：仅当本调用赢得 CAS 才回滚：reset syscall-trace 全局、停 auto-refresh、关 detach 管道、析构 ctx、摘 alive pid、
    状态回 UNINITIALIZED。触发点：shm 60 次打不开（`:949-955`）、扫表非 0（`:1067-1072`）或抛 `std::exception`（`:1073-1078`）、
    alive pid 登记失败（`:1138-1143`）、detach worker 起不来（`:1151-1154`）。
16. LD_PRELOAD 路径无论成败都进入原 main（`:539-541`）。
17. `bpftime_agent_control` 吞所有异常（`:154-157`）；`syscall_callback` 异常或无 impl 时退 `orig_hooker`，再无则 `-ENOSYS`（`:1186-1196`）；
    `__cudaRegisterFatBinary` 异常时调原函数（`:822-826`）；`ensure_detach_worker_started` 吞异常（`:525-528`）；IPC server 建立失败只 WARN（`:277-297,393-398`）。
18. auto-refresh 线程 `catch (const std::exception&)`（`:750-754,1128-1132`）。
19. 扫表中单个 handler 负返回值不中断（`bpf_attach_ctx.cpp:109-119`）。
**不是** fail-open 的地方（见 §7）：IPC 线程里的 refresh（无 try）、扫表中 attach impl 抛异常（整个 agent 放弃而非跳过一个 link）、
perform_detach（无 try，在 worker/IPC 线程里）、auto-refresh 只接 `std::exception`。

### 6.4 退出与资源
- server 永不析构 ctx；atexit 清理 shm（条件见 §3.2）。agent 永不析构 ctx_holder（正常路径）；shm 由 `__destruct_shm`（`bpftime_shm_internal.cpp:90-115`）
  以 SHM_OPEN_ONLY 身份把自己从 alive agent 集合摘掉，**agent 从不删段**。
- 已实跑：agent 先于 loader 启动且比 loader 晚退出时，loader（owner）退出时看到活 agent 不删段，之后 victim 退出也不删 → `/dev/shm/<名>` 残留（实验 4，已手动清理）。
- `/tmp/bpftime-shm-<名>.lock` 文件从不删除（`bpftime_shm_internal.cpp:195-208` 只 open/flock，析构 `:221-229` 只 unlock/close）。已实跑：跑完单测 /tmp 下多出 7 个 lock 文件（已删）。

## 7. 不直观的行为、疑似 bug、代码与注释不一致

1. **IPC refresh 中 attach 抛异常 → 宿主进程被 `std::terminate` 杀死**（确认，已实跑）。`agent.cpp:343` 在 IPC 线程里直接调
   `refresh_attach_session`，后者 `:698-699` 调 `init_attach_ctx_from_handlers` 无 try；Frida 挂钩失败抛 `std::runtime_error`
   （`frida_internal_attach_entry.cpp:140,166`）。实验 6：agent 先起（空表 READY），loader 建了对 4 字节函数的 uprobe，python 发
   `refresh ` → victim 退出码 134，宿主 stderr 出现 `terminate called after throwing an instance of 'std::runtime_error'`。
   违反 `CLAUDE.md:152-157`（终止宿主 + 写宿主 stderr）。`bpftime trace` 对已有 agent 的进程优先走 IPC refresh（`tools/cli/main.cpp:1173-1178`），
   `bpftime attach` 对已有 agent 的进程也会经 `try_forward_to_existing_agent` 走这条路（`agent.cpp:883`），都能触发。
   同类风险（推断，未实跑）：`perform_detach` 在 worker 线程（`:521`）与 IPC 线程（`:357`）里无 try，`detach_by_id` 若抛则 terminate；
   auto-refresh 线程只接 `std::exception`，非 std 异常会 terminate。
2. **一个 link 挂钩抛异常 → 整个 agent 初始化回滚**（确认，已实跑实验 1）。扫表循环只容忍负返回值（`bpf_attach_ctx.cpp:109-119`），异常会穿出
   `init_attach_ctx_from_handlers` → `agent.cpp:1073-1078` `init_fail`，同表其它本可成功的 link 全部不挂。现有指南"单个 handler 失败只记日志"只对返回码成立。
3. **SIGUSR1 处理器无保存/链式**（确认，已实跑）：`agent.cpp:1158` 裸 `signal()`；victim2 在 `main` 开头查到 SIGUSR1 已是 NON-DEFAULT。
   违反 `CLAUDE.md:158-159`。单测只覆盖失败路径保持 SIG_DFL（`shm_allocation_test_helper.c:200-207`）。
4. **`handle_mmap64` 每次回退泄漏一段匿名映射**（确认，已实跑：1000 次 → VmSize +4000 kB）。`syscall_context.cpp:1005-1006`，结果 `ptr` 未使用。
   而且非 MAP_FIXED 时这段映射与后面那次无关；宿主（loader）所有普通 mmap 都受影响（含 malloc 的大块分配）。
5. **`BPF_OBJ_GET_INFO_BY_FD` 对非 bpftime fd 返回 0 且不填输出**（确认，已实跑）。`syscall_context.cpp:743-786` 中 map/prog 以外的分支落到 `:785 return 0`，
   既不放行内核也不报错；prog 分支也只填 `id`（`:781`）。
6. **`try_startup` 每次调用都把 `enable_mock` 置 false 再置 true**（`syscall_context.cpp:239-246`，而几乎每个 `handle_*` 都先调它）。多线程 loader 中，
   线程 A 处在这个窗口时，线程 B 的 `bpf()`/`mmap()` 检查 `enable_mock` 看到 false → 直接走真内核/真 mmap（推断，未实跑；`relaxed` 原子，窗口虽短但真实存在）。
   另外 guard 无条件写回 true，会覆盖任何其它人设置的 false。
7. **新 syscall-server 进程会清空正在使用的 handler 表**：`start_up` 对"打开已存在 shm"的非 owner 也调用 `begin_new_session`（`syscall_server_utils.cpp:172`，
   `bpftime_shm_internal.cpp:1017-1031` → `reset_server_state` → `manager->clear_all`）。两个 loader 用同一个 shm 名并存时，后者会把前者的 map/prog/link 全清掉（确认：代码路径直接）。
   用 auto-refresh 的 agent 会因 epoch 变化拆掉全部 link 并按新表重挂（`bpf_attach_ctx.cpp:91-104`）；**没有** auto-refresh 的 agent（LD_PRELOAD、`bpftime attach`）
   保持旧 link 继续运行，其 prog 引用的 map fd 已被清/被新对象复用（推断，未验证后果）。
8. **宿主自身的 epoll 被破坏**（确认，已实跑）：`handle_epoll_create1` 无条件返回 bpftime epoll 假 fd（`syscall_context.cpp:1070-1078`），对普通 fd 的
   `epoll_ctl` 落到对 `/dev/null` 假 fd 的原函数 → `EINVAL`。loader 若自己也用 epoll（网络、timerfd 等）会失效。
9. **`syscall(__NR_memfd_create)` 一律被替换为 `/dev/null` 假 fd**（确认，已实跑）：`syscall_context.cpp:1216-1228`；宿主若用它做真 memfd 会坏。
10. **每次普通 ioctl 打一条 WARN**（确认，读码）：`syscall_context.cpp:1066`。
11. **verifier helper 列表 copy-paste 错误**（确认，读码）：`start_up` 中 `enable_ufunc_helper_group` 分支又把 **shm_maps** 组的 id 加了一遍（`syscall_server_utils.cpp:200-205`），
    ufunc 组 id 从未加入 `helper_ids`（只经 `non_kernel_helpers` 传原型）。仅 `ENABLE_EBPF_VERIFIER` 构建有影响。
12. **`BPF_PROG_LOAD` 注释与 libbpf 不符**：注释 "uprobe/uretprobe -> BPF_PROG_TYPE_SOCKET_FILTER"（`syscall_context.cpp:642`），但 libbpf 把 `SEC("uprobe")` 设为 KPROBE，
    故 uprobe 程序不进用户态 verifier（现有指南已指出，保留）。
13. **`create_kernel_bpf_map` 已存在时返回 0**（`syscall_context.cpp:387-391`），调用方把 0 当作 map fd 返回给 libbpf（确认读码；仅 kernel 模式）。
    另：失败日志打印的是未填充的 `info.id`（`:453-456`）。
14. **run_with_kernel 下 /sys mock 仍生效**（推断）：open/openat/fopen 的 mock 不看 `run_with_kernel`（`syscall_context.cpp:277-328,1152-1198`），libbpf 读到 uprobe=9、kprobe=8，
    再原样把 type=9/8 传给真 `perf_event_open`（`:818-826,877-885`）。在 uprobe/kprobe 动态 PMU 号不是 9/8 的内核上会建错类型的 perf event（本机真值恰为 9/8，无法实测）。
15. **run_with_kernel 下 perf buffer 大概率不可用**（推断，未验证）：SOFTWARE perf event 总是建 bpftime 假 fd（`:905-910`），但 mmap/epoll 在 kernel 模式下直接走原函数（`:940,952,1072,1083,1113`），
    对 `/dev/null` fd 的 mmap/epoll_ctl 会失败。
16. **`instantiate_bpf_link_handler_at` 用 `operator[]`**（`bpf_attach_ctx.cpp:323-324`）：target perf 未实例化（如 SOFTWARE perf 事件或实例化失败）时插入默认项（type 0、空 priv_data），
    随后 `attach_impls.find(0)` 失败返回 `-ENOTSUP`；若真有人注册了 type 0，会解引用空 `priv_data`（推断）。
17. **detach 失败仍 reset**：`perform_detach` 在 `destroy_all_attach_links` 返回错误（且它遇到第一个错误就停，`bpf_attach_ctx.cpp:556-561`）后仍 `reset_instantiated_state`
    （`agent.cpp:439-446`），销毁 `instantiated_progs`；没拆掉的钩子回调 lambda 里捕获的是裸 `prog` 指针（`bpf_attach_ctx.cpp:398-404`）→ 悬空指针（推断）。
    `refresh_attach_session`（`agent.cpp:695-696`）与扫表的 epoch 分支（`bpf_attach_ctx.cpp:101-102,145-146,157-158`）同样忽略 destroy 的返回值。
18. **`find_instantiated_prog` 无锁**（`bpf_attach_ctx.cpp:190-195`），而 auto-refresh 线程会在 `ctx_mutex` 下修改 `instantiated_progs`（std::map）→ tail call helper
    （`bpf_helper.cpp:595`）并发读写数据竞争（推断）。
19. **`mocked_mmap_values` 无锁**（`syscall_context.hpp:92`；写 `syscall_context.cpp:966,976,984,994`，读删 `:1127-1135`）（推断：多线程 loader 数据竞争）。
20. **`create_map_basic_info` 越界检查在访问之后**（`bpf_attach_ctx_cuda.cpp:340` 先 `local_basic_info[i]`，`:350` 才检查 `i>=size`）（确认读码；仅 CUDA，handler 表 > 1024 时 UB）。
21. **死代码/死字段**：`demo_ptx_prog.hpp` 无引用；`bpf_attach_ctx::current_id` 只写不读；`mocked_file_provider::replacement_file` 无人用；
    `injected_with_frida` 在 agent 内只写不读；CLI `--dry-run` 不生效；`handle_sysbpf` 里 `char *errmsg`（`syscall_context.cpp:491`）未用。
22. **CLI `detach` 先 IPC 再 SIGUSR1，同一 agent detach 两次**（`tools/cli/main.cpp:1272-1283`）；且在持有 shm `pid_set_lock` 的回调里做这些（`iterate_all_pids_in_alive_agent_set`
    在锁内调用 cb，`bpftime_shm_internal.cpp:1312-1330`），agent 侧 `remove_pid_from_alive_agent_set` 可能因 250ms 超时失败、pid 残留（推断）。alive 集合里若有被 SIGKILL 的
    agent 残留 pid 且被系统复用，`detach` 会给无关进程发 SIGUSR1（默认动作是终止）（推断）。
23. **`trace` 总是跑 cuobjdump**：即便目标不是 CUDA 程序也会对目标 exe 和其 cmdline 中的文件执行 `cuobjdump --extract-ptx all`（`tools/cli/main.cpp:603-755,1101`）；`trace` 的返回值是注入 rc 而非 loader 退出码（`:1259`）。
24. **README 与代码的小出入**：`runtime/syscall-server/README.md:44` 把 `BPFTIME_NOT_LOAD_PATTERN` 说成"skip verification"，实际只在 kernel 模式生效且是"用平凡程序代替真程序进内核"；
    `usage.md:162` 说"skip loading the eBPF program into the kernel"，实际仍向内核加载一个 2 指令程序（`syscall_context.cpp:412-426`）。`usage.md:149-153` 的 `BPFTIME_USE_JIT`
    在 runtime 中无任何读取（仅 `benchmark/deepflow/benchmark/run.py:85` 还在设），已过时。`BPFTIME_RUN_WITH_KERNEL`/`BPFTIME_DISABLE_JIT` 是"存在即真"，设成 `false` 也会开启。
25. `_IO_new_fopen` 的原函数回退用的是 `"fopen"` 符号（`syscall_server_main.cpp:458-460`），不是 `_IO_new_fopen` 本身（无害，记录）。
26. 范围外但与入口相关（交给 R4 确认）：`agent-transformer.cpp:79` 在 `dlmopen` 失败时 `exit(1)`（违反契约），`:84-88` 在找不到 `injected_with_frida` 时 WARN 后仍解引用空指针；
    其 logger 固定写 stderr（`:50-52`）。

## 8. 对照现有指南

对照文件：`/home/y/src/bpftime-wt-reading-guide/01-lifecycle.md`（第 1–10 步 + 深入参考）、`/home/y/src/bpftime-wt-reading-guide/03-agent-syscall-server.md`（全文）。

### 8.1 错误
- 03 §3.8 末段（`03-agent-syscall-server.md:562-564`）："这就是为什么 loader 先跑、agent 后跑，或者 agent 先跑、loader 后跑，最终都能对上。"
  → 只对带 `auto_refresh_ms` 的注入（`bpftime trace`）或有人发 IPC `refresh` 时成立。LD_PRELOAD（`bpftime start`）与 `bpftime attach` 的 data 为空串
  （`agent.cpp:539`、`tools/cli/main.cpp:1046`），只扫一次。已实跑实验 4：agent 先起、loader 2 秒后才建 handler，agent 日志 `Attach successfully`，计数器始终 0。
  01 第 7 步（`01-lifecycle.md:398-399`）的说法是对的，两章互相矛盾，以 01 为准。
- 01 第 2 步（`01-lifecycle.md:136`）"第一次有 bpf 相关调用进来时，syscall-server 做一次性启动" → 实际是**任一**被覆盖函数第一次进入 `handle_*`（包括普通 `close`/`open`/`read`/`mmap`/`ioctl`），
  已实跑：`close(-1)` 即建出 shm。03 §3.4（`:252`）"第一个真正需要 mock 的调用"也不准确，同样是"第一个通过开关的被拦截调用"，不论是否需要 mock。
- 01 第 8 步要点（`01-lifecycle.md:443-444`）/ 03 深入参考隐含"单个 handler 实例化失败只记一条日志，不中断扫描" → 只对返回负值成立；attach impl 抛异常（Frida 挂钩失败就是抛）会让整个
  agent 初始化 `init_fail`（实验 1 已实跑）。
- 03 §3.2（`:86-89`）/小结把 fail-open 写成全面成立（"agent 初始化中途失败……安静地返回"）→ IPC refresh 路径不在 try 内，可直接 terminate 宿主（实验 6 已实跑，见 §7.1）。
- 03 §3.3（`:118-119`）"libbpf 会读 `/sys` 下的几个文件，也会 `close` 假 fd" 无误；但 §3.6（`:366-373`）"只要被拦截的 open/openat/fopen 打开的是这几个固定路径就返回内容由 bpftime 指定的文件"
  → open/openat 返回的临时文件是**空的**，内容只经被拦截的 `read()` 从内存供给；只有 fopen 路径真写了内容（03 深入参考 `:837-840` 讲对了，正文不准）。另外 openat 用 `AT_FDCWD`+相对路径不会命中。
- 03 §3.4 表（`:243-245`）`enable_mock` "构造上下文、建日志期间；start_up 执行期间" → 实际是**每一次** `try_startup()` 调用期间（几乎每个 `handle_*` 都会调），不只是首次 start_up；这也是 §7.6 竞态的来源。
- 03 深入参考（`:851-852`）说泄漏的匿名映射"每次 fallthrough 会泄漏"——正确，已实跑量化（1000 次 = 4000 kB）。（非错误，补证据。）

### 8.2 遗漏
- `handle_sysbpf` 完整 cmd 表（03 只讲 3 个）：LOOKUP/UPDATE/DELETE/LOOKUP_AND_DELETE/GET_NEXT_KEY/FREEZE（假实现返回 0）/OBJ_GET_INFO_BY_FD（非 bpftime fd 返回 0 不填）/PROG_ATTACH；以及 `run_with_kernel` 下每个 cmd 的去向。
- `handle_perfevent` 的 kprobe（含 `cuda_` 前缀走 do_exit 真 kprobe）、tracepoint、UREPLACE(1009)/UPROBE_OVERRIDE(1008) 分支；SOFTWARE 分支不看 `run_with_kernel`。
- perf type 号与 `attach_impls` 键的"数值对应"：tracepoint perf(2) 交给 syscall_trace impl、kprobe(8) 交给 nv impl。
- ringbuf mmap（consumer/producer 按 prot 区分、忽略 offset）、array map mmap 失败时会落到真 mmap（与 perf 不同）。
- 宿主副作用：loader 自己的 epoll 失效、`syscall(memfd_create)` 被替换、每个 ioctl 打 WARN、`LD_PRELOAD`/`BPFTIME_USED` 被改。
- 新 syscall-server（哪怕非 owner）启动即 `begin_new_session` 清空全表——两个 loader 不能共用一个 shm 名。01 第 2 步只讲"旧 shm 被清"。
- agent 日志在初始化后改写到 **loader** 配置的路径（shm 中 runtime_config）；agent 的 JIT/fuzzy-backtracer 等配置同样来自 loader 的 env。
- agent 持 `shm_lifecycle_lock` 的时间覆盖整个扫表+JIT+挂钩（`agent.cpp:934-1145`），期间新 loader 的 `start_up`/退出清理都会阻塞（推断）。
- IPC 协议细节（status/unknown 返回），`refresh` 是"全拆后重扫"而 auto-refresh 是"增量"；auto-refresh 不会拆掉表里已删除的 link（除非 epoch 变）。
- agent 不 unset `LD_PRELOAD`：目标 exec 的子进程也被注入。
- CLI 五个子命令的具体行为（§3.9 表），尤其 `trace` 的降权、shm chown/chmod、cuobjdump、IPC 优先再 Frida、Ctrl+C 有序关闭顺序；`detach` 是 IPC+SIGUSR1 双发；`attach -s`/`start -s` 的 transformer + `dlmopen(LM_ID_NEWLM)`。
- `/tmp/bpftime-shm-<名>.lock` 永久残留；agent 比 loader 活得久时 shm 永久残留（实验 4）。
- 测试覆盖面：`test_shm_allocation_failure.cpp` 13 个用例覆盖 fail-open/stdio/清理；没有覆盖 agent 成功路径的信号行为、IPC、refresh 异常、mmap 泄漏。

### 8.3 讲得好、可以保留
- 03 §3.1 两个 .so 的职责划分与"两份 runtime 静态库、全局变量不互通"。
- 03 §3.3 `initialize_ctx` 四状态 + `__thread` 防自等待的解释、自测题 3。
- 03 §3.8 SIGUSR1 → 管道 → worker 的信号安全模式讲解；"持锁到登记 pid"的理由（自测题 5）。
- 03 深入参考的 gate 不一致（`epoll_ctl` 不查 `initializing_cuda`）、verifier 只看两种 prog type、SIGUSR1 不链式、`injected_with_frida` 只写不读、mmap 泄漏——均与源码一致。
- 01 第 7 步"必须先启动 loader 并等它加载完，再启动 nginx"——正确且重要（实验 4 证实）。
- 01 第 8 步 epoch 前后检、最多 3 轮、link 递归、防环；`load_prog_and_helpers` 先 JIT 后补注册 attach helper 的观察。
- 行号速查表抽查（`syscall_server_main.cpp:206-236`、`syscall_context.cpp:482-805`、`agent.cpp:830-1177`、`bpf_attach_ctx.cpp:316-419` 等）与源码一致。

## 9. 学习建议

### 9.1 推荐阅读顺序
1. `runtime/syscall-server/syscall-server.version` 与 `runtime/agent/agent.version`（先知道"拦了谁、暴露了谁"）。
2. `syscall_server_main.cpp:206-280`（`initialize_ctx`、`handle_exceptions`、`call_with_context`）→ `:468-533`（`syscall()` 分流）。
3. `syscall_context.cpp:237-254`（`try_startup`）→ `syscall_server_utils.cpp:147-228`（`start_up`）→ `:56-124`（退出清理）。
4. `syscall_context.cpp:482-805`（bpf cmd）→ `:806-935`（perf）→ `:949-1008`（mmap）→ `:1010-1122`（ioctl/epoll）→ 文件 mock（`:256-362,1152-1198` + `syscall_server_utils.cpp:299-350`）。
5. `agent.cpp:535-556`（入口）→ `:830-1177`（主线）→ `:419-529,557-568`（detach）→ `:271-417,676-763`（IPC/refresh）。
6. `bpf_attach_ctx.cpp:73-162`（扫表）→ `:197-267`（递归）→ `:294-510`（三类实例化）→ `:517-579`（拆除）。
7. `tools/cli/main.cpp:877-1290`（子命令分派）→ `:107-298`（run/spawn/inject）。

### 9.2 动手实验（全部在 `/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R3/`，源码 `loader.c`、`victim.c`、`victim2.c`、`hostside.c`、`ipc.py`）
`B=/home/y/src/bpftime-wt-official-177a1dc/build-jit`，每次 `export BPFTIME_GLOBAL_SHM_NAME=notes_R3_$RANDOM`，结束后删 `/dev/shm/notes_R3_*` 与 `/tmp/bpftime-shm-notes_R3_*.lock`。
1. 【已实跑】`nm -D --defined-only $B/runtime/syscall-server/libbpftime-syscall-server.so` → 15 个与 libc 同名符号；agent 导出 7 个（`__libc_start_main`、`bpftime_agent_main`、`bpftime_agent_control`、`bpftime_hooked_main`、`_bpftime__setup_syscall_trace_callback`、`syscall_callback`、`injected_with_frida`）。
2. 【已实跑】无 libbpf 的完整链路：`loader.c` 用裸 `syscall(__NR_bpf/__NR_perf_event_open)` 建 array map、一个"计数 +1"的 KPROBE 型 prog、uprobe perf（type 从被 mock 的 `/sys/.../uprobe/type` 读到 9）、link；
   `BPFTIME_LOG_OUTPUT=$PWD/log.txt LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so ./loader $PWD/victim2 0x1180 10 &`，2 秒后
   `LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so ./victim2 8`。预期 loader 每秒打印的 counter 以 10/秒增长（实测 0,0,0,10,20,…,80）。同一输出还能看到：
   `[1] shm exists at main start: 0`、`[2] … after close(-1): 1, BPFTIME_USED=1, LD_PRELOAD=(unset)`、`[3] … fd=4 -> /tmp/bpftime-mock.oVVTTU (deleted) … '9'`、
   `[4] MAP_CREATE fd=4 -> /dev/null`、`[8] BTF_LOAD … errno=1 (Operation not permitted)`、`[9] OBJ_GET_INFO_BY_FD(fd=0) ret=0 … untouched`、`[10] VmSize before=108140 kB after …=112140 kB`。
3. 【已实跑】detach：同上，victim 跑起来后 `kill -USR1 <victim pid>`，counter 停在 31；日志 `Detaching..` 来自另一个 tid。
4. 【已实跑】启动顺序：先 `LD_PRELOAD=agent ./victim2 8`，1 秒后用 `SLEEP_AFTER_STARTUP=2` 启 loader（先建 shm、2 秒后才建 handler）→ agent 日志 `Attach successfully`，counter 始终 0；且 shm 残留。
5. 【已实跑】IPC：`python3 -I ipc.py <pid> status|detach|"refresh auto_refresh_ms=300"` → `epoch_seq=2` / `ok` / `ok`，counter 停而后恢复。
6. 【已实跑】宿主崩溃：agent 先起，loader 对 `victim` 的 4 字节函数 `target_func`（偏移 0x1169）建 uprobe，再 `ipc.py <pid> "refresh "` → victim 退出码 134，stderr `terminate called after throwing an instance of 'std::runtime_error'`。
7. 【已实跑】宿主副作用：`LD_PRELOAD=server ./hostside` → `epoll_ctl(pipe)=-1 errno=22`、`epoll_wait=0`、`syscall(memfd_create)` 指向 `/dev/null`。
8. 【已实跑】`bpftime_runtime_tests "[syscall_server],[agent]"` → 13 cases / 87 assertions 全过。

### 9.3 学习者最容易误解的点
1. "loader 侧调用都成功了 = 探针已挂上"——错，挂钩只发生在 agent 扫表时；且 agent 是否会再扫表取决于注入方式（只有 `trace`/IPC refresh 会再扫）。
2. "syscall-server 只拦 bpf 相关调用"——它覆盖了 close/open/read/mmap/ioctl/epoll 等通用函数，会影响宿主自身行为（epoll、memfd、每次 mmap 泄漏、ioctl 日志），启动也由任意一个通用调用触发。
3. "fail-open 处处成立"——IPC refresh 路径与扫表中的 attach 异常是例外；SIGUSR1 处理器也会替换宿主的。
4. "shm 由 loader 建、loader 退出就没了"——有活 agent 时不删，之后无人再删；/tmp lock 文件永不删；第二个 loader 会清空第一个 loader 的表。
5. "agent 读自己的环境变量配置"——除 shm 名和启动最早期 logger 外，agent 用的是 loader 写进 shm 的 runtime_config（JIT、fuzzy backtracer、日志路径都跟 loader 走）。

## 10. 未读完/未确定的地方
- 范围内文件全部读完。`demo_ptx_prog.hpp` 只读了首尾并确认无引用，中间 PTX 文本未逐行读。
- `bpf_attach_ctx_cuda.cpp` 读完但无 CUDA 环境，未实跑；`find_nv_attach_impl`、`stop_cuda_watcher_before_shm_unmap`、`set/get_cuda_shared_mem_device_pointer` 的调用点在范围外，需 CUDA/attach 读者确认。
- CLI 未构建，五个子命令全部只读代码；`trace` 的降权/chown 与 Frida 注入未实跑（需 root）。
- `run_with_kernel` 相关（§7.13-15）全部为读码推断，需 root 才能实测。
- §7.6（try_startup 竞态）、§7.17（detach 失败后的悬空 prog）、§7.18-19（无锁读写）为推断，未构造复现。
- `bpftime_epoll_wait`、`bpftime_get_software_perf_event_raw_buffer`、`bpftime_maps_dup`、`bpftime_attach_perf_to_bpf(_with_cookie)` 等内部实现属 R2/R5 范围，只看了调用约定。
- syscall trace（`start -s`）链路只读了 transformer 入口，`setup_syscall_tracer`/`syscall_trace_attach_impl` 内部归 R4；"普通 LD_PRELOAD agent 注册了 syscall_trace impl 但没有 syscall 钩子，tracepoint link 会'挂上'却永不触发"为推断。
