# R6 GPU（CUDA attach / GPU map / PTX 生成）源码笔记

> 基线：`/home/y/src/bpftime-wt-official-177a1dc`（177a1dc465ff）。本机无 NVIDIA GPU，所有 GPU 行为均为读代码所得，
> 标「未验证」者未在 GPU 上跑过。`build-jit/` 是 **CUDA 关闭** 的构建（`CMakeCache.txt`: `BPFTIME_ENABLE_CUDA_ATTACH:BOOL=OFF`），
> 所以 nv_attach 相关二进制/单测都没有构建产物；只做了两个不需要 GPU 的 llvmbpf→PTX 实验（见 §9）。

## 0. 一句话职责 + 范围

**一句话**：bpftime 的 GPU 支持 = 在被追踪的 CUDA 进程里（agent 侧）用 Frida 拦截 CUDA 运行时/驱动 API，
把 fatbin 里的 PTX 抠出来，用 llvmbpf 把 eBPF 程序编成 PTX 函数并"缝"进目标 kernel 的入口/出口（或 ld/st 之后），
再拼上一段预编译好的"trampoline PTX"（GPU 侧 helper 实现），用 nvPTXCompiler 重新编成 cubin 并 `cuModuleLoadData`
成一个**新模块**，之后把应用的 kernel launch 重定向到新模块里的同名 kernel；GPU 上的 eBPF 通过三种通道与 CPU 交换数据：
(1) GPU map（显存 + CUDA IPC，或 host 共享内存 + `cudaHostRegister` 映射）直接读写；(2) 整个 bpftime shm 段被
`cudaHostRegister(Mapped)`，GPU 可以直接访问 shm 里的普通 map 值/ringbuf；(3) 一块放在 shm 里的 `CommSharedMem`
做"GPU→CPU 同步 RPC"（自旋锁 + flag 握手，CPU 侧由 watcher 线程轮询处理 map 操作、printk 等）。

**读了哪些目录/文件**

| 组 | 范围 | 行数 |
|---|---|---|
| A | `attach/nv_attach_impl/` 全部 41 个文件（含 `pass/`、`ptx_compiler/`、`test/`、`trampoline/`、`configs/`、README） | 11130（其中生成文件 `trampoline_ptx.h` 1879、死测试 `test_kernel_finder.cpp` 1291） |
| B | `runtime/src/bpf_map/gpu/`（14 文件）+ `runtime/src/bpf_map/gpu_kernel_shared/`（2 文件） | 2448 |
| C | runtime/vm/tools/cmake 里 grep（cuda/CUDA/nv_attach/gpu/ptx/NVPTX/spirv）命中的文件中与 GPU 相关的部分（清单见 §1-C） | 相关段落约 1500 行；所在文件合计 >12000 行 |
| D | `example/gpu/`（20 个例子目录）、`benchmark/gpu/`（略读） | — |

---

## 0.5 CUDA/PTX 背景知识（写作者需要知道的最少概念）

写"概览"章需要的最小词汇表，按"从源码到 GPU 执行"的顺序：

1. **host / device**：CPU 侧叫 host，GPU 侧叫 device。GPU 有自己的显存（device memory），CPU 不能直接解引用显存指针，
   反之 GPU 一般也不能直接访问普通 CPU 内存——除非用下面第 9 条的"映射/锁页内存"。
2. **kernel**：在 GPU 上并行执行的函数（CUDA C 里写 `__global__ void f(...)`）。一次 launch 启动成千上万个**线程**。
   线程按层次组织：**grid**（一次 launch 的全部线程）→ **block**（线程块，`blockIdx`/`blockDim`）→ **thread**（`threadIdx`）。
   硬件上 32 个线程组成一个 **warp**，同一 warp 锁步执行（SIMT）；block 被调度到某个 **SM**（流式多处理器）上。
   bpftime 的 GPU helper 503/504/505/508/509/510/511 就是读这些坐标（`default_trampoline.cu:437-515`）。
3. **`__device__` 函数 / `__constant__` / `__device__` 全局变量**：kernel 能调用的普通 GPU 函数；常量内存/全局变量在每个
   "模块"里各有一份。bpftime 的 trampoline 里 `constData`、`map_info` 就是 `__constant__` 变量（`default_trampoline.cu:114-115`）。
4. **PTX**：NVIDIA 的**虚拟汇编/中间表示**（文本），与具体 GPU 型号无关（有 `.version` ISA 版本和 `.target sm_XX`）。
   PTX 用无限的虚拟寄存器（`%r<N>`、`%rd<N>`），函数分 `.entry`（kernel 入口，可被 launch）与 `.func`（设备函数，只能被调用）。
   bpftime 的全部插桩都在 **PTX 文本层面用字符串/正则改写** 完成。
5. **SASS / cubin**：真正跑在某代 GPU 上的机器码（sm_80、sm_90…），装在 ELF 格式的 cubin 里。PTX → SASS 由 `ptxas`
   或其库形态 **nvPTXCompiler**（`nvPTXCompilerCompile`）完成；bpftime 用的是后者（库，在进程内编译），**不调用 nvcc**。
6. **fatbin**：nvcc 编出来的可执行文件里嵌着一个"胖二进制"，里面可同时有多份 cubin（不同 sm）和 PTX。fatbin 放在 ELF 的
   `.nv_fatbin` 段，外面包一层 `__fatBinC_Wrapper_t`（magic `0x466243b1`，放在 `.nvFatBinSegment`；
   `nv_attach_utils.hpp:12-18`、`nv_elf_introspect.cpp:252`）；内部每段以 magic `0xBA55ED50` 开头（`nv_attach_impl_frida_setup.cpp:264`）。
   **前提**：目标程序的 fatbin 里必须带 PTX（只有 SASS 时 `cuobjdump --extract-ptx` 抽不出东西，bpftime 无从插桩）。
   `cuobjdump` 是 CUDA toolkit 里拆 fatbin 的命令行工具，bpftime 运行时用它抽 PTX（`nv_attach_impl.cpp:770-844`）。
7. **CUDA runtime API vs driver API**：`cudaXxx`（libcudart，高层）与 `cuXxx`（libcuda，底层）。
   - nvcc 生成的程序在**静态构造期**自动调用 `__cudaRegisterFatBinary` / `__cudaRegisterFunction`（把 host 侧"桩函数地址"
     与 device kernel 名字关联）/ `__cudaRegisterVar` / `__cudaRegisterFatBinaryEnd`。bpftime 正是 hook 这组函数来"发现 kernel"。
   - `kernel<<<grid, block>>>(args)` 被编译成对 `cudaLaunchKernel(host_stub_ptr, grid, block, args, smem, stream)` 的调用；
     第一个参数是 **host 侧桩函数地址**，不是 GPU 地址。bpftime 用这个地址查表找到"打过补丁的 kernel"。
   - driver API 里：`CUmodule`（一个已加载的 cubin，`cuModuleLoadData`）、`CUfunction`（模块里的一个 kernel，`cuModuleGetFunction`）、
     `cuModuleGetGlobal`（取模块内全局变量的设备地址）、`cuLaunchKernel`（直接按 CUfunction 启动）。
8. **context / stream / event**：context 相当于 GPU 上的"进程地址空间"（每设备有一个 primary context）；stream 是有序的命令队列，
   kernel launch 是**异步**的（入队即返回）；event 用于标记/等待 stream 上某点完成。bpftime 在 detach 时用 event 等待
   已入队的插桩 kernel 跑完（`nv_attach_impl.cpp:1419-1529`）。
9. **锁页/映射内存与 UVA**：`cudaHostRegister(ptr, size, cudaHostRegisterMapped)` 把一段普通 CPU 内存锁页并映射到 GPU 地址空间，
   `cudaHostGetDevicePointer`/`cuMemHostGetDevicePointer` 拿到 GPU 可用的地址。GPU 对它的读写经 PCIe 直达 CPU 内存（慢但零拷贝）。
   bpftime 把**整个 shm 段**注册为映射内存（`bpftime_shm_internal.cpp:1243-1244`）。
10. **CUDA IPC**：`cuIpcGetMemHandle` 把本进程的显存分配导出成句柄，另一进程 `cuIpcOpenMemHandle` 打开得到自己的设备指针，
    双方访问同一块物理显存。bpftime 的 GPU array map：syscall-server（loader 进程）`cuMemAlloc` + 导出，agent（被追踪进程）打开。
    Tegra/Jetson 等不支持 IPC 的平台改用"HOST map"（数据放 shm、靠第 9 条映射）。
11. **内存栅栏**：GPU 的 `membar.sys` / `__threadfence_system()` 保证写对 CPU 可见；bpftime 的 GPU↔CPU 握手和 HOST map 都依赖它。
12. **CUDA Graph**：先把一串 kernel"录制/构建"成图再整体 launch（`cuGraphAddKernelNode` 等），绕过 `cudaLaunchKernel`；
    bpftime 单独 hook 了 graph 节点相关 API（`nv_attach_impl_frida_setup.cpp:625-775`）。
13. **GDRCopy**：NVIDIA 的开源库，把显存经 PCIe BAR 映射到 CPU 地址空间，CPU 读小块显存比 `cuMemcpyDtoH` 快。bpftime 可选用于
    host 侧读取 GPU map（`nv_gpu_gdrcopy.cpp`）。
14. **SPIR-V**：Khronos 的跨厂商 GPU 中间表示（OpenCL/Vulkan）。llvmbpf 能生成，但 **bpftime 主线没有使用**，只有子模块里的示例。

---

## 1. 文件清单

### 1-A `attach/nv_attach_impl/`

| 文件 | 行数 | 做什么（一句话） | 必读? |
|---|---|---|---|
| `CMakeLists.txt` | 83 | 建 3 个 pass .so、ptx_compiler .so、静态库 `bpftime_nv_attach_impl`；生成 `ptx_pass_config.h` 写入默认 pass/compiler 路径；链 Frida/OpenSSL/CUDA | 必读 |
| `nv_attach_impl.hpp` | 286 | 核心类 `nv_attach_impl`、`MapBasicInfo`、`nv_attach_entry`、`pass_cfg_with_exec_path`、全局 hook 状态 `nv_attach_hook_state`、`ATTACH_CUDA_PROBE/RETPROBE=8/9` | 必读 |
| `nv_attach_impl.cpp` | 1983 | 构造（装 Frida 钩子、dlmopen pass、加载 PTX 编译器）、`create_attach_with_ebpf_callback`、`extract_ptxs`（cuobjdump）、`hack_fatbin`（跑 pass）、`run_attach_entry_on_gpu`、memcpyToSymbol 镜像、late-attach bootstrap、detach 等待 | 必读 |
| `nv_attach_impl_frida_setup.cpp` | 1006 | Frida listener（`__cudaRegister*`、cudaMemcpyToSymbol）与 replace 钩子（cudaLaunchKernel/_ptsz、cuLaunchKernel、cuGraph*、cudaMemcpyFromSymbol*）的实现 | 必读 |
| `nv_attach_fatbin_record.hpp` | 55 | `fatbin_record`/`ptx_in_module`/`kernel_info`/`variable_info` | 必读 |
| `nv_attach_fatbin_record.cpp` | 282 | `try_loading_ptxs`：hack_fatbin→并行 nvPTXCompiler 编译→`cuModuleLoadDataEx`→写 `constData`/`map_info`；按名字找 CUfunction / 全局变量 | 必读 |
| `nv_attach_impl_patcher.cpp` | 102 | `filter_compiled_ptx_for_ebpf_program(input, new_name)`（directly-run 路径用）、`add_semicolon_for_variable_lines`（未被调用） | 选读 |
| `nv_attach_private_data.hpp` | 28 | `nv_attach_private_data`：kernel 名、`comm_shared_mem`、`map_basic_info`、eBPF 指令、程序名 | 必读 |
| `nv_attach_private_data.cpp` | 58 | `initialize_from_string`：按逗号拆 kernel 名列表 | 选读 |
| `nv_attach_utils.hpp` | 60 | `__fatBinC_Wrapper_t`、`try_get_original_func`（dlsym RTLD_NEXT）、各工具函数声明 | 选读 |
| `nv_attach_utils.cpp` | 166 | 拼 trampoline、`rewrite_ptx_target`（改 .version/.target）、`sha256`、`get_gpu_sm_arch`（读 env 或探测，并回写 env） | 必读 |
| `nv_elf_introspect.hpp` | 34 | late attach 用的 ELF 自省接口 | 选读 |
| `nv_elf_introspect.cpp` | 293 | `dl_iterate_phdr` 枚举模块、读磁盘 ELF 找 `.nv_fatbin`/`.nvFatBinSegment` 段、扫 wrapper、读符号表 | 选读 |
| `memory_utils.hpp` | 79 | `process_vm_readv`/ptrace 读远端内存模板（中文注释）——**全仓无人引用**，死代码 | 跳过 |
| `ptx_compiler/CMakeLists.txt` | 16 | 把 nvPTXCompiler 包成共享库 `nv_attach_impl_ptx_compiler` | 选读 |
| `ptx_compiler/ptx_compiler.hpp` | 65 | C ABI 声明 + `load_nv_attach_impl_ptx_compiler`（dlopen + dlsym 6 个函数） | 选读 |
| `ptx_compiler/ptx_compiler.cpp` | 140 | 对 nvPTXCompiler 的薄封装（create/compile/get log/get program），`setlocale` 规避崩溃 | 选读 |
| `pass/ptxpass_core/CMakeLists.txt` | 18 | 静态库 ptxpass_core，链 llvmbpf_vm | 跳过 |
| `pass/ptxpass_core/include/ptxpass/core.hpp` | 208 | pass 的 JSON 协议结构（`PassConfig`/`RuntimeRequest`/`RuntimeResponse`/`ExitCode`）、`AttachPointMatcher` 等 | 必读 |
| `pass/ptxpass_core/src/core.cpp` | 324 | **`compile_ebpf_to_ptx_from_words`（eBPF→PTX 的唯一入口）**、头部过滤、`find_kernel_body`、正则匹配 attach point | 必读 |
| `pass/ptxpass_core/src/nv_attach_impl_register_guard_patcher.cpp` | 601 | `add_register_guard_for_ebpf_ptx_func`：给 PTX 函数加寄存器保存/恢复（默认不启用） | 选读 |
| `pass/ptxpass_kprobe_entry/CMakeLists.txt` | 12 | 建 `libptxpass_kprobe_entry.so` | 跳过 |
| `pass/ptxpass_kprobe_entry/main.cpp` | 145 | 入口插桩：替换 stub 调用或在 kernel 第一行插 `call __probe_func__<k>` | 必读 |
| `pass/ptxpass_kretprobe/CMakeLists.txt` | 5 | 建 `libptxpass_kretprobe.so` | 跳过 |
| `pass/ptxpass_kretprobe/main.cpp` | 104 | 出口插桩：每个 `ret;`/`exit;` 前插 `call __retprobe_func__<k>` | 必读 |
| `pass/ptxpass_kprobe_memcapture/CMakeLists.txt` | 5 | 建 `libptxpass_kprobe_memcapture.so` | 跳过 |
| `pass/ptxpass_kprobe_memcapture/main.cpp` | 196 | 每条 ld/st 之后插一个 `__memcapture__N`，把该行 PTX 文本压到 eBPF 栈上 | 选读 |
| `configs/ptxpass/kprobe_entry.json` | 15 | kprobe_entry 的 JSON 配置样例——**运行时不读取** | 跳过 |
| `trampoline/default_trampoline.cu` | 670 | **GPU 侧 helper 的 CUDA 源码**（`_bpf_helper_ext_NNNN`、`make_helper_call` 握手、`CommSharedMem` 布局）+ 一个自测 main | 必读 |
| `trampoline/Makefile` | 26 | `clang++-17 -S --cuda-gpu-arch=sm_61` 生成 .s，截掉 `bpf_main` 之后的部分写成 `../trampoline_ptx.h` | 选读 |
| `trampoline/README.md` | 4 | 说明 trampoline 来源（提的是 sm_60，与 Makefile 的 sm_61 不一致） | 跳过 |
| `trampoline/.gitignore` | 6 | 忽略生成的 .s / test | 跳过 |
| `trampoline_ptx.h` | 1879 | **生成文件**：`static const char TRAMPOLINE_PTX[] = R"(...)"`，即上面 .cu 的 PTX（sm_61、.version 8.1） | 弄清用途即可 |
| `README.md` | 226 | 模块说明 + 配置参考表（部分过时，见 §8） | 选读 |
| `README-passes.md` | 56 | pass 端到端说明（**严重过时**：描述的是 stdin/stdout 可执行文件形态） | 跳过 |
| `test/CMakeLists.txt` | 40 | `bpftime_nv_attach_tests`（3 个源文件，`test_kernel_finder.cpp` 被注释掉） | 选读 |
| `test/nv_attach_impl_tests.cpp` | 85 | 测 `filter_compiled_ptx_for_ebpf_program` 去头/重命名 | 选读 |
| `test/test_register_guard.cpp` | 64 | 调 register guard 并打印，`REQUIRE(true)`（无断言） | 跳过 |
| `test/test_ptxpass.cpp` | 414 | ptxpass_core 的单测：JSON 解析、matcher、find_kernel_body、compile_ebpf_to_ptx_from_words 等 | 选读 |
| `test/test_kernel_finder.cpp` | 1291 | 调用已不存在的 `patch_with_probe_and_retprobe`，末尾 `REQUIRE(false)`；未编译的死测试 | 跳过 |

### 1-B GPU map

| 文件 | 行数 | 做什么 | 必读? |
|---|---|---|---|
| `gpu/nv_gpu_per_thread_array_map.hpp/.cpp` | 59/192 | `BPF_MAP_TYPE_PERGPUTD_ARRAY_MAP`(1502)：显存 `[max_entries][thread_count][value_size]`，CUDA IPC 共享 | 必读 |
| `gpu/nv_gpu_shared_array_map.hpp/.cpp` | 66/374 | `BPF_MAP_TYPE_GPU_ARRAY_MAP`(1503)：显存单份 `[max_entries][value_size]`，IPC；agent 侧自选 device/context、私有 stream | 必读 |
| `gpu/nv_gpu_shared_hash_map.hpp/.cpp` | 98/185 | `BPF_MAP_TYPE_GPU_HASH_MAP`(1501)：key 表在 shm（开放寻址），value 存在内嵌的 shared_array_map | 选读 |
| `gpu/nv_gpu_ringbuf_map.hpp/.cpp` | 79/114 | `BPF_MAP_TYPE_GPU_RINGBUF_MAP`(1527)：每 GPU 线程一个环，数据在 **shm（host 内存）**，GPU 经映射写 | 必读 |
| `gpu/nv_gpu_array_host_map.hpp/.cpp` | 63/150 | `BPF_MAP_TYPE_PERGPUTD_ARRAY_HOST_MAP`(1512)：per-thread，数据在 shm，`cuMemHostGetDevicePointer` | 选读 |
| `gpu/nv_gpu_shared_array_host_map.hpp/.cpp` | 62/154 | `BPF_MAP_TYPE_GPU_ARRAY_HOST_MAP`(1513)：单份，数据在 shm | 选读 |
| `gpu/nv_gpu_gdrcopy.hpp/.cpp` | 31/376 | 可选 GDRCopy 快读路径（dlopen libgdrapi），编译开关 `BPFTIME_ENABLE_GDRCOPY` | 选读 |
| `gpu/nv_gpu_gdrcopy_abi.hpp` | 65 | 无 `gdrapi.h` 时自带最小类型声明 | 跳过 |
| `gpu_kernel_shared/array_map_kernel_gpu.hpp/.cpp` | 62/318 | `BPF_MAP_TYPE_GPU_KERNEL_SHARED_ARRAY_MAP`(1504)：内核 array map mmap 进来再 `cuMemHostRegister`，GPU↔内核 eBPF 共享 | 选读 |

### 1-C 范围外文件里的 CUDA 相关部分（grep 结果，均已读相关段落）

| 文件:行 | 内容 | 必读? |
|---|---|---|
| `cmake/cuda.cmake:4-67` | `find_cuda()`：要求 `BPFTIME_CUDA_ROOT`；按架构选 `targets/<arch>-linux`；探测 CUDA 版本；链 `cuda cudart nvptxcompiler(_static) cupti nvrtc` | 必读 |
| `cmake/StandardSettings.cmake:21-25` | `option(BPFTIME_CUDA_ROOT …)`（注意被声明成 BOOL option）、`option(BPFTIME_ENABLE_CUDA_ATTACH … OFF)` | 选读 |
| `CMakeLists.txt:169-176` | 全局 `add_compile_definitions(BPFTIME_ENABLE_CUDA_ATTACH=1)`；`BPFTIME_ENABLE_GDRCOPY` 只是 `-D` 变量（无 option） | 选读 |
| `attach/CMakeLists.txt:19-21` | 仅 Linux 且开 CUDA 时 `add_subdirectory(nv_attach_impl)` | 跳过 |
| `runtime/CMakeLists.txt:91-104,177-189` | CUDA 时加入 `bpf_attach_ctx_cuda.cpp` 与 8 个 GPU map 源文件；链 CUDA_LIBS、nv_attach_impl、llvmbpf | 选读 |
| `runtime/agent/CMakeLists.txt:39-…` | `--whole-archive` 链入 nv_attach_impl/llvm vm | 跳过 |
| `runtime/agent/agent.version:2` | 导出 `__cudaRegisterFatBinary` | 选读 |
| `runtime/agent/agent.cpp:764-800` | `apply_injected_kv_overrides`：注入参数 `cuda_late_ptx_dir`/`cuda_disable_cuobjdump` → setenv | 选读 |
| `runtime/agent/agent.cpp:802-829` | agent 自己定义 `__cudaRegisterFatBinary` 包装：先 `register_llvm_vm_factory()` 再调原函数 | 必读 |
| `runtime/agent/agent.cpp:1037-1055` | 向 attach ctx 注册 `nv_attach_impl`（attach type 8/9） | 必读 |
| `runtime/include/bpf_attach_ctx.hpp:25-143,195-213,255-258` | `cuda::CommSharedMem`/`HelperCallRequest/Response`/`HelperOperation`/`CUDAContext`/`GPU_HELPER_MAX_BUF`；watcher 线程成员 | 必读 |
| `runtime/src/attach/bpf_attach_ctx_cuda.cpp` (429 行) | **CPU 侧 watcher 线程**（处理 GPU 发来的 helper 请求）、`create_map_basic_info`、`create_cuda_context` | 必读 |
| `runtime/src/attach/bpf_attach_ctx.cpp:120-136,163-188,339-387` | 第二遍扫表专处理 CUDA link；构造时建 CUDAContext+启动 watcher；CUDA link 实例化 | 必读 |
| `runtime/src/attach/demo_ptx_prog.hpp` (622 行) | 一份旧版 trampoline PTX 字符串常量 `DEMO_PTX_PROG`——**无人 include**，死代码 | 跳过 |
| `runtime/include/bpftime_prog.hpp:20-21,58-75,100-105` / `runtime/src/bpftime_prog.cpp:13-104,155-162,185-207` | `is_cuda()`=名字以 `cuda__` 开头；cuda 程序**不做 CPU JIT**；`compile_ptx_to_elf`（未被调用） | 必读 |
| `runtime/include/bpftime_shm.hpp:42-45,68,122-132,429-432` | `bpf_map_attr::gpu_thread_count=1024`；GPU map 类型枚举；`bpftime_poll_gpu_ringbuf_map` | 必读 |
| `runtime/src/bpftime_shm.cpp:694-704` | `bpftime_poll_gpu_ringbuf_map` 实现 | 选读 |
| `runtime/src/bpftime_shm_internal.hpp:21-29,107-110,315-322` / `.cpp:37,63-65,937-976,1040-1047,1208-1284,1398-1418` | shm 里建/找 `cuda_comm_shared_mem`；agent 侧 `register_cuda_host_memory`（整段 `cudaHostRegister`）；析构 unregister；GPU ringbuf drain | 必读 |
| `runtime/src/handler/map_handler.hpp:24-26,272-274,298-299` / `.cpp:15-23,69-84,100-107,219-…,749-760,949-1121,1286-1370` | GPU map 的创建/分派/`get_gpu_map_extra_buffer`/`get_gpu_map_max_thread_count`/线程数 env | 必读 |
| `runtime/include/bpftime_config.hpp:26,95-100` / `runtime/src/bpftime_config.cpp:163-174` | GDRCopy 配置项与 env | 选读 |
| `runtime/syscall-server/syscall_context.hpp:155-156,190-193` / `.cpp:214-235,515-526,855-876,1012-1019,1230-1237` | server 侧 `cuInit`、`initializing_cuda` 闸门、kernel-gpu 共享 map 类型、内核模式下 `cuda_` 前缀 kprobe | 选读 |
| `runtime/syscall-server/syscall_server_utils.cpp:169-171` / `syscall_server_main.cpp:536-549` / `syscall-server.version:2` | 启动时 `initialize_cuda`；导出 `bpftime_syscall_server__poll_gpu_ringbuf_map` | 选读 |
| `runtime/unit-test/CMakeLists.txt:63-65,125-129` / `runtime/unit-test/cuda/test_cuda_compile.cpp` (98) | "Test cuda compile"（名字不以 `cuda__` 开头，实际走 CPU JIT）、watcher 在 shm 拆除前停止 | 选读 |
| `runtime/README.md:59-63` | GPU maps 一段简介 | 跳过 |
| `tools/bpftimetool/main.cpp:28-31,159-161,302-385` / `CMakeLists.txt:4-50` | `bpftimetool run-on-cuda <prog> [count] [grid…block]` | 选读 |
| `tools/cli/main.cpp:560-740,1100-1240` | `bpftime trace`：对目标 exe 跑 cuobjdump 预抽 PTX 到临时目录，经注入参数传给 agent（late attach） | 选读 |
| `tools/aot/CMakeLists.txt:17-27` / `daemon/CMakeLists.txt:57-61` | CUDA 构建时链 CUDA 库（无 GPU 逻辑） | 跳过 |
| `vm/compat/include/bpftime_vm_compat.hpp:193-197` / `vm/compat/llvm-vm/compat_llvm.{hpp:45,cpp:72-75}` | compat 层 `generate_ptx` 转发（无调用者） | 跳过 |
| `vm/README.md:110-123,292-300` | 提到 `ptx_code_gen.cpp`、`do_get_ptx()`——**均不存在** | 跳过 |
| `vm/llvm-jit/include/llvmbpf.hpp:135-138` / `src/vm.cpp:350-359` | 公开 API `generate_ptx`/`generate_spirv`（**generate_ptx 的 target 参数被吞，见 §7**） | 必读 |
| `vm/llvm-jit/src/llvm_jit_context.hpp:40-75` / `.cpp:485-513,882-1065` | `createNVPTXTargetMachine`、`generate_ptx`、`generate_spirv`；只注册宿主后端 + NVPTX(+SPIR-V) | 必读 |
| `vm/llvm-jit/src/compiler.cpp:208-520,1098-1128,1751` | `generateModule(..., is_gpu)`：GPU 版主函数返回 void、参数 addrspace(1)、栈/调用栈用定长数组、LDDW src=1 无 map_by_fd 时直接用 imm | 必读 |
| `vm/llvm-jit/CMakeLists.txt:85-100,160-200` | LLVM 组件总含 NVPTX；SPIR-V 需 LLVM≥18；`LLVMBPF_ENABLE_PTX/SPIRV` 只控制示例 | 选读 |
| `vm/llvm-jit/example/ptx/*`（ptx_test.cpp 453、trampoline_ptx.h 747、default_trampoline.cu 456、.s 1160、README 282、CMake 63） | llvmbpf 独立的 PTX 演示（自带一份更老的 trampoline，sm_60，`GPU_HELPER_MAX_BUF=1<<25`） | 跳过（了解即可） |
| `vm/llvm-jit/example/spirv/*`（spirv_opencl_test.cpp 398、README 988、CMake 35）、`.github/workflows/test-spirv-opencl.yml` (90) | SPIR-V + OpenCL 演示与 CI | 跳过 |
| `vm/llvm-jit/test/unit-test/vm_test.cpp:500-512` | "Test NVPTX target is registered"（用默认 sm_60，掩盖了 §7 的 bug） | 选读 |
| `vm/llvm-jit/CLAUDE.md`、`README.md`、`.clangd`、`.gitignore` | 文档/配置中提到 PTX/SPIR-V | 跳过 |

### 1-D 示例与 benchmark（略读，每个一句）

`example/gpu/`：
| 目录 | 演示什么 |
|---|---|
| `cuda-counter` | 最基础：kprobe/kretprobe 挂 `vectorAdd`，用**普通 HASH map**（走 GPU→CPU RPC）计次数/耗时，`bpf_printk`；含 `run_nv_ptx_passes.sh` |
| `cudagraph` | 同上，但 kernel 经 CUDA Graph 启动，验证 cuGraph* 钩子 |
| `kernel_trace` | **stub 式挂载**：CUDA 源码里放空函数 `__bpftime_cuda__kernel_trace()`，pass 把对它的 call 改成 eBPF 函数；用 GPU ringbuf 上报，用户态 dlsym `bpftime_syscall_server__poll_gpu_ringbuf_map` 拉取 |
| `kernelretsnoop` | kretprobe + GPU ringbuf + PERGPUTD array，记录每线程退出时间（README 提醒要把 `BPFTIME_MAP_GPU_THREAD_COUNT` 调小以免 bad_alloc） |
| `threadhist` | kretprobe + PERGPUTD array，按线程计数（负载不均衡直方图） |
| `threadhist-gpu-kernel-shared-map` | 同上但用 GPU_KERNEL_SHARED_ARRAY_MAP + 内核模式（`BPFTIME_RUN_WITH_KERNEL`、`cuda__` 前缀、uretprobe 在内核侧） |
| `launchlate` | uprobe（CPU 侧 cudaLaunchKernel）+ GPU kprobe，测 launch 到实际开跑的延迟；GPU_ARRAY_MAP + ARRAY |
| `launchlate-kernel-gpu-shared-map` | 同上但 map 与内核 eBPF 共享 |
| `gpu_shared_map` | GPU_ARRAY_MAP 与 GPU_HASH_MAP 的行为演示 |
| `host_map_test` | 1512/1513 两种 HOST map（Tegra 路线）测试 |
| `threadscheduling` | helper 509/510/511 读 SM/warp/lane id，普通 HASH map |
| `threadscheduling_dynamic_hook` | 同上但用 GPU_HASH_MAP，kernel 签名带 int 参数 |
| `atomizer` | 用 helper 507（`exit;`）在 kernel 入口提前退出部分 block，实现 LithOS 的"kernel atomizer"；host 配置 block 划分 |
| `mem_trace` | `SEC("kprobe/__memcapture")` 触发 memcapture pass，每次 ld/st 后调用 eBPF |
| `directly_run_on_gpu` | 不挂 kernel，`bpftimetool run-on-cuda cuda__run` 把 eBPF 当独立 kernel 跑 |
| `cutlass` | CUTLASS GEMM 大负载 + kretprobe 计数（GPU array map） |
| `faiss-test` / `pytorch-test` / `llama-cpp-test` | 在真实框架（需带 PTX 编译）里挂 kretprobe + PERGPUTD array 做 threadhist |
| `rocm-counter` | AMD ROCm 版本（HIP 源）——本树里**没有** ROCm attach 实现（`BPFTIME_ENABLE_ROCM_ATTACH` 只出现在 `map_handler.cpp:1288,1329` 的 `#if`），属实验/占位 |

`benchmark/gpu/`：`workload/`（vec_add、matrixMul、simpleCUBLAS 负载）、`micro/`（JSON 驱动的探针开销微基准：empty/entry/exit/ringbuf/timer…，含 kernel-gpu-shared 变体与 P40/5090 结果表）、`nvbit/`（同负载的 NVBit 对照工具）、`host/`（host 侧读 GPU array / per-thread map 的性能，含 GDRCopy 对比）、`run_cuda_bench.py`（统一 runner）、`README.md`（P40：baseline 51.8μs，bpftime 81.1μs，NVBit 174.4μs）。

---

## 2. 核心数据结构

### 2.1 `nv_attach_impl`（`attach/nv_attach_impl/nv_attach_impl.hpp:145-281`）
继承 `base_attach_impl`，**每个 agent 进程一个**（agent 注册时 `std::make_unique`，`agent.cpp:1041`；`bpftimetool run-on-cuda` 也会建一个，`bpftimetool/main.cpp:336`），存在进程堆，随 `bpf_attach_ctx` 析构。关键字段：
- `hook_entries: map<int, nv_attach_entry>`（:246）——每个 CUDA link 一项。
- `pass_configurations: vector<unique_ptr<pass_cfg_with_exec_path>>`（:248-249）——构造时 dlmopen 进来的 pass。
- `patch_cache: map<sha256(请求JSON), RuntimeResponse>`（:250-251）——pass 结果缓存。
- `fatbin_records`（:202）、`current_fatbin`（:203，`__cudaRegisterFatBinary`…`End` 之间的"当前"记录）、
  `symbol_address_to_fatbin: map<host 桩/变量地址, fatbin_record*>`（:204）。
- `shared_mem_ptr`（:205）——**CommSharedMem 的设备地址**（来自 `cudaHostGetDevicePointer`），会被写进每个补丁模块的 `constData`。
- `map_basic_info: optional<vector<MapBasicInfo>>`（:206）——1024 项，写进补丁模块的 `map_info`。
- `ptx_compiler`（:208，dlopen 的 nvPTXCompiler 封装）、`module_pool`（sha256(cubin)→`ptx_in_module`，:210-211）、
  `ptx_pool`（sha256(PTX)→cubin，:213）。
- `patched_kernel_by_name`/`kernel_name_by_cufunction`（:253-254，late attach 与 driver API/graph 路径用）。
- `enabled`（:256，detach 最后一项时置 false）、late bootstrap 一组 once_flag/原子量（:259-265）、
  `host_symbol_ranges`（:268-273）、`patched_global_by_name`（:275-277）、`pending_launch_events_by_stream`（:279-280）。
- `original_cuda_launch_kernel` 等 11 个原函数指针（:219-229），与全局 `nv_attach_hook_state` 里的原子副本重复。

### 2.2 `nv_attach_hook_state`（`nv_attach_impl.hpp:118-136`，`nv_attach_impl.cpp:73-104`）
进程级单例（`std::optional` + `call_once` 惰性构造，永不析构）。存"当前活动 impl"（`active_impl`）和 Frida `gum_interceptor_replace`
得到的各原函数指针。replace 钩子是**全局只装一次**（`replace_hook_once` 检查 slot 非空则跳过，`nv_attach_impl.cpp:353-355`），
而 replacement 函数通过 `gum_invocation_context_get_replacement_data` 拿到这个 state，再读 `active_impl`——这样 impl 可被
析构重建而钩子不必重装。

### 2.3 `nv_attach_entry` / `nv_attach_private_data` / `pass_cfg_with_exec_path`
- `nv_attach_private_data`（`nv_attach_private_data.hpp:13-24`）：`code_addr_or_func_name`（第一个 kernel 名）、`func_names`
  （逗号分隔的全部 kernel 名，`nv_attach_private_data.cpp:40-58`）、`comm_shared_mem`、`map_basic_info`、`instructions`、`program_name`。
  由 attach ctx 的 private_data_creator 从 kprobe perf event 的函数名字符串构造（`bpf_attach_ctx.cpp:473-489`），
  再在 CUDA link 实例化时补齐后三个字段（`bpf_attach_ctx.cpp:354-366`）。
- `nv_attach_entry`（`nv_attach_impl.hpp:73-85`）：`instuctions`（原拼写）、`kernels`、`program_name`、`config`（指向匹配到的 pass）。
  `parameters`/`extras` 字段从未被使用。
- `pass_cfg_with_exec_path`（:87-116）：pass .so 路径、`PassConfig`、`print_config`/`process_input` 两个 C 函数指针、dlopen handle（析构 dlclose）。
- `MapBasicInfo`（:45-53，**与 `default_trampoline.cu:98-106` 必须逐字节一致**）：`bool enabled; int key_size, value_size, max_entries, map_type; void *extra_buffer; uint64_t max_thread_count;`
  —— x86-64/aarch64 下 sizeof=40，1024 项 = 40960 字节 = PTX 里 `.const .b8 map_info[40960]`（`trampoline_ptx.h:18`）。
  `extra_buffer` 对 GPU map 是**agent 进程里 GPU 可用的地址**（IPC 打开的设备指针或 host 映射地址），由
  `bpf_map_handler::get_gpu_map_extra_buffer()` 给出（`map_handler.cpp:1327-1370`）；`max_thread_count` 来自 `get_gpu_map_max_thread_count()`（:1286-1325）。
  **数组下标 = bpftime 的 map fd（handler 下标）**（`bpf_attach_ctx_cuda.cpp:337-366`），因为 GPU 侧 LDDW 不做重定位（§3.3）。

### 2.4 `fatbin_record` / `ptx_in_module`（`nv_attach_fatbin_record.hpp:13-50`）
- `fatbin_record`：`original_ptx`（文件名→PTX 文本）、`ptxs`（加载后的模块，shared_ptr）、`function_addr_to_symbol`（host 桩地址→`kernel_info{name, CUfunction, ptx}`）、
  `variable_addr_to_symbol`（host 变量地址→`variable_info{name, CUdeviceptr, size}`）、`ptx_loaded`、共享的 `module_pool`/`ptx_pool`。
  由 `__cudaRegisterFatBinary` 钩子（`nv_attach_impl_frida_setup.cpp:283-290`）或 late bootstrap（`nv_attach_impl.cpp:1834-1843,1873-1879`）创建，
  存进程堆，`clear_patched_state_for_next_session` 时整体丢弃（`nv_attach_impl.cpp:1531-1567`）。
- `ptx_in_module`：只包一个 `CUmodule`，析构 `cuModuleUnload`（`nv_attach_fatbin_record.cpp:48-52`）。

### 2.5 PTX pass 协议（`pass/ptxpass_core/include/ptxpass/core.hpp`）
- `PassConfig{name, description, attach_points{includes,excludes 正则}, attach_type, parameters, validation}`（:26-36）。
- `RuntimeRequest{input:{full_ptx, to_patch_kernel, global_ebpf_map_info_symbol="map_info", ebpf_communication_data_symbol="constData"}, ebpf_instructions:[{upper_32bit,lower_32bit}]}`（:71-80,93-129）。
- `RuntimeResponse{output_ptx, modified}`（:86-90）。`ExitCode`（:60-66）。
- pass 是 **共享库**，导出 `print_config(int len, char *out)` 与 `process_input(const char *in, int len, char *out)`，用 JSON 字符串往返；
  impl 用 `dlmopen(LM_ID_NEWLM, …)` 把每个 pass 装进**独立 link namespace**（`nv_attach_impl.cpp:522-523`），pass 自带一份 llvmbpf。

### 2.6 `cuda::CommSharedMem`（host：`runtime/include/bpf_attach_ctx.hpp:93-102`；device：`default_trampoline.cu:75-84`）
```
int flag1;        // GPU→CPU "有请求"
int flag2;        // CPU→GPU "处理完"
int occupy_flag;  // 未使用
int request_id;   // HelperOperation：1 lookup /2 update /3 delete /6 printk /14 pid_tgid /501 puts
long map_id;      // 实际是 map fd
HelperCallRequest req;   // union：key[MAX_BUF] / key+value+flags / fmt[1000]+args / data[10000]
HelperCallResponse resp; // union：int result / const void* value / u64 result
uint64_t time_sum[8];
```
- `GPU_HELPER_MAX_BUF = 1<<20`（`bpf_attach_ctx.hpp:43`），所以 `req` ≈ 2 MiB，`resp` 偏移 = 24 + 2097160 = **2097184**；
  生成的 `trampoline_ptx.h` 里 `make_helper_call` 正是 `ld.u64 %rd13, [%rd1+2097184]`（`trampoline_ptx.h:115`），两边一致。
  但 **trampoline 源码 `default_trampoline.cu:21` 写的是 `1 << 24`**——与生成物不一致（§7）。
- 生命周期：syscall-server（`SHM_REMOVE_AND_CREATE`）在 shm 里 `construct<CommSharedMem>("cuda_comm_shared_mem")` 并清零
  （`bpftime_shm_internal.cpp:960-974`）；agent（`SHM_OPEN_ONLY`）最多等 1s 找到它，然后 `register_cuda_host_memory()`
  把**整个 shm 段** `cudaHostRegister(Mapped)`（:937-959,1209-1256）；`CUDAContext` 构造时 `cudaHostGetDevicePointer` 得到其设备地址
  （`bpf_attach_ctx_cuda.cpp:405-426`），并存入进程级原子 `cuda_shared_mem_device_pointer_storage()`（`bpf_attach_ctx.hpp:125-141`）。

### 2.7 `cuda::CUDAContext`（`bpf_attach_ctx.hpp:103-121`）
`cuda_watcher_should_stop`（shared_ptr<atomic<bool>>）、`cuda_shared_mem`（host 指针）、`cuda_shared_mem_device_pointer`。
由 `bpf_attach_ctx` 构造函数创建（`bpf_attach_ctx.cpp:175-187`），同时 `start_cuda_watcher_thread()`；`bpf_attach_ctx` 析构或
shm 拆除前（`bpftime_shm_internal.cpp:63-65` → `bpf_attach_ctx_cuda.cpp:95-98`，以及 atexit `:281-283`）停止 watcher。
注意：`bpftimetool run-on-cuda` 里也会建一个 `bpf_attach_ctx`，从而在 bpftimetool 进程也起一个 watcher。

### 2.8 GPU map 实现（全部用 boost `construct` 建在 **shm** 中，`anonymous_instance`）
内存布局（`<数据位置>`；GPU 侧访问路径见 §3.5）：

| 类型(值) | 类 | 数据放在哪 | 布局 | `extra_buffer`（agent 侧） | `max_thread_count` |
|---|---|---|---|---|---|
| GPU_HASH_MAP(1501) | `nv_gpu_shared_hash_map_impl` | key 表在 shm（`key_buffer`，每槽 `[u32 filled][key]`，槽数 `next_prime(max_entries)`）；value 在内嵌 `nv_gpu_shared_array_map_impl` 的显存 | value `[max_entries][value_size]` | 内嵌 array 的 IPC 设备指针 | 1 |
| PERGPUTD_ARRAY_MAP(1502) | `nv_gpu_per_thread_array_map_impl` | 显存（server `cuMemAlloc`，`nv_gpu_per_thread_array_map.cpp:61`） | `[max_entries][thread_count][value_size]` | `cuIpcOpenMemHandle` 结果（按 pid 缓存，:19-46） | `thread_count` |
| GPU_ARRAY_MAP(1503) | `nv_gpu_shared_array_map_impl` | 显存（`nv_gpu_shared_array_map.cpp:42`） | `[max_entries][value_size]` | IPC 打开；agent 侧还选 device/context、建 NON_BLOCKING stream（:247-374） | 1 |
| GPU_KERNEL_SHARED_ARRAY_MAP(1504) | `array_map_kernel_gpu_impl` | 有内核 map：内核 array map 的 mmap 区（`array_map_kernel_gpu.cpp:41-100`）；否则 shm `value_data` | `[max_entries][value_size]`（mmap 版按 8 字节对齐页对齐） | `cuMemHostRegister`+`cuMemHostGetDevicePointer`；无内核 map 时用"相对 CommSharedMem 的偏移"推算设备地址（:248-316） | 0（未处理，走 "Not a GPU map"） |
| GPU_RINGBUF_MAP(1527) | `nv_gpu_ringbuf_map_impl` | **shm（host 内存）** `data_buffer`（`nv_gpu_ringbuf_map.cpp:12-23`） | 每线程一段 `entry_size = sizeof(header 24B) + max_entries*(8+value_size)`：`[head u64][tail u64][dirty int]` 后跟 `max_entries` 个 `[u64 size][value_size 字节]` | `cuMemHostGetDevicePointer(data_buffer)`（:25-54） | `thread_count`（shm 不够时自动减半，`map_handler.cpp:1063-1093`） |
| PERGPUTD_ARRAY_HOST_MAP(1512) | `nv_gpu_array_host_map_impl` | shm `data_buffer` | 同 1502 | `cuMemHostGetDevicePointer` | `attr.gpu_thread_count`（**不读 env**） |
| GPU_ARRAY_HOST_MAP(1513) | `nv_gpu_shared_array_host_map_impl` | shm | 同 1503 | 同上 | 1 |

- 枚举定义：`runtime/include/bpftime_shm.hpp:68,122-132`（`GPU_MAP_OFFSET=1500` + 内核 map 类型号）。trampoline 里**手抄了一份常量**（`default_trampoline.cu:86-96`），两边需同步。
- `bpf_map_handler::get_userspace_value_size()` 对 1502/1512 返回 `value_size * attr.gpu_thread_count`（`map_handler.cpp:76-82`）——即用户态
  `bpf_map_lookup_elem` 一次拿到"所有线程的值"。
- 1502/1503/1504/1501/1527 的创建都在 **syscall-server（loader）进程**里：`set_enable_mock(false)` 关掉 syscall 拦截 → 首次时
  `cuDeviceGet(0)`+`cuCtxCreate` 建上下文（static 变量）→ construct → 恢复 mock（`map_handler.cpp:950-1093`）。HOST 版不碰 CUDA。
- per-pid 缓存 `agent_gpu_shared_mem`（boost unordered_map in shm）是各 map 的通用模式。

### 2.9 GPU 侧（PTX）全局对象（trampoline，每个补丁模块各一份）
`constData`（u64，CommSharedMem 设备地址）、`map_info[1024]`、`__bpftime_comm_lock`（`.global u32`，GPU→CPU RPC 的设备级自旋锁）
（`trampoline_ptx.h:17-19`；源 `default_trampoline.cu:114-116`）。**每个被加了 trampoline 的 PTX 文件/模块都有自己的一份**（§7）。

---

## 3. 主要流程与调用链

### 3.1 构建期：trampoline 与 pass 的产生
- `trampoline/Makefile:10-18`：`clang++-17 -S default_trampoline.cu --cuda-gpu-arch=sm_61 -O2` → `default_trampoline-cuda-nvptx64-nvidia-cuda-sm_61.s`
  → `grep -B 10000 "// .globl	bpf_main"` 截取 `bpf_main` 之前的全部内容 → 写成 `../trampoline_ptx.h` 的原始字符串。
  **CMake 不会重新生成它**，是手工生成后提交的文件（Makefile 不在任何 CMake 目标中）。用途：`get_default_trampoline_ptx()` 返回它
  （`nv_attach_utils.cpp:15-18`），`wrap_ptx_with_trampoline` 把它拼在补丁 PTX 前面（:19-22）。
- `attach/nv_attach_impl/CMakeLists.txt:3-21,69-72`：编出 `libptxpass_kprobe_entry.so`、`libptxpass_kretprobe.so`、`libptxpass_kprobe_memcapture.so`
  与 `libnv_attach_impl_ptx_compiler.so`，并用 `file(GENERATE)` 把它们的**绝对构建路径**写进 `ptx_pass_config.h` 的
  `DEFAULT_PTX_PASS_EXECUTABLE`（冒号分隔）和 `DEFAULT_PTX_COMPILER_SHARED_LIB`。

### 3.2 启动：agent 侧装钩子与注册（LD_PRELOAD 场景）
`bpftime_agent_main` (`agent.cpp`) → `ctx_holder.ctx.register_attach_impl({8,9}, make_unique<nv_attach_impl>(), creator)` (`agent.cpp:1039-1055`)
→ `nv_attach_impl::nv_attach_impl()` (`nv_attach_impl.cpp:279-562`)：
1. `dlopen("libcuda.so.1")`（:285）、`gum_init_embedded` + 建 listener（GObject 类型在 `nv_attach_impl_frida_setup.cpp:49-55`）。
2. `gum_interceptor_attach`（只观察、不改行为）到 `__cudaRegisterFatBinary`（用 `dlsym(RTLD_NEXT)` 找，跳过 agent 自己的同名包装）、
   `__cudaRegisterFunction`、`__cudaRegisterVar`、`__cudaRegisterFatBinaryEnd`、`cudaMalloc`、`cudaMallocManaged`、`cudaMemcpyToSymbol(Async)`（:368-421）。
3. `gum_interceptor_replace`（替换实现）`cudaLaunchKernel`、`cudaLaunchKernel_ptsz`、`cuLaunchKernel`、`cuGraphAddKernelNode(_v2)`、
   `cuGraphExecKernelNodeSetParams(_v2)`、`cuGraphKernelNodeSetParams(_v2)`、`cudaMemcpyFromSymbol(Async)`（:422-463）。找不到符号只 DEBUG 跳过。
4. 读 `BPFTIME_PTXPASS_LIBRARIES` 或默认路径，逐个 `dlmopen(LM_ID_NEWLM)` + `dlsym(print_config/process_input)`，调用 `print_config` 拿 JSON 配置（:504-556）。
5. `load_nv_attach_impl_ptx_compiler(DEFAULT_PTX_COMPILER_SHARED_LIB)`（:557-561，`ptx_compiler.hpp:37-63`）。
另外，agent 自己导出 `__cudaRegisterFatBinary`（`agent.cpp:812-829`，`agent.version:2`）：只做"先 `register_llvm_vm_factory()` 再调原函数"，
保证 CUDA 静态构造早于 llvm VM 注册时也不出错。

### 3.3 实例化 CUDA link：从 shm 里的 prog/link 到 `hook_entries`
`bpf_attach_ctx::init_attach_ctx_from_handlers` (`bpf_attach_ctx.cpp:73-160`)：第一遍扫表（`handle_nv_attach_impl=false`，:106-119）
→ prog handler 实例化 `instantiate_prog_handler_at` (:294-315)：`bpftime_prog` 构造；`bpftime_prog_load` 对 `is_cuda()`（名字以 `cuda__` 开头，
`bpftime_prog.hpp:59-62`）**只 `ebpf_load` 不 JIT**（`bpftime_prog.cpp:185-207`）；
link 遇到 cuda 程序时直接 `return 0` 不登记（`bpf_attach_ctx.cpp:369-374`）
→ 第二遍扫表（`true`，:120-136）→ `instantiate_bpf_link_handler_at` (:316-418) 的 CUDA 分支 (:339-377)：
补齐 `program_name`、`comm_shared_mem = cuda_ctx->cuda_shared_mem_device_pointer`、`instructions = prog->get_insns()`（**原始字节码**）、
`map_basic_info = create_map_basic_info(1024)`（`bpf_attach_ctx_cuda.cpp:322-369`，遍历 handler 表，凡是 map 就按 fd 下标填一项）
→ `nv_attach_impl::create_attach_with_ebpf_callback` (`nv_attach_impl.cpp:160-237`)：
attach type 8→`"kprobe/"+name`，9→`"kretprobe/"+name`，按每个 pass 的 `attach_type` 和 include/exclude 正则（`core.cpp:48-71`）找**第一个**匹配的 pass；
记录 `hook_entries[id]`（**传入的 ebpf_run_callback 被丢弃**——GPU 程序不在 CPU 执行）；缓存 `shared_mem_ptr`、`map_basic_info`；
→ `start_late_bootstrap_async()` (:1953-1981，名字叫 async 实为**同步**执行) → `ensure_cuda_context_for_current_thread()`（cuInit + primary ctx retain + set current，:1719-1748）
→ `bootstrap_existing_fatbins_once()` (:1908-1943) → `bootstrap_existing_fatbins()` (:1814-1906)：扫已加载模块的 `.nv_fatbin`/`.nvFatBinSegment`，对每个 fatbin
`extract_ptxs` → 新建 `fatbin_record` → `try_loading_ptxs`（即 §3.4 后半段）→ `prefill_patched_kernel_functions_from_loaded_fatbins` 用 PTX 里的 `.entry` 名在补丁模块里取 CUfunction 填 `patched_kernel_by_name`（:1677-1708）。
（kprobe 事件从哪来：libbpf 对 `SEC("kprobe/<kernel>")` 调 `perf_event_open(kprobe 类型)` → syscall-server `bpftime_kprobe_create` → `bpf_perf_event_handler` 类型 `BPF_TYPE_KPROBE=8`/`KRETPROBE=9`（`perf_event_handler.cpp:195-213`、`bpftime_shm.hpp:59-60`），数值恰好等于 `ATTACH_CUDA_PROBE/RETPROBE`——即 **bpftime 用户态的"kprobe"就是 CUDA kernel 探针**。内核共存模式下名字以 `cuda_` 开头的 kprobe 被转成真实 `do_exit` kprobe + 用户态 CUDA 探针，并剥掉前缀（`syscall_context.cpp:855-876`）。）

### 3.4 发现 kernel + 打补丁 + 重编译 + 加载（启动期 `__cudaRegister*` 路径）
应用静态构造调用 `__cudaRegisterFatBinary(wrapper)`（经 agent 包装，`agent.cpp:812`）→ libcudart 原函数上的 Frida listener
`example_listener_on_enter` RegisterFatbin 分支 (`nv_attach_impl_frida_setup.cpp:254-291`)：从 `wrapper->data` 起沿 `0xBA55ED50` 头链算出 fatbin 总长 →
`nv_attach_impl::extract_ptxs` (`nv_attach_impl.cpp:645-888`)：若 `BPFTIME_CUDA_LATE_PTX_DIR` 是目录则直接读其中 `*.ptx`；若 `BPFTIME_CUDA_DISABLE_CUOBJDUMP=1` 返回空；
否则写 `/tmp/bpftime-fatbin-work.XXXXXX/temp.fatbin`，用 **posix_spawn** `/bin/sh -c "cd … && cuobjdump --extract-ptx all temp.fatbin"`（去掉 LD_PRELOAD；
注释说明避免在已初始化 CUDA 的进程里 fork）→ 读回所有 `.ptx` → 存为 `fatbin_record.original_ptx`，设为 `current_fatbin`。
→ 每次 `__cudaRegisterFunction(handle, hostFun, deviceFun, deviceName, …)`：RegisterFunction 分支 (:292-324) → `current_fatbin->try_loading_ptxs(impl)`（只做一次）
→ `fatbin_record::try_loading_ptxs` (`nv_attach_fatbin_record.cpp:190-280`)：
  1. `impl.hack_fatbin(original_ptx)` (`nv_attach_impl.cpp:889-1068`)：boost thread_pool 按 PTX 文件并行；对每个文件、每个 hook_entry、每个 kernel 组装
     `RuntimeRequest` JSON → 以 sha256 查 `patch_cache` → 未命中则调 pass 的 `process_input`（输出缓冲从 1MiB 倍增到 64MiB 直到 JSON 可解析）→ 链式更新 `current_ptx`；
     若任一 pass 报 `modified`，则 `filter_out_version_headers_ptx(wrap_ptx_with_trampoline(current_ptx))`（trampoline 在前，去掉重复的 `.version/.target/.address_size` 与注释行）；
     结果放进 `ptx_out["patched."+文件名] = (ptx, 是否加了 trampoline)`。
  2. pass 内部（以 kprobe_entry 为例，`pass/ptxpass_kprobe_entry/main.cpp:46-98`）：`ptxpass::compile_ebpf_to_ptx_from_words(words, "sm_61", "__probe_func__"+kernel, add_guard, false)`
     (`core.cpp:196-244`)：新建一个 `llvmbpf_vm`，**把 1/2/3/6/14/25/501-505/507-511 注册为占位 helper**（:205-223），`llvm_bpf_jit_context::generate_ptx(false, fname, "sm_61")`
     (`llvm_jit_context.cpp:902-972`；`generateModule(..., is_gpu=true)`) → 过滤掉 `.version/.target/.address_size/注释`、`_bpf_helper_ext_NNNN` 的 `.extern` 声明（core.cpp:245-286）。
     然后：若 PTX 中有 `call __bpftime_cuda__kernel_trace`（stub）就把被调函数名替换成 `__probe_func__<k>`（:60-82）；否则 `find_kernel_body` 找到 `.entry <k>(`，在第一个 `{` 后插入
     `call __probe_func__<k>;`（:84-97）；把编出的函数 PTX 拼在整份 PTX 前面。kretprobe 用正则在 kernel 体内每个（可带谓词/标号的）`ret;`/`exit;` 前插 call（`ptxpass_kretprobe/main.cpp:36-65`）。
  3. `compile_ptxs` (`nv_attach_fatbin_record.cpp:103-189`)：`get_gpu_sm_arch()`；thread_pool 并行，对**每个**（包括未修改的）PTX `rewrite_ptx_target` 改 `.target`/必要时 `.version`，
     sha256 查 `ptx_pool`，未命中则经 dlopen 的 nvPTXCompiler 以 `--gpu-name=sm_XX --verbose -O3` 编成 cubin。
  4. 对每个 cubin：sha256 查 `module_pool`，未命中则 `cuModuleLoadDataEx`（:229-239）；若加了 trampoline：`cuModuleGetGlobal("constData")` 写 `shared_mem_ptr`，
     `cuModuleGetGlobal("map_info")` 写 `map_basic_info`（:241-272）。
→ 回到 RegisterFunction 分支：`find_and_fill_function_info(hostFun, deviceName)` 在新模块里 `cuModuleGetFunction`（:79-101）→
  `symbol_address_to_fatbin[hostFun] = current_fatbin`，并 `record_patched_kernel_function`。`__cudaRegisterVar` 同理记录全局变量（`frida_setup.cpp:325-351`）。

### 3.5 一次 GPU 事件：从 launch 到 eBPF 执行到用户态读到
1. 应用 `kernel<<<g,b>>>(args)` → `cudaLaunchKernel(hostFun, g, b, args, smem, stream)` 被 replace 为
   `cuda_runtime_function__cudaLaunchKernel` (`frida_setup.cpp:429-451`) → `cuda_launch_kernel_common` (:92-246)：
   impl 为空/`!enabled`/stream 正在 capture → 走原函数；确保有 current context；
   命中 `symbol_address_to_fatbin[hostFun]` → **`cuLaunchKernel(补丁模块里的 CUfunction, 同样的 grid/block/smem/stream/args)`**（:165-193），
   失败则回落原 launch（fail-open）；成功则 `record_patched_launch(stream)`（建一个 CUevent 记在 stream 上，供 detach 等待）。
   未命中且 late bootstrap 完成 → `resolve_host_function_symbol`（dladdr，失败用 ELF 符号表；把 `__device_stub__` 前缀规整掉，:1634-1675）→
   `find_patched_kernel_function(name)` → 同样 `cuLaunchKernel`（:195-235）。driver API 的 `cuLaunchKernel` 与 cuGraph* 钩子则用
   `cuFuncGetName`/`cuKernelGetName` 取名后替换 CUfunction（:453-527,573-775）。
2. GPU 上每个线程执行补丁 kernel：入口（或 ret 前）`call __probe_func__<k>` → eBPF 编出来的 PTX 函数（无参数；r1 未定义；
   每线程 local memory 里有 `__local_depot0[16464]` 字节的 eBPF 栈+调用栈，见 §9 实验）。
3. eBPF 调 helper，例如 `bpf_map_lookup_elem(&map, &key)`：LDDW src=1 的 imm（**map fd**）原样成为 r1（`compiler.cpp:1114-1127`，GPU 编译用的 vm 没设 `map_by_fd`），
   → `_bpf_helper_ext_0001(map=fd, key)`（`default_trampoline.cu:201-248`）→ 读 `map_info[fd]`：
   - 1502：`array_map_offset` = `extra_buffer + key*max_thread_count*value_size + globalTid*value_size`（:189-199）直接返回设备指针，**零拷贝、无 CPU 参与**；
   - 1503/1504：`extra_buffer + key*value_size`；1512/1513：同上并 `membar.sys`；
   - 其他（含普通 HASH/ARRAY、1501 GPU_HASH）：把 key 拷进 `CommSharedMem.req` → `make_helper_call(fd, MAP_LOOKUP)`（:131-173）：
     按 lane 0..31 依次让活跃线程拿 `__bpftime_comm_lock`（atomicCAS 自旋），写 `request_id/map_id` → `membar.sys; flag1=1` → 自旋读 `flag2` 直到非 0 → `flag2=0` → 读 `resp` → 解锁。
4. CPU 侧 agent 里的 watcher 线程（`bpf_attach_ctx_cuda.cpp:111-289`）每 1ms 轮询 `flag1==1`：清 flag1 → 按 `request_id` 调
   `bpftime_map_lookup_elem/update/delete`、`bpftime_trace_printk`、pid_tgid、puts（SPDLOG_INFO 输出）→ lookup 结果若是 host 指针（`cuPointerGetAttribute` 不是 DEVICE/UNIFIED），
   就按"相对 CommSharedMem 的偏移"换算成设备地址（依赖整个 shm 段一次性 `cudaHostRegister`，偏移不变，:143-185）→ `seq_cst fence; flag2=1`。
   于是 GPU 拿到指向 **shm 中 map 值**的设备地址，可直接读写（例如 cuda-counter 对 HASH 值做 `__atomic_add_fetch`）。
5. 输出事件：`bpf_perf_event_output(ctx, &ringbuf, 0, &ev, sz)` → `_bpf_helper_ext_0025`（:359-411）→ 仅支持 1527：按全局线程号定位该线程的环（host 内存），满了返回 2，
   `dirty=1` → `tail` 原子加 → 写 `[size][data]` → `dirty=0`。
6. 用户态读取：
   - array 类：loader 进程 `bpf_map_lookup_elem(fd)` → syscall-server 拦截 → `bpftime_map_lookup_elem` → 如 `nv_gpu_per_thread_array_map_impl::elem_lookup`
     (`nv_gpu_per_thread_array_map.cpp:85-118`)：可选 GDRCopy，否则 `cuMemcpyDtoH` 把 `[key][all threads]` 拷到 shm 里的 `value_buffer` 再返回（server 用自己 `cuMemAlloc` 的原指针，IPC 双方是同一块物理显存）。
   - ringbuf：loader `dlsym("bpftime_syscall_server__poll_gpu_ringbuf_map")`（`example/gpu/kernel_trace/kernel_trace.c:91`）→ `syscall_server_main.cpp:537-549` → `syscall_context::poll_gpu_ringbuf_map` (`syscall_context.cpp:1231-1237`)
     → `bpftime_poll_gpu_ringbuf_map` (`bpftime_shm.cpp:695-703`) → `bpftime_shm::poll_gpu_ringbuf_map` (`bpftime_shm_internal.cpp:1399-1418`) → `nv_gpu_ringbuf_map_impl::drain_data` (`nv_gpu_ringbuf_map.cpp:56-81`)：
     逐线程，`head != tail` 时取一条回调（**每次调用每线程最多一条**；遇到任一 dirty 立即整体返回）。
   - HOST map：数据本就在 shm，`elem_lookup` 直接返回 shm 指针（`nv_gpu_array_host_map.cpp:84-94`）。

### 3.6 Late attach（对已在运行的 CUDA 进程 `bpftime trace`）
`bpftime trace --pid P …` (`tools/cli/main.cpp:1048-1240`) → `prepare_cuda_late_ptx_dir` (:603-740)：在**注入方**对目标 exe（及 cmdline 中存在的文件）fork+exec
`cuobjdump --extract-ptx all` 到 `/tmp/bpftime-late-ptx.XXXXXX`（0700，必要时 chown 给目标 uid）→ 起 loader（带 syscall-server）→ 通过 IPC refresh 或 Frida 注入 agent，
参数串里带 `cuda_late_ptx_dir=<dir>`（失败则 `cuda_disable_cuobjdump=1`，:1161-1166）→ agent `apply_injected_kv_overrides` setenv（`agent.cpp:787-797`）→
attach 时 §3.3 的 bootstrap 用该目录下的 PTX（`nv_attach_impl.cpp:653-678`；ELF 里找不到 fatbin 时也直接用目录兜底 :1887-1900）→ launch 时走 §3.5 第 1 步的"按名字"路径。
结束时 CLI 先 IPC `detach` agent，再停 loader（:1190-1240），原因：GPU map 显存属于 loader（IPC 导出方），loader 先退会让目标里的补丁 kernel 访问失效内存。
`detach_by_id` 删掉最后一项时：`enabled=false` → 最多等 2s 让已入队补丁 kernel 的 event 完成 → 清空所有补丁状态与模块 → 重置 bootstrap（`nv_attach_impl.cpp:132-153,1443-1567`）。

### 3.7 `bpftimetool run-on-cuda`：把 eBPF 当独立 kernel 跑
`bpftimetool/main.cpp:303-380` → 建 `bpf_attach_ctx` + 注册 nv impl → `init_attach_ctx_from_handlers` → `find_attach_entry_by_program_name` →
`nv_attach_impl::run_attach_entry_on_gpu` (`nv_attach_impl.cpp:1101-1270`)：`compile_ebpf_to_ptx_from_words(words, sm, "bpf_main", false, false)` →
`filter_compiled_ptx_for_ebpf_program(…, "bpf_main")`（`nv_attach_impl_patcher.cpp:57-100`，加 `.func bpf_main` 头）→ 拼 trampoline 并改 target → 把 `.func bpf_main` 改成 `.visible .entry bpf_main` →
直接链接的 nvPTXCompiler 编译 → **`cuCtxCreate` 新 context** → 加载模块、写 constData/map_info → 按给定 grid/block `cuLaunchKernel` + `cuCtxSynchronize` 循环 run_count 次。
（directly_run 示例用 `SEC("kprobe/__directly_run")`，需要有 pass 能匹配 `kprobe/__directly_run`——kprobe_entry 的 `^kprobe/.*$` 会匹配，于是它同时也会尝试给名为 `__directly_run` 的 kernel 插桩，通常找不到即无修改。）

### 3.8 cudaMemcpyToSymbol / FromSymbol 镜像
因为补丁 kernel 运行在**另一个模块**里，它看到的 `__device__`/`__constant__` 全局变量是新模块的副本，而应用 `cudaMemcpyToSymbol` 写的是原模块的变量。
所以：`cudaMemcpyToSymbol(Async)` 用 listener 在原调用执行前**额外**把同样数据拷进补丁模块的同名全局（`frida_setup.cpp:360-388` → `nv_attach_impl.cpp:1272-1412`，找不到时用宿主符号名/去 mangle 名在各模块里 `cuModuleGetGlobal`）；
`cudaMemcpyFromSymbol(Async)` 被 replace：若能定位补丁模块的变量就**改从补丁模块读**（`frida_setup.cpp:777-943`），否则调原函数。

---

## 4. 跨模块接口

### 4.1 本块对外提供
| 接口 | 定义 | 使用者（调用点） |
|---|---|---|
| `attach::nv_attach_impl`（类） | `nv_attach_impl.hpp:145` | `agent.cpp:1041`；`bpftimetool/main.cpp:336`；`bpf_attach_ctx_cuda.cpp:100-110`（`find_nv_attach_impl` dynamic_cast） |
| `attach::nv_attach_private_data` | `nv_attach_private_data.hpp:13` | `agent.cpp:1046`；`bpftimetool/main.cpp:341-342`；`bpf_attach_ctx.cpp:37,354-366,381-387` |
| `ATTACH_CUDA_PROBE/RETPROBE`（8/9） | `nv_attach_impl.hpp:43-44` | `agent.cpp:1040`；`bpftimetool/main.cpp:334-335`；（数值与 `bpftime_shm.hpp:59-60` 的 `BPF_TYPE_KPROBE/KRETPROBE` 相同，靠巧合/约定对上） |
| `attach::MapBasicInfo` | `nv_attach_impl.hpp:45` | `bpf_attach_ctx.hpp:256-257`、`bpf_attach_ctx_cuda.cpp:322-369` |
| `find_attach_entry_by_program_name`、`run_attach_entry_on_gpu` | `nv_attach_impl.hpp:172-176` | `bpftimetool/main.cpp:355-370` |
| `attach::try_get_original_func` | `nv_attach_utils.hpp:20-27` | `agent.cpp:815` |
| `nv_attach_impl.hpp` 头本身 | — | 被 `runtime/include/bpf_attach_ctx.hpp:27` 包含 → CUDA 构建下所有包含 bpf_attach_ctx.hpp 的 runtime 代码都间接依赖 CUDA/Frida 头 |
| `cuda::CommSharedMem` / `HelperOperation` / `CUDAContext` / `create_cuda_context` / `get_cuda_shared_mem_device_pointer` | `bpf_attach_ctx.hpp:36-143` | `bpftime_shm_internal.cpp:937-976`（construct/find）；`array_map_kernel_gpu.cpp:257-262`；`bpf_attach_ctx.cpp:180` |
| `stop_cuda_watcher_before_shm_unmap` | `bpf_attach_ctx_cuda.cpp:95` | `bpftime_shm_internal.cpp:37,64` |
| `bpftime_poll_gpu_ringbuf_map` / `bpftime_syscall_server__poll_gpu_ringbuf_map`（导出符号） | `bpftime_shm.hpp:430`、`syscall_server_main.cpp:537` | 示例 `kernel_trace.c:91`、`kernelretsnoop.c:87`（dlsym） |
| GPU map 类型枚举 | `bpftime_shm.hpp:122-132` | `map_handler.cpp` 各 switch；trampoline 手抄（`default_trampoline.cu:86-96`）；示例 .bpf.c 里手写数字宏 |
| `bpf_map_handler::get_gpu_map_extra_buffer` / `get_gpu_map_max_thread_count` / `try_get_nv_gpu_ringbuf_map_impl` | `map_handler.hpp:272-274,298-299` | `bpf_attach_ctx_cuda.cpp:343,364`；`bpftime_shm_internal.cpp:1409` |
| `bpftime_shm::register_cuda_host_memory` / `get_cuda_comm_shared_mem` / `poll_gpu_ringbuf_map` | `bpftime_shm_internal.hpp:315-322` | 自身构造 :958-959；`bpf_attach_ctx_cuda.cpp:386`；`array_map_kernel_gpu.cpp:257-258`；`bpftime_shm.cpp:699` |
| `runtime_config.enable_gpu_gdrcopy` / `gpu_gdrcopy_max_per_key_bytes` | `bpftime_config.hpp:95-100` | `nv_gpu_gdrcopy.cpp:148-159` |
| `syscall_context::initialize_cuda` / `initializing_cuda` | `syscall_context.hpp:155-156,190-191` | `syscall_server_utils.cpp:170`；各 handle_* 闸门（`syscall_context.cpp:259,281,…,1012-1019`） |
| llvmbpf：`llvmbpf_vm::generate_ptx/generate_spirv`、`llvm_bpf_jit_context::generate_ptx/generate_spirv` | `llvmbpf.hpp:135-138`、`llvm_jit_context.hpp:66-73` | `ptxpass/core.cpp:228`（bpftime 唯一实际调用，直接用内部 ctx）；`compat_llvm.cpp:74`（无调用者）；llvmbpf 示例与 `vm_test.cpp:508` |

### 4.2 本块依赖外部
| 依赖 | 调用点 |
|---|---|
| `base_attach_impl`（`allocate_id`、虚接口） | `nv_attach_impl.cpp:197`；`nv_attach_impl.hpp:145-155` |
| `bpf_attach_ctx` 的两遍扫描与 link 实例化（R?-attach/runtime 读者） | `bpf_attach_ctx.cpp:106-136,339-377` |
| Frida gum：`gum_interceptor_attach/replace/detach`、`gum_module_find_export_by_name`、`gum_invocation_context_*` | `nv_attach_impl.cpp:297-464,569-584`；`frida_setup.cpp` 全文 |
| llvmbpf 内部：`llvmbpf_vm::register_external_function/load_code`、`llvm_bpf_jit_context::generate_ptx`（**包含 src 下私有头 `llvm_jit_context.hpp`**） | `core.cpp:204-235`；`nv_attach_impl_patcher.cpp:2-3`（include 但未用） |
| runtime map/helper：`bpftime_map_lookup_elem/update/delete`、`bpftime_trace_printk` | `bpf_attach_ctx_cuda.cpp:140,202,217,231` |
| shm：`shm_holder.global_shared_memory.get_manager/get_open_type/set_enable_mock/get_cuda_comm_shared_mem` | `bpf_attach_ctx_cuda.cpp:335-337,386`；`map_handler.cpp:952-1092`；各 GPU map 的 `try_initialize_*` |
| CUDA driver/runtime、nvPTXCompiler（静态库，nv_attach_impl 与 ptx_compiler .so 都链） | 全模块；`cmake/cuda.cmake:59-66` |
| OpenSSL `SHA256` | `nv_attach_utils.cpp:81-92`；CMake :80-83 |
| boost asio thread_pool / boost process（仅非 Linux） / boost demangle | `nv_attach_impl.cpp:15-24,895,845-868,1304`；`nv_attach_fatbin_record.cpp:172` |
| nlohmann json（`third_party/json.hpp`） | `core.hpp:10`、`nv_attach_impl.cpp:548,998` |
| libbpf（`bpf_map_get_fd_by_id` 等） | `array_map_kernel_gpu.cpp:49,57,151,184,211`（该文件在 CUDA 构建中无条件编译，未见 `BPFTIME_BUILD_WITH_LIBBPF` 保护——需构建读者确认） |
| 外部可执行文件 `cuobjdump`、`/bin/sh` | `nv_attach_impl.cpp:707-844`；`cli/main.cpp:603-740` |
| 可选 `libgdrapi.so`（dlopen） | `nv_gpu_gdrcopy.cpp:30-122` |

---

## 5. 配置项、环境变量、编译开关

### 5.1 CMake / 编译期
| 名字 | 默认 | 定义 | 作用 |
|---|---|---|---|
| `BPFTIME_ENABLE_CUDA_ATTACH` | OFF | `cmake/StandardSettings.cmake:25`；`CMakeLists.txt:169-171` 全局宏；`runtime/CMakeLists.txt:91-104,177-189` 再次 PUBLIC 定义 | 构建 nv_attach_impl、GPU map、watcher、agent/server/tool 中所有 `#ifdef` 代码 |
| `BPFTIME_CUDA_ROOT` | ""（但 `option()` 使其成为 BOOL，缓存里显示 `BOOL=OFF`） | `StandardSettings.cmake:24`；`cuda.cmake:5-7` 未设则 FATAL | CUDA 根目录；库路径 `targets/<x86_64|aarch64>-linux/lib(/stubs)` 与 `extras/CUPTI/lib64` |
| `BPFTIME_ENABLE_GDRCOPY` | 未定义 | 只在 `CMakeLists.txt:173-176` 用作 `-D` 变量（**没有 option() 声明**） | 编译 GDRCopy 真实实现，否则 `nv_gpu_gdrcopy.cpp:354-376` 是空桩 |
| `BPFTIME_HAVE_GDRAPI_H` | 自动 | `nv_gpu_gdrcopy_abi.hpp:38-41`（`__has_include`） | 用系统 gdrapi.h 类型 |
| `BPFTIME_GPU_HELPER_MAX_BUF` | `1<<20` | `bpf_attach_ctx.hpp:39-45` | CommSharedMem 布局；改了必须重生成 trampoline_ptx.h（注释自述） |
| `CUDA_VERSION >= 13000` | — | `nv_attach_impl.cpp:1220-1224`；`map_handler.cpp:955-961` 等 | `cuCtxCreate` 签名差异 |
| `BPFTIME_ENABLE_ROCM_ATTACH` | 不存在 | 只在 `map_handler.cpp:1288,1329` 的 `#if` | 占位 |
| `ptx_pass_config.h`（生成） | 构建路径 | `nv_attach_impl/CMakeLists.txt:71-72` | 默认 pass .so 与 ptx_compiler .so 的**绝对构建路径**（安装后若 build 目录不存在则需 env 覆盖 pass；compiler 路径无 env 可改） |
| `LLVMBPF_ENABLE_PTX` / `LLVMBPF_ENABLE_SPIRV` / `LLVMBPF_CUDA_PATH` | OFF/OFF/— | `vm/llvm-jit/CMakeLists.txt:190-199`；`example/ptx/CMakeLists.txt:8-12` | 只控制 llvmbpf 示例；NVPTX 后端总是链入（`vm/llvm-jit/CMakeLists.txt:85-87`） |
| trampoline 生成 | `CLANG=clang++-17`、`PTX_TARGET_ARCH=sm_61` | `trampoline/Makefile:1-4` | 手工执行 |

依赖：CUDA toolkit（`cuda`、`cudart`、`nvptxcompiler_static`、另链 `cupti`/`nvrtc` 但**代码未使用**，`nvml.h` 只被 include，`cuda.cmake:62`、`nv_attach_impl.hpp:17`）；Frida gum；LLVM（含 NVPTX）；OpenSSL；Boost；运行时需要 `cuobjdump`（除非 late PTX dir）；目标程序需 **`-cudart shared`**（示例 Makefile 如此，`example/gpu/cuda-counter/Makefile:73`；静态 cudart 时 `dlsym(RTLD_NEXT,"__cudaRegisterFatBinary")` 和导出符号查找大概率找不到——「推断，未验证」）且 fatbin 内含 PTX。

### 5.2 运行时环境变量
| 变量 | 默认 | 读取位置 | 影响 |
|---|---|---|---|
| `BPFTIME_PTXPASS_LIBRARIES` | 构建路径列表 | `nv_attach_impl.cpp:507-518` | 冒号分隔的 pass .so 列表 |
| `BPFTIME_CUDA_LATE_PTX_DIR` | 无 | `nv_attach_impl.cpp:653`；由 agent 注入参数 `cuda_late_ptx_dir` 设置（`agent.cpp:787-791`） | 直接读目录里的 .ptx，不跑 cuobjdump |
| `BPFTIME_CUDA_DISABLE_CUOBJDUMP` | 无 | `nv_attach_impl.cpp:684`；注入参数 `cuda_disable_cuobjdump`（`agent.cpp:792-797`） | `=1` 时不抽 PTX（等于不插桩） |
| `BPFTIME_CUOBJDUMP` | 无 | `nv_attach_impl.cpp:715` | cuobjdump 路径/名字 |
| `BPFTIME_CUDA_ROOT`、`CUDA_HOME`、`CUDA_PATH`、`LLVMBPF_CUDA_PATH`、`CUDAToolkit_ROOT` | — | `nv_attach_impl.cpp:734-739`；`cli/main.cpp:572-576`；之后扫 `/usr/local/cuda*` | **只用于找 cuobjdump** |
| `BPFTIME_CUDA_DEVICE` | 0 | `nv_attach_impl.cpp:1730`；`frida_setup.cpp:121`；`nv_gpu_shared_array_map.cpp:266` | 选择建/取 primary context 的设备（README 未记载） |
| `BPFTIME_SM_ARCH` | 自动探测；失败回落 `sm_61` | `nv_attach_utils.cpp:97`；探测后 **`setenv` 回写**（:156） | 重编译目标架构 |
| `BPFTIME_MAP_GPU_THREAD_COUNT` | `attr.gpu_thread_count`=1024 | `map_handler.cpp:752-757`（仅 1502 与 1527 使用） | per-thread map/ringbuf 的线程容量 |
| `BPFTIME_GPU_ARRAY_GDRCOPY` | 0 | `bpftime_config.cpp:165-168` | `=1` 开启 GDRCopy |
| `BPFTIME_GPU_ARRAY_GDRCOPY_MAX_PER_KEY_BYTES` | 4096（`bpftime_config.hpp:26`），0=不限 | `bpftime_config.cpp:169-174`；`nv_gpu_gdrcopy.cpp:154-156` | 超过阈值走 cuMemcpy |
| `BPFTIME_RUN_WITH_KERNEL` / `BPFTIME_NOT_LOAD_PATTERN` | — | syscall-server（非本块） | 内核共存模式下 `cuda_` 前缀 kprobe 的特殊处理（`syscall_context.cpp:855-876`） |
| 文档提到但**代码不读**：`BPFTIME_PTXPASS_DIR`、`PTX_ATTACH_POINT` | — | `README-passes.md:41,55`；`README.md:199` | 无 |

---

## 6. 并发、生命周期与错误处理

- **锁/原子**：`cuda_symbol_map_mutex`（patched_kernel_by_name 等）、`late_bootstrap_mutex`、`host_symbol_cache_mutex`、`patched_global_cache_mutex`、`launch_event_mutex`；
  hook state 全部 `std::atomic` + acquire/release。**但 `hook_entries`、`fatbin_records`、`symbol_address_to_fatbin`、`current_fatbin` 无锁**：前者在 attach/detach（agent 线程、IPC/auto-refresh 线程）改，
  后者在应用线程的 launch/register 钩子里读——并发 detach 与 launch 时有数据竞争可能（推断）。
- **thread_pool 并行**：`hack_fatbin` 与 `compile_ptxs` 都按 `hardware_concurrency()` 建 boost thread_pool（`nv_attach_impl.cpp:895`、`nv_attach_fatbin_record.cpp:172`）。
  `compile_ptxs` 的 lambda 里 `throw std::runtime_error`（:144-145,164-165）——boost::asio thread_pool 中处理器抛异常会从工作线程的 `run()` 传出，
  大概率导致 `std::terminate`（推断，未验证）；而且 `ptx_pool->find/insert` 未加锁（:131,180-181，insert 在 `map_lock` 内但 find 不在）。
- **GPU→CPU RPC 同步**：GPU 侧设备级自旋锁 `__bpftime_comm_lock` + warp 内按 lane 串行（`default_trampoline.cu:131-173`）；CPU 侧单 watcher 线程 1ms 轮询（`bpf_attach_ctx_cuda.cpp:275-276`）。
  所以每次需要 CPU 的 helper 调用至少有毫秒级延迟，且**全设备串行**。fence：GPU `membar.sys`，CPU `atomic_thread_fence(seq_cst)`。
- **跨进程**：GPU array map 的显存属于 syscall-server/loader（`cuMemAlloc`+IPC 导出），agent 打开；`~nv_gpu_per_thread_array_map_impl` 只在 `SHM_REMOVE_AND_CREATE` 端 `cuMemFree`（`nv_gpu_per_thread_array_map.cpp:177-191`）。
  loader 先退出 → 目标里补丁 kernel 访问已释放显存。为此 detach 等 event（2s 超时，`nv_attach_impl.cpp:146`）、`bpftime trace` 先 detach 后停 loader（`cli/main.cpp:1190-1240`），agent detach 注释同（`agent.cpp:437-438`）。
- **会话切换**：shm epoch 变化时 attach ctx 拆全部 link → `detach_by_id` 最后一项 → `clear_patched_state_for_next_session` + 重置 bootstrap，使新会话能绑定新的 shm/map 布局（`nv_attach_impl.cpp:1531-1567,1945-1951`）。
  注意：`enabled=false` 后补丁模块被卸载，但**已装的 Frida 钩子永不卸载 replace**（析构只 `gum_interceptor_detach(listener)`，`nv_attach_impl.cpp:569-584`）；`active_impl` 置空后 replacement 走原函数。
- **fork**：未见 fork 处理。抽 PTX 刻意用 posix_spawn 避免在 CUDA 已初始化进程里 fork（`nv_attach_impl.cpp:773-775`）。
- **失败行为（fail-open 与否）**：
  - 补丁 launch 失败 → 回落原 launch（`frida_setup.cpp:186-189,227-231`）——fail-open。
  - 找不到 pass 匹配 → attach 返回 -1（`nv_attach_impl.cpp:231-236`），CUDA link 不建立。
  - pass 处理失败/编 eBPF 失败 → 该 PTX 文件从 `ptx_out` 缺失（lambda `return`，:995,1025）→ 不加载其模块 → 该文件里的 kernel 走原路径。
  - **但** `try_loading_ptxs` 在 `shared_mem_ptr==0`、模块加载失败、取 `constData/map_info` 失败时 **throw**（`nv_attach_fatbin_record.cpp:194-197,237-238,244-272`），它运行在 Frida 的 C 回调（`__cudaRegisterFunction` 的 on_enter）里——异常穿过 C 栈帧，大概率 terminate 宿主（推断）；
    `*hack_fatbin(...)` 对可能为 `nullopt` 的 optional 直接解引用（:200；`hack_fatbin` 仅在 debug 下 mkdtemp 失败才返回 nullopt）。
  - 构造函数 `*load_nv_attach_impl_ptx_compiler(...)`：dlopen 失败返回空 optional，**仍被解引用**（`nv_attach_impl.cpp:558`）→ UB（确认，读码）。
  - Frida 取不到 interceptor/listener 或 attach 失败 → 构造函数 throw（:300-309,335-339）；agent 注册处被 try 包住走 `init_fail`。
  - watcher 未知 request id 只 WARN 并照常置 flag2（不会卡死 GPU）。
  - `bpftime_shm::~bpftime_shm` 用 `fprintf(stderr, …)` 报告 unregister（`bpftime_shm_internal.cpp:1278-1282`）——与 CLAUDE.md "不要写宿主 stderr" 的契约冲突（该析构也在 agent 中运行）。
  - `get_gpu_sm_arch` 会 `setenv("BPFTIME_SM_ARCH")`，改变宿主进程环境（`nv_attach_utils.cpp:156`）。

---

## 7. 不直观的行为、疑似 bug、代码与注释不一致

1. **GPU→CPU RPC 的请求参数在拿锁之前写入共享区**（确认，读码）：`_bpf_helper_ext_0001/0002/0003/0006/0501` 先 `simple_memcpy(&req…)`，再调 `make_helper_call` 才 `spin_lock`
   （`default_trampoline.cu:241-245,298-305,317-320,330-341,416-427`）。多线程并发调用普通 map 的 lookup/update 时，key/value 会被别的线程覆盖，CPU 处理的可能是别人的 key。
   `example/gpu/README.md:290-296` 宣称"先拿锁再写参数"，与实现相反。
2. **`default_trampoline.cu` 与提交的 `trampoline_ptx.h` 不同步**（确认）：.cu 里 `GPU_HELPER_MAX_BUF = 1 << 24`（:21），而 host 侧是 `1 << 20`（`bpf_attach_ctx.hpp:43`），
   生成物里 resp 偏移 2097184 对应 1<<20（`trampoline_ptx.h:115`）。按 Makefile 重新生成会让 GPU/CPU 对 `CommSharedMem` 布局不一致。
   另：`trampoline/README.md` 说 sm_60，Makefile 用 sm_61，生成物 `.target sm_61`。
3. **per-thread map 线程数越界只打印不拦截**（确认）：`array_map_offset` 在 `tid >= max_thread_count` 时 `printf` 警告后仍返回越界地址（`default_trampoline.cu:189-199`），
   默认容量仅 1024（`bpftime_shm.hpp:45`），任何超过 1024 线程的 kernel 用 1502/1512 都会越界写显存/host 内存。ringbuf（0025）则正确返回 1（:366-370）。
   `getGlobalThreadId` 用 `int` 计算 x/y/z（:180-188），大 grid 下溢出（推断）。
4. **ringbuf 写入不检查 `data_size <= value_size`**（确认，`default_trampoline.cu:398-401`），会写进下一个槽；`drain_data` 遇到任一线程 `dirty` 就整体返回 0（`nv_gpu_ringbuf_map.cpp:64-66`），
   且先 `head++` 再回调读数据（:70-76），回调期间生产者可覆盖该槽（推断）。`elem_*` 设 `errno = -ENOTSUP`（负号，:90-112）。GPU 侧 `dirty=0` 之前没有 `membar.sys`，CPU 可能先看到 tail/dirty 后看到数据（推断）。
5. **GPU_HASH_MAP 桶数大于 value 数组容量**（确认）：桶数 `next_prime(max_entries)`（`nv_gpu_shared_hash_map.cpp:19`，`bpftime_hash_map.hpp:32-38` 返回 ≥n），但 value 存在大小为 `max_entries` 的 array 里，
   用桶下标当 array key（:58,86,99）→ 落到下标 ≥ max_entries 的键插入失败（E2BIG）。另：插入新键时把用户 `flags` 透传给 array（array 对 `BPF_NOEXIST` 恒 EEXIST，`nv_gpu_shared_array_map.cpp:133-136`），
   所以 `BPF_NOEXIST` 插新键必失败；删除直接 `set_empty`（:123）破坏线性探测链；GPU 侧 trampoline 对 1501 **没有快速路径**，全部走 CPU RPC；
   lookup 结果在单一 shm 暂存区 `value_buffer`，并发互相覆盖；头文件注释说 array 会被 handler 加锁而 `should_lock=false`（`.hpp:26-27,68-70`），实际靠 `PTHREAD_PROCESS_PRIVATE` 的 spinlock（放在 shm 里跨进程用，推断不可靠）。
6. **`llvmbpf_vm::generate_ptx(target_cpu)` 的 target 参数被吞**（确认，已实跑）：`vm.cpp:350-353` 调 `jit_ctx->generate_ptx(target_cpu)`，匹配到
   `generate_ptx(bool main_with_arguments, const std::string &func_name="bpf_main", const char *target_cpu="sm_60")`（`llvm_jit_context.hpp:67-69`），`const char*`→`bool` 隐式转换，
   于是**永远生成 sm_60**。`vm_test.cpp:508` 恰好用 sm_60 未暴露。bpftime 主线不受影响（`core.cpp:228` 直接调内部 ctx 且参数齐全），compat 层 `compat_llvm.cpp:74` 受影响但无调用者。
7. **helper 506 在 GPU 上不可用**（确认）：trampoline 定义了 `_bpf_helper_ext_0506`（membar），但 `compile_ebpf_to_ptx_from_words` 没注册 506（`core.cpp:205-223`），
   llvmbpf 遇未注册 helper 报错 → pass 失败。`example/gpu/README.md:275` 把 506 列为可用 helper。除表中 17 个 helper 外，其他 helper（如 `bpf_ktime_get_ns`、tail call）在 GPU 程序中都会导致编译失败。
8. **所有 PTX 都被重编译并替换 launch**（确认）：`compile_ptxs`/`try_loading_ptxs` 对未修改的 PTX 也编译加载（`nv_attach_fatbin_record.cpp:119-188,204-279`），
   `__cudaRegisterFunction` 只要在新模块里找到同名 kernel 就重定向——即**一旦有 CUDA link，fatbin 中所有 kernel 都改由 bpftime 从 PTX JIT 的副本运行**（失去 nvcc 预编 SASS 的优化，行为差异可能出现）。
9. **插桩前把 eBPF 编成 sm_61，再整体改 target**：三个 pass 都硬编码 `"sm_61"`（kprobe_entry:55，kretprobe:43，memcapture:141），合并后保留 trampoline 的 `.version 8.1 / .target sm_61`，
   原 PTX 的 `.version` 被 `filter_out_version_headers_ptx` 删掉（`core.cpp:165-190`），最后 `rewrite_ptx_target` 只改 target，且仅对 sm≥100 改 version（`nv_attach_utils.cpp:24-60`）。
   若应用 PTX 使用了 ISA 8.1 之后的特性，合并后可能编译失败（推断）。`rewrite_ptx_target` 中 sm_103→8.8 而 sm_120→8.7，顺序反直觉（可能是刻意对应各架构首个支持的 ISA，未核实）。
10. **register guard 默认关闭**：kprobe_entry 只有 `save_strategy=="full"` 才加（`ptxpass_kprobe_entry/main.cpp:119`），而 `get_default_config` 设 `"minimal"`（:36），配置文件不被读取，没有途径改成 full；
    kretprobe/memcapture 固定 false。且该 guard 保存的是 **eBPF 函数自己声明的寄存器**（`register_guard_patcher.cpp:198-318`），PTX 虚拟寄存器本就按函数作用域隔离，作用存疑（推断）。README 把它当主要机制描述。
11. **memcapture 在 ld/st 之后（不是之前）插 call，且不限定 kernel**：`out_body << line` 后才 `out_body << "call …"`（`ptxpass_kprobe_memcapture/main.cpp:55-148`），对整份 PTX 的所有函数生效；
    计数器从 1 开始，若同一 PTX 被多个 hook_entry/kernel 处理两次会产生重名 `__memcapture__1`（推断）。同理两个 kprobe 程序挂同一 kernel 会生成两个同名 `__probe_func__<k>`（推断，编译失败）。
12. **GPU 程序没有 ctx**：`compile_ebpf_to_ptx_from_words(..., with_arguments=false)`，生成的函数无参数（`compiler.cpp:356-361`），r1/r2 未初始化；GPU 探针**看不到 kernel 实参**。
    每线程 eBPF 栈在 local memory 中 16464 字节（`(STACK_SIZE*MAX_LOCAL_FUNC_DEPTH+10)*8` 加调用栈，见 §9 实验输出）。
13. **`create_map_basic_info` 越界写先于检查**（确认）：`local_basic_info[i]` 在 :340 取引用，`i >= size` 的检查在 :350；handler 表默认 6144 项（`bpftime_config.hpp:23`），若 map fd ≥1024 → vector 越界。
    GPU 侧 `map_info[fd]` 同样无界检查（`default_trampoline.cu:207`）。
14. **ptx_compiler 加载失败仍解引用**（确认）：`nv_attach_impl.cpp:558`。
15. **`elem_update` 在 agent 侧用 server 的设备指针**（确认）：`nv_gpu_per_thread_array_map_impl::elem_update` 用 `server_gpu_shared_mem`（`nv_gpu_per_thread_array_map.cpp:135`），
    在 agent（`SHM_OPEN_ONLY`）里那是另一进程的显存地址（CPU 侧 eBPF 程序或 watcher 调用 update 时出错，推断）。`map_get_next_key` 把局部指针 `next_key = 0`（:165，HOST 版 `nv_gpu_array_host_map.cpp:135`、`nv_gpu_shared_array_host_map.cpp:90` 同）而非写值。
16. **`BPFTIME_MAP_GPU_THREAD_COUNT` 与用户态 value 大小不一致**（确认读码，影响推断）：1502 实现按 env 的线程数分配并按 `entry_size=thread_count*value_size` 拷贝（`nv_gpu_per_thread_array_map.cpp:54,103-110`），
    而 handler 报给用户态的 value 大小用 `attr.gpu_thread_count`（恒 1024，`map_handler.cpp:78`）。设了 env 后用户 buffer 与 map 实际大小不符（update 时 `cuMemcpyHtoD` 读 entry_size 字节可越过用户缓冲）。
    1512 HOST 版完全不读 env（`map_handler.cpp:1098-1104`），与 README"Override for all GPU maps"不符。
17. **shm 中存放进程私有句柄**：`nv_gpu_shared_array_map_impl` 的 `agent_cuda_context`/`agent_stream`/`owner_cuda_context`（`.hpp:24-29`）、`array_map_kernel_gpu_impl` 的 `map_fd`/`mmap_ptr`（`.hpp:29,33`）都是对象字段而对象在 shm 里——
    多个 agent 进程或 server/agent 两侧会互相覆盖（`agent_gpu_shared_mem` 按 pid 区分，但这些字段没有）。`array_map_kernel_gpu.cpp:239-241` 的 static `registered_host_memory` 只记最后一个 map，`atexit` 每次注册一次。
18. **每个补丁模块各有一把 `__bpftime_comm_lock`**（推断）：trampoline 拼进每个被修改的 PTX 文件，多个模块的 kernel 并发调用 RPC 时互不互斥却共用一个 `CommSharedMem`。
19. **`start_late_bootstrap_async` 是同步的**（名不副实，注释承认，`nv_attach_impl.cpp:1961-1963`）；在 LD_PRELOAD 场景下它在 attach 时就会扫描主程序 ELF 并抽 PTX/编译一遍，之后 `__cudaRegisterFatBinary` 路径再处理一遍（依赖 sha256 缓存避免重复编译，推断）。
20. **`register_custom_helpers` 为空**，头文件注释却说要注册 `_bpf_helper_ext_0502/0503`（`nv_attach_impl.hpp:150-154` vs `nv_attach_impl.cpp:155-158`）。类注释"Attach implementation of syscall trace"是复制残留（`.hpp:142-144`）。
21. **cuda 程序识别靠名字前缀 `cuda__`**（`bpftime_prog.hpp:59-62`）；BPF_PROG_LOAD 的 `prog_name` 只有 16 字节，`cuda__` 占 6 个，若用 libbpf 名字截断需注意（推断）。`unit-test/cuda/test_cuda_compile.cpp:75-76` 名为 `test_cuda_prog__cuda`，并不被视为 cuda 程序，测试实际走 CPU JIT。
    `bpftime_prog.cpp:155-162` 的 if/else 两支完全相同，日志却说"Do not set lddw helpers due to cuda program"。
22. **只 hook 了 `cudaLaunchKernel(_ptsz)`/`cuLaunchKernel`/graph 节点 API**：`cudaLaunchKernelExC`、`cuLaunchKernelEx`、`cudaLaunchCooperativeKernel` 等未处理（推断：这类 launch 不会被插桩）；stream capture 中的 `cudaLaunchKernel` 直接走原函数（`frida_setup.cpp:109-111`）。
    镜像只覆盖 `cudaMemcpyTo/FromSymbol(Async)`，`cudaGetSymbolAddress` 等取得的原模块全局地址与补丁 kernel 看到的副本不一致（推断）。
23. **死代码/过时**：`memory_utils.hpp`、`demo_ptx_prog.hpp`、`test_kernel_finder.cpp`、`add_semicolon_for_variable_lines`、`patch_main_from_func_to_entry`（仅注释引用）、`compile_ptx_to_elf`（`bpftime_prog.cpp:42-103`，无调用）、
    `configs/ptxpass/kprobe_entry.json`、`nv_attach_entry::parameters/extras`、`CommSharedMem::occupy_flag/time_sum`、CUPTI/nvrtc 链接。
24. **cmake**：`BPFTIME_CUDA_ROOT` 被声明为 `option()`（BOOL，`StandardSettings.cmake:24`）；`BPFTIME_ENABLE_GDRCOPY` 无 option 声明。`ptx_pass_config.h` 写死**构建目录绝对路径**，安装后若删掉 build 目录，nv_attach_impl 找不到 pass/compiler（compiler 路径无 env 覆盖，推断）。
25. `scan_fatbin_wrappers` 注释说 magic 是 'FbC1'，数值 `0x466243b1` 并不是这四个 ASCII（`nv_elf_introspect.cpp:251-252`）——数值是 CUDA 的 `FATBINC_MAGIC`，注释不准。

---

## 8. 对照现有指南

对照文件：`00-architecture.md`、`01-lifecycle.md`、`02-runtime-shm.md`、`03-agent-syscall-server.md`、`04-attach-frida.md`、`05-vm-llvm-jit.md`、`06-maps-helpers.md`、`07-periphery.md`（grep GPU/CUDA/nv/ptx）。
现有指南**几乎没有 GPU 章节**，只在 10 余处一笔带过。

**错误/不准确**
- `05-vm-llvm-jit.md:734-735`："读 `compiler.cpp` 遇到 `kernel_compatible_mode` / `is_gpu` 分支，按'bpftime 主路径不走'跳过即可"——对 `is_gpu` 不准确：
  CUDA 构建下 bpftime 的 GPU 路径**就走** `is_gpu=true`（`ptxpass/core.cpp:228` → `llvm_jit_context.cpp:934-936` 传 `is_gpu=true`）。应改为"CPU 主路径不走；CUDA attach 走"。
- `05-vm-llvm-jit.md:731`：把 "SPIR-V 输出" 列为"bpftime 未接入的新接口"——正确；但同段若暗示 PTX 也未接入则不对：PTX 生成虽绕过 compat 层，却是 GPU 功能的核心依赖（通过 llvmbpf 内部头直连）。
- 其余提及（`01-lifecycle.md:987`、`02-runtime-shm.md:710,777,799`、`03-agent-syscall-server.md:235-244,727,823-832`、`04-attach-frida.md:803,831`、`07-periphery.md:556,559`）与源码核对一致，未发现错误。
  `04-attach-frida.md:831`"nv_attach_impl 占 attach 目录过半代码"：实测 nv_attach_impl 源码 10618 行 / attach 全部源码约 18618 行 ≈ 57%（含生成的 1879 行 trampoline_ptx.h），成立。

**遗漏（建议新增一章"GPU/CUDA 支持概览"，至少覆盖）**
1. 整体架构：agent 内 Frida hook CUDA 注册/launch API → cuobjdump 抽 PTX → pass(.so, dlmopen) 用 llvmbpf 把 eBPF 编成 PTX 并插到 entry/ret/ld-st → 拼 trampoline → nvPTXCompiler 重编 → 新模块 → launch 重定向（§3.2-3.5）。
2. "用户态 kprobe = CUDA kernel 探针"、程序名必须 `cuda__` 前缀、`SEC("kprobe/<mangled kernel>")`、逗号分隔多 kernel、`__memcapture` 特殊名、stub 式挂载（`__bpftime_cuda__kernel_trace`）。
3. GPU 与 CPU 的三条数据通道：GPU map（IPC 显存 / HOST 映射）、整段 shm `cudaHostRegister` 使普通 map 值可被 GPU 直接访问、CommSharedMem RPC + watcher 线程（含 1ms 轮询与全设备串行的性能含义）。
4. GPU map 类型表与内存布局（§2.8），以及"map 在 loader 进程分配、agent 打开 IPC"带来的退出顺序约束。
5. GPU helper 集合（1/2/3/6/14/25/501-505/507-511；506 不可用）与"其他 helper 会导致编译失败"、无 ctx、每线程 16KB 栈。
6. 编译开关/依赖/环境变量（§5），特别是运行时需要 cuobjdump、目标需带 PTX 且 `-cudart shared`、`BPFTIME_SM_ARCH`、`BPFTIME_MAP_GPU_THREAD_COUNT`。
7. late attach（`bpftime trace` 预抽 PTX）与 `bpftimetool run-on-cuda`。
8. trampoline_ptx.h 是手工生成并提交的文件，与 .cu 源码当前不同步（§7-2）。
9. llvmbpf 的 `generate_ptx` 公共 API bug（§7-6）可放在 vm 章"llvmbpf 新接口"处顺带提。

**讲得好、可以保留**
- `03-agent-syscall-server.md:235-244,823-832` 对 `initializing_cuda` 闸门、`mock_setter` 与 CUDA map 构造期间关闭拦截的解释准确，可直接引用到 GPU 章（对应 `map_handler.cpp:952,973` 的 `set_enable_mock`）。
- `02-runtime-shm.md:710,799` 对 `cuda_comm_shared_mem` 命名对象与"unmap 前先停 watcher"的描述准确。
- `01-lifecycle.md:987` / `02-runtime-shm.md:777` "第二遍扫表专处理 CUDA link（依赖 map 基本信息）"准确（`bpf_attach_ctx.cpp:120-136`）。
- `07-periphery.md:556` 对 `bpftime trace` 的 CUDA late-PTX 编排点到为止，正确。

另：仓库内文档（非现有指南）问题，供写作者避免照抄：`attach/nv_attach_impl/README.md` 称"用 NVCC 重编译"（实为 nvPTXCompiler 库）、"在 ld/st 之前注入"（实为之后）、`BPFTIME_CUDA_ROOT` 影响重编译（只影响找 cuobjdump）、`BPFTIME_MAP_GPU_THREAD_COUNT` 覆盖所有 GPU map（1512 不读）、`PTX_ATTACH_POINT`；
`README-passes.md` 整篇描述旧的"stdin/stdout 可执行文件 + `BPFTIME_PTXPASS_DIR` + `/tmp/bpftime-recompile-nvcc/out.fatbin`"形态，均已不存在；
`example/gpu/README.md:240-260` 称 ringbuf 在 GPU 显存（实为 host shm）、实现在 `nv_gpu_array_map.cpp`（文件不存在）、行号引用 `nv_attach_impl.hpp:33-34`（实为 :43-44）；`vm/README.md` 提到的 `ptx_code_gen.cpp`、`do_get_ptx()` 不存在。

---

## 9. 学习建议

**推荐阅读顺序**
1. `example/gpu/cuda-counter/cuda_probe.bpf.c`（看用户写法：`cuda__` 名、kprobe/kretprobe、501-505 helper）→ `example/gpu/README.md:215-300`（概念，注意 §8 列出的错误）。
2. `runtime/include/bpftime_shm.hpp:122-132`（map 类型）→ `runtime/include/bpf_attach_ctx.hpp:36-143`（CommSharedMem）。
3. `attach/nv_attach_impl/trampoline/default_trampoline.cu:69-515`（GPU 侧 helper；先看 `make_helper_call`、`_bpf_helper_ext_0001`、`0025`）。
4. `runtime/src/attach/bpf_attach_ctx_cuda.cpp:111-289`（CPU 侧 watcher）、`:322-369`（map_info）。
5. `runtime/src/attach/bpf_attach_ctx.cpp:120-136,339-377` → `nv_attach_impl.cpp:160-237`（attach 建立）。
6. `nv_attach_impl.cpp:279-562`（构造：钩哪些 API）→ `nv_attach_impl_frida_setup.cpp:248-389`（注册钩子）、`:92-246`（launch 重定向）。
7. `nv_attach_fatbin_record.cpp:190-280` → `nv_attach_impl.cpp:889-1068`（hack_fatbin）→ `pass/ptxpass_core/src/core.cpp:196-244` → `pass/ptxpass_kprobe_entry/main.cpp`、`ptxpass_kretprobe/main.cpp`。
8. `vm/llvm-jit/src/llvm_jit_context.cpp:882-972` + `compiler.cpp` 中 `is_gpu` 分支。
9. GPU map：`nv_gpu_per_thread_array_map.cpp` → `nv_gpu_ringbuf_map.cpp` → `map_handler.cpp:949-1121,1286-1370`。
10. 选读：late attach（`nv_attach_impl.cpp:1574-1981`、`cli/main.cpp:603-740`）、graph 钩子、symbol 镜像、GDRCopy。

**动手实验（无 GPU 也可做）**
- 「已实跑」实验 1：llvmbpf 公共 API 的 target 被忽略。源码 `/tmp/claude-1000/-home-y-src/0f93419b-e1c5-4e3e-9f75-4df37a800a83/scratchpad/R6/ptx_target.cpp`，
  用 `g++-13 -std=c++20 -c … -I vm/llvm-jit/include -I vm/llvm-jit/src -I third_party/spdlog/include -DSPDLOG_COMPILED_LIB`，链接
  `build-jit/vm/compat/llvm-vm/libllvmbpf_vm.a build-jit/third_party/spdlog/libspdlog.a $(llvm-config-19 --ldflags --libs all) -Wl,--allow-shlib-undefined`
  （必须用 g++-13，因为静态库是 gcc-13 LTO 产物）。输出：`requested sm_80 -> .target sm_60`。
- 「已实跑」实验 2：模拟 kprobe_entry pass 的 eBPF→PTX（`ptx_probe.cpp`，同目录同编译方式），程序 `lddw r1, map_fd=3; *(u32*)(r10-8)=0; r2=r10-8; call 1; exit`，
  调 `llvm_bpf_jit_context::generate_ptx(false, "__probe_func__vecadd", "sm_61")`。关键输出：
  `.visible .func __probe_func__vecadd()`（无参数）、`.local .align 8 .b8 __local_depot0[16464];`（每线程 eBPF 栈）、
  `.extern .func (.param .b64 func_retval0) _bpf_helper_ext_0001 (...)`（由 trampoline 提供实现）、`st.param.b64 [param0+0], 3;`（**map 参数就是 fd 3**，对应 `map_info[3]`）。
  这直观展示了"eBPF helper → `_bpf_helper_ext_NNNN` → trampoline"与"map fd 直传"两点。
- 读 `trampoline_ptx.h:62-126` 的 `make_helper_call`，找到 `[%rd1+2097184]`，自己算 `24 + 2*(1<<20) + 8`，理解布局常量为何"牵一发动全身"（§7-2）。
- 有 GPU 时（未验证）：`example/gpu/cuda-counter` 的 README 流程；设 `SPDLOG_LEVEL=debug` 让 `hack_fatbin` 把补丁 PTX 写到 `/tmp/bpftime-fatbin-work.*`（`nv_attach_impl.cpp:1051-1066`），
  可看到 trampoline + `__probe_func__…` + kernel 内的 `call`。

**学习者最容易误解的点**
1. **以为 eBPF 在 CPU 上被"转发"执行**——不是：探针体被编译成 PTX 在每个 GPU 线程里执行；只有部分 helper（普通 map 操作、printk、pid）才回 CPU，且很慢。
2. **以为 bpftime 原地改了应用的 kernel**——不是：它加载了一个**新模块**并把 launch 重定向过去；全局变量有两份，需要 memcpyToSymbol 镜像；并且一旦启用，fatbin 中所有 kernel 都从重编译副本运行。
3. **"kprobe" 在这里与内核无关**：bpftime 用户态 kprobe（type 8/9）= CUDA kernel 入口/出口；目标名是 mangled 的 device kernel 名。
4. **GPU map 的数据在哪**：1502/1503/1501 在显存（属于 loader 进程，IPC 共享）；1527 ringbuf、1512/1513 在 host shm；普通 map 也在 shm，GPU 通过整段映射访问——不要把 ringbuf 想成显存。
5. **map 在 GPU 上的"指针"其实是 fd 下标**：GPU 侧不做 LDDW 重定位，helper 第一个参数是 fd，用来索引常量内存 `map_info[1024]`。

## 10. 未读完/未确定的地方

- 所有 GPU 运行时行为（钩子是否命中、IPC/映射是否成功、性能数字）均**未在 GPU 上验证**；§7 中标「推断」的条目需有 GPU 的读者复核，尤其：1（参数竞态的实际影响）、4、11、17、18、19、22。
- `trampoline_ptx.h` 未逐行读，只核对了全局符号、函数列表、map 类型常量与 `make_helper_call` 的偏移；未逐函数比对与 .cu 的其余差异。
- `test_kernel_finder.cpp` 中 1270 行的 PTX 样例文本未读（文件已不编译）。`vm/llvm-jit/example/ptx|spirv` 的 README 与 .s 只略读；`example/gpu/*` 的 .c/.cu 与 benchmark 脚本只略读。
- `vm/llvm-jit/src/compiler.cpp` 只读了 `is_gpu` 相关分支与 LDDW 处理，未完整审查 GPU 下原子指令、local function call 在 PTX 中的正确性。
- 需要别的读者确认的跨模块点：
  - attach/runtime 读者：`bpf_attach_ctx` 的 epoch 重绑与 `detach_by_id` 在多线程（auto-refresh / IPC refresh 线程 vs 应用 launch 线程）下对 `hook_entries` 的竞态；`base_attach_impl::allocate_id` 语义。
  - maps 读者：handler 层对 `should_lock` 的使用方式（GPU hash map 的 `should_lock=false` 是否真的不加锁）；`get_userspace_value_size` 在 syscall lookup/update 拷贝路径中的实际用法（决定 §7-16 是否真越界）。
  - 构建读者：`array_map_kernel_gpu.cpp` 无条件 `#include <bpf/libbpf.h>`，在 `BPFTIME_BUILD_WITH_LIBBPF=OFF` 且 CUDA ON 时能否编译/链接。
  - vm 读者：llvmbpf 未注册 helper 时的报错路径（`compiler_utils.cpp:312-` 之后的 else 分支）确认"GPU 程序用了其他 helper 即编译失败"。
