# R5 attach（CPU 侧：base / simple / frida / trap / syscall_trace / text_segment_transformer）源码笔记

基线 `177a1dc465ff`，所有 `路径:行号` 相对 `/home/y/src/bpftime-wt-official-177a1dc`。
「推断」= 读代码得出但没跑过；「未验证」= 没条件实测；「已实跑」= 在本机 x86_64（Debian 13，非 root，`vm.mmap_min_addr=65536`）上跑过。

---

## 0. 一句话职责 + 范围

**职责**：attach 层负责"在对的时机、带着对的 ctx 调 runtime 给的 `ebpf_run_callback`"。本块包括：所有 attach 后端共用的接口契约（`base_attach_impl`）、
函数插桩的两个后端（x86_64/aarch64/armhf/macOS-arm64 用 Frida inline hook；riscv64 用断点 + SIGTRAP 的 trap 后端）、
系统调用跟踪（`syscall_trace_attach_impl` 负责分发，`text_segment_transformer` 负责把 `syscall` 指令改写成 `call *%rax` 拦截下来，zpoline 思路），
以及给宿主程序自定义事件源用的 `simple_attach_impl`。

**读了的目录/文件**：
- `attach/CMakeLists.txt`
- `attach/README.md`
- `attach/base_attach_impl/`（全部）
- `attach/simple_attach_impl/`（全部）
- `attach/frida_uprobe_attach_impl/`（全部，含 test/）
- `attach/trap_uprobe_attach_impl/`（全部，含 test/、README）
- `attach/syscall_trace_attach_impl/`（全部，含 test/、README、生成脚本）
- `attach/text_segment_transformer/`（全部，含 README）
- `cmake/frida.cmake`（全文 88 行）、`cmake/StandardSettings.cmake:76-82`（后端选择）
- `example/attach_implementation/`：核心部分（README、CMakeLists、nginx_plugin、nginx_plugin_adaptor、controller、nginx.conf）通读；`benchmark/` 只看了目录和 README 开头
- 为核对跨模块接口，另外看过（不在范围内，只读相关片段）：`runtime/agent/agent.cpp:955-1070,1180-1210`、`runtime/src/attach/bpf_attach_ctx.cpp:269-300,316-410,420-500,517-540`、`runtime/src/bpf_helper.cpp:662-682,780-800,895-910`、`runtime/src/bpftime_prog.cpp:235-250,289-330`、`runtime/agent/CMakeLists.txt`、`runtime/CMakeLists.txt:167-168`、`tools/cli/main.cpp:1005-1045`、`CMakeLists.txt:28-37,138-139,215-230`、`runtime/include/bpftime_shm.hpp:47-62`

**总行数**：`attach/` 去掉 `nv_attach_impl` 共 8763 行（`find attach -path attach/nv_attach_impl -prune -o -type f | xargs wc -l`），其中 trap 后端约 4700 行（引擎 1342 + 测试约 1900）。
example/attach_implementation 全目录 6273 行，核心部分约 530 行，其余是 benchmark（Python/各种插件）。

---

## 1. 文件清单

### 1.1 attach 根 / base / simple

| 文件 | 行数 | 做什么（一句话） | 必读？ |
|---|---|---|---|
| `attach/CMakeLists.txt` | 23 | 按 `BPFTIME_ENABLE_FRIDA` / `BPFTIME_ENABLE_TRAP_UPROBE` / 平台决定编哪些子目录 | 必读 |
| `attach/README.md` | 199 | attach 系统概述（有过时/不准处，见 §7） | 选读 |
| `attach/base_attach_impl/CMakeLists.txt` | 13 | INTERFACE 库，只导出头文件路径 | 跳过 |
| `attach/base_attach_impl/base_attach_impl.hpp` | 142 | 接口契约：`base_attach_impl` 虚基类、`ebpf_run_callback`、TLS 变量、`bpftime_set_retval`/`bpftime_override_return` | **必读** |
| `attach/base_attach_impl/attach_private_data.hpp` | 34 | private data 基类（`initialize_from_string` / `to_string`） | 必读 |
| `attach/base_attach_impl/bpftime_pt_regs.hpp` | 140 | 与内核 `bpf_tracing.h` 同布局的 `pt_regs`（x86_64/aarch64/riscv64/arm）+ `PT_REGS_*` 宏；trap 后端用 | 必读 |
| `attach/simple_attach_impl/CMakeLists.txt` | 11 | 静态库 | 跳过 |
| `attach/simple_attach_impl/simple_attach_impl.hpp` | 128 | 最简 attach 后端 + `add_simple_attach_impl_to_attach_ctx` 模板 | 必读（写自定义 attach 的入口） |
| `attach/simple_attach_impl/simple_attach_impl.cpp` | 55 | create/detach/trigger 实现 | 必读 |

### 1.2 frida_uprobe_attach_impl

| 文件 | 行数 | 做什么 | 必读？ |
|---|---|---|---|
| `CMakeLists.txt` | 72 | 静态库 + 10 个测试源 + stripped backtrace fixture so | 跳过 |
| `include/frida_uprobe_attach_impl.hpp` | 112 | attach type 常量、callback variant、`frida_attach_impl` 类 | **必读** |
| `include/frida_attach_entry.hpp` | 80 | 用户视角 entry（按 id），`run_callback<idx>` | **必读** |
| `include/frida_internal_attach_entry.hpp` | 50 | Frida 视角 entry（按函数地址），持有 listener | **必读** |
| `include/frida_attach_private_data.hpp` | 22 | `addr` + `module_name` | 选读 |
| `include/frida_attach_utils.hpp` | 41 | 地址解析工具 + 3 个自定义 helper 声明 | 选读 |
| `include/frida_register_conversion.hpp` | 36 | GumCpuContext↔pt_regs 声明 | 选读 |
| `include/frida_register_def.hpp` | 78 | x86_64/aarch64/arm 的 `pt_regs`（与 bpftime_pt_regs.hpp 共用 include guard） | 必读 |
| `include/frida_uprobe.hpp` | 10 | 聚合头 | 跳过 |
| `src/frida_uprobe_attach_impl.cpp` | 335 | 构造/attach_at/detach/create_attach_with_ebpf_callback/helpers/generate_stack | **必读** |
| `src/frida_internal_attach_entry.cpp` | 318 | ensure_listener、内部 entry 构造/析构、override handler、on_enter/on_leave | **必读** |
| `src/frida_attach_entry.cpp` | 14 | `get_type()` | 跳过 |
| `src/frida_attach_private_data.cpp` | 42 | 解析 "地址" 或 "module:offset" | 选读 |
| `src/frida_attach_utils.cpp` | 212 | map_files 路径还原、模块基址、符号查找、get_func_arg/ret/retval | 选读 |
| `src/frida_register_conversion.cpp` | 114 | 寄存器转换三套架构 | **必读** |
| `test/test_uprobe_uretprobe.cpp` | 170 | uprobe/uretprobe 叠加与撤销 | 选读 |
| `test/test_filter_attach_with_override.cpp` | 60 | override（filter） | 选读 |
| `test/test_replace_attach_with_override.cpp` | 56 | override 必然覆盖（replace 语义） | 选读 |
| `test/test_attach_with_unified_interface.cpp` | 63 | 通过 base 接口 + private data 挂 | 选读 |
| `test/test_attach_private_data_parsing.cpp` | 15 | "aaa:" → -EINVAL | 跳过 |
| `test/test_attach_failure_diagnostics.cpp` | 48 | 小函数挂钩失败的诊断信息 | 跳过 |
| `test/test_base_attach_impl.cpp` | 32 | `bpftime_set_retval`/`override_return` 无回调时抛异常 | 选读 |
| `test/test_back_trace.cpp` | 129 | generate_stack accurate/fuzzy | 选读 |
| `test/stripped_back_trace_fixture.cpp` | 32 | 无 unwind 信息的 5 层调用链 | 跳过 |
| `test/test_function_address_resolve.cpp` | 76 | 符号/模块偏移/map_files 解析 | 跳过 |
| `test/test_register_conversion.cpp` | 31 | x86 转换清零 orig_ax/cs/flags/ss | 选读 |

### 1.3 trap_uprobe_attach_impl

| 文件 | 行数 | 做什么 | 必读？ |
|---|---|---|---|
| `CMakeLists.txt` | 59 | 库 + 测试 + late injection host/module；`-Wl,-z,now`；测试 `--wrap=mprotect` | 选读 |
| `README.md` | 168 | 机制、宿主契约、限制、性能表 | **必读** |
| `include/trap_uprobe_attach_impl.hpp` | 127 | 公共类 `trap_attach_impl`、attach type 常量（与 Frida 同号） | 必读 |
| `include/trap_attach_private_data.hpp` | 30 | private data | 选读 |
| `include/trap_attach_utils.hpp` | 41 | 不依赖 Frida 的符号/模块解析 | 选读 |
| `src/trap_uprobe_attach_impl.cpp` | 1342 | 引擎：site 表（RCU）、线程槽池、slot 分配、三阶段写、SIGTRAP 处理、shadow stack、公共 API | **必读**（重点 33-330, 488-560, 656-762, 904-1069） |
| `src/trap_arch.hpp` | 93 | 架构接口（decode/trap_bytes/ucontext 读写/emulate） | 必读 |
| `src/trap_arch_riscv64.cpp` | 325 | 唯一的架构实现：RISC-V 解码、ebreak、模拟 pc 相对/跳转指令 | 选读 |
| `src/trap_attach_private_data.cpp` | 59 | 解析与 to_string | 选读 |
| `src/trap_attach_utils.cpp` | 295 | dl_iterate_phdr 枚举模块、读 ELF symtab 找非导出符号 | 选读 |
| `test/trap_test_common.hpp` | 12 | `TRAP_TEST_TARGET` 宏（noinline/noclone/used） | 跳过 |
| `test/test_trap_uprobe_uretprobe.cpp` | 219 | 基本 uprobe/uretprobe、递归、多实例独立 | 选读 |
| `test/test_trap_override.cpp` | 102 | filter/replace、双向互斥 | 选读 |
| `test/test_trap_unified_interface.cpp` | 93 | base 接口 + 非法 private data/type | 选读 |
| `test/test_trap_misc.cpp` | 188 | 空回调拒绝、回调内 detach 拒绝、private data、符号解析、SIGTRAP 链式转发 | 选读 |
| `test/test_trap_multithread.cpp` | 120 | 8 线程不丢事件；热函数上反复 attach/detach | 选读 |
| `test/test_trap_backtrace.cpp` | 42 | generate_stack 恒为 null | 跳过 |
| `test/test_trap_recursion.cpp` | 85 | 回调里调被探测函数；探测 malloc | 选读 |
| `test/test_trap_insn_decode.cpp` | 222 | RISC-V 各类首指令模拟、拒绝已有断点、三阶段写 | 选读（riscv only） |
| `test/test_trap_signal_safety.cpp` | 108 | 分配器压力、>1024 线程槽回收 | 选读 |
| `test/test_trap_concurrent_patch.cpp` | 215 | 并发 arm/disarm、2 mod 4 地址三阶段写并发 | 选读 |
| `test/test_trap_patch_failure.cpp` | 200 | 用 `--wrap=mprotect` 注入权限失败，测回滚 | 选读 |
| `test/late_injection_host.cpp` | 74 | 先起 worker 再 dlopen 模块 | 跳过 |
| `test/late_injection_module.cpp` | 87 | 包裹 malloc/free/mutex/__tls_get_addr，统计信号处理里的不安全调用 | 选读 |

### 1.4 syscall_trace_attach_impl

| 文件 | 行数 | 做什么 | 必读？ |
|---|---|---|---|
| `CMakeLists.txt` | 62 | 生成 `syscall_id_list.h`、库、测试 | 选读 |
| `generate_syscall_id_table.sh` | 5 | 用宿主 `cpp -dM` 展开 `<sys/syscall.h>` 生成 "name nr" 表 | 选读 |
| `README.md` | 58 | 概述 | 选读 |
| `include/syscall_trace_attach_impl.hpp` | 103 | ctx 结构（`trace_event_raw_sys_enter/exit`）、`ATTACH_SYSCALL_TRACE=2`、类 | **必读** |
| `include/syscall_trace_attach_private_data.hpp` | 21 | `sys_nr`(-1=全部)、`is_enter` | 必读 |
| `include/syscall_table.hpp` | 34 | tracepoint id 表 / syscall 名↔号表 | 选读 |
| `include/syscall_trace_attach.hpp` | 8 | 聚合头 | 跳过 |
| `src/syscall_trace_attach_impl.cpp` | 195 | `dispatch_syscall`、attach/detach、（死代码）`_bpftime__setup_syscall_hooker_callback` | **必读** |
| `src/syscall_trace_attach_private_data.cpp` | 63 | tracepoint id → (sys_nr, is_enter) | 必读 |
| `src/syscall_table.cpp` | 100 | 读 `/sys/kernel/tracing/events/syscalls/*/id` | 选读 |
| `test/test_private_data_parsing.cpp` | 60 | 解析 global enter/exit、fork | 跳过 |
| `test/test_syscall_dispatch.cpp` | 210 | 分发计数、回调异常被吞 | 选读 |

### 1.5 text_segment_transformer

| 文件 | 行数 | 做什么 | 必读？ |
|---|---|---|---|
| `CMakeLists.txt` | 22 | SHARED 库，输出名 `libbpftime-agent-transformer.so`，链 frida-gum（为了 capstone） | 选读 |
| `README.md` | 71 | 概述（未提 `mmap_min_addr`） | 选读 |
| `text_segment_transformer.hpp` | 20 | `setup_syscall_tracer` / `get_call_hook` / `set_call_hook` | 必读 |
| `text_segment_transformer.cpp` | 330 | 第 0 页蹦床、汇编 `syscall_hooker_asm`/`call_orig_syscall`、capstone 扫描改写 | **必读** |
| `agent-transformer.cpp` | 102 | LD_PRELOAD/Frida 入口：改写后用 `dlmopen` 把 agent 装进新命名空间并接上 hook | **必读** |

### 1.6 cmake

| 文件 | 行数 | 做什么 | 必读？ |
|---|---|---|---|
| `cmake/frida.cmake` | 88 | 下载 frida core/gum devkit 16.1.2，按 OS-arch 选包 | 选读 |
| `cmake/StandardSettings.cmake:76-82` | 7 | riscv → trap ON/Frida OFF，其余反之（`FORCE`） | **必读** |

### 1.7 example/attach_implementation（略读）

| 文件 | 行数 | 做什么 | 必读？ |
|---|---|---|---|
| `README.md` | 52 | 例子说明：nginx 模块用 eBPF 过滤 URL 前缀 | 选读 |
| `CMakeLists.txt` | 55 | 编 adaptor so、controller、下载编 nginx 1.22.1 + 动态模块 | 跳过 |
| `nginx_plugin_adaptor/nginx_plugin_adaptor.cpp` | 67 | **核心**：宿主自建 `bpf_attach_ctx`，用 `add_simple_attach_impl_to_attach_ctx` 注册 attach type 2001 | **必读** |
| `nginx_plugin/ngx_http_bpftime_module.c` | 136 | nginx ACCESS 阶段 handler，调 adaptor 的 `nginx_plugin_example_run_filter` | 选读 |
| `nginx_plugin/config` | 7 | nginx 模块配置，链接 adaptor | 跳过 |
| `nginx_plugin/.clangd` | 9 | IDE 配置 | 跳过 |
| `controller/controller.cpp` | 105 | 往 shm 写 prog/map/custom perf event(2001)/link，epoll 读 ringbuf | 选读 |
| `controller/request_filter.bpf.c` | 41 | eBPF：比较前缀、写 ringbuf | 选读 |
| `controller/request_filter.h` | 14 | ctx 与事件结构 | 选读 |
| `controller/CMakeLists.txt` | 22 | — | 跳过 |
| `nginx.conf` / `index.html` / `.gitignore` | 24/1/17 | 运行配置 | 跳过 |
| `benchmark/**`（约 5200 行：run_benchmark.py、utils.py、visualization.py、baseline/dynamic_load/wasm/luajit/rlbox/erim 插件、ebpf_controller、*.conf、结果 json） | — | 对比 eBPF 模块与原生 C/dlopen/Wasm/Lua/RLBox/ERIM 过滤器的 nginx 吞吐 | 跳过（未逐行读） |

---

## 2. 核心数据结构

### 2.1 base_attach_impl 契约（`attach/base_attach_impl/base_attach_impl.hpp`）

- `ebpf_run_callback = std::function<int(void *memory, size_t memory_size, uint64_t *return_value)>`（:42-43）。
  runtime 构造、后端调用；`memory` 就是交给 eBPF 的 ctx（uprobe 是 `pt_regs*`，syscall trace 是 `trace_event_raw_sys_enter/exit*`，simple 是宿主自定义结构）。
- `ebpf_helper_register_callback = std::function<int(unsigned index, const char *name, void *func)>`（:46-47）。
- `class base_attach_impl`（:49-89）：
  - 纯虚 `detach_by_id(int id)`（:52）、`create_attach_with_ebpf_callback(cb&&, const attach_private_data&, int attach_type)`（:59-61）。
  - `allocate_id()` 返回 `next_id++`（:64-67），`next_id` 初值 1（:88）。**id 只在单个 impl 实例内唯一**，不同 impl 可以返回同一 id；runtime 用 `(attach_id, impl*)` 对来记（`runtime/src/attach/bpf_attach_ctx.cpp:412`、:522）。
  - 可选虚函数 `register_custom_helpers`（默认空，:71-74）、`call_attach_specific_function(name, data)`（默认 WARN 返回 null，:77-83）。
- 线程局部状态：
  - `curr_thread_override_return_callback`（`inline thread_local std::optional<std::function<void(u64,u64)>>`，:19-20）：Frida override 与 syscall trace 用来把 `bpf_override_return` 的值"通知"给当前 hook 点。
  - `current_thread_attach_func_ip`（:22）：`bpf_get_func_ip` 的非信号路径数据源（`runtime/src/bpf_helper.cpp:677-682`）。
  - `signal_callback_context { function_ip; optional<u64> cookie; void(*override_return)(u64,u64); }`（:27-31）＋ `current_signal_callback`（initial-exec TLS 指针，:33-35）＋ `signal_handler_depth`（:36-39）：trap 后端专用。指针非空即"当前在 SIGTRAP 处理函数里"。
- `bpftime_set_retval(value)`（:97-115）、`bpftime_override_return(ctx, value)`（:118-138）：先看 `current_signal_callback`（有则调其 `override_return`，为空返回 `-EINVAL`）；否则调 `curr_thread_override_return_callback`，**都没有就 `throw std::invalid_argument`**。这两个就是 helper 58（override_return）和 187（set_retval）的实现（`runtime/src/bpf_helper.cpp:1280,1298`）。
- `attach_private_data`（`attach_private_data.hpp:12-31`）：虚 `initialize_from_string(sv)`、`to_string()`，默认实现都 `throw std::runtime_error`。runtime 先用注册时给的工厂 lambda 生成 private data（`bpf_attach_ctx.cpp:440-500`），再交给 `create_attach_with_ebpf_callback`；后端内部用 `dynamic_cast` 取具体类型。

**生命周期**：impl 对象由 runtime `bpf_attach_ctx::register_attach_impl` 以 `unique_ptr` 持有（`bpf_attach_ctx.cpp:269-294`，放进 `attach_impl_holders`，按 attach type 建索引 `attach_impls[ty]`），进程堆，随 attach ctx 销毁。

### 2.2 pt_regs（`attach/base_attach_impl/bpftime_pt_regs.hpp`、`attach/frida_uprobe_attach_impl/include/frida_register_def.hpp`）

- 两个头**共用 include guard `_BPFTIME_FRIDA_PT_REGS`**（`bpftime_pt_regs.hpp:13-18`，`frida_register_def.hpp:1-2`），谁先被包含谁生效。x86_64/aarch64/arm 定义一致；只有 `bpftime_pt_regs.hpp` 有 riscv64（:76-122）。
- x86_64：21 字段、内核顺序 `r15..r8 ax cx dx si di orig_ax ip cs flags sp ss`（`bpftime_pt_regs.hpp:26-48`），PARM1-6=`di si dx cx r8 r9`，`RET=sp`，`RC=ax`（:49-56）。
- aarch64：`regs[31] sp pc pstate`（:59-64），PARM1-8=x0-x7，`RET=regs[30]`（lr）。
- riscv64：`epc ra sp gp tp t0-2 s0-1 a0-7 s2-11 t3-6`，顺序 = `ucontext.__gregs[]`（:79-112），所以 trap 后端可以按下标整块拷。
- arm32：`uregs[18]`。
- **生命周期**：永远是回调栈上的局部变量（Frida：`frida_internal_attach_entry.cpp:298,315,260`；trap：`trap_uprobe_attach_impl.cpp:923,1000`），单向拷贝，回调返回即失效。

### 2.3 simple_attach_impl（`attach/simple_attach_impl/simple_attach_impl.hpp`）

- `callback_type = function<int(const string &attach_arg, const string &trigger_arg, const ebpf_run_callback &)>`（:19-21）。
- 类字段（:46-56）：`callback`（宿主给的"怎么准备 ctx 并跑 eBPF"）、`predefined_attach_type`、`ebpf_callback`、`usable_id`（-1 表示未挂）、`argument`（attach 时的字符串）。**每个实例只能挂一个**（`simple_attach_impl.cpp:11-14`）。
- `simple_attach_private_data { std::string data; }`（:59-69），`initialize_from_string` 直接存字符串。
- `add_simple_attach_impl_to_attach_ctx<T>(type, cb, attach_ctx)`（:100-123）：SFINAE 检查 `T` 有 `register_attach_impl`（:72-92，"替代 concept"），new 一个 impl、注册 `{type}` 和 private data 工厂，返回 `[impl_ptr](const string &s){ return impl_ptr->trigger(s); }`。返回的 trigger 闭包裸持 impl 指针，impl 归 attach ctx 所有。

### 2.4 Frida 后端：两层实体

**`frida_attach_impl`**（`include/frida_uprobe_attach_impl.hpp:65-108`）
- `interceptor`（`void*`，实为 `GumInterceptor*`，构造时 `gum_init_embedded(); gum_interceptor_obtain()`，`src/frida_uprobe_attach_impl.cpp:36-42`；析构 `gum_object_unref`，:31-34）。
- `attaches: unordered_map<int, unique_ptr<frida_attach_entry>>`（:99-100）——按 attach id。
- `internal_attaches: unordered_map<void*, unique_ptr<frida_internal_attach_entry>>`（:101-103）——按函数地址。
- `use_fuzzy_backtracer`（:104）。
- 无任何锁（推断：所有 attach/detach 默认在 agent 初始化线程做；与命中线程并发修改 `user_attaches` 是数据竞争，见 §6）。

**`frida_attach_entry`**（用户视角，`include/frida_attach_entry.hpp:14-76`）
- `self_id`、`callback`（`variant<callback_variant, ebpf_callback_args>`，前者是三种 `function<void(const pt_regs&)>`（单测用），后者是 `{ebpf_cb, attach_type}`（生产路径），`frida_uprobe_attach_impl.hpp:39-61`）、`function`（被 hook 地址）、`internal_attach`（反向裸指针）。
- `run_callback<idx>`（:24-61）：RAII 保存/恢复 `current_thread_attach_func_ip`；uprobe/uretprobe 设为函数地址，override 设为 0；调 native 回调或 `ebpf_cb(&regs, sizeof(regs), &ret)`，`ret` 丢弃，`err<0` 只打日志。
- `get_type()`（`src/frida_attach_entry.cpp:6-14`）：native 回调按 variant index 映射，eBPF 回调返回存的 `attach_type`。

**`frida_internal_attach_entry`**（Frida 视角，`include/frida_internal_attach_entry.hpp:15-47`）
- `function`、`interceptor`（构造末尾 `gum_object_ref`，析构 unref）、`user_attaches: vector<frida_attach_entry*>`、`uprobe_listener`、`uretprobe_listener`。
- override 状态：`is_overrided`、`user_ret`、`user_ret_ctx`、`override_return_callback`（:27-31）——**普通成员，不是 TLS**。
- 析构（`src/frida_internal_attach_entry.cpp:182-197`）：对每个非空 listener `gum_interceptor_detach + g_object_unref`；两个 listener 都空（说明是 replace）才 `gum_interceptor_revert`。

**attach type 常量**（`frida_uprobe_attach_impl.hpp:22-35`）：`ATTACH_UPROBE=6`、`ATTACH_URETPROBE=7`、`ATTACH_UPROBE_OVERRIDE=1008`、`ATTACH_UREPLACE=1009`；variant index 0/1/2。与 `runtime/include/bpftime_shm.hpp:57-61` 的 `bpf_event_type` 一致。注释 6/7 写反（见 §7）。

**`frida_attach_private_data`**（`include/frida_attach_private_data.hpp:11-18`）：`uint64_t addr`（**无初值**）、`module_name`；无 `to_string` 覆盖（调用会抛）。

### 2.5 trap 后端（`attach/trap_uprobe_attach_impl/src/trap_uprobe_attach_impl.cpp`）

全局唯一引擎 `trap_engine`（:276-330，`get()` 是函数内 static，:278-282；`trap_attach_impl` 构造时先触发一次，保证不在信号处理里首次构造，:1119-1125）。

| 结构 | 位置 | 字段/作用 | 生命周期 |
|---|---|---|---|
| `attach_entry` | :42-73 | `id type function owner callback`；`atomic<bool> enabled`；`atomic<unsigned> active_callbacks`；`run<idx>()` 先 ++active 再查 enabled，设置 `current_signal_callback->function_ip` | 引擎 `entries` vector 持有，**进程生命周期内永不释放** |
| `probe_site` | :83-100 | `addr`、`info`（长度+执行方式）、`orig[4]`、`trap[4]`、`trap_len`、`slot`/`slot_trap`（out-of-line 槽）、`armed`、`protections_dirty`、`protections`（每页原权限）、`entries`、`has_override/has_uprobe/has_uretprobe` | **发布后不可变**；修改 = 复制一份新 site 再发布；全部留在 `sites` 里永不释放 |
| `site_table` | :102-127 | `by_addr`（按地址排序）、`by_slot_trap`（按 slot 里第二个断点地址排序），都二分查找 | RCU 式：`atomic<site_table*> table` + 历史表全部留在 `tables` 里不释放（:311-319, :631-635） |
| `uret_frame`/`uret_stack` | :129-138 | `{function, orig_ret, sp}`，深度 128（`URET_STACK_DEPTH`，:36） | 在线程槽里 |
| `override_state` | :140-144 | `is_overrided value ctx` | 在线程槽里 |
| `trap_tls` | :149-160 | `owned`、`in_handler`、`uret`、`current_regs`、`current_pc/sp`、`phase`(1=入口,2=返回)、`return_value`、`override`、`callback`（`signal_callback_context`） | **静态数组 `constinit thread_slots[1024]`**（:161-162）；线程首次命中时 CAS 认领（:171-185），handler 末尾若空闲（不在 handler 且 uret 深度 0）归还（:187-194）；线程只持有 initial-exec TLS 指针 `tl_slot`（:168） |
| `code_region` | :207-212 | slot 内存区（RWX 页里每 32 字节一个 slot；W^X 回退时一页一个 slot） | 永不释放 |
| `page_protection` | :77-81 | 构建 site 时从 `/proc/self/maps` 读到的原权限 | 随 site |
| 引擎成员 | :317-329 | `mutex mu`（只保护控制面）、`table`、`tables`、`sites`、`slot_sites`、`entries`、`regions`、`uret_trampoline`、`previous_action`、`installed` | 进程级 |

`trap_attach_impl`（`include/trap_uprobe_attach_impl.hpp:75-113`）本身只存 `attaches: unordered_map<int, owned_entry{function,type}>`，真正的 entry 在引擎里；析构调 `detach_owner(this)`（:1127-1131）。

`trap::arch` 接口（`src/trap_arch.hpp:27-87`）：`insn_info{len, kind(execute_out_of_line|emulate)}`、`decode`、`trap_bytes`、`trap_pc/set_pc/get_sp/get/set_return_address/do_return/get_return_value`、`fill_pt_regs`、`get_arg`、`prepare_out_of_line`、`emulate`、`flush_icache`。**唯一实现是 `trap_arch_riscv64.cpp`，且该文件没有 `#if __riscv` 保护、直接用 `uc_mcontext.__gregs`**（:41-49,146-190），CMake 无条件编它（`CMakeLists.txt:1-7`），所以 trap 后端只能在 riscv64 上编译（推断）。

### 2.6 syscall_trace_attach_impl（`include/syscall_trace_attach_impl.hpp`）

- ctx：`trace_entry{u16 type; u8 flags; u8 preempt_count; int pid}`（:17-22，8 字节）、`trace_event_raw_sys_enter{ent; long id; ulong args[6]; char __data[0]}`（:24-29）、`trace_event_raw_sys_exit{ent; long id; long ret}`（:31-36）。与内核 tracepoint 布局一致；**`ent` 全 0**（`syscall_trace_attach_impl.cpp:58,84` memset）。
- `ATTACH_SYSCALL_TRACE = 2`（:39，= `PERF_TYPE_TRACEPOINT`，`runtime/include/bpftime_shm.hpp:51`）。
- `syscall_trace_attach_entry{cb, sys_nr, is_enter}`（:42-46）。
- 类字段（:90-98）：`orig_syscall`、`set<entry*> global_enter_callbacks, global_exit_callbacks`、`set<entry*> sys_enter_callbacks[512], sys_exit_callbacks[512]`、`unordered_map<int, unique_ptr<entry>> attach_entries`。无锁。
- 全局 `optional<syscall_trace_attach_impl*> global_syscall_trace_attach_impl`（`src/syscall_trace_attach_impl.cpp:16`），`set_to_global()` 写入（hpp:75-78），agent 在 `reset_syscall_trace_global` 清掉（`runtime/agent/agent.cpp:139-146`）。
- private data：`sys_nr`（-1=所有）、`is_enter`（`include/syscall_trace_attach_private_data.hpp:9-17`），字段均无初值。
- syscall 表：`get_global_syscall_tracepoint_name_table()`（tracepoint id → 名字，含 `raw_syscalls/sys_enter|sys_exit` → `"*SYS_ENTER"/"*SYS_EXIT"`，`src/syscall_table.cpp:58-98`）；`get_global_syscall_id_table()`（编译期从宿主头文件生成的 name↔nr，:17-56）。都是函数内 `static optional`，首次调用时构造（非线程安全的懒初始化，C++ 只保证 static 本身初始化线程安全，这里是 `if (!value) value = ...`，推断有竞争）。

### 2.7 text_segment_transformer

- 静态 `call_hook`（`text_segment_transformer.cpp:52`），初值 `call_orig_syscall`；`get_call_hook/set_call_hook`（:224-231）。
- 第 0 页：`[0,512)` 全是 `0x90`，`[512,525)` 是 `push %rax; movabs $syscall_hooker_asm,%rax; jmp *%rax`（:237-276），然后 `mprotect(0, 0x1000, PROT_EXEC)`（:279）。
- `MapEntry{begin,end,w,r,x}`（:206-220）——扫 `/proc/self/maps` 用。
- agent-transformer 的静态：`orig_main_func`、`injected_with_frida`（默认 true，被 `__libc_start_main` 置 false）（`agent-transformer.cpp:16-21`）。

---

## 3. 主要流程与调用链

### 3.1 后端装配（agent 启动时，编译期二选一）

`bpftime_agent_main` (`runtime/agent/agent.cpp`) → `ctx_holder.init()` → 
（`#if __linux__ && BPFTIME_BUILD_WITH_LIBBPF`）`register_attach_impl({ATTACH_SYSCALL_TRACE}, make_unique<syscall_trace_attach_impl>(), 工厂)` (`agent.cpp:964-988`)，先 `set_original_syscall_function(orig_hooker)` →
`#if BPFTIME_HAVE_TRAP_UPROBE_ATTACH` 注册 `trap_attach_impl` 负责 `{6,7,1008,1009}` (`agent.cpp:991-1012`) `#elif BPFTIME_HAVE_FRIDA_ATTACH` 注册 `frida_attach_impl(enable_frida_fuzzy_backtracer)` (`agent.cpp:1013-1033`) `#else` WARN 无后端 (`:1034`) →
`syscall_trace_impl_ptr->set_to_global()` (`:1058-1060`) → `init_attach_ctx_from_handlers` (`:1063` 起)。

- `register_attach_impl` (`runtime/src/attach/bpf_attach_ctx.cpp:269-294`) 先调 `impl->register_custom_helpers`，把后端 helper 直接写进 `this->helpers[idx]`（覆盖同号全局 helper），再记 `attach_impls[type] = (impl*, 工厂)`。
- 宏来源：`runtime/agent/CMakeLists.txt` 在 `BPFTIME_ENABLE_FRIDA` 时定义 `BPFTIME_HAVE_FRIDA_ATTACH=1`（:80），存在 `bpftime_trap_uprobe_attach_impl` 目标时定义 `BPFTIME_HAVE_TRAP_UPROBE_ATTACH=1`（:94）；而这两个开关由 `cmake/StandardSettings.cmake:76-82` 按 `CMAKE_SYSTEM_PROCESSOR` **FORCE** 设置（riscv → trap，否则 Frida）。

### 3.2 uprobe/uretprobe/override/ureplace 建立（Frida 后端）

`bpf_attach_ctx::instantiate_perf_event_handler_at` (`bpf_attach_ctx.cpp:420`) 拼 `"module:offset"` (`:448-451`) → 工厂 → `frida_attach_private_data::initialize_from_string` (`src/frida_attach_private_data.cpp:10-42`)
→ `resolve_mapped_module_path` (`src/frida_attach_utils.cpp:67-108`，只处理 `/proc/<self>/map_files/<a>-<b>`，在 maps 里找重叠行并校验 inode/dev) → `resolve_function_addr_by_module_offset` (`:110-135`，`gum_module_load` + `gum_module_find_base_address` + offset；空名或等于 `/proc/self/exe` → 主程序)。

`bpf_attach_ctx::instantiate_bpf_link_handler_at` (`bpf_attach_ctx.cpp:316`) 构造 lambda（设 cookie → `prog->bpftime_prog_exec`，`:397-406`）→ `frida_attach_impl::create_attach_with_ebpf_callback` (`src/frida_uprobe_attach_impl.cpp:175-279`)：
1. `dynamic_cast` 成 `frida_attach_private_data`，失败 `-EINVAL`（:182-183, 273-278）。
2. `module_name` 非空时逐行读 `/proc/self/maps`，用 `std::filesystem::equivalent` 比对，找不到返回 `-EINVAL`（只打 INFO，:190-230）——LD_PRELOAD 到无关进程时的正常路径。
3. 6/7/1008 → `attach_at_with_ebpf_callback(addr, {cb, type})`（:233-237）；1009 → 包一层 lambda：先跑 cb，成功后 `bpftime_set_retval(*return_value)`，以 **1008** 注册（:238-265）；其它 `-ENOTSUP`。

→ `attach_at` (`:48-97`)：地址 0 → `-EINVAL`；查 `internal_attaches`：没有则 `new frida_internal_attach_entry(addr, type, interceptor)`；有且 `has_override()` → `-EEXIST`（只单向检查）；然后 `ensure_listener(type)`；分配 id，建 `frida_attach_entry`，双向链接。

→ `frida_internal_attach_entry` 构造 (`src/frida_internal_attach_entry.cpp:145-180`)：
- 6/7：`ensure_listener` (`:115-143`)：uprobe 用 `gum_make_probe_listener(uprobe_listener_on_enter, this)`，uretprobe 用 `gum_make_call_listener(nullptr, uprobe_listener_on_leave, this)`，在 transaction 里 `gum_interceptor_attach(function, listener)`；失败构造诊断信息（`build_frida_attach_failure_message`，:99-111）并 `throw runtime_error`。
- 1008：transaction 里 `gum_interceptor_replace(function, __bpftime_frida_attach_manager__override_handler, this)`，失败抛；装 `override_return_callback = [&](ctx,v){ is_overrided=true; user_ret=v; user_ret_ctx=ctx; }`。
- 注意构造里抛出的 `runtime_error` 不会被 `create_attach_with_ebpf_callback` 捕获（它只 catch `bad_cast`），一直冒到 runtime/agent（推断）。

**拆除**：`detach_by_id` (`src/frida_uprobe_attach_impl.cpp:126-151`)：从内部 entry 的 `user_attaches` 里删，删 `attaches[id]`；`user_attaches` 空了才 `internal_attaches.erase` → 析构 → `gum_interceptor_detach`/`revert`。**listener 只增不减**：同址 uretprobe 全拆而 uprobe 还在时 call listener 留着。`detach_by_func_addr` (`:159-172`) 一次删掉该址全部。

### 3.3 Frida 命中（热路径）与回调分发

- uprobe：Frida 跳板 → `uprobe_listener_on_enter(ctx, user_data=internal entry)` (`src/frida_internal_attach_entry.cpp:293-308`) → 栈上 `pt_regs` ← `convert_gum_cpu_context_to_pt_regs` → 设 `current_thread_gum_cpu_context`（给 generate_stack）→ `iterate_uprobe_callbacks` (`:235-243`)：**遍历全部 user_attaches，按 `get_type()==6` 现场过滤** → `run_callback<0>` → `ebpf_cb` → runtime lambda → JIT 代码 → reset gum ctx。
- uretprobe：Frida 离开跳板 → `uprobe_listener_on_leave` (`:310-318`) → 转换 → `iterate_uretprobe_callbacks` (`:245-253`)；**不设** `current_thread_gum_cpu_context`，所以 uretprobe 里 `generate_stack` 返回 null（`src/frida_uprobe_attach_impl.cpp:299-302`）→ helper 返回 `-ENOENT`（`runtime/src/bpf_helper.cpp:795-798`）。
- override：函数入口被替换为 `__bpftime_frida_attach_manager__override_handler` (`:257-291`)：`gum_interceptor_get_current_invocation` → 转换 regs → `gum_invocation_context_get_replacement_data` 取内部 entry → `is_overrided=false` → **把 `curr_thread_override_return_callback` 设成本 entry 的回调（之后从不 reset）** → 取前 5 个参数 → `run_filter_callback`（找第一个 1008 entry 跑，`:220-233`，找不到抛）→ 若 `is_overrided` 返回 `user_ret`，否则 `func(arg0..arg4)` 重放原函数。

### 3.4 trap 后端：建立 → 命中 → 原指令 → uretprobe → 拆除

**建立**：`trap_attach_impl::create_attach_with_ebpf_callback` (`src/trap_uprobe_attach_impl.cpp:1230-1284`)：在信号回调里调用 → `-EOPNOTSUPP`；空 cb → `-EINVAL`；`dynamic_cast` 指针版，失败 `-EINVAL`；module 非空时 `get_module_base_addr`（dl_iterate_phdr）确认已加载；6/7/1008 直挂，1009 包 wrapper（`bpftime_set_retval`）后按 1008 挂 → `attach_at` (`:1133-1160`) → `trap_engine::attach` (`:656-762`)，持 `mu`：
1. `ensure_installed` (`:345-389`)：首次分配 uretprobe trampoline（一个只含 `ebreak` 的 slot）；若当前 SIGTRAP handler 不是我们的，`sigaction(SIGTRAP, {on_sigtrap, SA_SIGINFO|SA_NODEFER|SA_RESTART})` 并把旧的存进 `previous_action`（宿主后来改了 handler 时，下一次 attach 会重新插到前面并转发）。
2. 已有 armed site：有 override → `-EEXIST`；新类型是 1008 → `-EEXIST`（**双向互斥**，:671-685）；否则复制 site。已有未 armed site 且代码未变 → 复用 decode 结果和 slot（:690-698）。否则 `build_site` (`:561-611`)：`arch::decode`（riscv：已是 ebreak 拒绝；auipc/jal/jalr/branch/c.j/c.beqz/c.bnez/c.jr/c.jalr → emulate；其余 → 执行于 out-of-line，`trap_arch_riscv64.cpp:71-134`）→ 保存原字节 → `trap_bytes`（2 字节用 `c.ebreak 0x9002`，4 字节用 `ebreak 0x00100073`，`:136-144`）→ 逐页读 `/proc/self/maps` 原权限（读不到 fail closed `-EIO`）→ out-of-line 时 `alloc_slot(addr)`（±1GB 内，先复用 region，再 `MAP_FIXED_NOREPLACE` 在 ±16MB×k 处试 RWX，再任意地址 RWX，最后 W^X 回退为 RW 页、写完 `mprotect RX`，`:394-479`）→ `prepare_out_of_line`：slot = 原指令 + `ebreak`（`trap_arch_riscv64.cpp:200-214`）。
3. 新 entry 挂进 site，`publish(copy_table_with(site))`（RCU 发布，:613-635）。
4. **先发布、后写断点**（:735-737）→ `write_code` (`:534-559`)：对每页 `mprotect(原权限|WRITE)` → `store_patch` (`:488-528`)：对齐 2/4 字节用单次原子 store；**4 字节指令在 2 字节边界时三阶段**：低半写 `c.ebreak` → flush → 写高半 → flush → 写低半（:500-523）→ 逐页恢复原权限。失败分 `failed_before_write`/`failed_after_write`，后者回滚原字节，并发布一个"无回调"的 site（若回滚也写不了就保持 armed，命中只执行原指令），返回 `-EIO`（:738-756）。**从不对宿主代码页去掉 EXEC**（:530-533）。

**命中**：`ebreak` → 内核投递 SIGTRAP → `trap_engine::on_sigtrap` (`:1044-1069`)：保存 errno，`++signal_handler_depth`，取 pc：
1. pc == `uret_trampoline` → `handle_return`；
2. 在 `by_slot_trap` 里 → 这是 out-of-line 执行完原指令后的第二个断点 → `set_pc(site->addr + len)` 回到原函数下一条；
3. 在 `by_addr` 里 → `handle_hit`；
4. 都不是 → `chain_previous`（`SA_SIGINFO` 调 sa_sigaction；`SIG_IGN` 忽略；`SIG_DFL` 恢复默认并 `raise`；否则调 sa_handler，`:1020-1037`）。
末尾 `release_idle_slot()`、`--depth`、恢复 errno。

`handle_hit` (`:917-973`)：`claim_thread_slot()` 成功、`in_handler==0`、site armed 且有 entry 时：`in_handler=1`，`current_signal_callback=&slot->callback`，cookie reset，`arch::fill_pt_regs`（riscv：拷 32 个 `__gregs`，epc 改成探测地址，`trap_arch_riscv64.cpp:182-190`），记 `current_regs/pc/sp`，`phase=1`；
- 有 override：`callback.override_return = set_override`（写本线程槽，`:196-203`），只跑第一个 1008 entry；
- 否则跑全部 6 型 entry；
- 外面包 `try{}catch(...){}`；
- 被 override → `arch::do_return(uc, value)`（a0=value，pc=ra，`trap_arch_riscv64.cpp:171-175`）直接返回调用者；
- 有 uretprobe 且影子栈未满 → 压 `{function, orig_ret=ra, sp}`，把 `ra` 改成 trampoline；
- `in_handler=0`。
最后无论是否跑了回调都 `resume`：有 slot → pc 设为 slot；否则 `arch::emulate`（软件模拟 pc 相对/跳转，`trap_arch_riscv64.cpp:216-319`）。槽耗尽 / 嵌套命中（`in_handler!=0`）时**只执行原指令不跑回调**——这就是"回调里调被探测函数不会无限递归"的实现。

`handle_return` (`:975-1018`)：丢弃 `frame.sp + 8 < 当前 sp` 的帧（longjmp/异常遗弃的），弹出栈顶帧，`set_pc(orig_ret)`；嵌套时只恢复 pc；site 仍 armed 且有 uretprobe 才跑 7 型回调（`phase=2`、`return_value = a0`）。

**拆除**：`detach_by_id` (`:1212-1226`) → `trap_engine::detach` (`:785-836`)：复制 site 去掉 entry；空了就 `write_code(orig)` 恢复原指令（`failed_before_write` 时发布带 `protections_dirty` 的原 site 供重试，返回 `-EIO`）；发布新表 → `wait_for_quiescence`（先 `enabled=false`，再自旋 `sched_yield` 等 `active_callbacks==0`，`:641-648`）→ 只有权限没恢复时返回 `-EIO`。slot、site、entry 永不释放（防止竞态读到已释放内存）。

### 3.5 syscall trace：transformer 改写 → 分发

**装配（LD_PRELOAD 路径，`bpftime start -s`，`tools/cli/main.cpp:1012-1022`）**：
宿主进程 `LD_PRELOAD=libbpftime-agent-transformer.so`、`AGENT_SO=libbpftime-agent.so` →
transformer 的 `__libc_start_main` (`agent-transformer.cpp:32-46`) 置 `injected_with_frida=false`，用 `bpftime_hooked_main` 换掉 main →
`bpftime_hooked_main` (`:24-30`) → transformer 自己的 `bpftime_agent_main("")` (`:48-102`)：读 `AGENT_SO`（没有就用 Frida 传入的 data，都没有则 ERROR 并返回，宿主正常运行）→ `cs_arch_register_x86()` → `bpftime::setup_syscall_tracer()` →
`dlmopen(LM_ID_NEWLM, agent_so, RTLD_NOW|RTLD_LOCAL)`（失败 `exit(1)`）→ 写 agent 的 `injected_with_frida` → `dlsym("_bpftime__setup_syscall_trace_callback")` → `entry_func(&hook)`（hook 初值 = `call_orig_syscall`）→ agent 侧 `_bpftime__setup_syscall_trace_callback` (`runtime/agent/agent.cpp:1199-1212`)：`orig_hooker = *hooker; *hooker = &syscall_callback;` 然后调 agent 的 `bpftime_agent_main` 完成 3.1 的装配（syscall_trace impl 的 `orig_syscall = orig_hooker = call_orig_syscall`）→ 回到 transformer `set_call_hook(syscall_callback)` → 进入原 main。
Frida 注入路径（`bpftime attach -s <pid>`，`tools/cli/main.cpp:1036-1044`）：Frida 把 transformer 注入，入口 `bpftime_agent_main(data=agent 路径)`，其余相同。

`setup_syscall_tracer` (`text_segment_transformer.cpp:233-328`)：
1. `mmap(0, 0x1000, RWX, MAP_FIXED|...)`，失败 `exit(1)`（需要 `vm.mmap_min_addr=0`，否则 EPERM，已实跑，见 §9）。
2. 地址 0..511 填 `nop`，512 处写 `50 48 b8 <imm64> ff e0`（push rax; movabs; jmp rax）。
3. `mprotect(0, 0x1000, PROT_EXEC)`。
4. 扫 `/proc/self/maps`，跳过 `[stack]`、`[vsyscall]`、地址 0 那页，对每个 `x` 段 `rewrite_segment` (`:155-204`)：段整体 `mprotect RWX` → capstone x86-64 **线性**反汇编（遇解码失败 `break`，段剩余部分不再扫）→ 遇到 `syscall`/`sysenter`（都是 2 字节）且不是 transformer 自己的 `syscall_addr`，就改写为 `ff d0`（`call *%rax`）→ 恢复原权限。任何 mprotect/capstone 失败都 `exit(1)`。

**运行时一次系统调用**：应用执行原 `syscall` 位置 → `call *%rax`（rax=系统调用号 n，压返回地址）→ 跳到地址 n（第 0 页 nop 滑行）→ 512：`push %rax`（保存 n）、`movabs`、`jmp syscall_hooker_asm` → `syscall_hooker_asm` (`text_segment_transformer.cpp:57-97`)：`pop %rax` 恢复 n；n==15（rt_sigreturn）→ 直接走 `handle_sigreturn` 原样执行 `syscall`；n==56（clone）/435（clone3）→ `handle_clone/handle_clone3`：若给了子栈，把应用返回地址预先写到子栈顶（clone3 还把 `stack_size` 减 8），使子线程在 `ret` 时回到正确位置，然后原样 `syscall`（**这两个系统调用不经过 hook、不触发 eBPF**）(:72-75,119-140)；
其余：`mov (%rsp),%rcx` → 建帧、16 字节对齐 → 压 7 个寄存器 → 按 C ABI 重排（rdi=nr, rsi..r9=arg1..arg5, 栈上 arg6=r9）→ `call syscall_hooker_cxx` (`:148-153`) → `call_hook` = agent 的 `syscall_callback` (`runtime/agent/agent.cpp:1182-1197`，`try` 包住，异常时退回 `orig_hooker`) → `global_syscall_trace_attach_impl->dispatch_syscall` →
`dispatch_syscall` (`src/syscall_trace_attach_impl.cpp:18-95`)：
1. `exit`/`exit_group` 直接 `orig_syscall`（不跟踪，:24-27）；
2. 设 `curr_thread_override_return_callback`（捕获栈上 `is_overrided/user_ret`）；
3. 该号或全局 enter 回调非空 → 栈上 `trace_event_raw_sys_enter`（`ent` 全 0，`id`、`args[6]`）→ 先跑该号回调，再跑全局回调；每个回调拿 ctx 副本、`try/catch(...)` 吞异常（:40-53）；
4. reset 回调；**若 enter 回调 override 了 → 直接返回 user_ret，原系统调用不执行，exit 回调也不跑**（已实跑）；
5. 重新设回调 → `orig_syscall(...)`（= transformer 的 `call_orig_syscall`：寄存器换回 syscall ABI，执行 `syscall_addr` 处那条未改写的 `syscall`，`text_segment_transformer.cpp:99-112`）；
6. exit 回调（`trace_event_raw_sys_exit{id, ret}`）也可以 override 返回值；reset；返回。
→ 回到 `syscall_hooker_asm` 的 `leave; ret` → 回到应用 `call *%rax` 之后，即原 `syscall` 的下一条指令。

### 3.6 syscall trace 的 attach 建立

`instantiate_perf_event_handler_at` 对 `PERF_TYPE_TRACEPOINT` 传 `to_string(tracepoint_id)`（`bpf_attach_ctx.cpp:460-472`）→ `syscall_trace_attach_private_data::initialize_from_string` (`src/syscall_trace_attach_private_data.cpp:8-63`)：`stoi`（非数字会抛，未捕获）→ 读 tracefs 建 id→名字表（**在目标进程里读 `/sys/kernel/tracing`，非 root 时 Permission denied → `directory_iterator` 抛 `filesystem_error`**，推断；本机 `ls` 已实测无权限）→ `*SYS_ENTER/*SYS_EXIT` → `sys_nr=-1`；`sys_enter_X`/`sys_exit_X` → 查编译期 name→nr 表；找不到返回 `-EEXIST`（错误码选得怪）。
→ `create_attach_with_ebpf_callback` (`:121-168`)：type 必须是 2（否则 `-ENOTSUP`）；`sys_nr` 必须在 `[-1, 512)`（否则 `-EINVAL`）；插进对应 set。`detach_by_id` (`:97-120`) 从 set 和 map 删掉，找不到 `-ENOENT`。

### 3.7 simple attach（自定义事件源）

宿主：`add_simple_attach_impl_to_attach_ctx(2001, cb, ctx)` (`simple_attach_impl.hpp:100-123`) → `bpf_attach_ctx::register_attach_impl({2001}, ...)` → `init_attach_ctx_from_handlers` 实例化 custom perf event（`custom_perf_event_data.attach_argument` 作为 private data 字符串，`bpf_attach_ctx.cpp:488-500`）→ `simple_attach_impl::create_attach_with_ebpf_callback` (`simple_attach_impl.cpp:7-29`) 存下参数与 eBPF 回调。
触发：宿主调返回的 trigger(s) → `simple_attach_impl::trigger` (`:44-55`) → 宿主 `callback(attach_arg, trigger_arg, ebpf_callback)`，由宿主准备 ctx 并调 `ebpf_callback`，返回值由宿主解释。未挂时 `trigger` 返回 **1**。
controller 侧：`bpftime_add_custom_perf_event(2001, prefix)` + `bpftime_attach_perf_to_bpf`（`example/attach_implementation/controller/controller.cpp:77-80`）。

---

## 4. 跨模块接口

### 4.1 本块对外提供

| 接口 | 定义 | 使用方 |
|---|---|---|
| `base_attach_impl`（虚接口）、`attach_private_data` | `attach/base_attach_impl/base_attach_impl.hpp:49-89`、`attach_private_data.hpp:12-31` | `runtime/src/attach/bpf_attach_ctx.cpp:269-294`（register）、`:367,397`（create）、`:528`（detach）、`runtime/src/bpf_helper.cpp:793,860`（`call_attach_specific_function("generate_stack")`）；nv_attach_impl（R6） |
| `ebpf_run_callback`、`ebpf_helper_register_callback` | `base_attach_impl.hpp:42-47` | `bpf_attach_ctx.cpp:276,397-406` |
| `curr_thread_override_return_callback` | `base_attach_impl.hpp:19-20` | frida override handler、`syscall_trace_attach_impl.cpp:34,69,73,90`；runtime 其他处未见（grep） |
| `current_thread_attach_func_ip` | `:22` | `runtime/src/bpf_helper.cpp:681`（`bpf_get_func_ip`） |
| `current_signal_callback`、`signal_callback_context` | `:27-35` | `bpf_attach_ctx.cpp:399`（写 cookie）、`runtime/src/bpf_helper.cpp:665,679`（cookie / func_ip）、`runtime/src/bpftime_prog.cpp:241`（信号里只允许已准备的 JIT 程序） |
| `signal_handler_depth` | `:36-39` | `trap_uprobe_attach_impl/test/late_injection_module.cpp:16`（审计）；trap 引擎自增减 |
| `bpftime_set_retval` / `bpftime_override_return`（`extern "C" inline`） | `base_attach_impl.hpp:97-138` | helper 表 `runtime/src/bpf_helper.cpp:1280,1298`；信号安全白名单 `:899-900`；Frida/trap 的 ureplace wrapper |
| `pt_regs` + `PT_REGS_*` | `bpftime_pt_regs.hpp`、`frida_register_def.hpp` | 各后端、测试；eBPF 程序按内核布局读 |
| `simple_attach::add_simple_attach_impl_to_attach_ctx` / `simple_attach_impl` | `simple_attach_impl.hpp:100-123` | `example/attach_implementation/nginx_plugin_adaptor/nginx_plugin_adaptor.cpp:144` |
| `frida_attach_impl`、`frida_attach_private_data`、6/7/1008/1009 常量 | `frida_uprobe_attach_impl.hpp` | `runtime/agent/agent.cpp:1013-1033`；`runtime/unit-test/attach_with_ebpf/*`（`detach_by_func_addr` 在 `test_attach_filter_with_ebpf.cpp:80`） |
| Frida helper `bpftime_get_func_arg/ret/retval`（183/184/186） | `src/frida_attach_utils.cpp:176-212`，注册 `src/frida_uprobe_attach_impl.cpp:283-292` | 经 `register_custom_helpers` 写进 attach ctx helper 表 |
| `trap::trap_attach_impl`、`trap_attach_private_data`、`bpftime_trap_get_func_arg/ret/retval` | `include/trap_uprobe_attach_impl.hpp` | `agent.cpp:991-1012`；`bpf_attach_ctx.cpp:391-396`（`dynamic_cast` 判断后先 `prepare_for_signal_execution`）；`runtime/src/bpf_helper.cpp:901-907`（白名单）；`benchmark/trap-uprobe/trap_bench.cpp:85,102`；`runtime/unit-test/attach_with_ebpf/test_attach_uprobe_with_ebpf_trap.cpp:212` |
| `trap::split_write_count / thread_slot_exhaustion_count / check_probe_target / prepare_thread` | hpp:66-82 | 测试、benchmark |
| `syscall_trace_attach_impl`、`syscall_trace_attach_private_data`、`ATTACH_SYSCALL_TRACE`、`global_syscall_trace_attach_impl` | `include/syscall_trace_attach_impl.hpp` | `agent.cpp:58,142,964-988,1059,1187` |
| `_bpftime__setup_syscall_hooker_callback`、`_bpftime__syscall_dispatcher` | `src/syscall_trace_attach_impl.cpp:170-192` | **无人使用**（agent 自己实现了 `_bpftime__setup_syscall_trace_callback`/`syscall_callback`，且 agent.version 只导出后者，`runtime/agent/agent.version:2`）——死代码 |
| `libbpftime-agent-transformer.so`（`bpftime_agent_main`、`__libc_start_main`、`bpftime_hooked_main`、`syscall_hooker_asm` 等动态符号，已用 `nm -D` 确认） | `attach/text_segment_transformer/` | `tools/cli/main.cpp:82,1012-1044`；`.github/workflows/test-bpftrace.yml:117,222` |

### 4.2 本块依赖外部

| 依赖 | 位置 |
|---|---|
| frida-gum：`gum_init_embedded`、`gum_interceptor_obtain/attach/replace/detach/revert/begin_transaction/end_transaction`、`gum_make_probe_listener`、`gum_make_call_listener`、`gum_interceptor_get_current_invocation`、`gum_invocation_context_get_nth_argument/get_replacement_data/get_return_value`、`gum_cpu_context_get_nth_argument`、`gum_backtracer_make_accurate/fuzzy`、`gum_backtracer_generate`、`gum_module_load/find_base_address/find_export_by_name`、`gum_find_function` | `src/frida_uprobe_attach_impl.cpp:40-41,310-316`；`src/frida_internal_attach_entry.cpp:124-142,155-168,187-192,262-270`；`src/frida_attach_utils.cpp:139-156,179-211` |
| capstone（经 frida-gum 静态库）：`cs_open/cs_disasm_iter/cs_insn_name/cs_close/cs_arch_register_x86` | `text_segment_transformer.cpp:165-197`、`agent-transformer.cpp:73` |
| runtime agent 的 `injected_with_frida` 变量、`_bpftime__setup_syscall_trace_callback` | `agent-transformer.cpp:82-99`（dlsym）←→ `runtime/agent/agent.cpp:81,1199-1212` |
| runtime `bpf_attach_ctx::register_attach_impl`（simple 的 SFINAE 目标） | `simple_attach_impl.hpp:72-92,113` |
| runtime `bpftime_prog::prepare_for_signal_execution` / `helper_is_async_signal_safe`（trap 准入，不在 attach 目录） | `runtime/src/bpftime_prog.cpp:289-330`、`runtime/src/bpf_helper.cpp:895-910` |
| Linux：`/proc/self/maps`、`/proc/self/exe`、`/proc/<pid>/map_files`、`/sys/kernel/tracing/events/...`、`dl_iterate_phdr`、`dladdr`、`sigaction`、`mprotect`、`mmap(MAP_FIXED_NOREPLACE)` | 各处 |
| spdlog | 全部 |

需要别的读者确认的点见 §10。

---

## 5. 配置项、环境变量、编译开关

| 名字 | 默认 | 定义 | 读取 | 影响 |
|---|---|---|---|---|
| `BPFTIME_ENABLE_FRIDA` | 非 riscv：ON；riscv：OFF（**FORCE**，命令行 `-D` 会被覆盖） | `cmake/StandardSettings.cmake:78,81` | `attach/CMakeLists.txt:2,15`、`CMakeLists.txt:138,222,276`、`runtime/agent/CMakeLists.txt:6,56,79`、`runtime/CMakeLists.txt:190,260`、`tools/CMakeLists.txt:2` 等 | 编 Frida 后端和 transformer；agent 定义 `BPFTIME_HAVE_FRIDA_ATTACH` |
| `BPFTIME_ENABLE_TRAP_UPROBE` | riscv：ON；其他：OFF（FORCE） | `StandardSettings.cmake:77,80` | `attach/CMakeLists.txt:8`、`CMakeLists.txt:35`、`benchmark/trap-uprobe/CMakeLists.txt:1` | 编 trap 后端；agent 定义 `BPFTIME_HAVE_TRAP_UPROBE_ATTACH`；runtime 链接它（`runtime/CMakeLists.txt:167-168`） |
| `BPFTIME_TRAP_ATTACH_BUILD_TESTS` | ON（未定义时） | `attach/CMakeLists.txt:5-7` | `trap_uprobe_attach_impl/CMakeLists.txt:23`、`CMakeLists.txt:35-36`（开 ctest） | trap 测试 |
| `BPFTIME_BUILD_WITH_LIBBPF` | （R? 负责） | — | `runtime/agent/CMakeLists.txt:9,85`、`agent.cpp:964,1058,1181` | agent 是否注册 syscall trace |
| `TEST_LCOV` | OFF | `frida_uprobe_attach_impl/CMakeLists.txt:31`、`syscall_trace_attach_impl/CMakeLists.txt:31` | 同文件 | 覆盖率 |
| `ENABLE_EBPF_VERIFIER` | OFF | StandardSettings | frida/syscall CMake :48/:38 | 开时用 FetchContent 拉 Catch2 |
| `FRIDA_DOWNLOAD_URL_PREFIX` | "" | `cmake/frida.cmake:4` | :40-41 | 下载镜像前缀 |
| `FRIDA_VERSION` | 16.1.2 | `cmake/frida.cmake:12` | — | devkit 版本；支持 linux-x86_64 / linux-aarch64（→arm64）/ linux-arm*（→armhf）/ darwin-arm64（→macos-arm64），其他 `FATAL_ERROR`（:16-36）；下载 core+gum 两个 devkit，已存在 `third_party/frida/` 就不重新下（:49-65） |
| `enable_frida_fuzzy_backtracer`（运行时配置，环境变量 `BPFTIME_FRIDA_FUZZY_BACKTRACER`/CLI `--fuzzy-backtracer`，R? 负责） | false | runtime config | `agent.cpp:1017` → `frida_attach_impl(bool)` | generate_stack 用 fuzzy/accurate |
| `AGENT_SO` | 无 | — | `agent-transformer.cpp:56` | transformer 要加载的 agent 路径（优先于 Frida data） |
| `SPDLOG_LEVEL` 等 | — | — | `agent-transformer.cpp:52`（`spdlog::cfg::load_env_levels`） | transformer 日志级别；transformer 无条件建 stderr logger |
| 链接选项 `-Wl,-z,now` | — | `trap_uprobe_attach_impl/CMakeLists.txt:18`（INTERFACE） | — | 让 PLT 在首次信号前绑定 |
| 生成头 `syscall_id_list.h` | 构建时生成 | `syscall_trace_attach_impl/CMakeLists.txt:1-9` + `generate_syscall_id_table.sh` | `src/syscall_table.cpp:13` | 用**构建机**的 `cpp` 和 `<sys/syscall.h>`，交叉编译会得到宿主架构的号（推断） |
| `vm.mmap_min_addr`（内核 sysctl） | 一般 65536 | — | — | 非 0 时 transformer 的 `mmap(0)` 失败 → `exit(1)`（已实跑） |

常量：Frida `generate_stack` 最多 127 帧（`src/frida_uprobe_attach_impl.cpp:305-308`）；trap `SLOT_SIZE=32`、`URET_STACK_DEPTH=128`、`NEAR_DISTANCE=1GB`、`THREAD_SLOTS=1024`（`trap_uprobe_attach_impl.cpp:35-38,161`）；syscall 槽 512（`syscall_trace_attach_impl.hpp:95-96`、`text_segment_transformer.cpp:51`）。

---

## 6. 并发、生命周期与错误处理

**Frida 后端**
- 无锁：`attaches`、`internal_attaches`、`user_attaches` 都是普通容器。命中线程在 `iterate_*` 里遍历 `user_attaches` 的同时，另一线程 attach/detach 同一函数 → 数据竞争（推断，未验证；runtime 层是否有外层锁待 R? 确认）。
- override 状态 `is_overrided/user_ret` 在内部 entry 上（非 TLS），多线程同时命中同一 override 点存在竞争（推断，与现有指南一致）。
- `curr_thread_override_return_callback` 在 override handler 里设置后**从不 reset**（`src/frida_internal_attach_entry.cpp:267-268`，已实跑：跑完 override 后 `has_value()==1`）。之后同线程在非 override 上下文（例如 uprobe 程序误用 `bpf_override_return`）不会抛异常，而是写进上次那个内部 entry 的字段；若该 entry 已 detach 析构，lambda 捕获的 `this` 悬空 → use-after-free（推断）。
- 错误：attach 失败抛 `std::runtime_error`（带诊断）；private data 类型不对 `-EINVAL`；未知 type `-ENOTSUP`；`detach_by_id` 找不到 id **返回 0**，并设 `errno = -ENOENT`（负 errno，`src/frida_uprobe_attach_impl.cpp:144-150`，已实跑）。eBPF 回调错误只打日志。
- fork：Frida interceptor 状态随地址空间复制（推断，未在本块代码里看到 fork 处理）。

**trap 后端**
- 控制面一把 `std::mutex mu`；数据面（信号处理）完全无锁：只 `table.load(acquire)` 读不可变 site 表。新表发布前 site 已建好，断点在发布之后才写（:735-737），所以命中总能找到 site。
- 回调与 detach 的同步：`attach_entry::run` 先 `active_callbacks.fetch_add` 再查 `enabled`；detach 先 `enabled=false` 再自旋等 `active_callbacks==0`（都用 seq_cst，`trap_uprobe_attach_impl.cpp:51-60,641-648`）。
- 一切 site/slot/entry/table 都不释放（README:87-89）。
- 线程状态：1024 个静态槽 + initial-exec TLS 指针，首次命中 CAS 认领；槽耗尽时跳过回调但仍执行原指令，并计数（:183）。带未完成 uretprobe 帧退出的线程会泄漏一个槽（README:90-92）。
- 信号安全：`on_sigtrap` 保存/恢复 errno；handler 内不分配、不加锁、不打日志（`describe_target`/SPDLOG 只在控制面）；但 `handle_hit/handle_return` 里有 `try{}catch(...){}`——若原生回调真的抛了，异常对象分配本身不是 async-signal-safe（推断）。
- 在回调里调 attach/detach → `-EOPNOTSUPP`（`:1135,1203,1214,1234`）。
- `SA_NODEFER` 使嵌套命中可以再陷入；`in_handler` 防止递归跑回调。
- 宿主信号契约：链式转发；宿主后来改了 handler 时，下次 attach 把自己插回前面（`ensure_installed`）；引擎析构时若当前仍是我们的 handler 就恢复旧的（:334-343）。
- 错误：patch 前失败 `-EIO`（站点不变）；patch 后权限恢复失败回滚并 `-EIO`；detach 恢复失败可重试。`create_attach` 非法 private data `-EINVAL`、未知 type `-ENOTSUP`、地址空 `-EINVAL`、首指令不可重定位 `-ENOTSUP`、slot 分配失败 `-ENOMEM`。
- fail-open：ureplace 的 eBPF 回调返回 `<0`（比如程序未准备好 → `bpftime_prog_exec` 返回 `-ENOTSUP`）时不设返回值 → 原函数照常执行（`:1271-1275`；Frida 同理 `src/frida_uprobe_attach_impl.cpp:246-254`）。

**syscall trace**
- 无锁；`sys_*_callbacks` 是 `std::set`，attach/detach 与其他线程的 `dispatch_syscall` 并发 → 数据竞争（推断）。
- `dispatch_syscall` 用 `sys_nr` 直接下标 `[512]` 数组，**没有边界检查**（:55,81）；x86_64 当前最大号约 462 所以平时没事，但 x32 ABI（号带 `0x40000000`）或未来 >511 的号会越界（推断）。注意 transformer 层的 nop 滑板也只有 512 字节，号 ≥512 时 `call *%rax` 直接跳进 512 之后的跳转代码中间或页外 → 崩（推断）。
- 回调异常被吞（`try/catch(...)`，已有单测 `test_syscall_dispatch.cpp:245-270`）；agent 的 `syscall_callback` 也 `try` 包住并在失败时退回 `orig_hooker`（`agent.cpp:1186-1196`）。
- `_bpftime__setup_syscall_hooker_callback` 用 `assert`，Release 下 `global...value()` 会抛 `bad_optional_access`（死代码，影响不大）。
- tracefs 读不到时 `create_syscall_tracepoint_id_table` 抛（`read_id` 抛 `runtime_error`，`directory_iterator` 抛 `filesystem_error`），工厂 lambda 不捕获，冒到 agent 的 `init_attach_ctx_from_handlers` try 块（推断）。

**text_segment_transformer**
- 所有失败（`mmap(0)`、`mprotect`、`cs_open`、`dlmopen`）都 **`exit(1)` 结束宿主进程**（`text_segment_transformer.cpp:163,170,201,243,283`；`agent-transformer.cpp:79`）——与仓库 `CLAUDE.md`"注入代码不得结束宿主进程"的契约冲突（已实跑：`AGENT_SO=/nonexistent.so LD_PRELOAD=... /bin/echo hello` 输出 mmap EPERM 后 rc=1，`hello` 没打印）。
- `dlsym("injected_with_frida")` 为空时只 WARN，下一行仍解引用空指针（`agent-transformer.cpp:82-88`）。
- 改写时整段 `mprotect RWX` 并逐字节写 2 字节，Frida 注入到运行中进程时其他线程可能正执行到半改写的指令（推断）。
- agent 用 `dlmopen(LM_ID_NEWLM)` 装进**新链接命名空间**：它带着自己的一份 libc，且这些代码是在改写之后才映射的，所以 agent 自己的系统调用不会被改写、不会递归进 hook（推断，这是这样设计的关键原因）。副作用：之后应用再 `dlopen` 的库也不会被改写，其中的系统调用不会被跟踪（推断）。

**simple attach**：无锁；每实例一个 attach；`trigger` 未挂时返回 1（nginx 例子把非 0 当"放行"，所以没有程序时 fail-open）。

---

## 7. 不直观的行为、疑似 bug、代码与注释不一致

| # | 现象 | 位置 | 为什么可疑 | 确定程度 |
|---|---|---|---|---|
| 1 | Frida：先 uprobe 后 override，attach 返回成功 id，但 override 永不生效；之后再挂 uprobe 反被 `-EEXIST` 拒 | `src/frida_uprobe_attach_impl.cpp:65-83`、`src/frida_internal_attach_entry.cpp:115-118` | `has_override` 只单向检查；`ensure_listener` 对 1008 什么都不做；`has_uprobe_or_uretprobe()`（:209-218）定义了却没人调用 | **确认（已实跑**：`id1=1 id2=2 ret=102 up=1 ov=0`，随后 `id3=-17`） |
| 2 | Frida `detach_by_id` 对不存在的 id 返回 0，并把 `errno` 设为负数 `-ENOENT` | `src/frida_uprobe_attach_impl.cpp:144-150` | 调用方（runtime `:528`）据返回值判断成功；errno 不该是负数 | **确认（已实跑**：`ret=0 errno=-2`） |
| 3 | Frida override 未覆盖时用 5 参数重放原函数，第 6 个参数丢失 | `src/frida_internal_attach_entry.cpp:255,270-275,288-289` | x86_64 第 6 参数在 r9；也不转发浮点/栈参数；返回类型 `void*` 不保留浮点返回值（推断） | **确认（已实跑**：`f6(1..6)` 期望 6015，得到 94557299479352015） |
| 4 | ureplace 在 Frida/trap 内部被登记为 1008，`iterate_attaches` / `get_type()` 报 1008 而不是 1009 | `src/frida_uprobe_attach_impl.cpp:264-265`；`trap_uprobe_attach_impl.cpp:1279` | 对外查询拿不到真实类型 | **确认（已实跑**：`ureplace iterate id=1 type=1008`） |
| 5 | Frida override handler 设置的 `curr_thread_override_return_callback` 从不复位 | `src/frida_internal_attach_entry.cpp:267-268` | 之后同线程误调 `bpf_override_return` 不抛异常而写旧 entry；entry 析构后悬空 | 残留：**确认（已实跑）**；UAF 后果：推断 |
| 6 | 错误日志 `"Function {} was already attached ..."` 没传参数，打印字面 `{}` | `src/frida_uprobe_attach_impl.cpp:77-78` | 小毛病 | 确认（已实跑输出可见） |
| 7 | `ATTACH_UPROBE=6` 注释写"函数返回时"，`ATTACH_URETPROBE=7` 注释写"函数被调用时"，写反 | `include/frida_uprobe_attach_impl.hpp:20-25` | 以 `ensure_listener` 接线为准 | 确认 |
| 8 | Frida 测试文件的 `#if !defined(__x86_64__) && defined(_M_X64)` 永远不成立 | `test/test_uprobe_uretprobe.cpp:10-12` 等 3 处 | 本意是"只支持 x86_64"，实际无效 | 确认 |
| 9 | `frida_attach_private_data::initialize_from_string` 对非数字直接 `std::stoul` 抛异常；`addr` 无初值；无 `to_string` | `src/frida_attach_private_data.cpp:15,36`；hpp:13 | trap 版本有 try/catch 和初值，Frida 版没有 | 确认（读代码） |
| 10 | trap `to_string()` 有 module 时输出 `module:<绝对地址>`，而 `initialize_from_string` 把冒号后当偏移 → 往返不一致 | `src/trap_attach_private_data.cpp:54-59` vs :41-43 | 若有人用 to_string 序列化再解析会加两次基址 | 推断（目前 runtime 只在 kprobe 路径调 to_string） |
| 11 | trap 后端只有 riscv64 实现，`trap_arch_riscv64.cpp` 无架构宏保护；StandardSettings 又 FORCE 设置开关 | `trap_arch_riscv64.cpp`、`CMakeLists.txt:1-7`、`StandardSettings.cmake:76-82` | trap README 说"defaults to ON on riscv64"，其实是强制，x86 上 `-DBPFTIME_ENABLE_TRAP_UPROBE=ON` 会被覆盖 | 确认（cmake）；编译失败为推断 |
| 12 | `_bpftime__setup_syscall_hooker_callback` / `_bpftime__syscall_dispatcher` 是死代码；transformer 找的是 `_bpftime__setup_syscall_trace_callback`，但报错文案写的是 `..._hooker_callback` | `src/syscall_trace_attach_impl.cpp:170-192`；`agent-transformer.cpp:89-95` | 名字混乱，易误导读者以为 hooker_callback 是入口 | 确认（`nm -D` 已核对 agent 只导出 `_bpftime__setup_syscall_trace_callback` 和 `syscall_callback`） |
| 13 | transformer 的 `syscall_hooker_asm` 只把 6 个参数寄存器压栈，返回前**不恢复**（`leave; ret`）；而真 `syscall` 只破坏 rax/rcx/r11 | `text_segment_transformer.cpp:76-97` | glibc 等内联 syscall 的 asm 只声明 rcx/r11/memory 被破坏，编译器可能依赖 rdi/rsi/rdx/r8/r9/r10 在 syscall 后不变 → 寄存器被 C 函数改坏 | 推断，未验证（需 root 设 mmap_min_addr=0 才能跑） |
| 14 | `call *%rax` 以及跳板里的 push 会写 `rsp-8`、`rsp-16`…，覆盖调用者的 128 字节 red zone | 同上 + 第 0 页代码 | 叶子函数可能把局部变量放在 red zone | 推断，未验证 |
| 15 | rt_sigreturn 走 `handle_sigreturn` 时栈上还多着 `call` 压的返回地址（只 pop 了 rax），内核按 `regs->sp - 8` 找 sigframe → 会偏 8 字节；源码里 `// "addq $8, %rsp\n\t"` 被注释掉 | `text_segment_transformer.cpp:61-63,108-110` | 若 `__restore_rt` 所在 libc 段被改写，信号返回可能出错 | 推断，**不确定**（未能实测，需要别人在 root 环境验证） |
| 16 | `dispatch_syscall` 对 `sys_nr` 不做边界检查；transformer 的 nop 滑板也只 512 字节 | `syscall_trace_attach_impl.cpp:55,81`；`text_segment_transformer.cpp:51,246-276` | 号 ≥512 或负数越界 | 推断 |
| 17 | enter 回调 override 后原 syscall 不执行、exit 回调也不跑；exit 回调也能 override 返回值 | `syscall_trace_attach_impl.cpp:69-94` | 与内核 tracepoint 语义不同（内核 tracepoint 不能改返回值） | 确认（已实跑：`enter-override: ret=55 orig_calls=0 exit_runs=0`；`exit-override: ret=66 orig_calls=1`） |
| 18 | clone / clone3 / rt_sigreturn / exit / exit_group 不触发 syscall trace 程序 | `text_segment_transformer.cpp:61-75`；`syscall_trace_attach_impl.cpp:24-27` | 跟踪 clone 的程序永远收不到事件 | 确认（读代码） |
| 19 | transformer 所有失败都 `exit(1)` 杀宿主；`injected_with_frida` 符号缺失时空指针解引用 | 见 §6 | 违反 `CLAUDE.md` 宿主契约 | exit：**确认（已实跑）**；空指针：确认（读代码） |
| 20 | 第 0 页最终是 `PROT_EXEC`，注释说"保持空指针解引用的正常行为"；x86 上没有 PKU 时 exec-only 页通常仍可读，读 NULL 不再段错误 | `text_segment_transformer.cpp:277-279` | 会掩盖空指针读 bug | 推断 |
| 21 | 线性反汇编：遇到解码失败即 `break` 整段停止扫描；数据混在代码段里时可能误改 | `text_segment_transformer.cpp:178-183` | 漏改 / 误改 | 推断 |
| 22 | aarch64 分支只有 `// TODO`，但 attach/CMakeLists 在 Frida 开启的所有 Linux（含 aarch64）都编 transformer；汇编符号 `syscall_hooker_asm` 等未定义，capstone 也写死 `CS_ARCH_X86` | `text_segment_transformer.cpp:142-146,167`；`attach/CMakeLists.txt:15-17` | aarch64 上 syscall trace 不可用；so 能否链接/加载未验证 | 推断 |
| 23 | syscall 号表用构建机 `cpp` 生成，交叉编译（如 riscv64 toolchain）时得到的是构建机架构的号 | `generate_syscall_id_table.sh:1-5`、`CMakeLists.txt:1-5` | 号不对 | 推断 |
| 24 | `generate_syscall_id_table` 的 `while (ss) { ss >> name >> id; ... }` 最后一轮读失败时仍插入 `""` 和未初始化的 `id` | `src/syscall_table.cpp:29-42` | 多一条垃圾映射 | 推断 |
| 25 | private data 找不到 syscall 返回 `-EEXIST`（应为 ENOENT）；`stoi` 非数字抛异常未捕获 | `src/syscall_trace_attach_private_data.cpp:11,55,61` | 错误码不贴切 | 确认（读代码） |
| 26 | `bpftime_pt_regs.hpp` 与 `frida_register_def.hpp` 故意共用 include guard，两份定义靠人工保持一致 | 两文件头部 | 一处改了另一处没改会静默选错布局（ODR 风险） | 确认（设计如此） |
| 27 | Frida 自定义 helper 名字 `"bpf_get_func_ret_id"`；`bpf_get_retval`(186) 在这里返回被 hook 函数的返回值，内核里 186 是 cgroup 用的 get_retval；`bpf_get_func_arg` 不检查 n 上限且忽略 ctx | `src/frida_uprobe_attach_impl.cpp:286-291`、`src/frida_attach_utils.cpp:189-201` | 与内核语义不同（内核 n≥参数个数返回 -EINVAL） | 确认（读代码） |
| 28 | `bpf_get_func_ret` 在 Frida uprobe（入口）程序里也能调，返回的是此时 rax 里的垃圾 | `src/frida_attach_utils.cpp:176-187` | trap 版用 `phase!=2` 拒绝（`trap_uprobe_attach_impl.cpp:1325`） | 推断 |
| 29 | trap `handle_return` 用 `frame.sp + sizeof(uintptr_t) < sp` 判断遗弃帧——只对 riscv（返回时 sp 与入口相同）成立 | `trap_uprobe_attach_impl.cpp:983-985` | 若将来加 x86 实现（入口 sp 指向返回地址，返回后 sp+8）需重审 | 推断 |
| 30 | attach/README.md：组件树漏了 trap；说 "Linux: All attach types supported"（实际 trap 只 riscv、syscall trace 只 x86_64）；`syscall_trace_private_data` 名字写错；CUDA 类型 8/9 与 `bpf_event_type` 的 KPROBE/KRETPROBE 同号 | `attach/README.md:16-24,112-113,146,178` | 文档过时 | 确认 |
| 31 | text_segment_transformer/README 说"用 NOP 填充以保持空指针解引用行为"——NOP 实为滑板；也没提需要 `vm.mmap_min_addr=0` | README:29-31 | 误导 | 确认 |
| 32 | nginx 例子 `snprintf(buf, sizeof(buf), "%*s", (int)r->uri.len, r->uri.data)` 用的是**宽度**不是精度，会读到 uri 之后直到 NUL，再靠"截掉最后一个空格"补救 | `example/attach_implementation/nginx_plugin/ngx_http_bpftime_module.c:72-79` | 应为 `%.*s` | 推断（例子代码） |

---

## 8. 对照现有指南

对照文件：`/home/y/src/bpftime-wt-reading-guide/04-attach-frida.md`（全文）、`01-lifecycle.md` 第 10、11 步（:497-595）及其深入参考（:995-1030）。

### 8.1 错误 / 不准确

1. **4.3 表格 ureplace "原函数从不执行"** → 源码：ureplace wrapper 只在 eBPF 回调 `err>=0` 时 `bpftime_set_retval`，回调失败时不覆盖，原函数照常执行（fail-open）。位置：`attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:246-262`、`attach/trap_uprobe_attach_impl/src/trap_uprobe_attach_impl.cpp:1271-1276`。建议改为"正常情况下不执行；程序执行失败时退回执行原函数"。
2. **4.3 表格 "attach type 编号 ureplace=1009"** 没错，但应补一句：注册后内部按 1008 存，`iterate_attaches` 报 1008（已实跑，§7 #4）。
3. **4.2 "Frida 在进入时把栈上的返回地址换成自己的离开跳板"** → 仅 x86_64 是"栈上"；aarch64 返回地址在 LR。这是 Frida 内部机制（本仓库无源码），措辞应改为"返回地址（x86 在栈上，arm64 在 lr）"。
4. **4.1 "x86_64 / aarch64 上用 Frida 后端"** 不全：Frida 还支持 armhf、macOS arm64（`cmake/frida.cmake:24-33`），x86_64 macOS 不支持（FATAL_ERROR）。小问题。
5. 01-lifecycle 第 11 步 "和内核 uprobe 相比……完全不进内核：没有断点异常" → 只对 Frida 后端成立；trap 后端每次命中两次 SIGTRAP（README:76-80）。建议加限定"（Frida 后端）"。
6. 未发现行号级错误：抽查 `agent.cpp:991/1013`、`bpf_attach_ctx.cpp:316/420/448-451`、`bpf_helper.cpp:895-910`、`frida_uprobe_attach_impl.cpp:280-292`、`trap_uprobe_attach_impl.cpp:488/656/917/975/1020/1044`、`runtime/CMakeLists.txt:167-168` 都对得上。"17 个用例"已实跑核实。GumX64CpuContext 字段顺序已对 `frida-gum.h:35667-35688` 核实。

### 8.2 遗漏

1. **syscall trace + text_segment_transformer 整条链几乎没讲**（只在"对比读"一句带过）。学习者应知道：zpoline 式改写（`syscall`→`call *%rax`、第 0 页 nop 滑板 + 跳板）、需要 `vm.mmap_min_addr=0`、`dlmopen` 新命名空间的用意、clone/clone3/rt_sigreturn/exit 的特殊处理、`dispatch_syscall` 的 enter/exit/override 语义、ctx 结构 `trace_event_raw_sys_enter/exit`（`ent` 全 0）、tracepoint id 的解析需要目标进程能读 tracefs、只支持 x86_64、失败直接 `exit(1)`。（本笔记 §3.5、§3.6）
2. **simple_attach_impl 与 example/attach_implementation 没讲**——这是"怎么写/接一个自定义 attach"的官方示例：宿主自建 `bpf_attach_ctx`、自定义 attach type 2001、controller 用 `bpftime_add_custom_perf_event`。
3. **base 接口的细节**：attach id 是**每个 impl 实例**自增、不同 impl 可重号，runtime 以 `(id, impl*)` 记；`register_custom_helpers` 直接覆盖 attach ctx 里同号 helper（`bpf_attach_ctx.cpp:276-284`）；`attach_private_data` 默认方法会抛。
4. Frida `detach_by_id` 对未知 id 返回 0 + 负 errno（§7 #2）。
5. Frida override handler 留下 `curr_thread_override_return_callback` 不复位（§7 #5）。
6. Frida 自定义 helper 与内核语义差异（§7 #27/#28）；`bpf_get_func_ip` 在 override 程序里返回 0（`frida_attach_entry.hpp:34-39`，trap 同：`trap_uprobe_attach_impl.cpp:61-63`）。
7. Frida attach 失败抛 C++ 异常，`create_attach_with_ebpf_callback` 只 catch `bad_cast`，异常继续往上抛（§3.2）。
8. trap 后端的更多机制细节：RCU site 表、"先发布后写断点"、out-of-line slot 第二个断点的回跳、±1GB slot 分配与 W^X 回退、RISC-V 哪些指令被模拟、影子栈的 longjmp 清理、`wait_for_quiescence`、页权限失败的三种结局。指南深入参考有表格但没有这些。
9. `StandardSettings.cmake` 是 **FORCE** 设置，命令行改不了（指南说"强制设置"是对的，但没点明 `-D` 无效）。
10. 两份 `pt_regs` 头共用 include guard（§7 #26）。

### 8.3 讲得好、可以保留

- 4.1 "attach 层是黑盒调用者"、三参数回调的定位。
- 4.3 两种 Frida 原语（旁听 vs 顶替）的对比表（修正 ureplace 一格即可）。
- 4.4 两层实体 + mermaid 图 + "最后一个离开的关灯" + 不对称互斥的坑（已实跑证实）。
- 4.5 热路径时序、`pt_regs` 单向拷贝、uretprobe 拿不到栈。
- 4.6 用真实字节码偏移解释为什么布局必须和内核一致；x86/aarch64 对照表（已核实）。
- 4.7 override 四步通知链、5 参数限制（已实跑证实）。
- 4.8 trap 与 Frida 对比表、信号安全准入规则。
- 自测题 Q1–Q6 都与源码一致。

---

## 9. 学习建议

### 9.1 推荐阅读顺序

1. `attach/base_attach_impl/base_attach_impl.hpp`（全文 142 行）→ `attach_private_data.hpp` → `runtime/src/attach/bpf_attach_ctx.cpp:269-294,316-410`（看 runtime 怎么调）。
2. `attach/simple_attach_impl/simple_attach_impl.hpp/.cpp` + `example/attach_implementation/nginx_plugin_adaptor/nginx_plugin_adaptor.cpp`：最小的完整后端。
3. Frida：`include/frida_uprobe_attach_impl.hpp` → `src/frida_uprobe_attach_impl.cpp:48-151,175-279` → `include/frida_attach_entry.hpp` → `src/frida_internal_attach_entry.cpp:115-318` → `src/frida_register_conversion.cpp`。
4. syscall：`include/syscall_trace_attach_impl.hpp` → `src/syscall_trace_attach_impl.cpp:18-95` → `text_segment_transformer.cpp:233-328`（先看 C++ 部分）→ `:54-141`（汇编）→ `agent-transformer.cpp` → `runtime/agent/agent.cpp:1180-1212`。
5. trap（可选，riscv 才用）：`README.md` → `src/trap_arch.hpp` → `src/trap_uprobe_attach_impl.cpp` 按 `on_sigtrap`(1044) → `handle_hit`(917) → `handle_return`(975) → `attach`(656) → `store_patch`(488) 的顺序 → `trap_arch_riscv64.cpp`。

### 9.2 动手实验

1. **Frida 单测**（已实跑）
   ```bash
   T=/home/y/src/bpftime-wt-official-177a1dc/build-jit/attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests
   $T --list-tests      # 17 test cases
   $T                   # All tests passed (1072 assertions in 17 test cases)
   ```
2. **Frida 行为实验**（已实跑；源码 `/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R5/exp_frida.cpp`）。编译命令：
   ```bash
   B=/home/y/src/bpftime-wt-official-177a1dc
   g++-13 -std=c++20 -O1 -o exp_frida exp_frida.cpp -I$B/attach/frida_uprobe_attach_impl/include -I$B/attach/base_attach_impl \
     -I$B/build-jit/FridaGum-prefix/src/FridaGum -I$B/third_party/spdlog/include -DSPDLOG_COMPILED_LIB \
     $B/build-jit/attach/frida_uprobe_attach_impl/libbpftime_frida_uprobe_attach_impl.a \
     $B/build-jit/FridaGum-prefix/src/FridaGum/libfrida-gum.a $B/build-jit/third_party/spdlog/libspdlog.a -ldl -lpthread -lresolv
   ```
   关键输出：
   ```
   E1 uprobe-then-override: id1=1 id2=2 ret=102 up=1 ov=0        # override 静默失效
   [error] ... Function {} was already attached with replace or filter ...
   E1b add uprobe after: id3=-17                                   # 之后挂 uprobe 反被拒
   E2 detach unknown id ret=0 errno=-2
   E3 override not-taken f6(1..6)=94557299479352015 expected=6015  # 第 6 参数丢失
   E4 ureplace iterate id=1 type=1008
   E4 h2(1,2)=42
   E5 curr_thread_override_return_callback has_value after override run = 1
   ```
3. **syscall dispatch 语义**（已实跑；`.../scratchpad/R5/exp_sys.cpp`，链接 `libbpftime_syscall_trace_attach_impl.a` + spdlog，不需要 tracefs，因为直接手填 `sys_nr/is_enter`）：
   ```
   enter-override: ret=55 orig_calls=0 exit_runs=0
   exit-override: ret=66 orig_calls=1 exit_runs=1
   attach sys_nr=512 -> -22
   attach type 6 -> -95
   ```
4. **transformer 需要 mmap(0)**（已实跑，非 root）：
   ```bash
   T=$B/build-jit/attach/text_segment_transformer/libbpftime-agent-transformer.so
   LD_PRELOAD=$T /bin/echo hello          # 打印 "Please set AGENT_SO ..."，然后 hello，rc=0
   AGENT_SO=/nonexistent.so LD_PRELOAD=$T /bin/echo hello
   # [error] Failed to perform mmap: errno=1, message=Operation not permitted ; rc=1，hello 没打印
   ```
   完整跑通需要 root 下 `sysctl vm.mmap_min_addr=0`（会改系统安全设置，**本机是远程实验服务器，未做**）。
5. `nm -D` 看导出符号（已实跑）：transformer 导出 `__libc_start_main`、`bpftime_agent_main`、`syscall_hooker_asm` 等；agent 导出 `_bpftime__setup_syscall_trace_callback`、`syscall_callback`、`injected_with_frida`，**没有** `_bpftime__setup_syscall_hooker_callback`。
6. trap 后端：本机 x86 无法编译（推断），需 riscv64 实机或 qemu-user，按 trap README:143-166 的说明跑 `bpftime_trap_uprobe_attach_tests`。（未做）

### 9.3 最容易误解的点

1. "同一函数挂多个 uprobe 会改写多次" —— 不会，Frida 按地址一个内部 entry；trap 按地址一个 site。
2. "ureplace 是独立机制" —— 不是，它就是"回调后无条件 set_retval 的 override"，内部记为 1008；程序失败时原函数仍会执行。
3. "syscall trace 是 Frida 拦截 libc 的 syscall 函数" —— 不是。Frida 在这里只提供 capstone 和注入入口；真正的拦截是把**每一条 `syscall` 指令**改成 `call *%rax`，靠第 0 页的 nop 滑板把系统调用号变成跳转地址。
4. "eBPF 改 ctx 能改寄存器/参数" —— 不能，`pt_regs` 是栈上副本；改返回值只能靠 override/ureplace（uprobe）或 `bpf_override_return`（syscall trace，可同时作用于 enter/exit）。
5. "trap 后端是 x86 上的备选" —— 不是，只有 riscv64 实现，开关由架构强制决定。

---

## 10. 未读完/未确定的地方

- `example/attach_implementation/benchmark/**`（约 5200 行）只列了目录、读了 README 开头和几处 grep，没逐行读。
- `trap_arch_riscv64.cpp` 的立即数解码（jal/branch/c.j/c.beqz 位域）没有逐位对照 RISC-V 规范核算；trap 测试只读了源码，没法在 x86 上跑。
- §7 #13/#14/#15（transformer 寄存器不恢复、red zone、rt_sigreturn 栈偏移）都是读汇编推出的，需要有 root 的环境（`vm.mmap_min_addr=0`）实测确认；#15 我最没把握。
- §7 #22：aarch64 上 transformer 是否能编过/加载，未验证（需要 Jetson 侧确认）。
- Frida 内部的跳板、uretprobe 返回地址替换、并发 attach 时如何处理正在执行的线程，属于 frida-gum 实现，本仓库无源码，未核实。
- 需要其他读者确认的跨模块点：
  1. runtime 层（R? `bpf_attach_ctx`）在 attach/detach 时有没有锁，是否会在有命中线程时调 Frida/syscall_trace 的 detach（决定 §6 数据竞争是否真实可达）。
  2. `BPFTIME_BUILD_WITH_LIBBPF` 的默认值和它在 agent 里控制 syscall trace 注册的范围（agent 负责人）。
  3. `current_thread_bpf_cookie` 设置后是否会被复位（`bpf_attach_ctx.cpp:402`，helper 负责人）。
  4. nv_attach_impl 用的 attach type 8/9 与 `bpf_event_type::BPF_TYPE_KPROBE/KRETPROBE` 同号是否有冲突（R6）。
  5. `bpftime_prog::prepare_for_signal_execution` 的白名单与 trap 后端 helper 是否一致（R? prog/helper 负责人；本笔记只核对了 `bpf_helper.cpp:895-910` 片段）。
  6. tracefs 无权限时 syscall trace private data 抛出的异常在 agent 里到底被谁接住（agent 负责人）。

---

## 附：attach 类型 → 实现 → 触发机制 → 支持平台 总表

| attach 类型 | 编号 | 实现类 | 建立方式 | 触发机制 | eBPF ctx | 能改控制流？ | 支持平台 |
|---|---|---|---|---|---|---|---|
| uprobe | 6 | `frida_attach_impl` | `gum_interceptor_attach` + probe listener（同址共用一个） | Frida inline hook 跳板 → `uprobe_listener_on_enter` | `pt_regs`（栈副本） | 否 | Linux x86_64 / aarch64 / armhf，macOS arm64（`cmake/frida.cmake:16-36`） |
| 同上 | 6 | `trap::trap_attach_impl` | 首条指令换 `ebreak`/`c.ebreak` | SIGTRAP → `handle_hit`；原指令 out-of-line 执行或模拟 | `pt_regs`（从 ucontext 填） | 否 | 仅 Linux riscv64 |
| uretprobe | 7 | `frida_attach_impl` | `gum_make_call_listener(nullptr, on_leave)` | Frida 替换返回地址 → `uprobe_listener_on_leave` | `pt_regs`（ax/x0=返回值） | 否 | 同 Frida |
| 同上 | 7 | `trap_attach_impl` | 入口命中时把 `ra` 改成 trampoline，压影子栈 | 返回时 trampoline 的 `ebreak` → `handle_return` | `pt_regs`（a0=返回值） | 否 | riscv64 |
| uprobe_override（filter） | 1008 | `frida_attach_impl` | `gum_interceptor_replace` → `override_handler`（同址独占） | 调用被替换函数 → handler 跑程序 → `is_overrided` ? 返回值 : 5 参数重放原函数 | `pt_regs` | 是（`bpf_override_return`/`bpf_set_retval`） | 同 Frida |
| 同上 | 1008 | `trap_attach_impl` | 同 uprobe，site 标 `has_override` | SIGTRAP → 跑第一个 override → `do_return(a0=v, pc=ra)` 或继续原函数 | `pt_regs` | 是 | riscv64 |
| ureplace | 1009 | 上述两者，内部包装成 1008 | wrapper：程序成功后无条件 `bpftime_set_retval(r0)` | 同 override | `pt_regs` | 是（返回值 = r0；程序失败则原函数照跑） | 同各后端 |
| syscall trace（tracepoint `sys_enter_*`/`sys_exit_*`/`raw_syscalls`） | 2（`PERF_TYPE_TRACEPOINT`） | `syscall_trace_attach_impl` + `text_segment_transformer` | 启动时把所有可执行段的 `syscall`→`call *%rax`；attach 只是往 `[512]` set 里插回调 | `call *%rax` → 第 0 页 nop 滑板 → `syscall_hooker_asm` → agent `syscall_callback` → `dispatch_syscall` | `trace_event_raw_sys_enter`/`_exit` | 是（enter override 跳过原 syscall；exit 可改返回值） | Linux x86_64 only；需 `vm.mmap_min_addr=0`；需目标进程能读 tracefs；agent 侧需 `BPFTIME_BUILD_WITH_LIBBPF`；clone/clone3/rt_sigreturn/exit/exit_group 不触发 |
| 自定义（例：nginx 2001） | 宿主自定 | `simple_attach_impl` | `add_simple_attach_impl_to_attach_ctx` 注册；每实例一个 attach | 宿主代码显式调 trigger | 宿主自定义结构 | 由宿主解释返回值 | 任意 |
| CUDA probe/retprobe | 8/9（见 R6） | `nv_attach_impl` | — | — | — | — | R6 负责 |
