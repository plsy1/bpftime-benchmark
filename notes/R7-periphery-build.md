# R7 外围组件与构建系统（periphery-build）源码笔记

基线：`177a1dc465ff`，仓库根 `/home/y/src/bpftime-wt-official-177a1dc`。下文路径都相对仓库根。
子模块 `bpftime-verifier/ebpf-verifier` 在 `fd85b8a6`。
记号：「推断」= 读代码推出、没跑过；「未验证」= 没有确认的行为；「已实跑」= 在本机 `build-jit/` 上跑过。

## 0. 一句话职责 + 范围

本块管"主干以外"的部件：
1. **daemon**：不注入 loader，在内核里用 eBPF 监视并改写其它进程的 `bpf()` / `perf_event_open()` / `ioctl()`，把 uprobe 类 eBPF 程序转到 bpftime 共享内存里，由 agent 在用户态执行。
2. **bpftime-verifier**：PREVAIL 用户态验证器的薄封装，只在 `ENABLE_EBPF_VERIFIER=ON` 时编进 syscall-server 和 runtime。
3. **tools/bpftimetool**：直接读写 shm 的工具（load/import/export/remove/run，CUDA 构建另有 run-on-cuda）。
4. **tools/aot（bpftime-aot）**：用 LLVM 把 eBPF 字节码 AOT 编成本机 ELF `.o`，可以单独运行，也可以回填进 shm（build/compile/load/run）。
5. **构建系统**：根 `CMakeLists.txt`、`cmake/*`、`Makefile`、`installation.md`，以及 CI（`.github/workflows/*.yml`，只略读）。

读过的目录：`daemon/` 全部；`bpftime-verifier/` 除子模块外全部（子模块只看被调用的接口）；`tools/bpftimetool/`、`tools/aot/`（含 example/）、`tools/CMakeLists.txt`；`cmake/` 全部；根 `CMakeLists.txt`、`Makefile`、`installation.md`。各子目录的 `CMakeLists.txt` 只看产物和开关。`.github/workflows/` 14 个 yml 略读（3639 行）。

范围内文件总行数：**7273 行**（daemon 3509、verifier 封装 703、bpftimetool+aot+tools/CMakeLists 1795、cmake/ 623、根三件 643）。另外略读了 workflows 3639 行。

## 1. 文件清单

| 文件 | 行数 | 做什么（一句话） | 学习者是否必读 |
|---|---|---|---|
| daemon/CMakeLists.txt | 78 | 编内核监视程序 `.bpf.o`、生成 skeleton、把 placeholder 嵌进 C 文件，产出 `libbpftime_daemon.a` 和 `bpftime_daemon` | 选读 |
| daemon/.gitignore | 15 | 忽略列表 | 跳过 |
| daemon/README.md | 185 | daemon 使用说明（部分内容与代码不符，见 §7） | 选读 |
| daemon/bpf_tracer_event.h | 117 | 内核和用户态共用的事件结构 `struct event`、`bpf_fd_data`、`bpf_insn_data`、`MAKE_PFD` | 必读 |
| daemon/kernel/bpf_tracer.bpf.c | 735 | 内核监视程序本体：open/bpf/perf_event_open/close/ioctl 的 tracepoint，sched exec/exit | 必读 |
| daemon/kernel/bpf_defs.h | 118 | 手抄的 BPF 指令宏和 `PERF_EVENT_IOC_SET_BPF` 常量 | 跳过 |
| daemon/kernel/bpf_event_ringbuf.h | 34 | 256KB ringbuf `rb` 和 `fill_basic_event_info` | 选读 |
| daemon/kernel/bpf_kernel_config.h | 41 | rodata 配置（target_pid、enable_replace_* 等）和 `filter_target` | 必读 |
| daemon/kernel/bpf_obj_id_fd_map.h | 78 | `security_bpf_map` 和 `bpf_prog_kallsyms_add` 两个 kprobe，以及 id/指令快照 map | 必读 |
| daemon/kernel/bpf_pid_fd_map.h | 48 | `(pid,fd) → {类型, 内核 id}` 的 `bpf_fd_map` 及其读写函数 | 选读 |
| daemon/user/main.cpp | 105 | argp 命令行解析，调 `start_daemon` | 选读 |
| daemon/user/daemon.hpp | 15 | `start_daemon` 声明 | 跳过 |
| daemon/user/daemon_config.hpp | 57 | `daemon_config` 结构和默认值 | 必读 |
| daemon/user/bpf_tracer.cpp | 300 | 打开/配置/加载/挂载 skeleton，释放 `/a`，ringbuf 轮询主循环 | 必读 |
| daemon/user/handle_bpf_event.hpp | 50 | `bpf_event_handler` 类声明 | 选读 |
| daemon/user/handle_bpf_event.cpp | 460 | 事件分派：按事件类型打日志、调用 driver | 必读 |
| daemon/user/bpftime_driver.hpp | 93 | `bpftime_driver` 类声明（含一个未实现的函数） | 选读 |
| daemon/user/bpftime_driver.cpp | 410 | 把内核对象镜像进 shm：程序重定位、kernel-user map、uprobe、perf enable/disable、link | 必读 |
| daemon/user/bpf_prog_insns.hpp | 41 | 带容量检查的指令快照拷贝（`-E2BIG`） | 选读 |
| daemon/assets/embedfile.c | 54 | 构建期小工具：把文件转成 C 数组 | 跳过 |
| daemon/assets/placeholder | 2（二进制） | x86-64 PIE ELF（`a.c` 编出的空 main），daemon 写到 `/a` | 跳过 |
| daemon/test/CMakeLists.txt | 60 | `bpftime_daemon_tests`，用 `--wrap` 替换 libbpf 和 close | 选读 |
| daemon/test/common_def.hpp | 28 | `shm_remove` RAII 小工具（这些测试没有用到） | 跳过 |
| daemon/test/test_daemon.cpp | 16 | 空 TEST_CASE | 跳过 |
| daemon/test/test_daemon_driver.cpp | 79 | cookie 保留、快照容量上下界 | 选读 |
| daemon/test/test_daemon_fd.cpp | 230 | 每条返回路径都关闭临时 fd 的测试 | 选读 |
| daemon/test/asserts/malloc.json | 60 | 旧版 export 示例（map_type 1001/1002 = kernel-user map） | 选读 |
| bpftime-verifier/CMakeLists.txt | 53 | `bpftime-verifier` 静态库 + `bpftime_verifier_tests` | 选读 |
| bpftime-verifier/.gitignore | 1 | `dump*` | 跳过 |
| bpftime-verifier/README.md | 76 | API 用法示例（示例代码有语法错误） | 选读 |
| bpftime-verifier/include/bpftime-verifier.hpp | 72 | 对外 API：`verify_ebpf_program`、三个 `set_*`、描述符结构 | 必读 |
| bpftime-verifier/include/platform-impl.hpp | 17 | 内部：`bpftime_platform_spec` 和 thread_local 状态的 extern | 选读 |
| bpftime-verifier/src/bpftime-verifier.cpp | 159 | 组装 `raw_program` → unmarshal → `ebpf_verify_program`，捕获输出 | 必读 |
| bpftime-verifier/src/platform-impl.cpp | 114 | bpftime 定制的 `ebpf_platform_t`：程序类型、helper、map 类型的白名单 | 必读 |
| bpftime-verifier/test/simple.cpp | 36 | 合法/非法程序 | 选读 |
| bpftime-verifier/test/map.cpp | 146 | map 判空检查、helper/map 未声明 | 选读 |
| bpftime-verifier/test/non_kernel_helper.cpp | 29 | 自定义 helper 原型 | 选读 |
| tools/CMakeLists.txt | 8 | bpftimetool 总会编；cli 要 FRIDA；aot 要 LLVM_JIT 加 LIBBPF | 必读 |
| tools/bpftimetool/CMakeLists.txt | 62 | `bpftimetool` 可执行文件，链 runtime（和 bpftime-object） | 选读 |
| tools/bpftimetool/main.cpp | 399 | 五个（CUDA 下六个）子命令 | 必读 |
| tools/bpftimetool/README.md | 302 | 说明（JSON 格式一节与实际不符） | 选读 |
| tools/bpftimetool/test_load.py | 87 | CI 用的 `load` 参数校验测试 | 选读 |
| tools/aot/CMakeLists.txt | 43 | `bpftime-aot`（target 名 `bpftime-aot-cli`），整包链接 `bpftime_llvm_vm` | 选读 |
| tools/aot/main.cpp | 332 | build/compile/load/run 四个子命令 | 必读 |
| tools/aot/README.md | 260 | 说明（`-e` 输出 `.ll` 一说与代码不符） | 选读 |
| tools/aot/optimize.md | 69 | AOT 优化设想（设计文档，不对应代码） | 跳过 |
| tools/aot/test_run.py | 91 | CI 用的 `bpftime-aot run` 测试（用 cc 编 fixture） | 选读 |
| tools/aot/example/main.c | 68 | 链接 AOT `.o` 的驱动示例（无法编译，且有 bug，见 §7） | 选读 |
| tools/aot/example/Makefile | 6 | `clang -flto main.c do_count.o` | 跳过 |
| tools/aot/example/malloc.json | 65 | export 示例 | 跳过 |
| tools/aot/example/do_count.o / do_uprobe_trace.o | 二进制 | 已 AOT 好的 x86-64 可重定位目标（导出 `bpf_main`，引用 `_bpf_helper_ext_*`） | 选读 |
| tools/aot/example/.gitignore | 1 | `malloc` | 跳过 |
| CMakeLists.txt（根） | 282 | 顶层编排、静态大包、安装 | 必读 |
| cmake/StandardSettings.cmake | 112 | 几乎所有 `option()`，以及按架构强制设置 FRIDA/TRAP | 必读 |
| cmake/libbpf.cmake | 154 | ExternalProject libbpf/bpftool、拷头文件、`add_ebpf_program_target`、`add_bpf_skel_generating_target` | 必读 |
| cmake/CompilerWarnings.cmake | 102 | `set_project_warnings`（只用于 runtime） | 跳过 |
| cmake/frida.cmake | 88 | 下载 frida-core/gum devkit 16.1.2（带 SHA256） | 选读 |
| cmake/cuda.cmake | 67 | `find_cuda()` | 跳过（GPU 读者选读） |
| cmake/rocm.cmake | 8 | `find_rocm()`，没有任何地方 include | 跳过 |
| cmake/extract_and_rename.sh | 38 | 静态大包：解开 .a，给重名的 .o 改名 | 跳过 |
| cmake/aarch64-toolchain.cmake / arm-toolchain.cmake / riscv64-toolchain.cmake | 18×3 | 交叉编译工具链文件 | 跳过 |
| Makefile | 141 | cmake 命令的包装（`JOBS:=1`） | 选读 |
| installation.md | 220 | 官方安装说明（几处与代码不符，见 §8） | 选读 |
| .github/workflows/*.yml（14 个） | 3639 | CI；只略读构建和测试命令 | 选读 |

## 2. 核心数据结构

### 2.1 `struct event`（`daemon/bpf_tracer_event.h:41-104`）
- 内核往 ringbuf 里放的就是它，用户态原样读出。生命周期：内核 `bpf_ringbuf_reserve`（`daemon/kernel/bpf_event_ringbuf.h:24-32`），submit 后由 libbpf `ring_buffer__poll` 回调 `handle_event_rb`（`daemon/user/bpf_tracer.cpp:94-100`），回调结束后由 libbpf 回收。`exec_start` map 里也存了完整的 `struct event`（`daemon/kernel/bpf_tracer.bpf.c:672-677`）。
- `type`：`enum event_type`（`bpf_tracer_event.h:17-25`），取值 SYS_OPEN / SYS_CLOSE / SYS_BPF / SYS_IOCTL / SYS_PERF_EVENT_OPEN / BPF_PROG_LOAD_EVENT / EXEC_EXIT。
- `pid`（tgid）、`comm`。
- 联合体：`bpf_data`（`ret`、`bpf_cmd`、整份 `union bpf_attr`、`attr_size`、`map_id`，`:49-56`）；`open_data`（`:58-62`）；`perf_event_data`（`ret`、`pid`、`cpu`、`name_or_path[255]`、整份 `perf_event_attr`，`:64-72`）；`bpf_loaded_prog`（`type`、`insn_cnt`、`prog_name`、`insns_ptr`，`:74-80`）；`close_data`（`fd` + `bpf_fd_data`，`:82-85`）；`ioctl_data`（`fd`、`req`、`data`、`ret`、`bpf_prog_id`，`:87-94`）；`exec_data`（`:96-102`）。

### 2.2 `struct bpf_fd_data` 和 `bpf_fd_map`
- 定义：`daemon/bpf_tracer_event.h:27-39`。`type` 取 OTHERS / PERF / PROG / MAP；`kernel_id` 是 prog id 或 map id。
- 存在内核 HASH map `bpf_fd_map` 里，key 是 `MAKE_PFD(pid,fd) = pid<<32|fd`（`bpf_tracer_event.h:113-115`、`daemon/kernel/bpf_pid_fd_map.h:16-22`），最多 10240 项。
- 写入时机：BPF_PROG_LOAD / BPF_MAP_CREATE / BPF_LINK_CREATE / BPF_BTF_LOAD 成功返回时（`bpf_tracer.bpf.c:281-363`）；归 bpftime 管的 uprobe perf fd 在 `sys_exit_perf_event_open` 写入（`:519-520`）。删除只在 `sys_enter_close` 里，而且要先把 close 事件发出去（`:555-588`）。
- 读者：内核里的 `sys_enter_ioctl` 只处理在这张表里的 fd（`:610-613`），SET_BPF 用它把 prog fd 换成 prog id（`:651-663`）；用户态 driver 用它把 LDDW 里的 map fd 和 LINK_CREATE 里的 prog fd 换成内核 id（`daemon/user/bpftime_driver.cpp:52-71`）。

### 2.3 指令快照：`bpf_insn_data` / `bpf_prog_insns_map` / `insns_data`
- `bpf_insn_data` 定义在 `bpf_tracer_event.h:106-111`：`code[4096*8]` 加 `code_len`，大约 32KB。
- `bpf_prog_insns_map`：HASH，key 是 u64，value 是 `bpf_insn_data`，最多 10240 项（`daemon/kernel/bpf_obj_id_fd_map.h:33-38`）。先在 `BPF_PROG_LOAD` 进入时按 `pid_tgid` 存（`bpf_tracer.bpf.c:165-173`，`BPF_NOEXIST`），再由 kprobe `bpf_prog_kallsyms_add` 改成按 prog id 存，同时删掉 pid_tgid 那一项（`bpf_obj_id_fd_map.h:50-76`）。按 id 存的项**从来不删**。
- `insns_data` 是 BPF 程序里的**全局** `.bss` 变量（`bpf_obj_id_fd_map.h:40`），作为中转缓冲，不是 per-CPU 的（并发问题见 §6）。
- 用户态读出位置：`relocate_bpf_prog_insns` 用全局 `insn_data` 接收（`bpftime_driver.cpp:50`，`:80-82`），再由 `copy_captured_bpf_prog_insns` 做容量检查（`daemon/user/bpf_prog_insns.hpp:20-36`）。

### 2.4 内核侧其它 map（全部在 `bpf_tracer.bpf.c` 或其头文件里）
| map | 位置 | key → value | 用途 |
|---|---|---|---|
| `rb` | `bpf_event_ringbuf.h:19-22` | ringbuf 256KB | 事件通道 |
| `whitelist_hook_addr` | `bpf_tracer.bpf.c:19-24` | u64 → u32 | `-w` 白名单（按 `probe_offset` 匹配） |
| `open_param_start` | `:41-46` | pid_tgid → {fname, flags} | open/openat 的 enter→exit 暂存 |
| `bpf_param_start` | `:131-136` | pid_tgid → {cmd, attr, size} | bpf() 的 enter→exit 暂存 |
| `perf_event_open_param_start` | `:449-469` | pid_tgid → `perf_event_args_t` | perf_event_open 的原 attr、原路径、原偏移、用户态指针、`send_to_daemon` |
| `ioctl_param_start` | `:590-601` | pid_tgid → {fd, req, data} | ioctl 暂存 |
| `exec_start` | `:672-677` | pid → event | 只在 auto_attach 时加载（默认不加载） |
| `bpf_map_new_fd_args_map` | `bpf_obj_id_fd_map.h:15-20` | pid_tgid → map id | `security_bpf_map` kprobe 记下"本线程最近一次碰到的 map id" |
| `bpf_progs_new_fd_args_map` | `bpf_obj_id_fd_map.h:43-48` | pid_tgid → prog id | `bpf_prog_kallsyms_add` kprobe 记下"本线程最近一次加载的 prog id"（exit 里删除它的代码被注释掉了，`bpf_tracer.bpf.c:287-288`） |

rodata 配置（`daemon/kernel/bpf_kernel_config.h:14-26`，`bpf_tracer.bpf.c:26`）：`target_pid`、`current_pid`、`enable_replace_prog`、`enable_replace_uprobe`、`new_uprobe_path[255]`、`uprobe_perf_type`、`kprobe_perf_type`（只写不读）、`submit_bpf_events`、`enable_whitelist`。全部由 `start_daemon` 在 load 前写入（`daemon/user/bpf_tracer.cpp:193-222`）。

### 2.5 `struct daemon_config`（`daemon/user/daemon_config.hpp:18-55`）
| 字段 | 默认 | 谁设置 | 实际效果 |
|---|---|---|---|
| `pid` | 0 | `-p`（`main.cpp:46-54`） | 写进 rodata `target_pid`，0 表示**全系统** |
| `uid` | 0（`main.cpp:18` 初始化成 -1） | `-u` | **永远解析失败**（§7），而且从来没被使用 |
| `verbose` | false | `-v` | spdlog 降到 debug，并放行 libbpf DEBUG |
| `show_open` | false | `-o` | 为 false 时关掉 4 个 open 系列程序的 autoload |
| `enable_replace_prog` | false | 没有 CLI | rodata；为 true 时把 KPROBE/TRACEPOINT 程序换成空程序 |
| `enable_replace_uprobe` | true | 没有 CLI | rodata；uprobe 改写开关 |
| `new_uprobe_path` | "" | `main.cpp:96` 拷 argv[0] | **被忽略**，`bpf_tracer.cpp:202` 硬编码成 `/a` |
| `bpftime_cli_path` / `bpftime_tool_path` | `~/.bpftime/...` | — | 没有任何代码读取 |
| `is_driving_bpftime` | true | — | 为 false 时只打日志、不写 shm |
| `submit_bpf_events` | false | 没有 CLI | rodata；为 false 时 MAP_CREATE、BTF_LOAD、PROG_LOAD_EVENT、非 perf 的 close 都不上报 |
| `enable_auto_attach` | false | 没有 CLI | 为 false 时不加载 exec/exit 程序 |
| `duration_ms` | 1000 | — | 只在 `process_exec_maps` 里用（默认不生效） |
| `whitelist_uprobes` | 空 | `-w`（可重复，`strtoul` base 0） | 非空时启用白名单 |

生命周期：`main.cpp:18` 的静态对象，按值拷进 `start_daemon`、`bpftime_driver`、`bpf_event_handler`（三份副本）。

### 2.6 `bpftime_driver`（`daemon/user/bpftime_driver.hpp:23-89`）
- `pid_fd_to_id_map`：`std::map<uint64_t,int>`，key 是 `(目标进程 pid, 目标进程里的 perf fd)`，value 是 bpftime shm 里的 handler 下标。放在进程堆上，daemon 退出就没了；只有 uprobe 和（没被调用的）btf 会登记。
- `object`：skeleton 指针，用来查 `bpf_fd_map` 和 `bpf_prog_insns_map`。
- 构造：`bpftime_initialize_global_shm(SHM_REMOVE_AND_CREATE)` 加上从环境变量构造的 runtime config（`bpftime_driver.cpp:398-405`）。析构：`bpftime_destroy_global_shm()`（`:407-410`）。也就是说 **daemon 是 shm 的创建者和拥有者**：启动时会删掉已有的 shm。
- 在 `start_daemon` 的栈上创建（`bpf_tracer.cpp:241`），活到函数返回。

### 2.7 `bpf_event_handler`（`daemon/user/handle_bpf_event.hpp:17-38`）
- 保存 `current_pid`、`uprobe_type`、`kprobe_type`、配置副本和 driver 引用。构造时读 sysfs 拿 uprobe/kprobe 的 perf type，失败就 `exit(1)`（`handle_bpf_event.cpp:442-460`）。构造还会改写**全局静态数组** `perf_type_id_strings`（`:452`、`:459`）。

### 2.8 verifier：`BpftimeMapDescriptor` / `BpftimeHelperProrotype` 和 thread_local 状态
- 对外结构：`bpftime-verifier/include/bpftime-verifier.hpp:18-60`。`bpftime_return_type_t`、`bpftime_argument_type_t` 和 PREVAIL 的枚举值一一对应，用 `(int)` 强转（`src/bpftime-verifier.cpp:125-141`）。
- 内部状态（`src/platform-impl.cpp:101-103`）：`thread_local usable_helpers`（set）、`map_descriptors`（map fd → EbpfMapDescriptor）、`non_kernel_helpers`。写入方是 `set_*`（`bpftime-verifier.cpp:105-143`），读出方是 platform 回调。
- `bpftime_platform_spec`（`platform-impl.cpp:90-98`）：`ebpf_platform_t` 的 8 个回调，`map_record_size = 0`；`parse_maps_section` 和 `resolve_inner_map_references` 直接抛异常（`:50-57`、`:82-86`）。
- `verifier_options`（`bpftime-verifier.cpp:19-30`）：`check_termination=true`、`print_invariants=true`、`print_failures=true`、`no_simplify=true`、`allow_division_by_zero=false`、`strict=false`，都是硬编码。

### 2.9 bpftimetool / aot 用到的外部结构
- `bpf_prog_handler`（runtime，`runtime/src/handler/prog_handler.hpp:36` 有 `type` 字段）：bpftimetool `run` 用 `insns` / `aot_insns` / `name`；aot 的 `compile` 和 `load` 会**直接改写 shm 里的** `aot_insns`（`tools/aot/main.cpp:135-137`、`:174-177`）。
- export 出来的 JSON：顶层是对象，key 是 fd 槽位号的字符串（`runtime/src/bpftime_shm_json.cpp:298-359`），**不是** README 写的 `{"handlers":[...]}`。

## 3. 主要流程与调用链

### 3.1 daemon 启动
`main` (`daemon/user/main.cpp:87`) → `argp_parse`（`:98`）→ `start_daemon` (`daemon/user/bpf_tracer.cpp:168`)
→ `bpf_tracer_bpf__open` (`:185`) → 写 rodata (`:193-222`)，其中 `/a` 硬编码在 `:202`；`/a` 不存在时用 `std::ofstream` 写出嵌入的 `placeholder` (`:203-216`)
→ 按配置关闭 autoload (`:223-239`)
→ `bpftime_driver driver(env,obj)` (`:241`) → `bpftime_initialize_global_shm(SHM_REMOVE_AND_CREATE)` (`bpftime_driver.cpp:402`)
→ `bpf_event_handler(...)` (`bpf_tracer.cpp:243`) → 读 sysfs (`handle_bpf_event.cpp:67-87`)
→ `bpf_tracer_bpf__load` (`bpf_tracer.cpp:246`) → 填白名单 map (`:251-266`) → `bpf_tracer_bpf__attach` (`:267`)
→ `ring_buffer__new` (`:274`) → `while(!exiting){ ring_buffer__poll(300ms); process_exec_maps(); }` (`:283-293`)。

说明：shm 在 **load 内核程序之前**就以 REMOVE_AND_CREATE 打开，所以已有的 bpftime 会话会被清掉。写 `/a` 和读 sysfs 都需要 root。SIGINT 处理器只置 `exiting`（`:89-92`），退出时 destroy skeleton 并删除 shm（driver 析构）。

### 3.2 uprobe 劫持（核心机制）
目标工具 `perf_event_open(attr{type=uprobe, uprobe_path, probe_offset}, pid, cpu, ...)`
→ 内核 `tracepoint__syscalls__sys_enter_perf_event_open` (`bpf_tracer.bpf.c:471-501`)：`filter_target`（`bpf_kernel_config.h:28-40`），**先**把原 attr、`pid`、`cpu`、原路径（`bpf_probe_read_user_str`）、原 `probe_offset` 和两个用户态指针存进 args (`:481-490`)
→ `process_perf_event_open_enter` (`:397-447`)：只看 `attr.type == uprobe_perf_type`。`enable_replace_uprobe` 打开且白名单命中时：偏移清零 → 量出原路径长度 `size` → `bpf_probe_write_user(uprobe_path, new_uprobe_path, size)`（写入 `"/a"` 和补零，长度等于原路径长度，所以不会越界）→ 整个 attr 写回 → 返回 1。白名单不命中返回 0（交给内核正常处理）。**`enable_replace_uprobe` 关闭时也返回 1**（只上报、不改写，`:441-443`）
→ args 存进 `perf_event_open_param_start` (`:498`)
→ 内核用改写后的参数执行：在 `/a` 偏移 0 处注册一个真的内核 uprobe，返回一个真 fd
→ `tracepoint__syscalls__sys_exit_perf_event_open` (`:503-553`)：如果 `send_to_daemon`：在 `bpf_fd_map` 里登记 `(pid,ret)→PERF` (`:520`)；发出 `SYS_PERF_EVENT_OPEN` 事件，带**原** attr 和**原**路径 (`:523-538`)；再把路径和偏移写回用户内存 (`:541-545`)
→ 用户态 `bpf_event_handler::handle_perf_event_open` (`handle_bpf_event.cpp:300-353`)：只处理 `ret>=0` 且 `type==uprobe_type` 的事件；retprobe 位从 sysfs 读（**每个事件都读一次** `determine_uprobe_retprobe_bit()`，`:322-324`）；`ref_ctr_off = config>>32`
→ `bpftime_driver::bpftime_uprobe_create_server` (`bpftime_driver.cpp:314-334`)：`bpftime_find_minimal_unused_fd()` 取一个空槽 → `bpftime_uprobe_create(id, target_pid, name, offset, retprobe, ref_ctr_off)` → 在 `pid_fd_to_id_map[(工具 pid, 工具的 perf fd)]` 里记下 id。

说明：内核里那个真 uprobe 挂在 `/a` 上。只要没人执行 `/a`，它就永远不会触发。工具拿到的是一个合法的内核 perf fd，后续的 ioctl 和 link 调用都能在内核里成功；daemon 在旁边把同样的操作镜像进 shm。

### 3.3 程序指令捕获与 id 对应
`bpf(BPF_PROG_LOAD, attr)` → `tracepoint__syscalls__sys_enter_bpf` (`bpf_tracer.bpf.c:235-255`)：把 cmd/attr 存进 `bpf_param_start` → `process_bpf_syscall_enter` (`:220-233`) → `process_bpf_prog_load_events` (`:151-218`)：读 `insn_cnt`、`prog_type`、`insns`，把最多 4096 条指令拷进全局 `insns_data`，`code_len` 填**真实** `insn_cnt`（可能大于 4096）→ 按 pid_tgid 存进 `bpf_prog_insns_map`（`BPF_NOEXIST`）；只有 `submit_bpf_events` 打开时才发 BPF_PROG_LOAD_EVENT；`enable_replace_prog` 打开且类型是 KPROBE/TRACEPOINT 时，把用户内存里的指令改成 `r0=0; exit` 并清掉 func_info/line_info (`:192-216`)
→ 内核验证并分配 id 后调用 `bpf_prog_kallsyms_add` → kprobe (`bpf_obj_id_fd_map.h:50-76`)：`bpf_progs_new_fd_args_map[pid_tgid]=id`，把快照从 pid_tgid 移到 id 名下
→ `sys_exit_bpf` (`bpf_tracer.bpf.c:371-389`) → `process_bpf_syscall_exit` (`:273-369`)：PROG_LOAD 成功时 `bpf_fd_map[(pid,fd)] = {PROG, id}`（**不发事件**）；MAP_CREATE 时 id 来自 `security_bpf_map` kprobe（`bpf_obj_id_fd_map.h:22-31`），登记成 MAP，只有 `submit_bpf_events` 才发事件；LINK_CREATE **总是**发事件（`:327-350`）；BTF_LOAD 登记成 OTHERS。

说明：快照是 libbpf 交给内核的**原始指令**（CO-RE 已经由 libbpf 做完；map 引用还是进程内的 fd），不是内核改写后的 xlated 指令。

### 3.4 attach：从内核 link/ioctl 到 shm 里的 link
两条入口：
- (a) `BPF_LINK_CREATE` 事件 → `handle_bpf_event` (`handle_bpf_event.cpp:250-273`)：取 `link_create.prog_fd`、`target_fd`，attach_type 为 `BPF_PERF_EVENT` 时还取 `bpf_cookie` → `bpftime_attach_perf_to_bpf_fd_server` (`bpftime_driver.cpp:263-277`)：用 `bpf_fd_map` 把 prog fd 换成内核 prog id → (c)。
- (b) `ioctl(perf_fd, PERF_EVENT_IOC_SET_BPF, prog_fd)`：内核 exit 时已经把 prog fd 换成 prog id (`bpf_tracer.bpf.c:651-663`) → `handle_ioctl` (`handle_bpf_event.cpp:376-406`) → (c)。
- (c) `bpftime_attach_perf_to_bpf_server` (`bpftime_driver.cpp:279-312`)：用 `pid_fd_to_id_map` 找到 perf 对应的 shm id（找不到就报错返回）→ 程序还不在 shm 里时调用 `bpftime_progs_create_server(kernel_id, pid)` (`:163-212`)：
  `bpf_prog_get_fd_by_id` → `bpf_obj_get_info_by_fd` → 已存在就返回 → `relocate_bpf_prog_insns` (`:73-125`)：遇到 `LDDW` 且 `src_reg ∈ {1,2}`，用 `(pid, imm)` 查 `bpf_fd_map` 得到 **map id**，写回 `imm`；`CALL` 只打日志 → `bpftime_progs_create(kernel_id, ...)`：**用内核 prog id 作为 shm 槽位号** → `check_and_create_prog_related_maps` (`:127-161`)：用 `nr_map_ids` 再取一次 info，对每个 map id 调 `bpftime_maps_create_server` (`:214-261`)：`attr.type = 内核类型 + KERNEL_USER_MAP_OFFSET(1000)`，`kernel_bpf_map_id = id`，**用内核 map id 作为 shm 槽位号**
  → `bpftime_attach_perf_to_bpf[_with_cookie](perf_id, kernel_bpf_id[, cookie])`。

说明：指令里的 LDDW imm 被改成**内核 map id**，而 shm 里那个 map 的槽位号也正好是这个 id，所以程序在用户态执行时，"按 fd 查 map"实际查到的就是这个 kernel-user map。kernel-user map 再通过 `bpf_map_get_fd_by_id` 去读写真正的内核 map（`runtime/src/bpf_map/shared/hash_map_kernel_user.cpp:18` 等），因此运行 agent 的进程也需要相应权限（推断）。

### 3.5 enable/disable/close
- `ioctl(PERF_EVENT_IOC_ENABLE/DISABLE)`：内核只在 fd 在 `bpf_fd_map` 里时才记录 (`bpf_tracer.bpf.c:603-623`) → `handle_ioctl` → `bpftime_perf_event_enable_server`（找不到就 warn 并返回 0，`bpftime_driver.cpp:337-353`）/ `disable_server`（找不到就返回 -1，`:356-371`）。
- `close(fd)`：`sys_enter_close` 只处理 `bpf_fd_map` 里的 fd；`submit_bpf_events=false` 时只上报 PERF 类型 (`bpf_tracer.bpf.c:555-588`) → `handle_close_event` (`handle_bpf_event.cpp:217-225`) → `bpftime_close_server` (`bpftime_driver.cpp:373-383`) → `bpftime_close(id)`。prog/map/link 的 close 默认不上报，所以它们在 shm 里**只增不减**（推断）。

### 3.6 verifier 调用链（只在 `ENABLE_EBPF_VERIFIER=ON` 时编进来）
- 配置：syscall-server `start_up` (`runtime/syscall-server/syscall_server_utils.cpp:147`) 用 `std::call_once` 包起来 → 按 runtime_config 收集 kernel_utils、shm_maps 组的 helper id；ufunc 组见 §7 → `verifier::set_available_helpers` 和 `set_non_kernel_helpers` (`:181-215`)。
- map 描述符：每次 `bpftime_shm::add_bpf_map` 都会 `get_map_descriptors()`，加一项，再 `set_map_descriptors()` (`runtime/src/bpftime_shm_internal.cpp:1058-1069`)。
- 验证点：`syscall_context` 处理 `BPF_PROG_LOAD` (`runtime/syscall-server/syscall_context.cpp:628-703`)：`run_with_kernel` 时直接交给内核；否则 `prog_type==TRACEPOINT` 对应 section "tracepoint"，`==SOCKET_FILTER` 对应 "uprobe"，**其它类型（包括 libbpf 给 uprobe 用的 KPROBE）不验证**；`verifier_mode != NO_VERIFY` 时调用 `verify_ebpf_program` (`:650-688`)，失败时 STRICT 返回 `-1/EINVAL`，WARNING 打警告后继续加载。
- 封装内部：`verify_ebpf_program` (`bpftime-verifier/src/bpftime-verifier.cpp:64-103`) → 拼 `raw_program` → `prog.info = {platform=&bpftime_platform_spec, map_descriptors, type=get_program_type(section)}` (`:77-82`) → `global_program_info = prog.info`（PREVAIL 的 thread_local，`ebpf-verifier/src/spec_type_descriptors.hpp:75`）→ `unmarshal(prog, notes)`（`ebpf-verifier/src/asm_unmarshal.cpp:454-463`，只捕获 `InvalidInstruction`，其它异常会往外抛）→ `ebpf_verify_program`（`ebpf-verifier/src/crab_verifier.cpp:238-256`；分析过程中的 `std::runtime_error` 在 `:179-190` 被转成一条 warning）→ 返回 `{}` 或捕获到的文本。
- 平台回调 (`platform-impl.cpp`)：`get_program_type` 只接受 `uprobe*` / `uretprobe*` / `tracepoint*` 开头的 section，再转给 `g_ebpf_platform_linux`（`ebpf-verifier/src/linux/linux_platform.cpp:249`），其它抛异常 (`:18-31`)；`get_helper_prototype` 先查 non_kernel，再查 `get_helper_prototype_linux`，不可用时抛 "Unusable helper" (`:33-46`)，但 unmarshal 会先调用 `is_helper_usable`，不可用时报 `"invalid helper function id"`（`ebpf-verifier/src/asm_unmarshal.cpp:357-358`），所以用户实际看到的是后一种信息；`get_map_descriptor` 找不到时抛 "Invalid map fd"（`:61-68`，在分析阶段被捕获，变成 `exit: Invalid map fd: N`，见 `test/map.cpp:142-145`）；`get_map_type` 只接受 HASH / ARRAY / RINGBUF (`:70-81`)，其它类型也是在分析阶段抛出、变成 warning（`ebpf-verifier/src/crab/ebpf_domain.cpp:1667`）。

### 3.7 bpftimetool 子命令（`tools/bpftimetool/main.cpp`）
入口 `main` (`:149`)：先 `bpftime_set_logger(BPFTIME_LOG_OUTPUT 或 ~/.bpftime/runtime.log)` (`:154-157`)，没有参数时打印用法 (`:158-174`)，然后 `spdlog::cfg::load_env_levels()` (`:176`)。
| 子命令 | 位置 | 做什么 | shm 打开方式 |
|---|---|---|---|
| `load <fd> <JSON文本>` | `:179-202` | `strtol` 严格校验 fd → `bpftime_import_shm_handler_from_json(fd, json)` 只导入一个 handler | CREATE_OR_OPEN |
| `export <file>` | `:203-241` | 拒绝把输出写到 `/dev/shm/<名字>` → `bpftime_export_global_shm_to_json` | OPEN_ONLY（捕获 interprocess_exception） |
| `import <file>` | `:241-255` | 设 runtime config → `bpftime_import_global_shm_from_json`；先导入非 link，再导入 link（`bpftime_shm_json.cpp:240-268`） | CREATE_OR_OPEN |
| `remove` | `:255-263` | `bpftime_remove_global_shm()` | — |
| `run <id> <data> [repeat N] [type T]` | `:264-301` → `bpftime_run_ebpf_program` (`:66-137`) | 从槽位复制出一个**新的进程内** `bpftime_prog`，挂上 kernel_utils 和 shm_maps 两组 helper；JIT 走 `bpftime_prog_load(true)`，AOT 走 `load_aot_object(aot_insns)`（`aot_insns` 为空时报错），INTERPRET 走 `bpftime_prog_load(false)`；把整个数据文件当成 ctx，循环 N 次，打印**平均**耗时和最后一次返回值 (`:35-64`) | OPEN_ONLY |
| `run-on-cuda` | `:302-392`（只在 CUDA 构建里有） | 建 `bpf_attach_ctx`，注册 nv_attach_impl，按程序名在 GPU 上运行 | OPEN_ONLY |

### 3.8 bpftime-aot 子命令（`tools/aot/main.cpp`）
| 子命令 | 位置 | 做什么 |
|---|---|---|
| `build <EBPF_ELF> [-o dir] [-e]` | `:73-104` | `bpf_object__open`（**不 load**）→ 对每个 program 取 `bpf_program__insns`，挂上 1000 个 `empty_helper`（`create_all_helpers`，`:58-71`）→ `bpftime_prog_load(true)` → `vm_instance->do_aot_compile(emit_llvm_ir)` → 写到 `<dir>/<prog>.o` |
| `compile [-o dir] [-e]` | `:106-142` | 以 OPEN_ONLY 打开 shm，**只处理找到的第一个** prog handler（`return 0` 在循环里面，`:138`），写 `.o`，并把结果写回该 handler 的 `aot_insns` |
| `load <PATH> <ID>` | `:144-183` | 把任意 ELF 文件的字节原样塞进 `prog.aot_insns`（ID 用 `atoi` 解析） |
| `run <PATH> [MEMORY]` | `:185-239` | `bpftime_initialize_global_shm(SHM_NO_CREATE)`（不建 shm，`:218`）→ 设 runtime config → 建一个 0 条指令的空 `bpftime_prog("bpf_main")`，挂 kernel_utils + shm_maps 两组 helper → `load_aot_object` → 用 MEMORY 文件内容当 ctx 执行 → 往 stdout 打 `Output: <retval>` |

说明：`-e` 只让 llvmbpf 把 IR **额外打印出来**（`vm/llvm-jit/src/llvm_jit_context.cpp:557-570`），文件仍然叫 `<name>.o`、内容仍然是目标码。README 说"输出 `.ll` 文件"是错的。

## 4. 跨模块接口

### 4.1 本块对外提供
| 接口 | 定义 | 调用点 |
|---|---|---|
| `bpftime::verifier::verify_ebpf_program` | `bpftime-verifier/include/bpftime-verifier.hpp:14`，实现在 `src/bpftime-verifier.cpp:64` | `runtime/syscall-server/syscall_context.cpp:657` |
| `verifier::set_available_helpers` / `set_non_kernel_helpers` | 头文件 `:62-65` | `runtime/syscall-server/syscall_server_utils.cpp:212,214` |
| `verifier::get_map_descriptors` / `set_map_descriptors` | 头文件 `:67-68` | `runtime/src/bpftime_shm_internal.cpp:1059,1068` |
| `BpftimeHelperProrotype` 等类型 | 头文件 `:18-60` | `runtime/src/bpf_helper.cpp:1415`（`get_ufunc_helper_protos`），`runtime/extension/extension_helper.cpp:4`，`runtime/include/bpftime_helper_group.hpp:9`，`runtime/src/handler/handler_manager.hpp:34`（只 include） |
| CMake 目标 `bpftime-verifier` 和变量 `BPFTIME_VERIFIER_INCLUDE` | `bpftime-verifier/CMakeLists.txt:1,16` | `runtime/CMakeLists.txt:205-209`、`runtime/syscall-server/CMakeLists.txt:56-60` |
| CMake 函数 `add_ebpf_program_target` / `add_bpf_skel_generating_target` | `cmake/libbpf.cmake:110,98` | `daemon/CMakeLists.txt:2,5`，以及 runtime/unit-test、benchmark 等（未逐一列出） |
| CMake 变量 `LIBBPF_INCLUDE_DIRS` / `LIBBPF_LIBRARIES`，目标 `libbpf`、`bpftool`、`copy_headers` | `cmake/libbpf.cmake:31-32,17,87,58` | 几乎所有需要 libbpf 的目标 |
| `find_cuda()` | `cmake/cuda.cmake:4` | daemon、bpftimetool、aot、runtime、attach 等的 CMakeLists |
| `set_project_warnings()` | `cmake/CompilerWarnings.cmake:7` | 只有 `runtime/CMakeLists.txt:228`（PUBLIC，会传给链接 runtime 的目标） |
| daemon 不对外提供 C/C++ 接口（`libbpftime_daemon.a` 只被 `bpftime_daemon` 和 `bpftime_daemon_tests` 链接，`daemon/CMakeLists.txt:67`、`daemon/test/CMakeLists.txt:25-30`） | | |

### 4.2 本块依赖的外部接口
daemon（`bpftime_driver.cpp`，`handle_bpf_event.cpp`）：
- `bpftime_initialize_global_shm` / `bpftime_destroy_global_shm`（`runtime/src/bpftime_shm_internal.cpp:49,59`），调用点 `bpftime_driver.cpp:402,409`。
- `construct_runtime_config_from_env`（`runtime/src/bpftime_config.cpp:89`）+ `bpftime_set_runtime_config`（`runtime/src/bpftime_shm_internal.cpp:1179`），调用点 `bpftime_driver.cpp:403-404`。
- `bpftime_find_minimal_unused_fd`（`runtime/src/bpftime_shm.cpp:64`），调用点 `bpftime_driver.cpp:26`。
- `bpftime_progs_create`（`bpftime_shm.cpp:74`）、`bpftime_maps_create`（`:81`）、`bpftime_uprobe_create`（`:220`）、`bpftime_perf_event_enable/disable`（`:239,244`）、`bpftime_attach_perf_to_bpf[_with_cookie]`（`:249,255`）、`bpftime_close`（`:282`）、`bpftime_is_map_fd` / `bpftime_is_prog_fd`（`:327,337`），调用点分别在 `bpftime_driver.cpp:197,254,325,345,363,302-304,382,149/248,185/289`。
- `KERNEL_USER_MAP_OFFSET`（`runtime/include/bpftime_shm.hpp:66`），用在 `bpftime_driver.cpp:236`；runtime 只实现了 1001/1002/1006/1004 这 4 种（`runtime/include/bpftime_shm.hpp:113-120`，`runtime/src/handler/map_handler.cpp:188-207`）。
- `ebpf_inst` / `EBPF_OP_LDDW` / `EBPF_OP_CALL`（vm-core 的 `ebpf_inst.h`）。
- libbpf：`bpf_prog_get_fd_by_id`、`bpf_map_get_fd_by_id`、`bpf_obj_get_info_by_fd`、`bpf_map__lookup_elem`、`ring_buffer__*`、skeleton。
- 测试还用到 `bpftime::shm_holder.global_shared_memory.get_manager()` 和 `bpf_link_handler::attach_cookie`（`daemon/test/test_daemon_driver.cpp:39-49`）。

bpftimetool：`bpftime_initialize_global_shm`、`shm_holder`、`handler_manager::get_handler/size`、`bpf_prog_handler`、`bpftime_prog`（`runtime/include/bpftime_prog.hpp:25-57`）、`bpftime_helper_group::get_kernel_utils_helper_group/get_shm_maps_helper_group/add_helper_group_to_prog`（`runtime/include/bpftime_helper_group.hpp:44-50`）、`bpftime_import_shm_handler_from_json` / `bpftime_import_global_shm_from_json` / `bpftime_export_global_shm_to_json`（`runtime/src/bpftime_shm_json.cpp:230,100,270`）、`bpftime_remove_global_shm`（`runtime/src/bpftime_shm_internal.cpp:81`）、`get_global_shm_name`、`bpftime_set_logger`（`runtime/include/bpftime_logger.hpp:39`）、`DEFAULT_LOGGER_OUTPUT_PATH`（`runtime/include/bpftime_config.hpp:13`）；CUDA 构建另用 `bpf_attach_ctx`、`nv_attach_impl`。

bpftime-aot：除上面这些外，还有 `bpftime_prog::get_vm()->vm_instance->do_aot_compile`（`vm/compat/llvm-vm/compat_llvm.cpp:40`，`vm/llvm-jit/src/vm.cpp:319`）、`load_aot_object`、libbpf 的 `bpf_object__open`、`bpf_program__insns`、`bpf_program__insn_cnt`。链接时用 `--whole-archive` 拉进 `bpftime_llvm_vm`，为的是让 VM 工厂的构造函数注册生效（`tools/aot/CMakeLists.txt:29-35`）。

verifier 依赖的子模块接口：`raw_program` / `program_info`（`ebpf-verifier/src/spec_type_descriptors.hpp:53,67`）、`global_program_info`（`:75`）、`unmarshal`（`asm_unmarshal.cpp:454`）、`ebpf_verify_program`（`crab_verifier.cpp:238`）、`ebpf_platform_t`（`platform.hpp:33-45`）、`g_ebpf_platform_linux`、`get_helper_prototype_linux`（`linux/linux_platform.cpp:249`、`linux_platform.hpp:5`）、`ebpf_verifier_options_t`（`config.hpp:5`）。CMake 目标 `ebpfverifier`（`bpftime-verifier/CMakeLists.txt:2,12`）。

## 5. 配置项、环境变量、编译开关

### 5.1 CMake 选项总表
（"生效位置"写的是被读取的地方；"本机 build-jit"是 `build-jit/CMakeCache.txt` 里的值）

| 选项 | 默认 | 定义 | 作用 / 生效位置 | 本机 build-jit |
|---|---|---|---|---|
| `CMAKE_BUILD_TYPE` | Debug（未设置时） | `CMakeLists.txt:40-42` | Debug 时加 `-D_DEBUG`（`:108-110`），arm32 上加 `-latomic`（`:95-98`）；`:92` 打印 "Enabling ubsan" 但**没有加任何 ubsan 参数** | RelWithDebInfo |
| `BPFTIME_LLVM_JIT` | ON | `cmake/StandardSettings.cmake:10`；`vm/CMakeLists.txt:11-13` 有兜底 | 编 llvm-vm（`vm/compat/CMakeLists.txt:14`）；runtime 定义 `BPFTIME_LLVM_JIT=1`，关闭时改为 `DEFAULT_VM_NAME="ubpf"`（`runtime/CMakeLists.txt:127-131`）；agent / syscall-server / cli 整包链接 llvm_vm；`tools/CMakeLists.txt:5` 只有它打开（加上 LIBBPF）才编 aot；根 `:11-13` 加 `-lncurses`；静态大包 `:202,208` | ON |
| `BPFTIME_UBPF_JIT` | ON | `StandardSettings.cmake:11`；`vm/CMakeLists.txt:15-17` | 编 ubpf-vm；agent / syscall-server / cli 用 `--whole-archive` 链接（`runtime/agent/CMakeLists.txt:26-30` 等） | ON |
| `BPFTIME_BUILD_EXECUTABLE` | OFF | `StandardSettings.cmake:5` | **没有任何地方读取** | OFF |
| `BPFTIME_WARNINGS_AS_ERRORS` | OFF | `:17` | 在 `set_project_warnings` 里加 `-Werror`（`CompilerWarnings.cmake:70-72`），只作用于 runtime 及其 PUBLIC 传递对象 | OFF |
| `BPFTIME_CUDA_ROOT` | ""（但用 `option()` 声明，缓存里变成 BOOL OFF） | `:24` | `find_cuda()` 用它拼路径（`cmake/cuda.cmake:5-67`） | `BOOL=OFF` |
| `BPFTIME_ENABLE_CUDA_ATTACH` | OFF | `:25` | 全局 `-DBPFTIME_ENABLE_CUDA_ATTACH=1`（`CMakeLists.txt:169-171`）；编 nv_attach_impl（`attach/CMakeLists.txt:19`）；runtime 加 GPU map 源文件（`runtime/CMakeLists.txt:91-101`）；bpftimetool 加 `run-on-cuda` | OFF |
| `BPFTIME_ENABLE_UNIT_TESTING` | OFF | `:32` | `enable_testing()`（根 `:34-38`）；编 daemon/test（`daemon/CMakeLists.txt:71-73`）、runtime/unit-test（要求同时开 LIBBPF，`runtime/CMakeLists.txt:281`）、vm 测试、llvm-jit 测试 | ON |
| `BPFTIME_USE_CATCH2` | OFF | `:34` | **没有任何地方读取** | — |
| `BPFTIME_VERBOSE_OUTPUT` | ON | `:43` | **没有任何地方读取** | ON |
| `BPFTIME_ENABLE_LTO` | OFF | `:52` | `CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE`（`:53-61`） | **ON** |
| `BPFTIME_ENABLE_CCACHE` | OFF | `:63` | **不起作用**：只要找到 ccache 就会启用（`:64-68`） | OFF |
| `BPFTIME_ENABLE_ASAN` | OFF | `:70` | `-fsanitize=address,undefined`（`:71-74`）；runtime 里另有分支（`runtime/CMakeLists.txt:217`） | OFF |
| `BPFTIME_ENABLE_TRAP_UPROBE` | riscv 上 ON，其它 OFF，**FORCE** | `:76-82` | 编 trap_uprobe_attach_impl（`attach/CMakeLists.txt:8-11`）；根 `:34-38` 的测试开关 | OFF |
| `BPFTIME_ENABLE_FRIDA` | riscv 上 OFF，其它 ON，**FORCE** | `:76-82` | `include(cmake/frida.cmake)`（根 `:138-140`）；编 frida attach、text_segment_transformer、`tools/cli`；runtime 和 agent 链接 frida-gum | ON |
| `BPFTIME_ENABLE_MPK` | OFF | `:84-87` | 全局 `-DBPFTIME_ENABLE_MPK`；源码用在 `runtime/src/bpftime_shm*.cpp`、`bpftime_prog.cpp` | OFF |
| `BPFTIME_ENABLE_IOURING_EXT` | OFF | `:89-92` | 全局宏 + runtime 链接 uring（`runtime/CMakeLists.txt:198-203`）；源码 `runtime/extension/extension_helper.cpp` | OFF |
| `ENABLE_EBPF_VERIFIER` | OFF | `:95` | 根 `:144-154` 编 bpftime-verifier（否则改用 `third_party/Catch2`）；runtime 和 syscall-server 定义 `ENABLE_EBPF_VERIFIER ENABLE_BPFTIME_VERIFIER` 并链接（`runtime/CMakeLists.txt:205-209`，`runtime/syscall-server/CMakeLists.txt:56-60`）；几个测试目录改为 FetchContent 拉 Catch2 | OFF |
| `BUILD_BPFTIME_DAEMON` | ON | `:98` | 根 `:259-261`（还要求 LIBBPF） | **OFF** |
| `BPFTIME_BUILD_KERNEL_BPF` | ON | `:101` | **没有任何地方读取**（installation.md 和 Makefile 里还在传它） | ON |
| `BPFTIME_BUILD_STATIC_LIB` | OFF | `:104` | 根 `:198-257` 生成 `libbpftime.a`（解开再重新打包 vm/llvm/ubpf/frida/trap/libbpf/syscall_trace/runtime/spdlog），并安装到 `~/.bpftime` | OFF |
| `BPFTIME_BUILD_WITH_LIBBPF` | ON | `:107` | 全局 `-DBPFTIME_BUILD_WITH_LIBBPF=1`（根 `:104-106`）；`include(cmake/libbpf.cmake)`（`:133-135`）；daemon、aot、runtime/object、kernel-user map 源文件、benchmark 都要求它 | ON |
| `ENABLE_PROBE_READ_CHECK` | ON | `:110` | 只给 runtime 定义 PRIVATE 宏（根 `:189-191`）和单测（`runtime/unit-test/CMakeLists.txt:75-77`）；源码 `runtime/src/bpf_helper.cpp` | **OFF** |
| `ENABLE_PROBE_WRITE_CHECK` | ON | `:112` | 同上（根 `:193-195`） | **OFF** |
| `TEST_LCOV` | OFF | 各子目录各自 `option()`（`bpftime-verifier/CMakeLists.txt:37` 等） | 给测试或可执行文件加 `--coverage` 一类参数；daemon 加在 `bpftime_daemon` 上（`daemon/CMakeLists.txt:75-78`） | OFF |
| `SPDLOG_ACTIVE_LEVEL` | 未定义时为 TRACE | 根 `:159-165` | 编译期日志级别；llvm-jit 子项目自己也有一份逻辑（`vm/llvm-jit/CMakeLists.txt:13-17`） | `SPDLOG_LEVEL_INFO`（命令行传入） |
| `BPFTIME_ENABLE_GDRCOPY` | 未定义 | 根 `:173-176`（不是 option） | 全局宏，源码 `runtime/src/bpf_map/gpu/nv_gpu_gdrcopy.cpp` | — |
| `BUILD_ATTACH_IMPL_EXAMPLE` | 未定义 | 根 `:265-267`（不是 option） | 编 `example/attach_implementation` | — |
| `BPFTIME_TRAP_ATTACH_BUILD_TESTS` | 未定义时为 ON | `attach/CMakeLists.txt:5-7` | trap 测试 | — |
| `FRIDA_DOWNLOAD_URL_PREFIX` | "" | `cmake/frida.cmake:4` | 下载镜像前缀 | "" |
| `BPFTIME_BPF_CLANG` | 用 find_program 依次找 clang、clang-20、clang-18 | `cmake/libbpf.cmake:111` | 编 `.bpf.c` | `/usr/bin/clang-19`（推断：命令行指定，因为 find_program 的名字列表里没有 clang-19；也可能是系统 `clang` 指向它，未确认） |
| `LLVM_DIR` | — | 给 find_package(LLVM) 用 | | `/usr/lib/llvm-19/lib/cmake/llvm` |
| `BPFTIME_BUILD_STANDALONE_VM` | 未定义 | `vm/CMakeLists.txt:31-34` | 单独编 vm 时引入 libbpf.cmake | — |
| llvm-jit 子项目：`ENABLE_LLVM_SHARED`、`BUILD_LLVM_AOT_CLI`、`BPFTIME_ENABLE_LLVM_PRELOAD`、`LLVMBPF_ENABLE_PTX/SPIRV` | NO/NO/OFF/OFF | `vm/llvm-jit/CMakeLists.txt:57-59,190-191` | 归 R5（VM）的范围 | 全 OFF |
| `BPFTIME_ROCM_ROOT` | — | `cmake/rocm.cmake:2` | `rocm.cmake` 没有被 include，**完全不起作用** | — |

另外有两条全局效果：根 `CMakeLists.txt:7-9` 给所有 C/C++ 编译加 `-fno-omit-frame-pointer`（`CMAKE_LINKER_FLAGS` 不是 CMake 的标准变量，那一行无效）；`:14` 全局定义 `LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1`；`:142` 设 `CMAKE_POSITION_INDEPENDENT_CODE YES`；`:129-131` 禁止在源码目录内构建；C99 / C++20（`:112-113`）。

### 5.2 构建产物总表（`BUILD_SHARED_LIBS` 未设置时）
| 产物（build 目录下） | CMake 目标 | 来源目录 | 用途 | 编译条件 | 安装到 |
|---|---|---|---|---|---|
| `runtime/syscall-server/libbpftime-syscall-server.so` | bpftime-syscall-server | runtime/syscall-server | 给 loader 用的 LD_PRELOAD 库，拦截 bpf() 等 | 总会编 | `~/.bpftime`（根 `:276-280`） |
| `runtime/agent/libbpftime-agent.so` | bpftime-agent | runtime/agent | 注入目标进程，执行程序、做 attach | 总会编 | `~/.bpftime` |
| `attach/text_segment_transformer/libbpftime-agent-transformer.so` | bpftime_text_segment_transformer | attach/text_segment_transformer | `-s` 时先预载的 syscall 改写器 | FRIDA | `~/.bpftime` |
| `runtime/libruntime.a` | runtime | runtime | 核心库（shm、handler、map、helper、prog） | 总会编 | — |
| `runtime/object/…`（OBJECT 库） | bpftime-object | runtime/object | 解析 bpf ELF | LIBBPF | — |
| `vm/vm-core/libbpftime_vm.a` | bpftime_vm | vm/vm-core | VM 抽象和工厂 | 总会编 | — |
| `vm/compat/llvm-vm/libbpftime_llvm_vm.a`、`libllvmbpf_vm.a` | bpftime_llvm_vm、llvmbpf_vm | vm/compat/llvm-vm、vm/llvm-jit | LLVM JIT/AOT | LLVM_JIT | — |
| `vm/compat/ubpf-vm/libbpftime_ubpf_vm.a`、`ubpf/lib/libubpf.a` | bpftime_ubpf_vm | vm/compat/ubpf-vm | uBPF | UBPF_JIT | — |
| `attach/frida_uprobe_attach_impl/libbpftime_frida_uprobe_attach_impl.a` | 同名 | attach/frida_uprobe_attach_impl | Frida uprobe | FRIDA | — |
| `attach/syscall_trace_attach_impl/libbpftime_syscall_trace_attach_impl.a` | 同名 | attach/syscall_trace_attach_impl | syscall tracepoint attach | UNIX 非 APPLE | — |
| trap_uprobe / simple / nv_attach 等 | 见 `attach/CMakeLists.txt` | attach/* | 其它 attach 后端 | 各自条件 | — |
| `tools/bpftimetool/bpftimetool` | bpftimetool | tools/bpftimetool | shm 检修工具 | 总会编 | `~/.bpftime`（`tools/bpftimetool/CMakeLists.txt:62`） |
| `tools/cli/bpftime` | bpftime-cli-cpp | tools/cli | 注入器 CLI（R3 负责） | FRIDA | `~/.bpftime` |
| `tools/aot/bpftime-aot` | bpftime-aot-cli | tools/aot | AOT 编译和运行 | LLVM_JIT 且 LIBBPF | `~/.bpftime` |
| `daemon/bpftime_daemon`、`daemon/libbpftime_daemon.a`、`daemon/embedfile`、`daemon/bpf_tracer.bpf.o`、`bpf_tracer.skel.h`、`placeholder.c` | 见 `daemon/CMakeLists.txt` | daemon | daemon | BUILD_BPFTIME_DAEMON 且 LIBBPF | `bpftime_daemon` 装到 `~/.bpftime` |
| `bpftime-verifier/libbpftime-verifier.a`、`bpftime_verifier_tests` | bpftime-verifier | bpftime-verifier | 用户态验证器 | ENABLE_EBPF_VERIFIER | — |
| `libbpftime.a` | bpftime_static_target | 根 | 单个静态大包 | BUILD_STATIC_LIB | `~/.bpftime` |
| `libbpf/libbpf.a`（以及 `libbpf/libbpf/` 下的 objdir 副本）、`bpftool/bpftool` | libbpf、bpftool（ExternalProject） | third_party/libbpf、third_party/bpftool | 链接、生成 skeleton | LIBBPF（bpftool 不是 EXCLUDE_FROM_ALL，**每次全量构建都会编**；libbpf 设了 `BUILD_ALWAYS TRUE`） | — |
| 测试：`bpftime_runtime_tests`、`bpftime_daemon_tests`、`bpftime_frida_uprobe_attach_tests`、`bpftime_syscall_trace_attach_tests`、`bpftime_trap_uprobe_attach_tests`、`bpftime_nv_attach_tests`、`bpftime_verifier_tests`、`bpftime_ubpf_lifetime_test`、llvm-jit 测试 | 各子目录 | — | 单测 | UNIT_TESTING 等 | — |
| `benchmark/simple-benchmark-with-embed-ebpf-calling` 等 | benchmark | benchmark | 基准 | LIBBPF（GPU 那几个另需 CUDA） | — |

注：安装目标写的是 `~/.bpftime` 或 `$ENV{HOME}/.bpftime`，**不受 `CMAKE_INSTALL_PREFIX` 控制**（根 `:274-280`，各 tools 的 CMakeLists）。用 `sudo cmake --install` 会装到 `/root/.bpftime`（推断）。

本机 `build-jit` 里实际存在的产物（已实跑 `find`）：agent.so、syscall-server.so、transformer.so、libruntime.a、vm 系列 .a、frida/syscall_trace attach 的 .a、`bpftimetool`、`bpftime_runtime_tests`、`bpftime_frida_uprobe_attach_tests`、libbpf。**没有** `tools/cli/bpftime` 和 `tools/aot/bpftime-aot`：它们有 ninja 规则，但没有被构建，说明当时只构建了部分 target（推断）。daemon 和 verifier 都没有配置进去。

### 5.3 从零构建的最小命令
官方最小命令（`installation.md:44-53,80-82`）：装依赖、`git submodule update --init --recursive`，然后
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO
cmake --build build --target install -j"$(nproc)"
```
默认就会打开 LLVM_JIT、UBPF、FRIDA（非 riscv）、LIBBPF、DAEMON，所以还需要 LLVM ≥ 15 的开发包、clang（编 `.bpf.c`）、能联网下载 frida devkit（`cmake/frida.cmake:69-88`，或者预先放进 `third_party/frida/`）。

从本机 `build-jit/CMakeCache.txt` 反推出来的配置命令（推断：`UNINITIALIZED` 类型的项，以及与默认值不同的 BOOL 项，都是命令行传入的）：
```bash
cmake -B build-jit -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DBPFTIME_LLVM_JIT=ON -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBUILD_BPFTIME_DAEMON=OFF \
  -DBPFTIME_ENABLE_LTO=ON \
  -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF \
  -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO \
  -DBPFTIME_BPF_CLANG=/usr/bin/clang-19
```
注意：这和 `08-practice.md:126-131` 推荐的命令**不一样**：本机关掉了 probe check、开了 LTO、把日志级别设成了 INFO。指南的实验 5.1 需要 DEBUG 日志、实验 6.3 要对比 probe check，**用本机 build-jit 做不了这两个实验**。写作者要么提醒读者自己按指南的命令重新构建，要么改写实验。

### 5.4 环境变量（本块读取或文档提到的）
| 变量 | 读取位置 | 作用 |
|---|---|---|
| `SPDLOG_LEVEL` | `spdlog::cfg::load_env_levels()`：`daemon/user/main.cpp:97`、`bpf_tracer.cpp:175`、`tools/bpftimetool/main.cpp:176`、`tools/aot/main.cpp:250` | 运行时日志级别（受编译期 `SPDLOG_ACTIVE_LEVEL` 限制） |
| `BPFTIME_LOG_OUTPUT` | `tools/bpftimetool/main.cpp:154`、`tools/aot/main.cpp:246` | 日志去向，默认 `~/.bpftime/runtime.log`，可设为 `console` |
| `BPFTIME_GLOBAL_SHM_NAME` | runtime 的 `get_global_shm_name()`（daemon、tool、aot 都会受影响） | shm 名，默认 `bpftime_maps_shm` |
| `BPFTIME_VERIFIER_LEVEL` | `runtime/src/bpftime_config.cpp:137-155` | STRICT / WARNING（默认）/ NO_VERIFY |
| `BPFTIME_RUN_WITH_KERNEL`、`BPFTIME_NOT_LOAD_PATTERN` | `runtime/syscall-server/syscall_context.cpp:179-193` | 交给内核加载（这时用的是内核 verifier）；按名字把程序替换成空程序（这条在 syscall-server 里，和 daemon 的 `enable_replace_prog` 是两套独立实现） |
| runtime 的其它配置变量 | `construct_runtime_config_from_env`（daemon `bpftime_driver.cpp:403`，bpftimetool import `:251`，aot run `:219`） | 例如 helper 组、max fd 等（R2/R3 负责） |
| `BPFTIME_DAEMON_CONFIG` | **没有任何代码读取**，只在 `daemon/README.md:119` 出现 | — |
| `BPFTIME_VM_NAME` | runtime/VM（R5 负责）；`tools/aot/test_run.py:36` 设为 `llvm` | 选择 VM |

## 6. 并发、生命周期与错误处理

daemon：
- 单线程：主循环在 `start_daemon` 里串行地 poll ringbuf，回调同步调用 driver，没有锁。`exiting` 是 `volatile sig_atomic_t`（`bpf_tracer.cpp:37`）。
- 内核侧的 enter/exit 配对都按 `pid_tgid`（线程）做 key，所以同一线程里的系统调用不会串。但是：
  - `insns_data`（`bpf_obj_id_fd_map.h:40`）和 `old_uprobe_path` / `new_attr_buffer`（`bpf_tracer.bpf.c:392-395`）都是**全局** `.bss` 变量。两个 CPU 同时执行 BPF_PROG_LOAD 或 perf_event_open 时会互相覆盖（推断，属于数据竞争）。
  - `bpf_map_new_fd_args_map` 和 `bpf_progs_new_fd_args_map` 只记"本线程最近一次"碰到的 id，exit 时不删（`bpf_tracer.bpf.c:287-288,306-307,337-338`）。BPF_LINK_CREATE 用的是"本线程最近一次加载的 prog id"，并不是 attr 里的 prog_fd（`:327-336`）。如果做 link 的线程以前从没加载过程序，就打出 "Unable to get prog id" 并**不发事件**，这个 link 会丢（推断）。
- 跨进程：daemon 以 `SHM_REMOVE_AND_CREATE` 建 shm，它就是 shm 的主人；目标进程用 `bpftime start` 带上 agent，打开同一块 shm（`daemon/README.md:86-112`）。agent 是否能接到 daemon **之后**才建的 uprobe，取决于 agent 侧的刷新机制，不在本块范围（未验证，交 R3/R4）。
- fork：daemon 不处理。
- 错误处理：sysfs 读取失败直接 `exit(1)`（`handle_bpf_event.cpp:42,61,450,456`）。`ring_buffer__poll` 出错只打日志，不退出（`bpf_tracer.cpp:285-289`）。回调返回负数时 libbpf 会中止本轮消费（例如 disable 一个没记录过的 fd 会返回 -1，`bpftime_driver.cpp:359-361`），这一轮剩下的事件要等下一次 poll 才处理（推断）。driver 里的内核临时 fd 都用 `BOOST_SCOPE_EXIT_ALL` 关闭，并保存/恢复 errno（`bpftime_driver.cpp:171-176,221-226`）。
- 资源：`bpf_prog_insns_map` 的 value 约 32KB，max_entries 10240，HASH map 默认预分配，**仅这一张 map 就会占用约 320MB 内核内存**（推断，按 htab 预分配估算）。

verifier：
- 所有配置都是 thread_local（`platform-impl.cpp:101-103`），PREVAIL 自己的状态也是 thread_local（`global_program_info`）。syscall-server 只在第一个触发 `start_up` 的线程里设置 helper 列表（`std::call_once`，`syscall_server_utils.cpp:149`），map 描述符写在建 map 的线程里，验证在加载程序的线程里做。三者不在同一线程时，就会出现假的验证失败（WARNING 档下只是告警）。
- 异常：`get_program_type` 抛出的异常不会被 `verify_ebpf_program` 捕获，会一直传到 syscall-server 的 `bpf()` 处理函数里。但 syscall-server 只会传 "tracepoint" 或 "uprobe"，所以实际触发不到（推断）。

bpftimetool / aot：
- 都是短命进程，单线程。`run` 复制出来的 `bpftime_prog` 放在进程堆上，用完就丢。aot 的 `compile` 和 `load` **直接写** shm 里的 `aot_insns`，没有加任何锁（`tools/aot/main.cpp:135-137,174-177`）；如果同时有 agent 在读，就有竞争（推断）。
- import 遇到不支持的 handler 时中途 `return -1`，**前面已经导入的不会回滚**（已实跑，见 §9）。

## 7. 不直观的行为、疑似 bug、代码与注释不一致

1. **daemon `-u` 永远失败**：`daemon/user/main.cpp:58` 写的是 `if (errno || uid < 0 || uid >= -1)`，任何非负 uid 都满足 `uid >= -1`，于是总是走 usage 退出。而且 `env.uid` 从来没被使用（内核里没有 uid 过滤）。README 却宣传 `sudo bpftime_daemon -u 1000`（`daemon/README.md:132-133`）。确定程度：确认（读代码）。
2. **daemon 不加 `-p` 时劫持全系统的 uprobe**：`target_pid=0` 时 `filter_target` 只排除 daemon 自己（`bpf_kernel_config.h:28-40`）。这期间机器上任何进程（bpftrace、perf、其它 libbpf 工具）用 perf_event_open 创建的 uprobe，都会被改写到 `/a`、在内核里失效。确定程度：推断（逻辑上很直接）。
3. **只接管 uprobe，不接管 kprobe/tracepoint/legacy uprobe**：内核侧只比较 `attr.type == uprobe_perf_type`（`bpf_tracer.bpf.c:407`）；`kprobe_perf_type` 写了但没有读；用户态其它类型一律 "Unsupported perf event type"（`handle_bpf_event.cpp:342-345`）。通过 tracefs `uprobe_events` 创建的旧式 uprobe（`PERF_TYPE_TRACEPOINT`）也不会被接管。确认。
4. **map 类型只支持 4 种**：driver 统一把内核类型加 1000（`bpftime_driver.cpp:236`），runtime 只实现了 HASH / ARRAY / PERCPU_ARRAY / PERF_EVENT_ARRAY 的 kernel-user 版本（`runtime/include/bpftime_shm.hpp:113-120`）。用 RINGBUF（类型 27 → 1027）的现代 libbpf 工具，map 建不出来，整个程序也就挂不上（推断：没看 map_handler 的 default 分支到底怎么报错，交 R6 确认）。
5. **prog id 和 map id 共用 shm 槽位，可能冲突**：程序用内核 prog id 当槽位（`bpftime_driver.cpp:197`），map 用内核 map id 当槽位（`:254`）。内核里 prog id 和 map id 是**两套独立的 IDR**，同一个数字可以同时是 prog 和 map；uprobe 又用 `find_minimal_unused_fd` 抢小号槽位（`:320`）。一旦撞号，`bpftime_is_map_fd(id)` 或 `is_prog_fd(id)` 就会误判"已存在"，或者创建失败。另外内核 id 超过 `max_fd_count`（默认 6144，`runtime/include/bpftime_config.hpp:23`）时也会失败。确定程度：推断。
6. **LINK_CREATE 依赖"同一线程先加载过程序"**：见 §6。推断。
7. **全局 `.bss` 缓冲有数据竞争**：见 §6。推断。
8. **`bpf_prog_insns_map` 约 320MB 预分配**：见 §6。推断。
9. **`process_exec_maps` 遇到一个短命进程就 `return 0`**（`bpf_tracer.cpp:143-146`），导致整轮遍历中止、`pid_map` 不更新。默认 `enable_auto_attach=false`，exec 程序不加载，所以平时不会触发。确认（读代码），影响很小。
10. **配置项写了但没有入口**：`enable_replace_prog`、`submit_bpf_events`、`enable_auto_attach`、`duration_ms`、`is_driving_bpftime` 都没有命令行参数；`bpftime_cli_path`、`bpftime_tool_path` 没人读（`daemon_config.hpp:29-48`）。`main.cpp:96` 把 argv[0] 拷进 `new_uprobe_path`，随后被 `/a` 覆盖（`bpf_tracer.cpp:196-202`）。确认。
11. **声明了却没有实现的函数**：`bpftime_tracepoint_create_server` 只在 `bpftime_driver.hpp:75-76` 声明，没有定义。`bpftime_btf_load_server`（`bpftime_driver.cpp:385-396`）和 `get_kernel_bpf_prog_insns`（`:31-48`）都没人调用。`handle_bpf_event` 的 `BPF_PROG_LOAD` 分支（`handle_bpf_event.cpp:274-280`）是死代码，因为内核从不为 PROG_LOAD 发 SYS_BPF 事件（`bpf_tracer.bpf.c:281-297`）；而且它用 `bpf_prog_type_strings[attr.prog_type]` 时没有做边界检查。确认。
12. **白名单按什么匹配**：`-w` 的帮助文字写的是 "function addresses"（`main.cpp:29-30`），内核实际比较的是 `probe_offset`，即**文件偏移**（`bpf_tracer.bpf.c:410-411`）。确认。
13. **`/a` 占位文件**：只有 x86-64 版本（`daemon/assets/placeholder`，`file` 输出是 x86-64 PIE），在 aarch64 上也会写这个文件。内核注册 uprobe 时只需要文件存在、偏移合法，所以多半不影响（推断）。daemon 会在根目录留下 `/a`，而且从来不删。
14. **daemon 测试的 CMake 里有死代码**：`daemon/test/CMakeLists.txt:49-60` 遍历一个空列表，循环体里还引用了 `bpftime_runtime_tests`；`common_def.hpp` 没被用到；`test_daemon.cpp` 是空用例。确认。
15. **verifier 实际几乎不验证 uprobe 程序**：libbpf 给 `SEC("uprobe")` / `SEC("uretprobe")` 设的 prog_type 是 KPROBE（`third_party/libbpf/src/libbpf.c:9832-9836`），而 syscall-server 只验证 TRACEPOINT 和 SOCKET_FILTER（`syscall_context.cpp:640-656`）。结果真正会进验证器的，是 tracepoint 程序，以及 libbpf 做特性探测时加载的 SOCKET_FILTER 小程序（推断）。注释 `// uprobe/uretprobe -> BPF_PROG_TYPE_SOCKET_FILTER`（`:642`）和 libbpf 的实际行为不一致。确定程度：确认（libbpf 源码）；端到端没有实跑。
16. **ufunc helper 组加错了**：`syscall_server_utils.cpp:200-206` 在 `enable_ufunc_helper_group` 分支里加的仍然是 `get_shm_maps_helper_group()` 的 id，没有加 ufunc 组的 id。结果 ufunc helper 有原型，却不在 usable 集合里，会被判成 "invalid helper function id"。确定程度：推断（交 R3 确认）。
17. **verifier 测试的越界读**：`bpftime-verifier/test/simple.cpp:30-31` 把 `sizeof(bad_ebpf_program)`（16 字节）当成指令条数传入，实际读了 16 条，越界。测试本身期望失败，所以能通过。确认。
18. **verifier README 示例写不出来**：`bpftime-verifier/README.md:62` 是 `std::optional<std::string> result = auto ret = ...`，语法错误；`:13` 用的变量 `${BPFTIME_VERIFIER_INCLUDES}s` 名字不对（实际是 `BPFTIME_VERIFIER_INCLUDE`）。确认。
19. **bpftimetool export 出来的 prog type 是垃圾值**：`bpf_prog_handler` 的构造函数没有给 `type` 赋值（`runtime/src/handler/prog_handler.cpp:9-19`），export 原样写出（`bpftime_shm_json.cpp:302`）。已实跑：用 `load` 导入 `type:2`，export 出来是 `1307815640`。官方示例 `daemon/test/asserts/malloc.json` 里也是 `1516411092`。只影响 JSON。确认（交 R2 记录）。
20. **export 和 import 不对称**：export 会写 `epoll_handler` 和 `memfd_handler`（`bpftime_shm_json.cpp:336-355`），import 不认识这两种，中途 `-1` 退出，前面已导入的部分保留（`:105-221`）。prog 导出时**没有** `attach_fds`，import 却会去读它（`:130`；nlohmann 对缺失的 key 返回 null，遍历 null 是空循环，所以不报错）。link 导出的是 `h.prog_id/attach_target_id`。已实跑：导入含 memfd 的 JSON，返回 255，只剩 fd 3。确认。
21. **bpftimetool README 的 JSON 格式写错了**：README 写 `{"handlers":[{"fd":3,...}]}`（`tools/bpftimetool/README.md:201-241`），实际是 `{"3":{...}}`；README 也没介绍 `load` 子命令。确认。
22. **`bpftimetool run` 参数解析**：`argc==5`（`run id file repeat`，少一个值）时多出来的参数被**悄悄忽略**（`main.cpp:278-297` 只在 `argc>5` 时解析），`argc==7` 时 argv[6] 同样被忽略；`repeat` 必须写在 `type` 前面。已实跑：`run 3 in.bin repeat` 返回 0，按默认值执行。确认。
23. **`run ... type INTERPRET` 在默认 llvm VM 下其实是 JIT**：`bpftime_prog_load(false)` 之后，`ebpf_exec` 进入 `llvmbpf_vm::exec`，没有编译过的函数时会现场 `compile()`（`vm/llvm-jit/src/vm.cpp:145-160`）。已实跑：INTERPRET 跑 1 次耗时 3.8ms，基本就是 JIT 编译时间；跑 1000 次平均 3.9µs。把 `BPFTIME_VM_NAME` 设成 `ubpf` 后，1000 次平均仍然是 4.1µs，看不出差别，原因未确定（交 R5）。所以"INTERPRET 档"给出的数字不能拿来当解释器性能。
24. **aot `compile` 只编第一个程序**：`tools/aot/main.cpp:138` 的 `return 0` 在循环里面。README 说的是"编译 shm 里的程序"（复数）。确认。
25. **aot `build` 不做 map 重定位**：只调用了 `bpf_object__open`，没有 load，所以 `bpf_program__insns` 里的 LDDW map 引用还没有被填成可用的 fd（推断，依据 libbpf 的语义）。带 map 的程序 build 出来后，`__lddw_helper_map_by_fd` 拿到的 id 没有意义。README 的说法也是"从 shm compile 才会做重定位"。
26. **aot `-e` 不会生成 `.ll`**：见 §3.8。确认。
27. **aot 示例跑不起来**：`tools/aot/example/main.c` 缺 `#include <stdarg.h>` 和 `_GNU_SOURCE`，clang-19 报 3 个 error（已实跑）；`:53` 的 `uint64_t* value_ptr = (uint64_t*)value_ptr;` 是用自己初始化自己，补齐头文件后运行会段错误，rc=139（已实跑）。把它改成 `(uint64_t*)value` 后能正常输出 `counter_map: 1`（已实跑，改的是 scratchpad 里的副本）。`va_start(args, fmt_str)` 用的不是最后一个具名参数，属于未定义行为，被 pragma 压掉了警告。
28. **构建系统里的几处坑**：
    - `BPFTIME_ENABLE_FRIDA` / `BPFTIME_ENABLE_TRAP_UPROBE` 每次 configure 都按架构 FORCE 覆盖（`StandardSettings.cmake:76-82`），用户在 x86 上传 `-DBPFTIME_ENABLE_FRIDA=OFF` 不起作用。`vm/compat/CMakeLists.txt:6` 会再 include 一次 StandardSettings，再执行一遍 FORCE。确认。
    - `BPFTIME_ENABLE_CCACHE` 不起作用，找到 ccache 就用（`:63-68`）；`BPFTIME_BUILD_KERNEL_BPF`、`BPFTIME_BUILD_EXECUTABLE`、`BPFTIME_USE_CATCH2`、`BPFTIME_VERBOSE_OUTPUT` 都是死选项；`BPFTIME_CUDA_ROOT` 用 `option()` 声明成了 BOOL；`cmake/rocm.cmake` 没人 include。确认。
    - 根 `CMakeLists.txt:92` 的 "Enabling ubsan for Debug builds" 只是一句日志，没有对应的编译参数。确认。
    - 根 `:231` 用了不存在的变量 `${CMAKE_CURRENT_BUILD_DIR}`，展开后是相对路径 `libbpf/libbpf/libbpf.a`。因为那条自定义命令的 `WORKING_DIRECTORY` 是构建目录，所以碰巧能用（推断）。
    - Makefile 的 `unit-test-runtime` 目标里，`export BPFTIME_VM_NAME=llvm` 单独占一行（`Makefile:48`），每行 recipe 是独立的 shell，这个 export 对下一行无效。确认。
    - Makefile 的 `benchmark` 目标写死了 `-DLLVM_DIR=/usr/lib/llvm-15/cmake`（`:83,93`），`release-with-llvm-jit` 和 `release` 效果一样（LLVM_JIT 默认就是 ON）；`build-wo-libbpf` 的注释抄成了 "iouring"（`:62`）。确认。
    - 安装位置写死为 `~/.bpftime`，不受 prefix 控制（见 §5.2）。
29. **daemon README 与代码不符**：`BPFTIME_DAEMON_CONFIG`（`README.md:119`）不存在；"Adjust duration_ms in config"（`:185`）没有入口；"Can filter by PID/UID"（`:158`）中 UID 不可用；"Uses boost::interprocess shared memory at /dev/shm/bpftime_maps_shm"（`:140`）是默认名，可以被 `BPFTIME_GLOBAL_SHM_NAME` 改掉。确认。
30. **CI 覆盖的空白**：verifier 的 CI 只编 `bpftime_verifier_tests`（`test-verifier.yml:47-51`），**没有一个 job 在 `ENABLE_EBPF_VERIFIER=ON` 下编 syscall-server**，所以验证器和 syscall-server 的集成路径没有被 CI 编译过。daemon 只在 `build-gcc13.yml:100-117` 里以 `LLVM_JIT=OFF` 编译并跑 `bpftime_daemon_tests`，内核劫持链没有任何端到端测试。确认（读 yml）。

## 8. 对照现有指南

对照章节：`07-periphery.md`（全部）、`08-practice.md` 的 8.2 环境准备（`:88-260`）。

### 错误
- `07-periphery.md:228`（7.2 "一个容易踩的坑"）说"线程 B 看到的是空配置，任何 helper 调用都会被判为 'Unusable helper'"。实际上 unmarshal 会先调 `is_helper_usable`，抛出的是 `InvalidInstruction`，用户看到的信息是 **"invalid helper function id"**（`ebpf-verifier/src/asm_unmarshal.cpp:357-358`；`bpftime-verifier/test/map.cpp:124-125` 断言的就是这条）。"Unusable helper"（`platform-impl.cpp:42`）只有在 `get_helper_prototype` 被直接调用时才会出现。深入参考表（`:498`）里把 "Unusable helper" 标成入口，也应该一起改。
- `07-periphery.md:194` 和 `:458` 说"其它 map 类型一律抛异常，这时不是'验证失败'而是'不支持'，换类型或关掉验证即可"。实际 `get_map_type` 是在抽象解释阶段被调用的，抛出的 `runtime_error` 会被 `crab_verifier.cpp:179-190` 捕获并**变成一条验证 warning**。所以 WARNING 档下照常加载，STRICT 档下按验证失败处理。只有不支持的 **section/程序类型**才会真的抛到封装外面，而 syscall-server 从不传这种 section。
- 深入参考里 `verify_ebpf_program` 的行号是 `:77-104`，实际函数范围是 `bpftime-verifier/src/bpftime-verifier.cpp:64-103`（`:77-82` 是 `prog.info` 那一段）。其它 daemon 相关行号我抽查了 8 处，全部正确。
- `08-practice.md:126-131` 推荐的构建命令和本机 `build-jit` 的实际配置不一致（见 §5.3）。`08-practice.md:736` 自己也承认 build-jit "不带 daemon"，但没提它关了 probe check、开了 LTO、日志级别是 INFO，这三点会影响实验 5.1 和 6.3。

### 遗漏
- **bpftime-aot 完全没有讲**（07 里搜不到 "aot"）。四个子命令、"compile 只编第一个程序"、"run 不建 shm"、`-e` 的真实效果、示例无法编译，这些都没有。
- **bpftimetool 的细节**：JSON 的真实格式（以 fd 为 key 的对象）、export/import 不对称、prog type 是垃圾值、`run` 是把整个数据文件当 ctx 并报告平均耗时、INTERPRET 在 llvm VM 下其实是 JIT、AOT 需要先 `bpftime-aot compile/load`、argc 解析会静默忽略参数。
- **daemon 的适用范围和限制**：只接管 uprobe（不管 kprobe/tracepoint）；不加 `-p` 时影响全系统；map 类型只支持 4 种 kernel-user map（ringbuf 不行）；prog id 和 map id 可能撞槽位；`-u` 坏了；它是 shm 的创建者，用的是 REMOVE_AND_CREATE，会清掉已有会话；prog/map/link 在 shm 里只增不减；需要 root（写 `/a`、读 sysfs、加载内核程序），而 kernel-user map 在目标进程里还要 `bpf_map_get_fd_by_id`，所以目标进程大概率也得有特权。
- **daemon 对内核指令做的是"原始指令"重定位**：只处理 `src_reg` 为 1 或 2 的 LDDW，`CALL` 原样保留。指南"细节与坑"提到了 MAP_IDX(5) 不处理，但没有提 fd_array 和 kfunc 等新特性同样不处理。
- **`enable_replace_uprobe=false` 时 `process_perf_event_open_enter` 也返回 1**（只上报、不改写，内核 uprobe 和用户态 uprobe 同时生效）。
- **构建系统**：CMake 选项总表、FORCE 覆盖、死选项、安装路径写死、bpftool 和 libbpf 每次都会被构建，指南都没有讲。08 只讲了 4 个开关。
- **CI 怎么测**（可以给学习者一张"官方怎么测"的表）：
  | workflow | 构建配置 | 跑什么 |
  |---|---|---|
  | `test-runtime.yml:114-128,258-274` | Debug，LLVM_JIT，UNIT_TESTING，STATIC_LIB，probe check 开；另有一组 MPK | `bpftime_runtime_tests`；`example/malloc` 端到端（`:146-155`）；Ubuntu 22.04 / Fedora 39 / openEuler |
  | `test-attach.yml:66-100` | Debug，TEST_LCOV，不编 daemon | frida 和 syscall_trace 两个 attach 测试 |
  | `test-examples.yml:76-116` | RelWithDebInfo，JIT 开和关两套，日志输出到 console | 编译并运行 `example/*` |
  | `test-bpftrace.yml:93-100,185,537` | RelWithDebInfo + JIT | 用 bpftrace 加 syscall-server 跑 openat、malloc uprobe |
  | `test-tools.yml:50-62` | RelWithDebInfo + JIT | `test_run.py`（aot run）、`test_load.py`（bpftimetool load） |
  | `test-verifier.yml:47-51` | Release，LLVM_JIT=NO，VERIFIER=YES，Ninja | 只跑 `bpftime_verifier_tests` |
  | `build-gcc13.yml:67-117` | gcc-13，LLVM_JIT=OFF；另有无 libbpf 的一组；daemon 一组 | 只编译；`bpftime_daemon_tests` |
  | `test-riscv64.yml:79-120` | 交叉编译到 riscv64，TRAP_UPROBE | qemu 下跑 trap 测试和 runtime 测试 |
  | `test-vm.yml:54-56` | `make build-vm` | `ctest --test-dir vm/build` |
  | `test-nginx-attach.yml:48-55` | BUILD_ATTACH_IMPL_EXAMPLE | nginx attach 示例 |
  | `test-ptxpass.yml`、`test-gpu-examples.yml` | CUDA，自托管 GPU 机器 | PTX pass 和 GPU 示例 |
  | `benchmarks.yml:81-91` | `make benchmark` | uprobe、syscall、mpk 基准 |
  | `docker.yml` | — | 构建并推送镜像 |
- installation.md 的错误，指南可以提醒一句：工具名写成了 "bpftime-vm"（`installation.md:64`，实际叫 `bpftime-aot`）；"(With ubpf jit)" 两节其实也默认打开了 LLVM JIT（`:128-140`）；`ENABLE_PROBE_READ_CHECK` 的说明写成了 "bpf_probe_write"（`:120`）；`BPFTIME_ENABLE_CCACHE "Default to OFF"` 但实际上会自动启用；`BPFTIME_BUILD_KERNEL_BPF` 不起作用。

### 讲得好、可以保留的部分
- 7.1 "两层 eBPF" 的划分，以及"daemon 替代的是 loader 一侧，目标进程仍要 agent"，都准确。
- uprobe 改写的时序图和代码节选准确：备份在改写之前，exit 时先 submit 再还原，写入长度取原路径长度。
- `relocate_bpf_prog_insns` 的直觉解释和行号（`bpftime_driver.cpp:73-125`、LDDW `:101-112`）都正确。
- 7.2 的三档 `BPFTIME_VERIFIER_LEVEL`、thread_local 的三处线程归属、"verifier 通过 ≠ 运行正确"，都准确。"细节与坑"里已经点出 KPROBE 类型的 uprobe 不会被验证（`07-periphery.md:550`，标了"推断，未实测"），我用 libbpf 源码（`libbpf.c:9832-9836`）确认了这一点，可以把"推断"去掉，并提到正文里。
- `test_daemon.cpp` 的 `#error` 条件永远不成立，这个分析正确。
- 8.2 的产物表、两个关键开关（probe check、SPDLOG_ACTIVE_LEVEL）、make 目标对照表都准确（我核对了 `Makefile:35-103`）。

## 9. 学习建议

### 推荐阅读顺序
1. `daemon/bpf_tracer_event.h`：先认识事件和 fd 表的结构。
2. `daemon/kernel/bpf_kernel_config.h` → `bpf_tracer.bpf.c` 的 perf_event_open 一对（`:391-553`）→ bpf 一对（`:125-389`）→ ioctl / close（`:555-670`）→ `bpf_obj_id_fd_map.h` 的两个 kprobe。
3. `daemon/user/bpf_tracer.cpp` 的 `start_daemon`（`:168-300`）。
4. `daemon/user/handle_bpf_event.cpp` 的 `handle_event`（`:408`）→ `handle_perf_event_open`（`:300`）→ `handle_ioctl` / `handle_bpf_event`。
5. `daemon/user/bpftime_driver.cpp` 的 `bpftime_attach_perf_to_bpf_server`（`:279`）→ `bpftime_progs_create_server`（`:163`）→ `relocate_bpf_prog_insns`（`:73`）→ `bpftime_maps_create_server`（`:214`）。
6. verifier：`include/bpftime-verifier.hpp` → `src/bpftime-verifier.cpp:64-103` → `src/platform-impl.cpp` → 回到 `runtime/syscall-server/syscall_context.cpp:628-703` 看它在哪里被调用。
7. 工具：`tools/bpftimetool/main.cpp:149` 起的 `main`；`tools/aot/main.cpp:241` 起的 `main`。
8. 构建：`cmake/StandardSettings.cmake` → 根 `CMakeLists.txt` → `cmake/libbpf.cmake:110-154`（`.bpf.c` 怎么编）→ `tools/CMakeLists.txt`。

### 动手实验
1. **bpftimetool 往返**（已实跑；不需要 root；记得换 shm 名）：
   ```bash
   export BPFTIME_GLOBAL_SHM_NAME=lab_$RANDOM BPFTIME_LOG_OUTPUT=console
   T=build-jit/tools/bpftimetool/bpftimetool
   $T load 3 '{"type":"bpf_prog_handler","name":"ret42","attr":{"type":2,"cnt":2,"insns":"b70000002a0000009500000000000000"}}'
   printf abcd > in.bin
   $T run 3 in.bin repeat 1000 type JIT     # 预期 Return value: 42
   $T export out.json && cat out.json        # 预期 key 是 "3"；"type" 是一个大数（垃圾值，见 §7-19）
   $T run 3 in.bin type AOT                  # 预期 "AOT instructions not found"，rc=1
   $T remove
   ```
   关键输出：`Time taken: 3 ns / Return value: 42`（JIT，1000 次）；INTERPRET 跑 1 次 `Time taken: 3832784 ns`；export 出来是 `"type": 1307815640`。
2. **import 不对称**（已实跑）：准备一个 JSON，fd 3 是 prog，fd 4 是 `memfd_handler`，fd 5 是 map。`import` 返回 255；再 export，只剩 `['3']`。
3. **load 参数校验**（已实跑）：`python3 tools/bpftimetool/test_load.py build-jit/tools/bpftimetool/bpftimetool`，输出 `PASS: bpftimetool load argument handling`。
4. **AOT 产物是什么**（已实跑）：`nm tools/aot/example/do_count.o`，可以看到 `T bpf_main` 和 `U _bpf_helper_ext_0001/0002/0006/0014`、`U __lddw_helper_map_by_fd`。这样就能理解 AOT 的 ABI：入口叫 `bpf_main`，helper 和 map 重定位都变成外部符号。想链接 `example/main.c`，要补上 `#include <stdarg.h>`、`-D_GNU_SOURCE`，并修掉 `:53`，才会输出 `counter_map: 1`。
5. **看 CMake 选项实际取值**：`grep -E '^(BPFTIME|ENABLE|BUILD)_' build-jit/CMakeCache.txt`，对照 §5.1；再试 `cmake -B /tmp/x -DBPFTIME_ENABLE_FRIDA=OFF ...`，看到缓存里仍然是 ON（未实跑，因为规则禁止重新 configure；预期来自 `StandardSettings.cmake:80-81` 的 FORCE）。
6. **daemon**（需要 root、需要单独构建 daemon，**未实跑**）：`cmake -DBUILD_BPFTIME_DAEMON=ON ...` 之后按 `daemon/README.md` 操作：`sudo SPDLOG_LEVEL=Debug build/daemon/bpftime_daemon -p <loader 的 pid>`（一定要带 `-p`，否则影响全系统）。用 `sudo cat /sys/kernel/debug/tracing/trace_pipe` 可以看到 `bpf_printk` 打出的 "Send perf event at offset ..."。注意它会删掉现有的 shm，并在根目录写一个 `/a`。

### 学习者最容易误解的点
1. **daemon 不是"把内核程序搬进用户态"那么通用**：它只管 uprobe 类 perf event，map 只支持 4 种；而且内核里的原程序**仍然加载着**，只是挂在 `/a` 上不触发（`enable_replace_prog` 默认关）。
2. **daemon 不负责注入目标进程**：目标进程仍要靠 `bpftime start`（agent）启动；daemon 只扮演 loader 一侧（相当于 syscall-server），而且它是 shm 的创建者。
3. **verifier 默认不存在；存在时也基本不验证 uprobe 程序**（KPROBE 类型被跳过）；默认档位只告警。
4. **`bpftimetool run type INTERPRET` 不一定是解释执行**；`run` 跑的是 shm 里程序的**副本**，不会影响 agent。
5. **`-DBPFTIME_ENABLE_FRIDA=OFF` 之类的开关可能根本不起作用**；安装位置总是 `~/.bpftime`。

## 10. 未读完/未确定的地方

- `.github/workflows/*.yml` 只略读了构建和测试命令，没有逐行读（特别是 `test-bpftrace.yml` 1051 行、`test-gpu-examples.yml` 857 行）。
- 子模块 ebpf-verifier 只看了被调用的接口和异常捕获点，没读验证算法。
- daemon 整条链路**没有实跑**：本机 build-jit 没编 daemon，实跑又需要 root、会删 shm、会写 `/a`，这些都与"不重新 configure/build"的规则冲突。§7 里第 2、4、5、6、7、8 条都是推断。
- 待其他读者确认：
  - agent 能否接到 daemon **后来才建**的 uprobe 和 link（R3/R4）。
  - kernel-user map 遇到不支持的类型（如 1027）时，`map_handler` 的具体失败方式（R6）。
  - `bpf_prog_handler::type` 没有初始化（R2，`runtime/src/handler/prog_handler.cpp:9-19`）。
  - `BPFTIME_VM_NAME=ubpf` 下 bpftimetool INTERPRET 的耗时为什么仍是每次约 4µs；VM 是怎么选出来的（R5）。
  - syscall-server 的 ufunc helper 组 id 加错（R3，`syscall_server_utils.cpp:200-206`）。
  - `bpftime_find_minimal_unused_fd` 和外部指定槽位（kernel id）混用时的语义（R2）。
- `cmake/libbpf.cmake` 里 libbpf 的 ExternalProject 到底把 `libbpf.a` 装到哪里：本机两个位置都有文件（`build-jit/libbpf/libbpf.a` 和 `build-jit/libbpf/libbpf/libbpf.a`），没有细究 libbpf Makefile 的 install 规则。
- `BPFTIME_BPF_CLANG=/usr/bin/clang-19` 是命令行传进来的，还是系统 `clang` 指向它，没有确认。
- 根 `CMakeLists.txt` 之外各子目录 CMakeLists 的细节（例如 runtime 的完整链接列表、agent 的 version script）只看了产物和开关，没有逐行读。
