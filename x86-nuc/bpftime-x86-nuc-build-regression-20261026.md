# x86 NUC：构建与回归（2026-10-26）及交给 Jetson 的问题

NUC 会话按交接说明第 4 节候选目标 1 做的第一件事：在 x86 上编译官方 `177a1dc` 和开发分支
`codex/uprobe-official-177a1dc465ff`（`a7d54d7e`），跑单测，确认 Jetson 上的修复与优化在 x86 上也成立。
只做构建和正确性，**没有计时**。原始数据、脚本和日志在 `benchmark-results/jetson` 分支
`uprobe/x86-nuc-build-regression-20261026/`（README 有全表）。

**用户定的分工（10/26）**：NUC 上不改开发分支，发现的问题记录下来交给 Jetson 会话处理（第 4 节）。

## 1. 条件（与 Jetson 的差异）

| 项 | NUC（x86） | Jetson（ARM64） |
|---|---|---|
| CPU | Intel i5-1240P，4 个 P 核 × 2 线程 + 8 个 E 核 | Orin，6 × Cortex-A78AE |
| 内核 | Debian 13 `6.12.113+deb13-amd64`（BTF、`FTRACE_SYSCALLS`） | `6.8.12-1021-tegra-btf` |
| 编译器 | gcc-13.3 | gcc-13 |
| LLVM（JIT） | **19.1.7**（Debian 13 没有 16） | 16（每日回放用 15） |
| BPF clang | 19.1.7 | 15 |
| Boost | 1.83 | 1.83 |
| Frida | 16.1.2 linux-x86_64 | 16.1.2 linux-arm64 |
| CMake 选项 | 与 Jetson 基线 `baseline-177a1dc-jit-llvm16-20261021/run-build.sh` 相同，只换 LLVM 路径和 BPF clang | — |
| 频率 | 未固定（不计时） | — |

## 2. 结果

| 检查 | 官方 177a1dc | 开发分支 a7d54d7e |
|---|---|---|
| 配置 + 构建（LLVM 19） | 通过 | 默认配置失败（4.2），传 `-DDIAGNOSTIC_BPF_CLANG=/usr/bin/clang-19` 后通过；llvmbpf 补丁 overlay 在 LLVM 19 上照常生成 |
| 每日过滤器（`config.json` 的 `test_filter`） | 34 个用例全过（同 Jetson） | **110 个用例 / 287,652 个断言全过**（同 Jetson） |
| Frida 挂钩测试（逐用例） | 17/17 | **19/19** |
| 完整 runtime 单测（逐用例，每个一个进程） | 104/106 | 180/182，两个失败与官方相同（4.3） |

- **LLVM 19 可用**：`local-call-return-block.patch`、array JIT overlay 都正常生成，JIT 相关用例全过。暂不需要 LLVM 16。
- **ARM 专有路径的 x86 一侧**：`Uretprobe callbacks can generate a backtrace` 在 x86 走 `#else`（gum 回溯器，入口和返回的栈相同），
  Jetson 上从没跑过这一支，通过；`strip_code_pointer_auth` 在无 PAC 平台上是恒等，通过。
- 完整 runtime 单测放在一个进程里跑会在 `Test attach replace with ebpf` 处段错误退出（官方第 2 个用例、开发分支第 5 个），
  后面的用例都跑不到，所以改为逐用例运行（`run-each-case.py`）。

## 3. 下一步（NUC）

候选目标 2（真实工具审计搬到 x86，重点是 gum 取栈下的 memleak 和 syscall tracepoint 工具）和目标 3（性能对照）待用户选定。

## 4. 交给 Jetson 会话的问题（NUC 上未修改源码）

### 4.1 `bpf_get_stackid` 的错误在 64 位返回值里仍是正数（开发分支 `d766e6ad`，与架构无关）

- 位置：`runtime/src/bpf_helper.cpp:1224`，`bpftime_get_stackid` 返回 `int64_t`：
  `return ret < 0 ? ret : (uint32_t)ret;`
- 根因：`int` 与 `uint32_t` 的公共类型是 `unsigned int`，两个分支都变成无符号（gcc-13 `-Wduplicated-branches` 警告
  "this condition has identical branches"，见 `dev/build.log`）。`-EEXIST` 返回为 `0x00000000ffffffef`（4294967279），不是内核的 `-17`。
  `d766e6ad` 说明里的"`bpf_get_stackid` 错误保持负数"实际没有做到。
- 影响：BPF 程序把返回值存进 `int` 再判断不受影响（截断回 -17，例如 bpftime 的 get_stack_id 示例 `int stack_id`）；
  存进 `long`/`__u64` 或用 64 位比较 `< 0` 时，会把错误当成有效 id（内核里是 `0xffffffffffffffef`）。
  已审计的 18 个工具里哪些属于后一种，NUC 上没有逐个核对。
- 官方：`return (uint32_t)ret;`，同样把错误变成看起来有效的 id（`fill_stack_trace` 在官方里不返回 `-EEXIST`，但 `-1` 也一样被转成正数）。
- 缺的测试：现有 `test_stack_trace_map.cpp` 只测 map 层 `fill_stack_trace(...) == -EEXIST`，没测 helper 的 64 位返回值。
  不修就失败的测试可以是：栈 map 一个桶里先放一条栈，再用不同的栈、不带 `BPF_F_REUSE_STACKID` 调 helper，断言返回值 `== (int64_t)-EEXIST`。
- 建议改法（未实施）：`return ret;`（`int` 转 `int64_t` 符号扩展，非负 id 不变）。

### 4.2 开发分支在只有新版 clang 的机器上配置失败

- 位置：`benchmark/CMakeLists.txt:59`，`find_program(DIAGNOSTIC_BPF_CLANG NAMES clang-16 clang-15 clang REQUIRED)`。
- 现象：Debian 13 只有 `clang-19`，配置报 "Could not find DIAGNOSTIC_BPF_CLANG" 失败（`dev-attempt1-no-diagnostic-clang/configure.log`）。
  这只是诊断用 benchmark（`array-helper-jit-layers` 等）的依赖，却让整个构建失败。官方没有这段。
- 绕过：配置时传 `-DDIAGNOSTIC_BPF_CLANG=/usr/bin/clang-19`。
- 建议（未实施）：先用已解析的 `BPFTIME_BPF_CLANG`，或者找不到时跳过诊断目标，而不是 `REQUIRED`。

### 4.3 `Test attach replace with ebpf` 在 gum 初始化之前调用 gum（官方也有，测试问题）

- 测试先调用 `register_ufunc_for_print_and_add` → `attach::find_function_addr_by_name` → `gum_find_function`，
  再构造 `frida_attach_impl`（其中才 `gum_init_embedded`）。单独运行或排在其他 Frida 用例之前时，崩溃在
  `gum_find_function → g_hash_table_new_full → g_slice_alloc`（SIGSEGV）。
- 生产路径只在 gum 初始化之后调用这个函数，不影响工具。每日过滤器不含这个用例，所以 Jetson 上没出现过。
- 另一个"失败"`Test tail calling from userspace to kernel` 需要 root 建内核 map，用 sudo 跑两边都通过，不是问题。

### 4.4 次要

- 开发分支新警告 `runtime/src/attach/bpf_attach_ctx.cpp:114` 有符号与无符号比较（`insn.imm == helper_id`），对正常 helper 号无影响，未深入。
