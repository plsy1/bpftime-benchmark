# x86 NUC 发现、交给 Jetson 会话处理的问题（2026-10-26）

NUC 会话在 x86 上构建、测试开发分支 `codex/uprobe-official-177a1dc465ff`（`a7d54d7e`）时发现的问题。
用户定的分工：NUC 上不改开发分支，问题由 Jetson 会话处理（修复配"不修就失败"的测试，并在官方原版上确认）。
x86 的全部结果和结论在 [`summry/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/summry/x86-nuc)
（[x86 报告](https://github.com/plsy1/bpftime-benchmark/blob/summry/x86-nuc/x86-nuc/bpftime-x86-nuc-build-regression-20261026.md)，下面是它的第 4 节）和
[`benchmark-results/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/x86-nuc)
（构建日志在 `uprobe/x86-nuc-build-regression-20261026/`）。

## 交给 Jetson 会话的问题（NUC 上未修改源码）

### 1. `bpf_get_stackid` 的错误在 64 位返回值里仍是正数（开发分支 `d766e6ad`，与架构无关）

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

### 2. 开发分支在只有新版 clang 的机器上配置失败

- 位置：`benchmark/CMakeLists.txt:59`，`find_program(DIAGNOSTIC_BPF_CLANG NAMES clang-16 clang-15 clang REQUIRED)`。
- 现象：Debian 13 只有 `clang-19`，配置报 "Could not find DIAGNOSTIC_BPF_CLANG" 失败（`dev-attempt1-no-diagnostic-clang/configure.log`）。
  这只是诊断用 benchmark（`array-helper-jit-layers` 等）的依赖，却让整个构建失败。官方没有这段。
- 绕过：配置时传 `-DDIAGNOSTIC_BPF_CLANG=/usr/bin/clang-19`。
- 建议（未实施）：先用已解析的 `BPFTIME_BPF_CLANG`，或者找不到时跳过诊断目标，而不是 `REQUIRED`。

### 3. `Test attach replace with ebpf` 在 gum 初始化之前调用 gum（官方也有，测试问题）

- 测试先调用 `register_ufunc_for_print_and_add` → `attach::find_function_addr_by_name` → `gum_find_function`，
  再构造 `frida_attach_impl`（其中才 `gum_init_embedded`）。单独运行或排在其他 Frida 用例之前时，崩溃在
  `gum_find_function → g_hash_table_new_full → g_slice_alloc`（SIGSEGV）。
- 生产路径只在 gum 初始化之后调用这个函数，不影响工具。每日过滤器不含这个用例，所以 Jetson 上没出现过。
- 另一个"失败"`Test tail calling from userspace to kernel` 需要 root 建内核 map，用 sudo 跑两边都通过，不是问题。

### 4. 次要

- 开发分支新警告 `runtime/src/attach/bpf_attach_ctx.cpp:114` 有符号与无符号比较（`insn.imm == helper_id`），对正常 helper 号无影响，未深入。

### 5. 分支改名：去掉 `codex/` 前缀（用户 10/26 要求，交给 Jetson 会话做）

`codex/` 是当时 Codex 编程助手默认加的前缀，用户不想要。这些分支由 Jetson 这边建、由每日自动化维护，
NUC 上不动，由 Jetson 会话统一改。建议的新名字（用户同意按此执行）：

| 现在 | 改成 | 说明 |
|---|---|---|
| `codex/uprobe-official-177a1dc465ff` | `dev/uprobe-official-177a1dc465ff` | 当前开发分支；保持自动化生成的 `<前缀>-<官方提交号>` 格式 |
| `codex/uprobe-official-8da867d541f8` | `archive/uprobe-official-8da867d541f8` | 已被 177a1dc 取代 |
| `codex/uprobe-map-optimization` | `archive/uprobe-map-optimization` | 10/14 前的 map 优化分支 |
| `codex/official-no-btf` | `archive/official-no-btf` | 7–8 月无 BTF 构建；**main 的 CI 仍在用，见下** |

要一起改的地方（10/26 在 GitHub 上统计）：

1. **每日自动化**（`automation/jetson-daily`）：`tools/jetson-upstream/config.json` 的 `candidate.branch_prefix`
   `codex/uprobe-official` → `dev/uprobe-official`；`tools/jetson-upstream/README.md` 两处。`push_after_test` 是 true，
   不改前缀的话下次跟到官方新提交时还会推出新的 `codex/` 分支。改之前先停每日定时器，改完再开。
2. **main 的 GitHub Actions**：`.github/workflows/` 下 9 个工作流（bench-core、build-jetson-no-btf-image、build-prebuilt、mpk、
   official-no-btf-nginx-path-perf-x64、ssl-nginx、syscall、syscount-nginx、uprobe）的默认 ref 是 `codex/official-no-btf`，
   `README.md` 一处；`docker/jetson-repro/README.md` 提到的 `codex/jetson-no-btf-docker` 远端已不存在，顺手核对。
3. **远端分支**：先推新名（`git push origin origin/codex/<旧>:refs/heads/<新>`），确认 CI、自动化、本机工作区都切过去后再删旧名。
4. **Jetson 本机工作区**：`git branch -m <旧> <新>` 加 `git branch -u origin/<新>`（开发 worktree、每日状态目录里的候选和回放 worktree）。
5. **文档**：`summry/jetson` 21 个文件 34 处，`benchmark-results/jetson` 78 个文件 81 处（多为历史结果里的说明，可只改入口 README 和接力文档，
   历史目录加一句"已改名"即可）。提交信息里的旧名不用改。
6. **x86 分支**：`summry/x86-nuc`、`benchmark-results/x86-nuc` 共 5 处，改名完成后由 NUC 会话更新；
   完成后请在本文件这一条下面写一句"已完成"和新名字，NUC 会话会从这里读。

**已完成（2026-10-27 10:25 JST，Jetson 会话）**。新名字：

| 旧名（远端已删除） | 新名 | 提交 |
|---|---|---|
| `codex/uprobe-official-177a1dc465ff` | `dev/uprobe-official-177a1dc465ff` | `627a2201`（含下面第 1–4 条的修复，见末尾） |
| `codex/uprobe-official-8da867d541f8` | `archive/uprobe-official-8da867d541f8` | `2eea622f`（未变） |
| `codex/uprobe-map-optimization` | `archive/uprobe-map-optimization` | `b9fee864`（未变） |
| `codex/official-no-btf` | `archive/official-no-btf` | `1a1dc0d6`（未变） |

- 每日自动化：`automation/jetson-daily` `56bcefe9`，`candidate.branch_prefix` 为 `dev/uprobe-official`，README 已改；
  定时器改名期间停过，已重新开启（10:22 补跑一次，官方无新提交，未生成新分支）。
- main：`b7270747`，9 个工作流的默认 ref 和 `README.md` 改为 `archive/official-no-btf`；`docker/jetson-repro/README.md`
  里的 `codex/jetson-no-btf-docker` 远端确实已不存在，改成说明默认构建 `archive/official-no-btf`。
- Jetson 本机：开发 worktree、8da867 候选 worktree、主仓库检出都已改名并跟踪新远端；每日状态的迁移记录已改。
- 文档：`summry/jetson` 全部替换为新名（本文件除外）；`benchmark-results/jetson` 历史目录未改，入口 `README.md` 加了新旧名对照。
- x86 两个分支里的 5 处留给 NUC 会话。


## 第 1–4 条的处理结果（2026-10-26/27，Jetson 会话，`dev/uprobe-official-177a1dc465ff`）

| # | 提交 | 不修就失败的测试 | 官方 177a1dc |
|---|---|---|---|
| 1 | `a0b0eb2f` 返回 `ret` | `get_stackid returns errors as negative 64-bit values`（一桶栈 map 冲突，修复前得到 4294967279） | 也有：所有结果转成 `uint32_t` |
| 2 | `fb506ff4` 诊断 clang 先用 `BPFTIME_BPF_CLANG`，再找 clang 15–20，找不到就跳过诊断目标 | `check-clang19-configure.sh`：只留 clang-19 的查找根下，旧文件报与 NUC 相同的错，新文件通过并用 clang-19 编 BPF 对象 | 没有这段 |
| 3 | `7b6fa4fd` `frida_attach_utils` 的三个 gum 查找函数先确保 gum 已初始化 | 现有 `Test attach replace with ebpf` 单独运行：修复前 SIGSEGV（139），修复后通过 | 也有：同样崩溃 |
| 4 | `627a2201` 显式转换 | 只是警告，行为不变，无法构造失败测试 | — |

修复后每日过滤器 110 个用例、frida 18 个、daemon 10 个（root）全部通过。记录在 `benchmark-results/jetson` 的 `uprobe/nuc-fixes-20261026/`。
