# R4 vm-helpers（VM 三层 + llvmbpf + helper + ufunc）源码笔记

基线：官方 `177a1dc465ff`，`vm/llvm-jit` 子模块 = llvmbpf `87bac39ad879`。
下文 `path:line` 均相对仓库根 `/home/y/src/bpftime-wt-official-177a1dc`。
实验程序与产物在 `/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R4/`（`exp.cpp`、`ir.cpp`、`ubpf_local.cpp`、`aot.o`）。

## 0. 一句话职责 + 范围

本块负责"把一段 eBPF 字节码变成可调用的本机函数并提供 helper"：
vm-core（C ABI 壳）→ compat（`bpftime_vm_impl` 抽象 + 名字工厂 + llvm/ubpf 两个适配器）→ llvmbpf（eBPF→LLVM IR→JIT/AOT/PTX/SPIR-V）；
runtime 侧的 `bpftime_prog`（一个程序 = 一个 VM 实例）、helper 实现与分组注册（`bpf_helper.cpp`）、ufunc（BPF 调宿主原生函数）。

读了：
- `runtime/src/bpf_helper.cpp`、`bpftime_prog.cpp`、`ufunc.cpp` 及头文件 `runtime/include/bpftime_prog.hpp`、`bpftime_helper_group.hpp`、`bpftime_ufunc.hpp`；
  为理解接口顺带读了（不在范围、不列入文件清单）：`runtime/src/bpftime_internal.h`、`runtime/extension/extension_helper.cpp:133-196`、
  `runtime/src/attach/bpf_attach_ctx.cpp:43-60,295-315,380-400`、`runtime/src/bpftime_shm.cpp:380-410,570-671`、
  `attach/base_attach_impl/base_attach_impl.hpp:90-130`、`runtime/src/bpf_map/userspace/prog_array.cpp:84-175`、`ringbuf_map.cpp:258-330`、
  `runtime/src/bpftime_config.cpp:45-140`、`tools/aot/main.cpp:60-140,200-240`、`tools/bpftimetool/main.cpp:95-125`。
- `vm/` 除子模块外全部（CMake、Makefile、README、.gitignore、vm-core、compat、example、test）。
- `vm/llvm-jit`：`CMakeLists.txt`、`src/` `include/` `cli/` 全部；`test/` `example/` 略读。
- 单测：见 §1 末表。

总行数：范围内非测试源文件约 9.37k 行（compiler.cpp 1758、bpf_helper.cpp 1444、llvm_jit_context.cpp 1069 是大头）；读过的测试约 2.6k 行。

## 1. 文件清单

### 1.1 runtime 侧

| 文件 | 行数 | 做什么 | 必读 |
|---|---|---|---|
| runtime/src/bpf_helper.cpp | 1444 | 所有内核兼容 helper 的实现、`bpf_func_id` 枚举、三组 helper 表、async-signal-safe 白名单 | 必读 |
| runtime/src/bpftime_prog.cpp | 356 | `bpftime_prog`：建 VM、设 lddw 回调、load/JIT/exec/AOT、信号路径准入；CUDA PTX→ELF（宏控） | 必读 |
| runtime/src/ufunc.cpp | 184 | ufunc 全局表、按名查 id、分发器 `__ebpf_call_ufunc_dispatcher`、参数类型转换 | 选读 |
| runtime/include/bpftime_prog.hpp | 109 | `bpftime_prog` 类定义、两个 TLS（cookie / tail_call_ret） | 必读 |
| runtime/include/bpftime_helper_group.hpp | 63 | `bpftime_helper_info`、`bpftime_helper_group` | 必读 |
| runtime/include/bpftime_ufunc.hpp | 78 | ufunc 类型枚举、`ebpf_ufunc_func_info`、扩展 helper 编号 1003–1010 | 选读 |

### 1.2 vm/（不含子模块）

| 文件 | 行数 | 做什么 | 必读 |
|---|---|---|---|
| vm/CMakeLists.txt | 77 | 选项 `BPFTIME_LLVM_JIT/UBPF_JIT`；子目录；ubpf 生命周期单测；死变量 `test_sources` | 选读 |
| vm/Makefile | 84 | 独立构建 vm 的 make 目标（build-llvm / build-ubpf …），已陈旧 | 跳过 |
| vm/README.md | 471 | VM 架构说明，**大量与代码不符**（见 §8） | 跳过（或只看图） |
| vm/.gitignore | 2 | build、.cache | 跳过 |
| vm/vm-core/CMakeLists.txt | 17 | `bpftime_vm` 静态库，链 compat + 后端 | 跳过 |
| vm/vm-core/include/ebpf-vm.h | 242 | 对外 C API（`ebpf_create/register/load/exec/compile/...`）与常量 | 必读 |
| vm/vm-core/src/ebpf-vm.cpp | 99 | C API 一行转发到 `vm_instance->...` | 必读（很短） |
| vm/compat/CMakeLists.txt | 22 | `bpftime_vm_compat` INTERFACE 库，按选项加子目录 | 跳过 |
| vm/compat/include/bpftime_vm_compat.hpp | 265 | 抽象类 `bpftime_vm_impl`、工厂表、`struct ebpf_vm` 真身 | 必读 |
| vm/compat/include/ebpf_inst.h | 202 | 旧版指令定义（字段 `code/dst_reg/src_reg/off/imm`），runtime 用它 | 选读 |
| vm/compat/llvm-vm/CMakeLists.txt | 14 | OBJECT 库 + 静态库；`add_subdirectory(../../llvm-jit)` | 选读 |
| vm/compat/llvm-vm/compat_llvm.hpp | 50 | `bpftime_llvm_vm` 双继承适配器 | 必读 |
| vm/compat/llvm-vm/compat_llvm.cpp | 89 | 转发实现 + `constructor(0)` 注册 "llvm" | 必读 |
| vm/compat/ubpf-vm/CMakeLists.txt | 29 | ExternalProject 构建 third_party/ubpf | 跳过 |
| vm/compat/ubpf-vm/compat_ubpf.hpp | 51 | `bpftime_ubpf_vm` 声明，helper id 重映射表 | 选读 |
| vm/compat/ubpf-vm/compat_ubpf.cpp | 267 | ubpf 适配：helper 重映射到 1..63、加载时打 lddw 补丁、`bpf_tail_call` 设 unwind | 选读（有 bug） |
| vm/compat/ubpf-vm/README.md | 5 | 两句说明 | 跳过 |
| vm/example/main.c | 51 | C API 示例（有 bug，见 §7） | 跳过 |
| vm/example/bpf_progs.h | 121 | 示例字节码数组 | 跳过 |
| vm/test/ubpf_lifetime.cpp | 83 | 用 `--wrap` 检查每个 ubpf VM 恰好 create/destroy 一次 | 选读 |

### 1.3 vm/llvm-jit（llvmbpf 子模块）

| 文件 | 行数 | 做什么 | 必读 |
|---|---|---|---|
| vm/llvm-jit/CMakeLists.txt | 199 | `llvmbpf_vm` 库、LLVM 组件（宿主后端+NVPTX[+SPIR-V]）、cli/test/example | 选读 |
| vm/llvm-jit/include/llvmbpf.hpp | 170 | 公共 API `llvmbpf_vm`（字段即状态全集） | 必读 |
| vm/llvm-jit/src/vm.cpp | 359 | `llvmbpf_vm` 实现：注册、load、惰性 exec、compile、AOT、调参 setter、`get_compiled_code` | 必读 |
| vm/llvm-jit/src/llvm_jit_context.hpp | 78 | `llvm_bpf_jit_context` 声明、lddw 符号名常量 | 必读 |
| vm/llvm-jit/src/llvm_jit_context.cpp | 1069 | LLVM 初始化、O0–O3 优化、LLJIT 创建与 helper 符号绑定、JIT/AOT/加载 AOT/PTX/SPIR-V、array map 内联 | 必读（前 900 行） |
| vm/llvm-jit/src/compiler.cpp | 1758 | `generateModule`：字节码→IR 的全部翻译 | 必读（核心） |
| vm/llvm-jit/src/compiler_utils.hpp | 118 | 翻译辅助声明、`ext_func_sym` 命名规则 | 必读 |
| vm/llvm-jit/src/compiler_utils.cpp | 403 | ALU/ST/LDX/JMP/端序/helper 调用/原子操作的 IR 生成 | 必读 |
| vm/llvm-jit/src/ebpf_inst.h | 256 | 新版指令定义（字段 `opcode/dst/src/offset/imm`，含 MEMSX、MOVSX、JA_IMM、ATOMIC） | 选读 |
| vm/llvm-jit/cli/CMakeLists.txt | 49 | `bpftime-vm` 可执行；ExternalProject 克隆 libbpf **到源码树内** | 跳过 |
| vm/llvm-jit/cli/main.cpp | 414 | `bpftime-vm build/run`：ELF→每个程序一个 AOT .o；跑 AOT .o；解析 BTF.ext 行号 | 选读 |
| vm/llvm-jit/cli/README.md | 19 | cli 说明（描述的是 argparse 版界面，与代码不符） | 跳过 |

test/ 与 example/（略读，列出）：
- `vm/llvm-jit/test/CMakeLists.txt`(80)：只加 `bpf_conformance_runner` 与 `unit-test`；后面的 `foreach(file ${test_sources})` 用的是未定义变量，所以 `test/src/*.c`（test.c 344、test_jit.c 51、test_vm.c 41、test_core_minimal_ffi.c 43）与 `test/include/*`（test_bpf_progs.h 118、test_defs.h 10、test_minimal_bpf_host_ufunc.h 212）**都不被构建**（死代码）。
- `test/unit-test/`：`vm_test.cpp`(512)、`bpf_prog_test.cpp`(48)、`bpf_progs.h`(489)、`CMakeLists.txt`(23) → 可执行 `llvm_jit_tests`（本树 build-jit 未构建）。
- `test/bpf_conformance_runner/main-bpf-conformance.cpp`(123)：从 stdin/参数读 base16 程序，注册 1000 个空 helper，`compile()`+`exec()` 打印 r0；配合外部 bpf_conformance 套件。
- `test/test-cases/*.data`（132 个，ubpf 风格用例）+ `test/test_framework/*.py`（pytest，`test_jit.py` 等）；`test/README.md` 列出预期失败项。
- example：`basic.cpp`(112)、`maps.cpp`(131)、`array_map_inline_bench.cpp`(182)、`CMakeLists.txt`(20)、`xdp-counter.json`、`inline/`（libmap.c/.ll、main.c、xdp-counter.ll）、`standalone/`（main.c + 2203 行 .ll）、`load-llvm-ir/`、`ptx/`（ptx_test.cpp 453、trampoline_ptx.h 747、README 282、CUDA 示例）、`spirv/`（spirv_opencl_test.cpp 398、README 988）。

### 1.4 读过的单测

| 文件 | 行数 | 覆盖点 |
|---|---|---|
| runtime/unit-test/test_probe.cpp | 254 | probe_read / write / read_str 正常与 -EFAULT；SIGSEGV 处置不被改动 |
| runtime/unit-test/test_get_current_pid_tgid.cpp | 86 | fork / 孙进程后 pid/tid 缓存刷新 |
| runtime/unit-test/test_get_current_uid_gid.cpp | 67 | `--wrap getuid/getgid`，验证每次实时查询与位布局 |
| runtime/unit-test/test_get_stack.cpp | 146 | mock attach impl（type 6）验证 get_stack 字节语义、skip、build-id 拒绝 |
| runtime/unit-test/attach_with_ebpf/helper.hpp | 15 | `create_bpftime_prog`：只加 kernel 组 + `load(false)` |
| runtime/unit-test/attach_with_ebpf/test_helpers.cpp | 57 | 加载 helpers.bpf.o，kernel 组，期望返回 23 |
| runtime/unit-test/attach_with_ebpf/test_ufunc_register.cpp | 102 | `bpftime_ufunc_register_ufunc(1/2, …)` + ufunc 组，interp/JIT 两次都得 17 |
| runtime/unit-test/attach_with_ebpf/test_attach_replace.cpp | 122 | `bpftime_ufunc_resolve_from_info` + frida override + `bpftime_set_retval` |
| runtime/unit-test/attach_with_ebpf/test_attach_uprobe_with_ebpf_trap.cpp | 252（读 140-252） | trap 后端准入：未审计 helper → `prepare_for_signal_execution` 返回 -ENOTSUP；信号路径不惰性编译 |
| runtime/unit-test/tailcall/test_user_to_user_tailcall.cpp | 79 | 用户态→用户态 tail call，期望 0x1234 |
| runtime/unit-test/tailcall/test_user_to_kernel_tailcall.cpp | 126 | 用户态→内核 prog（`bpf_prog_test_run_opts`） |
| runtime/test/src/test_ufunc.cpp | 46 | **死代码**：用的 API（`bpftime_probe_create_ctx` 等）已不存在；`runtime/test/CMakeLists.txt` 里所有源都被注释 |
| runtime/unit-test/cuda/test_cuda_compile.cpp | 98 | 仅列出（CUDA 宏控，未细读） |
| vm/test/ubpf_lifetime.cpp | 83 | 见上 |
| vm/llvm-jit/test/unit-test/vm_test.cpp / bpf_prog_test.cpp | 512 / 48 | llvmbpf API：空程序报错、重复注册、AOT 往返、"Already compiled"、LDXS 符号扩展、缺 helper/缺 map_val 报错、array map 内联、BTF 行号、PTX 目标；外部函数、原子 add |

## 2. 核心数据结构

### 2.1 `struct ebpf_vm`（C 层句柄）
- 定义：`vm/compat/include/bpftime_vm_compat.hpp:260-263`，字段 `std::string vm_name` + `std::unique_ptr<bpftime_vm_impl> vm_instance`。头文件 `vm/vm-core/include/ebpf-vm.h:54` 只给前置声明（C 侧不透明）。
- 生命周期：`ebpf_create` 里 `new`（`vm/vm-core/src/ebpf-vm.cpp:6-11`），`ebpf_destroy` `delete`（:19-22）。进程堆。每个 `bpftime_prog` 一个（`runtime/src/bpftime_prog.cpp:125`、`:152`），`~bpftime_prog` 销毁（:165-169）。
- 注意：`tools/aot/main.cpp:97,129` 直接 `vm->vm_instance->do_aot_compile(...)`，绕过 C API（C++ 能看到真身）。

### 2.2 `bpftime_vm_impl`（多后端抽象）
- `vm/compat/include/bpftime_vm_compat.hpp:27-198`。纯虚只有 `load_code`(:90) 和 `exec`(:110)；其余都有"未实现"的默认实现（打 CRITICAL/WARN/DEBUG 并返回失败值）：`get_error_message`(:37)、`toggle_bounds_check`(:47，DEBUG，返回 false)、`register_error_print_callback`(:58，WARN)、`register_external_function`(:74)、`unload_code`(:96)、`compile`(:118)、`set_lddw_helpers`(:140)、`set_pointer_secret`(:156)、`set_unwind_function_index`(:173)、`do_aot_compile`(:180)、`load_aot_object`(:186)、`generate_ptx`(:193)。
- 工厂：`detail::get_vm_factory_map()`(:204-225) 用 union + CAS 实现"首次调用者构造、永不析构"的 map；`create_vm_instance`(:228-244) 名字空抛 `runtime_error`，查不到也抛；`register_vm_factory`(:246-257) 重复则 WARN 覆盖。
- `precompiled_ebpf_function = uint64_t(*)(void *mem, size_t mem_len)`（:25）。

### 2.3 `bpftime_llvm_vm`（llvm 适配器）
- `vm/compat/llvm-vm/compat_llvm.hpp:14-46`：同时继承 `bpftime::llvmbpf_vm` 和 `compat::bpftime_vm_impl`，每个 override 一行转发（`compat_llvm.cpp:23-75`）。
- **没有** override：`toggle_bounds_check`、`register_error_print_callback`、`set_unwind_function_index`、`set_pointer_secret` → 走基类默认（失败/告警）。
- `do_aot_compile` 失败时把 optional 转成异常抛出（`compat_llvm.cpp:40-50`）。
- 注册：`__attribute__((constructor(0))) register_llvm_vm_factory()` → `register_vm_factory("llvm", …)`（`compat_llvm.cpp:82-86`）。

### 2.4 `bpftime_ubpf_vm`（ubpf 适配器）
- `vm/compat/ubpf-vm/compat_ubpf.hpp:12-48`：持有 `struct ubpf_vm*`、5 个 lddw 回调、`std::map<size_t,size_t> helper_id_map`（bpftime 编号 → ubpf 编号）、`next_helper_id = 1`。
- 构造 `ubpf_create()`，失败抛异常（`compat_ubpf.cpp:24-31`）；析构 `ubpf_destroy`（:33-36）；禁止拷贝（hpp:16-17，单测 `vm/test/ubpf_lifetime.cpp:8-9` static_assert）。
- 注册：`__attribute__((constructor)) register_ubpf_vm_factory()` → "ubpf"（`compat_ubpf.cpp:261-265`）。

### 2.5 `llvmbpf_vm`（llvmbpf 公共类）
定义 `vm/llvm-jit/include/llvmbpf.hpp:55-166`，进程堆（被 `bpftime_llvm_vm` 继承，生命周期同 `ebpf_vm`）。关键字段（:141-165）：
- `map_by_fd/map_by_idx/map_val/var_addr/code_addr`：lddw 回调，`set_lddw_helpers`（`vm.cpp:305-316`）写入。
- `instructions`：`std::vector<ebpf_inst>`，`load_code`（`vm.cpp:129-138`，长度须 8 的倍数）赋值；`unload_code`（:140-143）只清它。
- `ext_funcs`：`vector<optional<external_function{name, fn}>>`，构造时大小 `MAX_EXT_FUNCS=8192`（`vm.cpp:70-74`）；下标 = helper 编号；越界 `-E2BIG`、重复 `-EEXIST`（:85-99）。
- `array_maps`：`unordered_map<handle, array_map_descriptor>`，`register_array_map`（:101-127）。bpftime compat 层未接入 → 恒空。
- `jit_ctx`：`unique_ptr<llvm_bpf_jit_context>`，构造时创建。
- `optimization_level=3`、`target_cpu_`、`target_features_`、`disabled_passes_`、`log_passes_`、`kernel_compatible_mode_=false`、`line_info_`：调参/扩展，bpftime 均未接入。
- `jitted_function`：`optional<precompiled_ebpf_function>`，编译/加载 AOT 成功后写；**一次写入、永不清空**（`unload_code` 不清）。
- `error_msg`。

### 2.6 `llvm_bpf_jit_context`（编译器本体）
- `vm/llvm-jit/src/llvm_jit_context.hpp:36-74`。字段：`vm&`、`unique_ptr<pthread_spinlock_t> compiling`（:39）、`optional<unique_ptr<LLJIT>> jit`（:59，public）。
- 构造（`llvm_jit_context.cpp:485-513`）：进程级一次性 LLVM 初始化（CAS 静态 `llvm_initialized`），只注册宿主后端 + NVPTX（+SPIR-V，宏 `LLVMBPF_HAS_SPIRV`）；`pthread_spin_init(PRIVATE)`。析构 `pthread_spin_destroy`（:550-553）。LLJIT 实例随 context 存活（JIT 代码内存由它持有）。
- lddw 运行时符号名（hpp:23-27）：`__lddw_helper_map_by_fd/map_by_idx/map_val/var_addr/code_addr`；实际只定义/使用 `__lddw_helper_map_val`。

### 2.7 `ebpf_inst`（两份同名定义！）
- `vm/compat/include/ebpf_inst.h:22-28`：`code, dst_reg:4, src_reg:4, off, imm`（runtime 用，如 `bpftime_prog.cpp:308-316` 用 `inst.code/src_reg`）。
- `vm/llvm-jit/src/ebpf_inst.h:27-33`：`opcode, dst:4, src:4, offset, imm`（llvmbpf 用；CMake 以 PUBLIC 导出 src 目录，`vm/llvm-jit/CMakeLists.txt:21-27`）。
- 两者 include guard 都是 `EBPF_INST_H`，布局相同（8 字节）但字段名不同 → C++ ODR 违规。LTO 链接时 g++ 报 `warning: type 'struct ebpf_inst' violates the C++ One Definition Rule [-Wodr]`（已实跑：链接 `ubpf_local.cpp` 时出现）。

### 2.8 `bpftime_prog`
- `runtime/include/bpftime_prog.hpp:25-106`。字段：`name`、`ebpf_vm *vm`、`jitted`、`program_loaded`、`signal_ready`、`loaded_from_aot`、`map<unsigned,bool> signal_safe_helpers`、`ebpf_jit_fn fn`、`vector<ebpf_inst> insns`（副本）、`char *errmsg`、`bpftime_ufunc_ctx *ufunc_ctx`（**从未使用、未初始化**）、CUDA 下 `ptx_code/cuda_elf_binary`。
- 谁创建：`bpf_attach_ctx::instantiate_prog_handler_at`（`runtime/src/attach/bpf_attach_ctx.cpp:302`，`make_unique`，由 `instantiated_progs` 持有）、`bpftime_object`（`runtime/object/bpf_object.cpp:88,109`）、`bpftime_tail_call` 的临时 fallback（`runtime/src/bpf_helper.cpp:599-611`，栈上 `std::optional`）、`tools/aot`、`tools/bpftimetool`。进程堆/栈，不在 shm。
- TLS：`current_thread_bpf_cookie`、`current_thread_tail_call_ret`（`runtime/src/bpftime_prog.cpp:105-106`）。

### 2.9 helper 注册结构
- `bpftime_helper_info{unsigned index; string name; void *fn}`（`runtime/include/bpftime_helper_group.hpp:15-19`）。
- `bpftime_helper_group{ map<unsigned, bpftime_helper_info> helper_map }`（:24-57）；三个静态组都是函数内 `static const`，进程级、只读、不在 shm。
- `register_helper` 限 index ≤ 999（`bpf_helper.cpp:1131`）；但静态组用 map 构造函数直接初始化，绕过此检查（所以 ufunc 组能用 1000/1001）。

### 2.10 ufunc 结构
- `ebpf_ufunc_func_info{char name[64]; ufunc_func func; ret_type; arg_types[6]; num_args; id; is_attached}`（`runtime/include/bpftime_ufunc.hpp:44-52`）。
- `struct bpftime_ufunc_ctx{ ebpf_ufunc_func_info ufunc_funcs[MAX_UFUNC_FUNCS=32768]; size_t ufunc_func_cnt; }`（`runtime/src/bpftime_internal.h:28-31`）；唯一实例 `global_ufunc_ctx`（`runtime/src/ufunc.cpp:15`），进程 BSS，约 32768×~112B≈3.6MB（推断，按字段大小估算），无锁。
- BPF 侧约定（`runtime/unit-test/assets/ufunc.bpf.h:15-33`）：`struct arg_list{u64 args[6]}`，helper 1000 = `ufunc_call(id, &arg_list)`，1001 = `ufunc_find_func_id(name)`。

## 3. 主要流程与调用链

### 3.1 程序实例化 + helper 注册 + JIT（主路径）
`bpf_attach_ctx::instantiate_prog_handler_at` (runtime/src/attach/bpf_attach_ctx.cpp:295-315)
→ `bpftime_prog::bpftime_prog(insns,cnt,name)` (runtime/src/bpftime_prog.cpp:108-130)：拷贝指令；按 `runtime_config.vm_name`（默认 "llvm"，`runtime/include/bpftime_config.hpp:17`，env `BPFTIME_VM_NAME`）`ebpf_create`(:125)；`ebpf_set_lddw_helpers(vm, map_ptr_by_fd, nullptr, map_val, nullptr, nullptr)`(:128-129)
→ `ebpf_create` (vm/vm-core/src/ebpf-vm.cpp:6-11) → `create_vm_instance` (vm/compat/include/bpftime_vm_compat.hpp:228-244) → 工厂回调 `create_llvm_vm_instance` (vm/compat/llvm-vm/compat_llvm.cpp:12-16) → `llvmbpf_vm()` (vm/llvm-jit/src/vm.cpp:70-74) → `llvm_bpf_jit_context()` (llvm_jit_context.cpp:485-513)
→ `load_prog_and_helpers` (bpf_attach_ctx.cpp:43-60)：按 config 依次 kernel 组（仅 `__linux__`）、ufunc 组、shm map 组 `add_helper_group_to_prog`
→ `add_helper_group_to_prog` (runtime/src/bpf_helper.cpp:1163-1169) → `bpftime_prog_register_raw_helper` (bpftime_prog.cpp:278-287)：`ebpf_register`，成功时记录 `helper_is_async_signal_safe`
→ `ebpf_register` (ebpf-vm.cpp:36-40) → `llvmbpf_vm::register_external_function` (vm.cpp:85-99)：只写 `ext_funcs[idx]`
→ `bpftime_prog_load(jit)` (bpftime_prog.cpp:171-224)：`ebpf_load`(:179) → `llvmbpf_vm::load_code`；jit=true 时 `ebpf_compile`(:209)，失败 WARN "falling back to interpreter" 但仍返回 0
→ `llvmbpf_vm::compile` (vm.cpp:174-197) → `do_jit_compile` (llvm_jit_context.cpp:515-549)：持自旋锁；`create_and_initialize_lljit_instance`(:686-860) 绑定 helper；`generateModule(..., patch_map_val_at_compile_time=true)`(:527)；`optimizeModule`(:206-290)；若 `inline_array_map_lookup_helpers` 改了 IR 再优化一遍；`addIRModule`
→ `get_entry_address` (:862-880)：`lookup("bpf_main")`，此刻真正 codegen + 链接。
→ 之后 `instantiate_prog_handler_at` 再登记 attach 私有 helper（bpf_attach_ctx.cpp:310-312）——**在编译之后**（见 §7）。

### 3.2 字节码 → IR（`generateModule`，vm/llvm-jit/src/compiler.cpp:208-1758）
1. 准备：空程序报错(:222-226)；声明 lddw helper 函数（只为传入的 `lddwHelpers`，map_val 用 i64(i64)，其余 i64(i32)）(:237-260)；为每个已注册 helper 声明 `i64 _bpf_helper_ext_%04u(i64×5)`(:262-277)；kernel_compatible_mode 下为 bpf-to-bpf 目标建外部伪函数并把 `codegen_end_pc` 截到第一个被调目标(:278-310)。
2. 切基本块(:311-331)：pc0；任何 JMP/JMP32 类（含 CALL/EXIT）的下一条；`JA_IMM`（gotol）目标 `i+imm+1`（**无越界检查**）；其它跳转目标 `i+offset+1`（有 `< size` 检查）。注意：**局部调用目标 `i+imm+1` 不在这里标记**，靠"前一条通常是 exit"碰巧成为块首。
3. `bpf_main` 签名(:332-363)：普通 `i64 (ptr, i64)`；GPU `void(ptr addrspace(1), i64)`；kernel-compat 只有一个参数。
4. setupBlock(:369-460)：r0..r10 各 `alloca i64`；数据栈 `alloca i64 × (64*32+10)`（GPU 用数组类型），`r10 = stackBegin + 64*32 个 i64` = 16384 字节处；`callStack = alloca ptr × 320`，`callItemCnt = 0`；`r1 = mem`，`r2 = mem_len`。
5. 为每个块首建 `bb_inst_N`；若前一条是 `CALL src=1` 且 **`i > 1`**，登记 `localFuncRetBlks[i] = blockaddress`(:466-491)。
6. `exitBlock`：`ret load r0`(:496-507)；`localFuncReturnBlock`：弹返回地址、恢复 r6–r9、`callItemCnt -= 5`、`r10 += STACK_SIZE(64)`、`indirectbr`(:509-561)。
7. 可选 DWARF 行号（BTF line info）(:562-588, :607-623)。
8. 主循环 `for (uint16_t pc…)` + `switch(opcode)`(:593-1737)；寄存器号 >10 报错(:625-630)。
9. 无终结指令的块补 `br` 到下一块(:1739-1746)；非 GPU 跑 `verifyModule`(:1751-1755)。

### 3.3 指令类别 → IR 对照表

| eBPF 类别 / opcode | IR | 位置 | 备注 |
|---|---|---|---|
| ALU64/ALU ADD SUB MUL OR AND XOR（IMM/REG） | `add/sub/mul/or/and/xor`；ALU32 操作数为 i32（dst 直接 `load i32` 读寄存器槽低 4 字节，依赖小端），结果 `zext` 到 i64 | compiler.cpp:633-669, 770-793, 879-890；compiler_utils.cpp:6-57 | IMM 在 ALU64 下符号扩展 |
| DIV（offset=0 无符号，offset=1 有符号） | `udiv`/`sdiv`；sdiv 特判 INT_MIN/-1；最后 `select(src==0, 0, result)` | compiler.cpp:671-769 | **select 不能阻止除零指令执行 → SIGFPE**；64 位 sdiv 对 src 做 trunc32+sext（错误） |
| MOD | `urem`/`srem` + `select(src==0, dst, ...)` | :845-878 | 同上两个问题（smod 64 位 trunc：:863-871） |
| LSH/RSH/ARSH | `shl/lshr/ashr`，移位量 `urem 64/32` | :794-836, :951-971 | |
| NEG | `neg` | :837-844 | |
| MOV / MOVSX（offset 8/16/32） | 直接存；MOVSX = trunc 到 i8/16/32 再 sext | :891-949 | offset 非 0/8/16/32 报错 |
| END：LE / BE（ALU）/ BSWAP（ALU64 0xd7） | 需翻转时逐字节 `lshr+and` 再 `shl+or` 拼接；LE 只 trunc | :972-987；compiler_utils.cpp `emitALUEndianConversion` | imm 必须 16/32/64 |
| ST/STX B/H/W/DW | `gep i8 (load ptr dst), off` → `trunc` → `store` | :989-1013；compiler_utils.cpp `emitStore` | 无边界检查 |
| LDX / LDXS（MEMSX） | `gep i8 (load ptr src), off` → `load iN` → `zext`/`sext` | :1014-1049 | |
| LDDW（0x18） | 见 3.4 | :1052-1340 | 校验下一槽全 0 |
| JA | `br` 到 `pc+1+off` | :1342-1352 | |
| JA_IMM（JMP32\|JA，gotol） | `br` 到 `pc+1+imm` | :1354-1364 | |
| JEQ/JGT/JGE/JNE/JSGT/JSGE/JLT/JLE/JSLT/JSLE（JMP、JMP32，IMM/REG） | `icmp eq/ugt/uge/ne/sgt/sge/ult/ule/slt/sle` + `condbr` | :1504-1636；compiler_utils.cpp `emitCondJmpWithDstAndSrc`、`emitJmpLoadSrcAndDstAndZero` | JMP32 操作数 `load i32` |
| JSET | `and` 后 `icmp ne 0` | :1538-1559 | |
| CALL src=0（helper）与 0x8d | `call i64 @_bpf_helper_ext_%04u(r1..r5)` → 存 r0；imm==12 后 `br exitBlock` | :1366-1482；compiler_utils.cpp `emitExtFuncCall` | 0x8d（CALLX）被当成按 imm 调 helper（"workaround"注释 :1367-1369），与内核 callx 语义不同 |
| CALL src=1（bpf-to-bpf） | 自建调用栈 + `br`；返回走 `indirectbr` | :1371-1468 | 见 3.5 |
| EXIT | `callItemCnt==0 ? exitBlock : localFuncReturnBlock` | :1483-1496 | |
| ATOMIC_32/64（0xc3/0xdb）ADD/AND/OR/XOR(±FETCH)、XCHG | `atomicrmw <op> … monotonic`；FETCH 时旧值写回 src（32 位 zext） | :1637-1689；compiler_utils.cpp `emitAtomicBinOp` | 内存序 Monotonic |
| CMPXCHG | 先**非原子** `load` 旧值，再 `cmpxchg monotonic monotonic`，r0 = 先前那次 load 的值 | :1690-1720 | 并发下 r0 可能不是 cmpxchg 真正比较的值 |
| 其它（LD_ABS/IND、may_goto、load-acquire/store-release 等） | 报 "Unsupported or illegal opcode" | :1730-1735 | |

注：`compiler_utils.cpp` 的函数行号（文件内）：`emitLoadALUSource` 6、`emitLoadALUDest` 35、`emitStoreALUResult` 45、`emitALUEndianConversion` 59、`emitALUWithDstAndSrc` 125、`emitStoreLoadingSrc` 136、`emitStoreWritingResult` 146、`emitStore` 156、`emitJmpLoadSrcAndDstAndZero` 167、`loadJmpDstBlock` 198、`loadCallDstBlock` 217、`emitLDXLoadingAddr` 263、`emitLoadX` 285、`emitCondJmpWithDstAndSrc` 295、`emitExtFuncCall` 312（r0 写回 356-358、tail call 跳出 362-364、报错 367-371）、`emitAtomicBinOp` 372（Monotonic 388）。

实跑 IR（`ir.cpp`，`do_aot_compile(print_ir=true)`，打印的是**优化前** IR，见 `llvm_jit_context.cpp:564-568`）关键片段：
```
%stackBegin = alloca i64, i32 2058 ; %stackEnd = getelementptr i64, ptr %stackBegin, i32 2048
store i64 5, ptr %r1                          ; lddw src=1：map_by_fd(5) 编译期烧成常量 5
%3 = call i64 @__lddw_helper_map_val(i64 5)   ; lddw src=2（AOT 路径运行时调用）
%10 = call i64 @_bpf_helper_ext_0001(i64 %5, ..., i64 %9)
%36 = add i64 %35, 64                         ; localFuncReturnBlock：r10 += 64（字节）
```

### 3.4 lddw 各 src_reg（llvmbpf，compiler.cpp:1052-1340）
JIT 路径 `patch_map_val_at_compile_time=true`（llvm_jit_context.cpp:527），AOT 路径 false（:560），PTX/SPIR-V true（:935,:1030）。

| src | 含义 | llvmbpf 行为 | kernel_compat |
|---|---|---|---|
| 0 | 64 位立即数 | `imm | next.imm<<32` 常量 (:1093-1097) | 同 |
| 1 | map fd | 有 `map_by_fd` → 常量 `map_by_fd(imm)`；否则常量 `(int64)imm` (:1098-1128) | 外部符号 `__llvmbpf_pseudo_map_fd_%08x` (:1099-1110) |
| 2 | map value | mapPtr 同上；编译期：缺 `map_val` 报错，否则常量 `map_val(mapPtr)+next.imm`；运行期（AOT）：`call __lddw_helper_map_val(mapPtr) + next.imm`，未定义该符号则报错 (:1129-1200) | `__llvmbpf_pseudo_map_value_fd_%08x_off_%08x` |
| 3 | var_addr | 缺回调报错，否则常量 `var_addr(imm)` (:1201-1221) | 报错 |
| 4 | code_addr | 同上 (:1222-1243) | 报错 |
| 5 | map_by_idx | 有回调常量，否则 `(int64)imm`（INFO 日志）(:1244-1269) | 报错 |
| 6 | map_by_idx + map_val | 同 src=2 (:1270-1338) | 报错 |
| 7+ | — | **静默不写 dst**（if 链没有 else，`break` 掉） | 同 |

bpftime 只提供 `map_by_fd = map_ptr_by_fd`（返回 fd 本身，`runtime/src/bpftime_shm.cpp:635-648`；非 map 返回 `INVALID_MAP_PTR=-1`）和 `map_val`（`bpftime_shm.cpp:650-671`，取 key=0 的 value 地址）。所以 bpftime 下 src=3/4 编译失败，src=5/6 用 imm 当句柄（推断：一般不会出现）。

ubpf 适配器在 `load_code` 里自己做同样的 lddw 修补（`vm/compat/ubpf-vm/compat_ubpf.cpp:106-197`）：src 0–6 全支持（缺回调报 -EINVAL），算出 64 位值写回两槽并把 `src_reg` 清 0。

### 3.5 bpf-to-bpf 调用（compiler.cpp:1371-1468 / 509-561）
调用：`callItemCnt += 5`；`callStack[cnt-1] = blockaddress(返回块)`；`callStack[cnt-2..cnt-5] = r6..r9`；`r10 -= STACK_SIZE`；`br 目标块`。返回：`EXIT` 发现 `callItemCnt != 0` → `localFuncReturnBlock` 恢复并 `indirectbr`。
- `STACK_SIZE = (512+7)/8 = 64`（单位是 i64 槽，compiler.cpp:42），但 r10 存的是字节地址，`r10 -= 64` 只移 64 字节（:1455-1461，返回 :548-554）。**已实跑确认栈帧重叠**（见 §9）。
- 无深度检查；`callStack` 320 槽（深度 64），数据栈预留 32 帧。
- `localFuncRetBlks` 条件 `i > 1`（:482）：调用在 pc0 时返回块不登记 → Release 下 `localFuncRetBlks[pc+1]` 插入空指针 → 运行时 `indirectbr` 到空地址。**已实跑：SIGSEGV**。

### 3.6 helper 绑定（llvm_jit_context.cpp:686-860 + compiler_utils.cpp:312-371）
- 命名：`ext_func_sym(idx)` = `"_bpf_helper_ext_%04u"`（compiler_utils.hpp:39-44）。
- 绑定端：遍历 8192 个 `ext_funcs`，已注册的做成 `absoluteSymbols`（Callable|Exported）定义进 main JITDylib（:768-807）；同时 `map_val` 非空时定义 `__lddw_helper_map_val`（:808-858）。另挂 `DynamicLibrarySearchGenerator::GetForCurrentProcess`（:759-767），并预先 `AddSymbol` 三个 ORC 运行时包装符号（:696-714）。
- 引用端：`generateModule` 只为已注册的名字建声明；`emitExtFuncCall` 找不到 → `"Ext func not found: _bpf_helper_ext_XXXX"`（`allow_implicit_external` 只在 kernel_compat 下为真，调用点 compiler.cpp:1471-1475 两个参数都传 `kernel_compatible_mode`）。
- 调用约定：一律 `i64(i64,i64,i64,i64,i64)`，传 r1..r5，返回写 r0。
- ubpf：`register_external_function` 每注册一个就分配新 id（从 1 递增）并 `ubpf_register`；名字是 "bpf_tail_call" 时 `ubpf_set_unwind_function_index`（compat_ubpf.cpp:55-67）。`load_code` 把所有 `code==0x85` 的 imm 换成映射后的 id，不在表里则 -EINVAL（:83-104）。ubpf 上限 `MAX_EXT_FUNCS 64`（`third_party/ubpf/vm/ubpf_vm.c:33`，仅查常量）。

### 3.7 惰性 JIT 与执行
`bpftime_prog_exec` (runtime/src/bpftime_prog.cpp:238-276)：
- 信号路径（`attach::current_signal_callback` 非空）：要求 `signal_ready && jitted && return_val`，直接 `fn()`，否则 `-ENOTSUP`（:241-248）。
- 普通路径：`bpftime_protect_disable()`(:255) → jitted 则 `fn(mem,len)`(:263)，否则 `ebpf_exec`(:266) → 返回值 `current_thread_tail_call_ret.value_or(val)` 并清 TLS(:271-272) → `bpftime_protect_enable()`(:274)。
- `ebpf_exec` (ebpf-vm.cpp:57-61) → llvm：`llvmbpf_vm::exec` (vm.cpp:145-172)：有缓存直调；否则 `compile()`，成功则写缓存再递归调用；失败返回 -1 且**不缓存失败**（下次再编）。→ ubpf：`ubpf_exec` 真解释器（compat_ubpf.cpp:215-218）。
- `jit_enabled=false`（env `BPFTIME_DISABLE_JIT`，`runtime/src/bpftime_config.cpp:106-107`）对 llvm 只是把编译推迟到首个事件。

### 3.8 AOT 产物格式与加载
- 生成：`llvmbpf_vm::do_aot_compile` (vm.cpp:318-327) → `llvm_bpf_jit_context::do_aot_compile(bool)` (llvm_jit_context.cpp:623-655)：收集已注册 helper 名；非 kernel_compat 时若 `map_val` 非空加入 `__lddw_helper_map_val` → `do_aot_compile(ext, lddw, print_ir)` (:555-621)：`generateModule(..., false)`，可选打印**优化前** IR，`optimizeModule`，宿主 TargetMachine（尊重 target_cpu/features），`addPassesToEmitFile(ObjectFile)` → 字节数组。
- 格式（已实跑 `aot.o`）：宿主架构的**可重定位 ELF（ET_REL）**，`readelf -h` → `Type: REL`、`Machine: X86-64`；`nm` → `T bpf_main`、`U _bpf_helper_ext_0001`、`U __lddw_helper_map_val`。即：入口 `bpf_main(void*, u64)`，helper 与 map_val 以未定义符号留待加载时解析。map fd（lddw src=1/2 的 mapPtr）在 AOT 编译时就烧进代码。
- 加载：`llvmbpf_vm::load_aot_object` (vm.cpp:329-348)：已编译则报 "Already compiled" → `jit_ctx->load_aot_object` (llvm_jit_context.cpp:657-683)：`jit` 已存在抛异常；新建 LLJIT（同样绑定当时已注册的 helper 和 map_val）→ `addObjectFile` → 试 `get_entry_address`。所以**加载方必须在 load 之前按相同编号注册 helper**。
- bpftime 侧：`bpftime_prog::load_aot_object` (bpftime_prog.cpp:342-354) → `ebpf_load_aot_object` (ebpf-vm.cpp:93-99) → compat (compat_llvm.cpp:52-56)；置 `jitted=true, loaded_from_aot=true`，**不置 `program_loaded`**。消费者：`tools/aot/main.cpp:222-226`（先注册 kernel+shm 组再 load）、`tools/bpftimetool/main.cpp:110-114`（从 shm 的 `prog.aot_insns`）。生产者：`tools/aot/main.cpp:72-140`（注册 1000 个 `empty_helper`，先 `bpftime_prog_load(true)` JIT 一遍再 `do_aot_compile`；`compile` 子命令把结果写回 shm 的 `aot_insns`）。
- llvmbpf 自带 cli：`vm/llvm-jit/cli/main.cpp` `build` 注册 1000 个 fn=nullptr 的伪 helper（:254-258）后 `do_aot_compile`，每个程序写 `<name>.o`；`run` 只注册 helper 6（:311-312）后 `load_aot_object` + `exec`。
- `get_compiled_code()` (vm.cpp:278-303) 为了拿代码大小会**再 AOT 编译一次**并解析 `bpf_main` 符号大小（:14-66）。

### 3.9 tail call（bpf_helper.cpp:555-660 + compiler_utils.cpp:356-364 + bpftime_prog.cpp:271）
1. 程序 `call 12` → `bpftime_tail_call(ctx, prog_array_fd, index)`（3 参）。
2. 校验 prog array（`bpftime_is_prog_array`）→ `bpftime_map_lookup_elem(fd,&idx)` 得到目标 fd（`prog_array.cpp:115-144`：用户态 prog 存编码后的 bpftime fd；内核 prog 存 prog id，lookup 时 `BPF_PROG_GET_FD_BY_ID` 现开一个 fd）。
3. 若是 bpftime prog fd：TLS 深度计数 ≤ 32（:574-591）；优先 `get_global_attach_ctx().find_instantiated_prog(fd)`（异常吞掉）；否则从 shm handler 现场构造 `bpftime_prog` 并 `load_prog_and_helpers`（会 JIT）；**拷贝 ctx 前 64 字节到栈上 `context[64]`** 后 `bpftime_prog_exec(context, 64, &retval)`；把 retval 写 `current_thread_tail_call_ret`，返回 0。
4. 否则（内核 prog）：同样拷 64 字节，`bpf_prog_test_run_opts`，`close(fd)`（关闭的是 lookup 时新开的 fd，正确），retval 写 TLS（:633-653）；无 libbpf 时 -ENOTSUP。
5. JIT 侧：llvmbpf 对 helper 12 调用后**无条件** `br exitBlock`（compiler_utils.cpp:362-364）；ubpf 侧是"返回 0 才 unwind"（ubpf 语义，compat_ubpf.cpp:64-65）。
6. 外层 `bpftime_prog_exec` 用 TLS 值覆盖返回值。

### 3.10 输出路径：perf_event_output 与 ringbuf
- `bpf_perf_event_output(ctx, map, flags, data, size)` (bpf_helper.cpp:487-553)：`my_sched_getcpu` → `sched_getaffinity` 保存 → `sched_setaffinity` 绑到当前 CPU → `bpftime_map_get_info` 查类型 →
  - `PERF_EVENT_ARRAY`：`bpf_map_lookup_elem(fd, &cpu, false)` 得到 perf handler fd → `bpftime_perf_event_output` (bpftime_shm.cpp:575-596) → 软件 perf buffer `output_data`；
  - `KERNEL_USER_PERF_EVENT_ARRAY`（Linux+libbpf）：`bpftime_shared_perf_event_output` (bpftime_shm.cpp:599-624) → `output_data_into_kernel`；
  - 其它：-1。
  → `restore_affinity` 恢复亲和性并保留 errno（:546-552）。`flags`（BPF_F_CURRENT_CPU / 索引）与 `ctx` **完全忽略**。
- ringbuf：`bpf_ringbuf_reserve` (:450-459) → `bpftime_ringbuf_reserve` (bpftime_shm.cpp:388-399) → `ringbuf::reserve(size, self_fd)`，记录头 `ringbuf_hdr{u32 len; i32 fd}`（`ringbuf_map.cpp:258-261,321-325`）；`bpf_ringbuf_submit/discard` (:461-485) 从 `((int32_t*)data)[-1]` 取回 fd 再 `bpftime_ringbuf_submit(fd, data, discard)`；`bpf_ringbuf_output` (:432-448) = reserve + memcpy + submit。flags 非 0 只 WARN。

### 3.11 ufunc
- 是什么：让 BPF 程序通过两个扩展 helper 调用**宿主进程里的原生 C 函数**（可用于 hotpatch/替换函数逻辑，例 `example/hotpatch/redis/ufunc.bpf.h`）。
- 怎么用：宿主侧 ① `bpftime_ufunc_resolve_from_info(info, resolver)`（ufunc.cpp:52-72）：用 resolver（如 `attach::find_function_addr_by_name`）按名字解析地址，追加到 `ufunc_funcs[cnt++]`；或 ② `bpftime_ufunc_register_ufunc(id, info)`（:47-50）直接写指定槽（不增 cnt）。② 开启 ufunc helper 组（`BPFTIME_HELPER_GROUPS` 含 `ufunc`，或不设该变量时三组全开，`bpftime_config.cpp:92-104`）。BPF 侧 `UFUNC_CALL_n(id, …)` 把参数装进栈上 `arg_list`，`call 1000(id, &arg_list)`；或 `UFUNC_CALL_NAME_n` 先 `call 1001(name)` 查 id。
- 调用链：`call 1000` → `__ebpf_call_ufunc_dispatcher(id, arg_list)` (ufunc.cpp:74-113)：id 越界 -1；无函数或 `is_attached` 返回 0；`num_args>6` 返回 -1；按 `arg_types` `to_arg_val` (:115-151) 转换；**只传前 5 个参数**，以 `void*` 方式调用 `func(a0..a4)`；返回值 `from_arg_val` (:153-182)。`call 1001` → `__ebpf_call_find_ufunc_id` (:30-45)：在 `[0, cnt)` 线性 `strcmp`。
- 组注册：`get_ufunc_helper_group()` (bpf_helper.cpp:1181-1186) → `get_extension_helper_group()`（`runtime/extension/extension_helper.cpp:133-196`）：1001、1000，另按宏 1003/1004（FS helper）、1006–1010（io_uring）。

## 4. 跨模块接口

### 4.1 本块对外提供
| 接口 | 定义 | 调用点 |
|---|---|---|
| C API `ebpf_create/destroy/register/load/unload_code/exec/compile/set_lddw_helpers/get_vm_name/load_aot_object` | vm/vm-core/include/ebpf-vm.h；ebpf-vm.cpp | runtime/src/bpftime_prog.cpp:125,128,152,156,160,166,167,179,209,266,281,303,332,346；benchmark/test_embed.c:155；vm/test/ubpf_lifetime.cpp；runtime/test/include/test_minimal_bpf_host_ufunc.h:194（死代码） |
| `ebpf_toggle_bounds_check/set_error_print/set_unwind_function_index/set_pointer_secret` | ebpf-vm.cpp:24-34,82-91 | 仅 vm/example/main.c:32（toggle）；runtime 不调用 |
| `struct ebpf_vm` 内部 `vm_instance->do_aot_compile` | bpftime_vm_compat.hpp:260 | tools/aot/main.cpp:97,129 |
| `bpftime_vm_impl`、`register_vm_factory`、`create_vm_instance` | bpftime_vm_compat.hpp | compat_llvm.cpp:82-86、compat_ubpf.cpp:261-265；第三方后端可扩展 |
| `llvmbpf_vm` | vm/llvm-jit/include/llvmbpf.hpp | compat_llvm.hpp:14；vm/llvm-jit/cli/main.cpp:237,310；bpf_conformance_runner；llvmbpf example/test |
| 构建目标 `bpftime_vm`、`bpftime_llvm_vm(_obj)`、`bpftime_ubpf_vm(_obj)`、`llvmbpf_vm` | vm/**/CMakeLists.txt | runtime/agent/CMakeLists.txt:27-46（llvm 的 OBJECT 文件直接塞进 agent :36）；runtime/syscall-server/CMakeLists.txt:16-24；tools/cli/CMakeLists.txt:12-19（`--whole-archive`）；顶层 CMakeLists.txt:197-216（静态总库） |
| `bpftime_prog`（ctor、`bpftime_prog_load`、`exec`、`register_raw_helper`、`prepare_for_signal_execution`、`load_aot_object`、`get_vm`、`get_insns`、`prog_name`、`is_cuda`） | runtime/include/bpftime_prog.hpp | runtime/src/attach/bpf_attach_ctx.cpp:43-60,302-312,393；runtime/object/bpf_object.cpp:88,109；bpf_helper.cpp:604-621；tools/aot/main.cpp:88-92,205-227；tools/bpftimetool/main.cpp:99-118；CUDA attach（未读） |
| TLS `current_thread_bpf_cookie` / `current_thread_tail_call_ret` | bpftime_prog.cpp:105-106 | cookie：runtime/src/bpftime_shm.cpp:262-269（及 attach 回调设置，未读）；tail_call_ret：bpf_helper.cpp:629,652、bpftime_prog.cpp:271-272 |
| `bpftime_helper_group` 三组 + `add_helper_group_to_prog` | bpf_helper.cpp:1163-1413 | bpf_attach_ctx.cpp:46-58；runtime/syscall-server/syscall_server_utils.cpp:188-202；tools/aot/main.cpp:60-70,222-225；tools/bpftimetool/main.cpp:102-105；多处单测 |
| `helper_is_async_signal_safe` | bpf_helper.cpp:895-910 | bpftime_prog.cpp:284 |
| helper C 函数（`extern "C"`，如 `bpftime_probe_read`、`bpf_probe_read_str`、`bpftime_get_stack`…） | bpf_helper.cpp | 单测直接调用（test_probe.cpp:26-31 等） |
| ufunc：`bpftime_ufunc_register_ufunc`、`bpftime_ufunc_resolve_from_info`、`__ebpf_call_*`、`to_arg_val/from_arg_val` | ufunc.cpp | extension_helper.cpp:136-146；unit-test test_ufunc_register.cpp:45-56、test_attach_replace.cpp:57-70、test_shm_progs_attach.cpp:82-92 |
| `compile_ptx_to_elf`（CUDA 宏） | bpftime_prog.cpp:42-104 | CUDA attach（未读） |

### 4.2 本块依赖外部
| 依赖 | 被调用处 |
|---|---|
| `bpftime_get_runtime_config/set_runtime_config`（config 模块） | bpftime_prog.cpp:114,140-141；bpf_helper.cpp:609 |
| `map_ptr_by_fd`、`map_val`（shm 模块，bpftime_shm.cpp:635-671） | bpftime_prog.cpp:128,156,160（作为 lddw 回调） |
| `bpftime_protect_disable/enable`（MPK） | bpftime_prog.cpp:255,274 |
| `attach::current_signal_callback`、`current_thread_attach_func_ip`、`bpftime_override_return/set_retval`（attach/base_attach_impl.hpp:22,35,97-130） | bpftime_prog.cpp:241；bpf_helper.cpp:665,679,681,1276-1298,897-900 |
| trap helper `bpftime_trap_get_func_arg/ret/retval`（宏 `BPFTIME_HAVE_TRAP_UPROBE_ATTACH`） | bpf_helper.cpp:902-908 |
| `shm_holder.global_shared_memory.bpf_map_*`（from_syscall=false） | bpf_helper.cpp:299-341,519-521 |
| `bpftime_map_lookup_elem`、`bpftime_is_prog_array`、`bpftime_is_prog_fd`、`bpftime_map_get_info`、`get_handler`、`try_get_stack_trace_impl` | bpf_helper.cpp:512,558,564,573,601-603,815-820 |
| `bpftime_ringbuf_reserve/submit`、`bpftime_perf_event_output`、`bpftime_shared_perf_event_output` | bpf_helper.cpp:440-446,458,470,483,529,536 |
| `bpftime_get_current_thread_cookie` | bpf_helper.cpp:668 |
| `get_global_attach_ctx()`（弱符号默认实现抛异常，bpf_helper.cpp:58-65）→ `find_instantiated_prog`、`get_attach_impl_by_attach_type(6)`、`call_attach_specific_function("generate_stack")` | bpf_helper.cpp:595,784-794,851-861 |
| `load_prog_and_helpers` | bpf_helper.cpp:607 |
| `my_sched_getcpu`（platform_utils） | bpf_helper.cpp:490,686 |
| `get_extension_helper_group`（extension 模块） | bpf_helper.cpp:1183-1185 |
| LLVM（OrcJIT/LLJIT、PassBuilder、TargetMachine、DIBuilder）；ubpf（`ubpf_create/register/load/exec/compile/...`）；spdlog；libbpf（cli、tail call 内核路径 `bpf_prog_test_run_opts`）；nvPTXCompiler（CUDA） | 各处 |

## 5. 配置项、环境变量、编译开关

| 名字 | 默认 | 定义 | 读取 | 影响 |
|---|---|---|---|---|
| `BPFTIME_VM_NAME` | "llvm"（`DEFAULT_VM_NAME`，runtime/include/bpftime_config.hpp:17） | env | runtime/src/bpftime_config.cpp:130-135（`set_vm_name` 用 `strcpy` 进 128 字节数组，config.hpp:110-113） | 选后端；空串会让 `create_vm_instance` 抛异常 |
| `BPFTIME_DISABLE_JIT` | 未设 = jit_enabled true（config.hpp:55） | env | bpftime_config.cpp:106-107 | `bpftime_prog_load(false)`：llvm 推迟到首事件编译；ubpf 解释执行 |
| `BPFTIME_HELPER_GROUPS` | 未设 = kernel/ufunc/shm_map 全开（bpftime_config.cpp:99-103）；注意结构体默认值 ufunc=false（config.hpp:58-60），只有走 env 构造才全开 | env | bpftime_config.cpp:92-98，token `ufunc`/`kernel`/`shm_map` | 决定 `load_prog_and_helpers` 注册哪些组 |
| `BPFTIME_LLVM_SONAME` | 未设 | env | llvm_jit_context.cpp:719（仅 `BPFTIME_ENABLE_LLVM_PRELOAD`） | openEuler 预加载 libLLVM |
| `SPDLOG_LEVEL` | — | env | cli/main.cpp:365 `load_env_levels` | cli 日志 |
| `ENABLE_PROBE_READ_CHECK` / `ENABLE_PROBE_WRITE_CHECK` | ON（cmake/StandardSettings.cmake:110,112）；**本树 build-jit 为 OFF**（CMakeCache） | CMake → `CMakeLists.txt:189-195` 给 runtime 加宏 | bpf_helper.cpp:68-73,97-177,343-418 | 关闭时 probe_read/write/read_str 退化为裸 memcpy/逐字节读，坏指针直接崩宿主 |
| `BPFTIME_LLVM_JIT` / `BPFTIME_UBPF_JIT` | 都 ON（StandardSettings.cmake:10-11；vm/CMakeLists.txt:11-17） | CMake | vm/CMakeLists.txt:36-42、compat/CMakeLists.txt:14-22、runtime/CMakeLists.txt:127-134 | 编哪个后端；`BPFTIME_BPF_RUNTIME_NAME` 两个都开时被后者覆盖为 ubpf，但该变量只用于"都没开"的告警 |
| `BPFTIME_ENABLE_MPK` | OFF | StandardSettings.cmake:84-86 | bpftime_prog.cpp:297-300 | 开启时 trap 信号路径一律 -ENOTSUP |
| `BPFTIME_HAVE_TRAP_UPROBE_ATTACH` | 按平台 | runtime/CMakeLists.txt:169 | bpf_helper.cpp:7-9,902-908 | trap helper 进入 async-signal-safe 白名单 |
| `ENABLE_BPFTIME_VERIFIER` | OFF | runtime/CMakeLists.txt:205-209 | bpf_helper.cpp:31-33,1415-1442 | ufunc helper 原型供 verifier |
| `BPFTIME_ENABLE_CUDA_ATTACH` | OFF | — | bpftime_prog.hpp:20-23,58-78；bpftime_prog.cpp:13-15,41-104 | PTX→ELF、`is_cuda()`（名字前缀 `cuda__`） |
| `BPFTIME_BUILD_WITH_LIBBPF` | ON（build-jit） | — | bpf_helper.cpp:25-28,533-539,633-659；compat/llvm-vm/CMakeLists.txt:10-14（决定 llvm-jit 子目录是否 EXCLUDE_FROM_ALL） | 共享 perf array、tail call 内核路径 |
| `BPFTIME_ENABLE_FS_HELPER`、`BPFTIME_ENABLE_IOURING_EXT` | — | — | extension_helper.cpp:148-193 | ufunc 组额外 helper 1003/1004、1006–1010 |
| `EBPF_STACK_SIZE`=512、`MAX_EXT_FUNCS`=8192、`EBPF_MAX_INSTS`=65536 | — | ebpf-vm.h:33-49；bpftime_vm_compat.hpp:13-20；llvmbpf.hpp:12-14；llvm_jit_context.hpp:31-34 | compiler.cpp:42；vm.cpp:71 | `EBPF_MAX_INSTS` 在 llvmbpf 中**未被检查** |
| llvmbpf CMake：`ENABLE_LLVM_SHARED`、`BUILD_LLVM_AOT_CLI`、`BPFTIME_ENABLE_LLVM_PRELOAD`、`LLVMBPF_ENABLE_PTX`、`LLVMBPF_ENABLE_SPIRV` | NO/NO/OFF/OFF/OFF | vm/llvm-jit/CMakeLists.txt:57-59,190-191 | :61-101,121-126,158-174,193-199 | 注意 `LLVMBPF_ENABLE_SPIRV` 的 `option()` 在 :191，晚于 :91/:163 的使用（首次 configure 只有 `-D` 传入才生效） |
| SPDLOG_ACTIVE_LEVEL | Debug=TRACE，否则 INFO | vm/llvm-jit/CMakeLists.txt:13-19 | — | Release 下 DEBUG/TRACE 宏被编译掉 |

## 6. 并发、生命周期与错误处理

- **LLVM 全局初始化**：静态 `llvm_initialized` + CAS（llvm_jit_context.cpp:483-510），只保证"一个线程初始化"，后到者不等待完成（推断：并发首建 VM 时可能在初始化完成前使用 LLVM）。
- **编译锁**：`do_jit_compile`、`generate_ptx`、`generate_spirv` 持 `compiling` 自旋锁（:517,908,1003）；`do_aot_compile`、`load_aot_object` **不持锁**。`llvmbpf_vm::exec/compile` 对 `jitted_function` 的检查-编译-写回不在锁内（vm.cpp:148-171,176-191）：两个线程同时首次 `exec` 会各编一次，后者替换 `jit_ctx->jit`，旧 LLJIT 析构后先拿到的函数指针悬空（推断，未构造用例）。
- **工厂表**：union+CAS，同上只防重复构造；注册发生在加载期构造函数里。
- **TLS**：`current_thread_bpf_cookie`、`current_thread_tail_call_ret`、tail call 深度计数（bpf_helper.cpp:575）、`bpftime_cached_tid`（:214）、ringbuf/prog_array 的 TLS lookup 值（外部）。
- **fork**：`pthread_atfork` 在子进程刷新 probe 用 pid（bpf_helper.cpp:70-72）与 pid/tid 缓存（:212-231）；注册失败则退回每次实时查询（:239-248、:113、:154）。单测 test_get_current_pid_tgid.cpp 覆盖到孙进程。
- **全局非线程安全**：`global_ufunc_ctx` 无锁；`bpftime_get_current_comm` 的函数内 `static std::string` 惰性填充有数据竞争（bpf_helper.cpp:279-294）；`rand()`（glibc 内部有锁，推断）。
- **错误处理（fail 方式）**：
  - llvmbpf API 全 `noexcept`，内部 try/catch 转 `error_msg`；但 `create_and_initialize_lljit_instance` 与 `generate_ptx/spirv` 用 `static ExitOnError`（:688,742,750,907,938,1002,1033）——宿主目标探测失败或 PTX 生成时 `generateModule` 失败会**直接 `exit()` 整个进程**。
  - `create_vm_instance` 抛 `std::runtime_error`，经 `extern "C" ebpf_create` 传出（C 调用者无法捕获）。
  - `bpftime_prog_load(true)` 编译失败只 WARN 并返回 0（fail-open）；llvm 后端此后每次 exec 重编、每次失败（程序体永不执行，`bpftime_prog_exec` 只打 ERROR，返回 res<0）。
  - helper 内部异常：`bpftime_set_retval/override_return` 无回调时 `throw`（base_attach_impl.hpp）；`get_global_attach_ctx` 弱实现 `throw`。异常穿过 JIT 帧（推断：依赖 ORC 注册的 EH frame）。
  - `add_helper_group_to_prog` 忽略单个注册返回值（bpf_helper.cpp:1165-1167），恒返回 0。
  - ubpf：`register_external_function` 即使 `ubpf_register` 失败（id≥64）也已写入 `helper_id_map`（compat_ubpf.cpp:60-63）。
- **信号路径**：`prepare_for_signal_execution`（bpftime_prog.cpp:289-340）一次性审计：拒绝 CUDA/AOT/非 llvm/ubpf 后端/CALLX(0x8d)/ubpf 下的局部调用/白名单外 helper；然后 eager JIT。白名单 `helper_is_async_signal_safe`：173、174、58、187（+trap 183/184/186），并且要求函数指针也对得上。

## 7. 不直观的行为、疑似 bug、代码与注释不一致

1. **运行时除数为 0 → 宿主进程 SIGFPE（x86）**。DIV/MOD 用 `select(src==0, …, udiv/sdiv/urem/srem(dst,src))`（compiler.cpp:681-764, 851-875），除法指令仍无条件执行；LLVM 里除零是 UB。**已实跑确认**：除数从内存读入 0 时 udiv64/umod64/sdiv64 子进程均被 signal 8 杀死，O0 也一样；常量 0 除数时被常量折叠所以返回 0（解释了 test-cases 的 div-by-zero 用例为何能过）。内核语义：结果 0 / 保持 dst。确定程度：确认（x86_64）。aarch64 硬件除零不陷入，但仍是 IR 层 UB（推断）。
2. **64 位有符号 DIV/MOD（offset=1）把寄存器除数截成 32 位再符号扩展**（compiler.cpp:698-702, 867-871），注释说是为了"src 应符号扩展"，只对 IMM 正确。**已实跑**：0x200000000 sdiv 0x100000003 得 0xaaaaaaaa（应为 1）；smod 得 2（应为 0xfffffffd）。另外 `src_is_zero` 在截断前计算，若低 32 位为 0 而高位非 0 → 截断后除 0 → SIGFPE（推断）。确认。
3. **bpf-to-bpf 调用栈帧只隔 64 字节**（compiler.cpp:42,1455-1461,548-554；IR 实跑可见 `add i64 %35, 64`）。**已实跑**：调用方写 [r10-72]=0x11，被调方写 [r10-8]=0x22，返回后调用方读到 0x22（应 0x11）。现有指南标为"未验证"，现已确认。
4. **pc0 处的局部调用使 JIT 代码崩溃**（`i > 1`，compiler.cpp:482）。**已实跑**：llvm 后端 SIGSEGV；同程序把调用挪到 pc1 则正确返回 0x11。确认。
5. **ubpf 适配器把局部调用（src_reg=1）当 helper 改写 imm**（compat_ubpf.cpp:83-104 只看 `code==EBPF_OP_CALL`，不看 src_reg）。**已实跑**：注册了 helper 2 时，`call +2`（src=1）被改成 imm=1（映射后的 helper id）但 src 仍为 1，ubpf 把它当局部调用跳到错误位置：pc0 版返回 0x1、pc1 版返回 0x0（期望 0x11）。若 imm 不在映射表则加载失败。`bpftime_prog.cpp:316-321` 的注释承认"只有 LLVM 兼容层能区分局部调用"。确认。
6. **tail call 失败不落空执行**：llvmbpf 对 helper 12 无条件 `br exitBlock`（compiler_utils.cpp 文件内 362-364）。**已实跑**：helper 12 返回 -1，后续 `r0=7` 未执行，返回 0xffffffffffffffff。内核语义是失败则继续执行下一条。ubpf 后端是"返回 0 才 unwind"，两后端行为不一致。确认。
7. **`unload_code()` 不清 JIT 缓存**（vm.cpp:140-143；头文件注释 llvmbpf.hpp:76 说会清）。**已实跑**：加载返回 1 的程序→exec→unload→加载返回 2 的程序→exec 仍得 1。`bpftime_prog_unload` 在未 jitted 时调它，配合惰性 JIT 也会踩到（test_ufunc_register.cpp 先 interp 后 JIT 能过只因两次是同一份代码）。确认。
8. **`generate_ptx(target_cpu)` 实参落到 bool 形参**：`vm.cpp:352` 调 `jit_ctx->generate_ptx(target_cpu)`，而该函数签名是 `(bool main_with_arguments, string func_name, const char *target_cpu)`（llvm_jit_context.hpp:66-69）。指针转 bool=true，目标 CPU 恒为默认 sm_60。**已实跑**：请求 sm_80，输出 `.target sm_60`。确认。
9. **CMPXCHG 的 r0 来自 cmpxchg 之前的一次非原子 load**（compiler.cpp:1698-1718），并发下与真正比较的旧值不一致；所有原子操作用 `Monotonic`（compiler_utils.cpp 文件内 388；compiler.cpp:1713-1714），比内核 BPF_FETCH 类原子的全序语义弱。推断（需并发测试）。
10. **`!err` 判断写反**：`mainDylib.define(...)` 返回 `llvm::Error`，非 PRELOAD 分支 `if (auto err = …; !err) SPDLOG_DEBUG("failed…")`（llvm_jit_context.cpp:804-806, 855-857）——成功时打 "failed"，失败时 Error 未被检查（LLVM ABI-breaking-checks 构建会 abort，本树定义了 `LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING`）。确认（读码）。
11. **`uint16_t` 程序计数器**：`for (uint16_t i = 0; i < insts.size(); i++)`（compiler.cpp:281,314）与 `pc`（:593）、跳转目标计算全是 uint16_t；指令数 > 65535 时循环永不结束/目标回绕。`EBPF_MAX_INSTS` 没被检查。推断（未实跑大程序）。
12. **`JA_IMM`（gotol）切块时 `blockBegin[i + curr.imm + 1] = true` 无越界检查**（compiler.cpp:322-325），恶意 imm → 堆越界写。推断。
13. **lddw src_reg ≥ 7 静默不写目标寄存器**（compiler.cpp:1093-1338 的 if 链无 else）；ubpf 适配器同情形报错。确认（读码）。
14. **`CALL|0x8`（0x8d，CALLX）被当成按 imm 调 helper**（compiler.cpp:1367-1369），而内核 callx 用寄存器；runtime 的信号准入明确拒绝 0x8d（bpftime_prog.cpp:312-313）。确认（读码）。
15. **局部调用目标不参与切块**（compiler.cpp:311-331）：只有在前一条是跳转/exit 时才成为块首；否则 `loadCallDstBlock` 报 "jump to an illegal position"（推断：clang 产物一般满足）。
16. **helper 在编译之后才登记 attach 私有 helper**（bpf_attach_ctx.cpp:304 先 load，:310-312 后注册），依赖惰性 JIT"碰巧"二次编译成功（现有指南 5.5 已讲）。
17. **`ebpf-vm.h:64` 注释说 `vm_name` 用 "" 表默认**，实际空串抛异常（bpftime_vm_compat.hpp:231-234）；`ebpf-vm.h:151` 说 `ebpf_exec` 用解释器，llvm 实为惰性 JIT。确认。
18. **`bpftime_prog` 第二个构造函数会改全局 runtime config**（bpftime_prog.cpp:140 `bpftime_set_runtime_config(std::move(config))`）——构造一个程序对象产生全局副作用；`is_cuda()` 两个分支设置的 lddw 回调完全相同（:154-162，日志说"不设"）。确认。
19. **`bpftime_prog::load_aot_object` 不置 `program_loaded`**（bpftime_prog.cpp:342-354），之后 `prepare_for_signal_execution` 返回 -EINVAL（且 AOT 本就被拒）。`errmsg` 每次 strdup 不释放（泄漏）。`ufunc_ctx` 成员未用未初始化。确认。
20. **`bpftime_tail_call` 固定拷贝 ctx 前 64 字节**（bpf_helper.cpp:613-619, 634-639）：ctx 小于 64 字节时越界读（单测 test_user_to_user_tailcall.cpp:63-64 传的就是 4 字节 `int ctx`）；被调程序看到的是副本，修改不回传；`mem_len` 恒为 64。另：内核 prog 的新 fd 若与某个 bpftime 假 fd 数值相同，会被 `bpftime_is_prog_fd` 误判为用户态程序（推断）。嵌套 tail call 里内层 `bpftime_prog_exec` 结束时调用 `bpftime_protect_enable()`，外层程序仍在运行（MPK 开启时可能出问题，推断）。
21. **`bpf_perf_event_output` 忽略 `flags` 与 `ctx`**，总写当前 CPU 那格；每次 3 个亲和性系统调用（:499,502,549）。确认。
22. **ringbuf helper 失败返回 `(uint64_t)-1` 而非 `-errno`**；`bpf_ringbuf_discard` 的 WARN 文案写成 "submit"（bpf_helper.cpp:480-481）。确认。
23. **`bpf_get_current_comm` 返回可执行文件 basename，不是线程 comm**；`strncpy` 名字长于 size 时不补 NUL；失败返回 1（内核为负 errno）（bpf_helper.cpp:276-297）。确认。
24. **`bpf_get_prandom_u32` 用 `rand()`**：未播种、只有 31 位（RAND_MAX）（bpf_helper.cpp:179-182）。确认。
25. **helper 45 注册名拼成 "bpf_probe_str"**（bpf_helper.cpp:1309）；名字只对 ubpf 的 "bpf_tail_call" 判断有意义，故无功能影响。确认。
26. **`bpftime_trace_printk`**：`va_start(args, fmt_str)` 用的是局部变量（警告被关），GCC 实际以最后一个具名参数 `fmt_size` 为界，于是格式参数取 r3–r5；忽略 `fmt_size`；输出到 stdout；恒返回 0（内核返回写入字节数）（bpf_helper.cpp:78-90；cli/main.cpp:202-214 复制了一份）。在 Apple arm64（变参走栈）上会读错（推断）。
27. **`bpftime_xdp_adjust_head` 搬移逻辑可疑**（bpf_helper.cpp:730-746）：头部空间不足时 `memmove(buffer_start + (buffer_start - data), xdp->data, len)`，只有原 headroom 为 0 时位置才对；`data_end` 未随之后移；未检查 `data_end + d ≤ buffer_end`。内核此时直接返回 -EINVAL。推断（未构造用例）。
28. **死代码**：`bpftime_ktime_get_coarse_ns`（:184-194，实际注册的是 `bpf_ktime_get_coarse_ns`）、`bpftime_xdp_load_bytes`（:760-770 未注册）、`vm/CMakeLists.txt:54-59` 的 `test_sources`、`vm/llvm-jit/test/CMakeLists.txt:16` 起的 foreach、`runtime/test/` 整个目录。确认。
29. **ufunc 问题**（ufunc.cpp）：只向原生函数传 5 个参数却允许 `num_args` 到 6（:96-108）；DOUBLE 参数按位放进整型寄存器传递、返回值只取整型返回寄存器，原生函数若真声明 `double` 形参/返回会拿错（x86_64/aarch64 浮点走 XMM/V 寄存器，推断）；INT8/16/32 不做截断/符号扩展；`bpftime_ufunc_register_ufunc` 不查 id 上界、不增加 `ufunc_func_cnt`，因此按名查找（1001）找不到这样注册的函数；`resolve_from_info` 容量检查差一（最多 MAX-1 项）；`is_attached` 为真时静默返回 0。确认（读码）+ 推断（ABI 部分）。
30. **vm/example/main.c**：`res = ebpf_exec(vm, &m, sizeof(m), &res)` 用返回码覆盖结果；程序调用 helper 3 但未注册，llvm 下编译必失败；失败时打印的 `errmsg` 是 load 阶段的（推断，未运行）。
31. **llvmbpf cli 的 CMake 把 libbpf 克隆进源码树** `vm/llvm-jit/cli/libbpf_project`（cli/CMakeLists.txt:18-31），本树里该空目录确实存在。cli README 描述的 argparse 界面与手写解析不符。
32. **`register_vm_factory` 的 `constructor(0)` 优先级**（compat_llvm.cpp:82）在 GCC 保留区间 0–100 内（推断：编译告警）；ubpf 用默认优先级。两者都依赖目标文件不被静态链接器丢弃（agent 直接塞 OBJECT 文件、syscall-server/cli 用 `--whole-archive`）。
33. **ubpf 只有 63 个 helper 槽**：每注册一次占一个，默认三组共 31+2+6=39，再加 attach 私有 helper，接近上限；超出后 `ubpf_register` 失败被忽略（推断）。

## 8. 对照现有指南

对照文件：`/home/y/src/bpftime-wt-reading-guide/05-vm-llvm-jit.md`（全文）、`06-maps-helpers.md`（6.3、6.5、6.6、6.7 与深入参考的 helper 部分）。

### 错误 / 需要更正
- 05「细节与坑·bpf-to-bpf」把"每帧只挪 64 字节"标为"疑似 bug，未构造用例验证" → 已实跑确认栈帧重叠（§7-3），可改为确认。
- 05 §5.5 "为什么多传参数是安全的"举 `bpftime_trace_printk` 为"在两种架构上都能正常工作" → 工作原因是 `va_start` 的未定义用法恰好让变参从第 3 个整型寄存器开始；Apple arm64 变参走栈会坏（推断）。Q5 答案对此已有保留，正文可同步。
- 06 §6.6 "`bpf_probe_read_str` … 失败时会把整个 `dst` 清零，语义和内核一致" → 只有**读**失败才清零（bpf_helper.cpp:396-399）；写 dst 失败（:410-411）直接返回 -EFAULT 不清零；未开 `ENABLE_PROBE_READ_CHECK` 时无任何容错。小修。
- 06 §6.6 "编译开关默认打开" 正确，但应补一句：本仓库的 build-jit 构建（CMakeCache `ENABLE_PROBE_READ_CHECK:BOOL=OFF`）是关的——读者在该构建上做实验会看到裸 memcpy 行为。
- 05 深入参考行号核对：抽查 `compat_llvm.cpp:82-86`、`ebpf-vm.cpp:36-40`、`compat_ubpf.cpp:217/33-36`、`compiler_utils.cpp:125-134/285-293/312-371`、`bpftime_prog.cpp:128-129`、`bpf_helper.cpp:555-631` 均正确。

### 遗漏（学习者应该知道）
- **运行时除零让宿主 SIGFPE**（§7-1）——这是 JIT 层最严重的语义缺陷，指南完全没提。
- **64 位 sdiv/smod 截断除数**（§7-2）。
- **ubpf 适配器破坏局部调用**（§7-5），以及 tail call 失败语义两后端不一致（§7-6）。
- **完整的指令类别 → IR 对照表**（§3.3），包括 MOVSX/LDXS/BSWAP/gotol/原子/CMPXCHG、CALLX 当 helper、哪些 opcode 不支持。
- **lddw src 0–6 全表**与 src≥7 静默（§3.4）；05 只讲了 src 1/2。
- **AOT 产物格式**（ET_REL、`bpf_main` + 未定义 `_bpf_helper_ext_*`/`__lddw_helper_map_val`）、加载时机要求、map fd 在 AOT 编译期被烧死、`get_compiled_code` 会再编一次（§3.8）。指南只在 README 级别提到 AOT。
- **`generate_ptx` 目标 CPU 参数被吞**（§7-8）、PTX/SPIR-V/LLJIT 初始化失败会 `exit()` 进程（§6）。
- **CMPXCHG 非原子读旧值、原子序 Monotonic**（§7-9）。
- **两份 `ebpf_inst`（字段名不同、同 include guard）**（§2.7）——读者在 runtime 看到 `inst.code/src_reg`、在 llvmbpf 看到 `inst.opcode/src` 会困惑。
- **ufunc 是什么、怎么用**（§3.11）及其 ABI 问题（§7-29）：指南只有一行"扩展机制"。
- **helper 总表**（见下方 §8 附表）——指南只讲了 25、112 和 tail call。
- `bpf_perf_event_output` 忽略 flags/ctx；ringbuf submit 靠 `ptr[-1]` 取 fd（与 ringbuf 记录头布局耦合）。
- `bpftime_tail_call` 固定拷 64 字节 ctx 的越界读与语义差异（06 深入参考提到"拷 64 字节"，但没说越界/不回传）。
- `vm/README.md` 大面积失真（不存在的 `optimizer.cpp/native_code_gen.cpp/ptx_code_gen.cpp/llvm_bpf_jit.hpp`、`ebpf_unregister`、`ebpf_aot_compile`、`set_inline_map`、`do_get_ptx`、`LLVM_JIT_DUMP_IR`，错误的 `ebpf_exec` 签名与 lddw 回调返回类型，`example/main.cpp` 实为 `main.c`），应提醒读者别信。
- `unload_code` 已实跑确认不清缓存（05 只说"实现并没有动"）。

### 讲得好、可保留
- 05 §5.1 三层图、§5.3 编译时序图、§5.5 helper 绑定"绑定端/引用端"讲法与代码一致，可保留。
- 05 §5.7 惰性 JIT 三结论（关 JIT 只是推迟；编译失败每事件重试；trap 例外）准确。
- 05 "细节与坑"里的工厂表 union+CAS 解释、`!err` 写反、`i > 1`、array map 内联未接入、tail call 失败直接退出——均与代码一致（后两者我已实跑确认）。
- 06 §6.3 三组表、§6.5 perf 输出路径与代码节选、§6.6 process_vm_readv 容错与 pid/fork 缓存讲解准确。

### §8 附表：helper 实现总表

"真 syscall"指每次调用是否必然进内核（vDSO 不算）。行号均为 `runtime/src/bpf_helper.cpp`（另注者除外）。组：K=kernel utils（:1189-1370）、S=shm maps（:1371-1413）、U=ufunc/extension（extension_helper.cpp:133-196）。

| 编号 | 注册名 | C 函数（实现行） | 组 | 真 syscall | 注意事项 |
|---|---|---|---|---|---|
| 1 | bpf_map_lookup_elem | bpftime_map_lookup_elem_helper (299-304) | S | 否（shm）；内核共享 map 类型会走 bpf()（推断） | map 参数是 fd（lddw src=1 烧入的 `map_ptr_by_fd` 结果），`(int)map` |
| 2 | bpf_map_update_elem | bpftime_map_update_elem_helper (306-313) | S | 同上 | from_syscall=false |
| 3 | bpf_map_delete_elem | bpftime_map_delete_elem_helper (315-320) | S | 同上 | |
| 4 | bpf_probe_read | bpftime_probe_read (121-136) | K | 是：`process_vm_readv`（开 CHECK 且 size>0） | size<0 → -EFAULT；失败**不清零** dst；关 CHECK 为 memcpy |
| 5 | bpf_ktime_get_ns | bpftime_ktime_get_ns (269-274) | K | 否（steady_clock） | CLOCK_MONOTONIC |
| 6 | bpf_trace_printk | bpftime_trace_printk (78-90) | K | write（stdout 缓冲） | 忽略 fmt_size；va_start 未定义用法；恒返回 0 |
| 7 | bpf_get_prandom_u32 | bpftime_get_prandom_u32 (179-182) | K | 否 | `rand()`，31 位、未播种 |
| 8 | bpf_get_smp_processor_id | bpftime_get_smp_processor_id (684-692) | K | 视 glibc（`sched_getcpu`） | 出错返回 0 |
| 12 | bpf_tail_call | bpftime_tail_call (555-660) | K | 用户态目标否；内核目标是（bpf PROG_GET_FD_BY_ID + PROG_TEST_RUN + close） | 同步执行目标；拷 64 字节 ctx；深度 32；llvm 下调用后无条件退出 |
| 14 | bpf_get_current_pid_tgid | bpftime_get_current_pid_tgid (233-259) | K | 每线程首次 gettid | 进程/线程缓存 + atfork 刷新 |
| 15 | bpf_get_current_uid_gid | bpf_get_current_uid_gid (261-267) | K | 是（getgid+getuid 每次） | |
| 16 | bpf_get_current_comm | bpftime_get_current_comm (276-297) | K | 首次 readlink | 返回 exe basename；可能不补 NUL；失败返回 1 |
| 25 | bpf_perf_event_output | bpf_perf_event_output (487-553) | K | 是（sched_getaffinity + sched_setaffinity×2） | 忽略 flags/ctx；PERF_EVENT_ARRAY / KERNEL_USER 两条路 |
| 27 | bpf_get_stackid | bpftime_get_stackid (773-826) | K | 视 Frida 栈回溯 | 只支持 BPF_F_USER_STACK；依赖 attach type 6 的 "generate_stack"；持 map 锁 |
| 28 | bpf_csum_diff | bpftime_csum_diff (695-726) | K | 否 | 移植自 ebpf-for-windows；int 形参 |
| 36 | bpf_probe_write_user | bpftime_probe_write_user (162-177) | K | 是：`process_vm_writev`（开 WRITE CHECK） | len<0 → -EFAULT |
| 44 | bpf_xdp_adjust_head | bpftime_xdp_adjust_head (730-746) | K | 否 | 搬移逻辑可疑（§7-27） |
| 45 | bpf_probe_str（应为 bpf_probe_read_str） | bpf_probe_read_str (367-418) | K | 是（每块一次 readv + 一次 writev） | 与 114 共用实现 |
| 58 | bpf_override_return | bpftime_override_return (attach/base_attach_impl.hpp:118) | K | 否 | 无回调时抛异常；信号路径安全 |
| 65 | bpf_xdp_adjust_tail | bpftime_xdp_adjust_tail (748-758) | K | 否 | |
| 67 | bpf_get_stack | bpftime_get_stack (828-888) | K | 视 Frida | 字节语义、skip、拒 build-id（单测 test_get_stack.cpp） |
| 87 | bpf_map_push_elem | bpftime_map_push_elem_helper (322-327) | S | 否 | |
| 88 | bpf_map_pop_elem | bpftime_map_pop_elem_helper (329-334) | S | 否 | |
| 89 | bpf_map_peek_elem | bpftime_map_peek_elem_helper (336-341) | S | 否 | |
| 112 | bpf_probe_read_user | bpftime_probe_read (121-136) | K | 同 4 | |
| 113 | bpf_probe_read_kernel | bpftime_probe_read (121-136) | K | 同 4 | 用户态没有"内核内存"，按用户态读 |
| 114 | bpf_probe_read_str | bpf_probe_read_str (367-418) | K | 同 45 | 返回含 NUL 长度；按页切块 |
| 125 | bpf_ktime_get_boot_ns | bpftime_ktime_get_boot_ns (196-206) | K | 否 | CLOCK_BOOTTIME，失败退 MONOTONIC |
| 130 | bpf_ringbuf_output | bpf_ringbuf_output (432-448) | K | 否（shm） | 失败返回 -1；flags 只 WARN |
| 131 | bpf_ringbuf_reserve | bpf_ringbuf_reserve (450-459) | K | 否 | 记录头存 fd |
| 132 | bpf_ringbuf_submit | bpf_ringbuf_submit (461-472) | K | 否（唤醒消费者另见 epoll 模块，未读） | 从 `ptr[-1]` 取 fd |
| 133 | bpf_ringbuf_discard | bpf_ringbuf_discard (474-485) | K | 否 | WARN 文案错写 submit |
| 160 | bpf_ktime_get_coarse_ns | bpf_ktime_get_coarse_ns (420-430) | K | 否 | 另有未注册的 `bpftime_ktime_get_coarse_ns` |
| 173 | bpf_get_func_ip | bpftime_get_func_ip (677-682) | K | 否 | 信号 ctx 优先，否则 TLS；async-signal-safe |
| 174 | bpf_get_attach_cookie | bpftime_get_attach_cookie (662-675) | K | 否 | 信号 ctx 优先，否则 TLS；无则 0 |
| 182 | bpf_strncmp | bpftime_strncmp (92-95) | K | 否 | `strncmp(s1, s2, s1_sz)` |
| 187 | bpf_set_retval | bpftime_set_retval (attach/base_attach_impl.hpp:97) | K | 否 | 无回调抛 `invalid_argument` |
| 1000 | __ebpf_call_ufunc_dispatcher | ufunc.cpp:74-113 | U | 否 | 见 §3.11/§7-29 |
| 1001 | __ebpf_call_find_ufunc_id | ufunc.cpp:30-45 | U | 否 | 只搜 `[0,cnt)` |
| 1003/1004 | bpftime_get_abs_path / bpftime_path_join | extension（宏 FS_HELPER） | U | 未读 | |
| 1006–1010 | io_uring_* | extension（宏 IOURING_EXT） | U | 是（io_uring） | 未读 |

未注册但定义：`bpftime_xdp_load_bytes`(760-770)、`bpftime_ktime_get_coarse_ns`(184-194)。attach 层私有 helper（如 Frida 的 get_func_arg 等）由 attach 模块经 `register_custom_helpers` 注入，不在本表（R? attach 读者确认）。

## 9. 学习建议

### 推荐阅读顺序
1. `vm/vm-core/include/ebpf-vm.h`（只看函数声明）→ `vm/vm-core/src/ebpf-vm.cpp`（全是一行转发）→ `vm/compat/include/bpftime_vm_compat.hpp:27-263` → `vm/compat/llvm-vm/compat_llvm.{hpp,cpp}`。建立三层图。
2. `vm/llvm-jit/include/llvmbpf.hpp:55-166`（字段即状态）→ `vm/llvm-jit/src/vm.cpp`（`register_external_function`、`load_code`、`exec`、`compile`）。
3. `llvm_jit_context.cpp`：`do_jit_compile`(515) → `create_and_initialize_lljit_instance`(686) → `get_entry_address`(862)；然后 AOT：`do_aot_compile`(555/623) 与 `load_aot_object`(657)。
4. `compiler.cpp:208-631`（准备与切块、setupBlock）→ 主 switch 按 §3.3 表逐类看 → `compiler_utils.cpp` 对应 emit 函数。
5. runtime：`bpftime_prog.cpp`（构造、load、exec）→ `bpf_attach_ctx.cpp:43-60` → `bpf_helper.cpp:1129-1413`（组）→ 挑 helper 读：probe_read(97-177, 343-418)、perf(487-553)、ringbuf(432-485)、tail_call(555-660)。
6. 选读：ufunc（`ufunc.cpp` + `runtime/unit-test/assets/ufunc.bpf.h`）、ubpf 适配器、llvmbpf cli。

### 动手实验
所有实验程序都链接 `build-jit` 里已有的静态库，编译命令（在 scratchpad/R4 下，用 bash 以便变量分词）：
`g++-13 -std=c++20 -O1 -flto=auto -I$R/vm/llvm-jit/include -I$R/vm/llvm-jit/src exp.cpp -o exp <build-jit 中 bpftime-vm-cli 的 LINK_LIBRARIES 去掉 libbpf.a> -lncurses -lpthread`（链接库清单从 `build-jit/build.ninja:3085` 抠出，存于 scratchpad/R4/libs.txt）。

1. **llvmbpf 语义差异合集（已实跑）** `SPDLOG_LEVEL=off ./exp`：
   ```
   [bpf2bpf-stack] exec=0 ret=0x22 expect(kernel semantics)=0x11 MISMATCH
   [sdiv64-reg] exec=0 ret=0xaaaaaaaa expect(kernel semantics)=0x1 MISMATCH
   [smod64-reg] exec=0 ret=0x2 expect(kernel semantics)=0xfffffffd MISMATCH
   [udiv64-by-zero] exec=0 ret=0x0 ... OK          ← 除数是常量，被折叠
   [udiv64-by-zero-from-mem] child killed by signal 8
   [umod64-by-zero-from-mem] child killed by signal 8
   [sdiv64-by-zero-from-mem] child killed by signal 8
   [udiv64-by-zero-from-mem-O0] child killed by signal 8
   [tailcall-fail-fallthrough] exec=0 ret=0xffffffffffffffff expect=0x7 MISMATCH
   [reload] after unload+load(p2) ret=1 (expect 2)
   [ptx] requested sm_80, got: .target sm_60
   ```
2. **看 IR 与 AOT 产物（已实跑）** `./ir` 打印优化前 IR（见 §3.3 片段），并写出 `aot.o`；`readelf -h aot.o` → `Type: REL`，`nm aot.o` → `T bpf_main / U _bpf_helper_ext_0001 / U __lddw_helper_map_val`。
3. **两后端对局部调用（已实跑）** `./ubpf_local`：
   ```
   [ubpf/call-at-pc0] load=0 ret=0x1 (kernel semantics: 0x11)
   [ubpf/call-at-pc1] load=0 ret=0x0 (kernel semantics: 0x11)
   [llvm/call-at-pc0] killed by signal 11
   [llvm/call-at-pc1] load=0 ret=0x11
   ```
   需要 `-Wl,--whole-archive` 链入 `libbpftime_ubpf_vm.a libbpftime_llvm_vm.a`，否则工厂构造函数被丢弃（顺带演示 §7-32）。链接时可见 `-Wodr` 告警（§2.7）。
4. **helper 单测（已实跑）**：
   `cd build-jit/runtime/unit-test && BPFTIME_GLOBAL_SHM_NAME=notes_R4_xxx SPDLOG_LEVEL=off ./bpftime_runtime_tests "Test bpftime_probe_read,Test repeated valid probe memory access,Test bpf_probe_read_str,Test probe access preserves application SIGSEGV handlers,Test bpftime_probe_write_user,bpf_get_current_pid_tgid reports the calling process after fork,[uid_gid],get_stack*,Test helpers,Test ufunc register,Test tail calling from userspace to userspace"`
   → `All tests passed (93 assertions in 16 test cases)`（注意 Catch2 多个过滤用逗号分隔；本构建 PROBE CHECK 关闭，`#ifdef ENABLE_PROBE_READ_CHECK` 段未参与）。跑完删除 `/dev/shm/notes_R4_xxx`。
5. **（未实跑）** 用 `bpftime-vm build x.bpf.o -emit-llvm` 看真实 clang 程序的 IR（需要构建 `vm/compat/llvm-vm/llvm-jit/cli/bpftime-vm`，本树未构建）。

### 学习者最容易误解的点
1. "关掉 JIT = 解释执行"：llvm 后端没有解释器，只是推迟编译；只有 ubpf 是解释器。
2. "map 指针"：lddw src=1 烧进代码的是 **fd 本身**，helper 再按 fd 找 shm 里的 map；src=2 才是真实地址。AOT 时这些 fd 在编译期被固定。
3. "helper 运行时查表"：没有表，链接期就绑成了绝对地址；也因此编译后再注册无效。
4. "eBPF 语义 = 内核语义"：除零、64 位有符号除法、bpf-to-bpf 栈帧、tail call 失败、原子序在 llvmbpf 里都与内核不同（§7）。
5. "ufunc 就是另一种 helper"：它是经由 helper 1000/1001 的**二级分发**，参数打包在 BPF 栈上的 `arg_list` 里，函数表在进程全局、按名或按槽注册。

## 10. 未读完/未确定的地方

- `third_party/ubpf` 本体未读（按任务要求），ubpf 对 `src_reg=1` 局部调用的具体执行路径只通过实验观察（ret=0x1/0x0），未逐行确认原因。
- `runtime/extension/extension_helper.cpp` 里 FS / io_uring 扩展 helper 的实现未读；attach 层私有 helper（Frida/trap 的 get_func_arg 等）未读。
- CUDA 路径（`compile_ptx_to_elf`、`test_cuda_compile.cpp`）、`generate_spirv`、`kernel_compatible_mode` 全路径只读了代码，未运行。
- §7-9（CMPXCHG/原子序）、§7-11（>65535 指令）、§7-12（gotol 越界）、§7-20 的 fd 冲突与 MPK 嵌套、§7-27（xdp_adjust_head）、§7-29 的浮点 ABI、§6 的并发首次编译竞态：均为读码推断，未构造用例。
- `perf_event_output` 的 KERNEL_USER 分支是否每次有 syscall、map helper 对内核共享 map 是否走 bpf()：未验证（属 maps 读者范围）。
- `vm/llvm-jit/test/test_framework/*.py`、`test-cases/*.data`、example 的 ptx/spirv/inline/standalone 只列出未细读；`llvm_jit_tests` 与 conformance runner 在本树未构建，没跑。
- `runtime/unit-test/attach_with_ebpf/test_attach_uprobe_with_ebpf_trap.cpp` 只读了 140-252 行；本构建未启用 trap，未运行。
- `test_attach_filter_with_ebpf.cpp`、`test_attach_uprobe_with_ebpf.cpp`、`test_shm_progs_attach.cpp` 只 grep 了与 helper/ufunc 相关的行（属 attach 读者范围）。
