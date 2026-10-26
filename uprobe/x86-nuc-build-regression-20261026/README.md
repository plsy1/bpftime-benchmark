# x86 NUC：官方 177a1dc 与开发分支的构建和回归（2026-10-26）

候选目标 1（交接说明第 4 节）：在 x86 上编译官方 `177a1dc` 和开发分支 `codex/uprobe-official-177a1dc465ff`，
跑每日过滤器、完整 runtime 单测和 Frida 挂钩测试，确认 Jetson 上的修复与优化在 x86 上也成立。只做构建和正确性，不计时。

## 机器与构建

- `debian-ichi-nuc`：Intel i5-1240P（CPU 0–7 为 4 个 P 核 × 2 线程，8–15 为 E 核），Debian 13，内核 `6.12.113+deb13-amd64`（BTF）。
  频率未固定（只构建和跑单测，没有计时），见 `environment.txt`。
- 工具链：gcc-13.3、**LLVM/clang 19.1.7**（Jetson 为 LLVM 16、BPF clang 15；Debian 13 没有 LLVM 16）、cmake 3.31、Ninja、Boost 1.83（与 Jetson 相同）、Frida 16.1.2 x86_64 devkit。
- CMake 选项：与 Jetson 基线 `baseline-177a1dc-jit-llvm16-20261021/run-build.sh` 相同（RelWithDebInfo、LTO、LLVM JIT + ubpf、单测 ON、
  probe read/write 检查 OFF、verifier/daemon/AOT CLI OFF、spdlog INFO），只换了 LLVM 路径和 BPF clang。逐项记录在 `*/cmake-options.txt`；
  两边只差开发分支独有的 `BPFTIME_ENABLE_MAP_PRODUCTION_AB`、`BPFTIME_PATCH_EXECUTABLE`。
- 源码：`official/source-state.txt`（`177a1dc465ff…`）、`dev/source-state.txt`（`a7d54d7eb7a0…`），两边 `vm/llvm-jit` 都是干净的 `87bac39a`。
- 脚本：`run-build.sh`（配置 + 构建）、`run-tests.sh`（过滤器、整套 runtime、Frida 各跑一次）、`run-each-case.py`（每个用例单独一个进程）。

## 结果

| 检查 | 官方 177a1dc | 开发分支 a7d54d7e |
|---|---|---|
| 配置 + 构建（LLVM 19） | 通过 | 第一次配置失败（见下文第 1 条），传 `-DDIAGNOSTIC_BPF_CLANG` 后通过；llvmbpf 补丁 overlay（`compiler_local_call.cpp`、`llvm_jit_context_array_inline.cpp`）在 LLVM 19 上照常生成 |
| 每日过滤器（`test-filter.txt`） | 34 个用例 / 55,119 个断言全过（`[ratelimit]` 无匹配，属正常） | **110 个用例 / 287,652 个断言全过**（Jetson 也是 110 个） |
| Frida 挂钩测试，逐用例 | 17/17 | **19/19**，含 x86 走 gum 回溯器的 uretprobe 取栈测试（`#else` 分支，Jetson 上从没跑过）和 PAC 去签名在无 PAC 平台上的恒等测试 |
| 完整 runtime 单测，一个进程 | 第 2 个用例 SIGSEGV 退出 | 第 5 个用例 SIGSEGV 退出 |
| 完整 runtime 单测，逐用例 | 104/106 | 180/182 |

逐用例的 2 个失败两边相同，都不是开发分支引入的：

- `Test tail calling from userspace to kernel`：要建内核 map，普通用户 `map_fd = -1`；用 sudo 跑两边都通过（10 个断言）。
- `Test attach replace with ebpf`（SIGSEGV，官方也有）：测试在构造 `frida_attach_impl`（其中调用 `gum_init_embedded`）**之前**
  就通过 `attach::find_function_addr_by_name` 调用 `gum_find_function`，此时 GLib slice 分配器尚未初始化，崩溃位置是
  `gum_find_function → g_hash_table_new_full → g_slice_alloc`（用预加载的 SIGSEGV 回溯库得到）。生产路径只在 gum 初始化之后调用这个函数，
  所以这是测试本身的问题。Jetson 只跑每日过滤器，这个用例不在里面。

## 发现

1. **开发分支在只有 clang-19 的机器上配置失败**：`benchmark/CMakeLists.txt:59` 的
   `find_program(DIAGNOSTIC_BPF_CLANG NAMES clang-16 clang-15 clang REQUIRED)` 只是诊断用 benchmark 的依赖，却能让整个配置失败。
   这次构建通过显式传 `-DDIAGNOSTIC_BPF_CLANG=/usr/bin/clang-19` 绕过，没有改源码；同样交给 Jetson 会话。
2. **`d766e6ad` 没有真正让 `bpf_get_stackid` 的错误保持负数**（与架构无关）：`runtime/src/bpf_helper.cpp:1224`
   `return ret < 0 ? ret : (uint32_t)ret;` 中 `int` 与 `uint32_t` 的公共类型是 `unsigned int`，两个分支相同（gcc-13 `-Wduplicated-branches` 警告），
   返回 `int64_t` 时 `-EEXIST` 是 `0x00000000ffffffef`（4294967279），而不是内核的 `-17`。用 `int` 接收返回值的程序不受影响，
   用 64 位比较 `< 0` 的程序会把它当成有效 id。官方 `return (uint32_t)ret;` 同样如此。现有测试只覆盖 map 层的 `fill_stack_trace`，没覆盖 helper 返回值。**按用户分工 NUC 上不修，已交给 Jetson 会话**（`summry/jetson` 分支 `x86-nuc/bpftime-x86-nuc-build-regression-20261026.md` 第 4 节）。
3. 开发分支另有一个新警告 `bpf_attach_ctx.cpp:114` 有符号与无符号比较（`insn.imm == helper_id`），暂未分析。`ebpf_inst` ODR 警告两边都有，属于已知的老问题。
