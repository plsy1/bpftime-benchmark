# x86 跳板寄存器修复在 x86 上的验证（2026-10-29 夜，NUC）

分两轮：

- 第 1 节、第 2 节：`0a602133`，在 x86 上链接失败。
- 第 3 节起：`e299cd3f`（"text segment transformer: define the x86 XSAVE area size in the asm"），链接问题已修。验证通过，并按正确方式跑完了 syscount-nginx。

---

## 第一轮：`0a602133`

按 `summry/jetson` 的 `syscall/x86-nuc-x86-hook-registers-handoff-20261029.md` 做。开发分支 `0a602133`（"text segment
transformer: keep every register across the x86 hook"，在 `59e5e154` 之后）。

**结果：在 x86 上编不过（链接失败），按交接第 1 节第 1 步停下。** 后面需要新构建的步骤（测试、regcheck、nginx、
`run_correct.py`）这一轮都没有做。

## 1. 构建（`build/`）

`run-build.sh`：选项与 `build-59e5e154` 相同（gcc-13 / LLVM 19.1.7 / **LTO**），构建目录 `build-0a602133`，退出码 1。

```
FAILED: attach/text_segment_transformer/libbpftime-agent-transformer.so
/usr/bin/ld: /tmp/ccvp1RkE.ltrans0.ltrans.o: in function `syscall_hooker_asm':
<artificial>:(.text+0x4d): undefined reference to `bpftime_xsave_area_size'
/usr/bin/ld: ... relocation R_X86_64_PC32 against undefined hidden symbol `bpftime_xsave_area_size' can not be used when making a shared object
FAILED: attach/text_segment_transformer/bpftime_text_segment_transformer_tests
... undefined reference to `bpftime_xsave_area_size'
```

- **原因**：
  - `bpftime_xsave_area_size` 定义为 `extern "C" __attribute__((visibility("hidden"))) uint64_t ... = 0;`（`text_segment_transformer.cpp:69`）。
  - C++ 代码只在 `setup_syscall_tracer` 里写它，读它的只有文件级 asm（`subq bpftime_xsave_area_size(%rip), %rsp`）。
  - GCC 的 LTO 不解析顶层 asm 字符串里的引用。在它看来这个隐藏变量从来没被读过，于是把它删掉了。LTO 中间对象里还有这个符号（`gcc-nm-13`：`B bpftime_xsave_area_size`），到 ltrans 阶段就没了。
- **为什么 Jetson 的检查没发现**：clang 交叉汇编和语法检查都不经过 GCC LTO 链接。
- **修法建议（未验证，NUC 上没改代码）**：给这个变量加 `__attribute__((used))`，让 LTO 保留它。也可以让 C++ 代码通过 asm 输入操作数引用它，或者改成 `.comm`/asm 里定义、C++ 用 `extern` 引用。
- 其余目标（agent、syscall server、bpftimetool、runtime 测试）都链接成功，失败的只有 transformer 的 `.so` 和测试。

## 2. "不修就失败"：修复前的版本也链接不过（`before-fix-test/`）

按交接第 1 节第 3 步：在临时 worktree 里检出 `0a602133`，只把 `text_segment_transformer.cpp` 换回 `59e5e154` 的版本（`source-state.txt`），
用同样的选项只构建 `bpftime_text_segment_transformer_tests`。这个 worktree 在 Claude 的临时目录里，只在本地用，不提交。

```
FAILED: attach/text_segment_transformer/bpftime_text_segment_transformer_tests
text_segment_transformer.cpp:431: undefined reference to `syscall_hooker_asm'
text_segment_transformer.cpp:346: undefined reference to `syscall_addr'
... undefined reference to `call_orig_syscall'
```

- **原因**：
  - 旧版的 x86 汇编写在一个没人调用的函数 `__asm_holder()` 里。
  - 链接共享库 `libbpftime-agent-transformer.so` 时，这个函数碰巧保留了下来（10/26 以来一直能用）。但链接新的测试可执行文件时，LTO 把它整个丢掉了，汇编里定义的符号也就跟着没了。
  - 这正是 `0a602133` 把汇编改成文件级 `.pushsection` 的原因。
- **结论**：在 LTO 构建下，"旧 transformer + 新测试"本身就链接不过，无法用它演示"修复前测试失败"。要演示，需要不开 LTO 构建，或者把旧汇编单独移到文件级。

---

## 第二轮：`e299cd3f`

按 `summry/jetson` 的 `syscall/x86-nuc-x86-hook-registers-handoff-2-20261029.md` 做。机器 19:03 重启过（在这一轮开始之前），
Docker 重启后是停止状态，测量时临时启动，测完再停掉。

## 3. 构建与测试（`build-e299cd3f/`）

- **构建**：选项同前（gcc-13 / LLVM 19.1.7 / LTO），退出码 0。`libbpftime-agent-transformer.so` 和 `bpftime_text_segment_transformer_tests` 都链接成功，警告与 `build-59e5e154` 相同（只是顺序不同）。
- **每日过滤器**：113/113。
- **`bpftime_syscall_trace_attach_tests`**（root）：8/8。
- **`bpftime_text_segment_transformer_tests`**（root，`vm.mmap_min_addr` 65536 → 0 → 65536）：连跑 3 次，**每次都是 2/3**。
  - 新用例 `x86: a rewritten syscall keeps registers and the red zone` 的两个 SECTION 都以退出码 1 失败（`tests-transformer-{1,2,3}.log`）。
  - 原因是**测试本身**：子进程直接调用 `bpftime::setup_syscall_tracer()`，没有先调用 `cs_arch_register_x86()`。
  - 于是 `cs_open` 返回 `CS_ERR_ARCH`（日志 "Failed to open capstone instance: 2, Invalid/unsupported architecture"），`rewrite_segment` 直接 `exit(1)`，寄存器检查根本没执行。
  - 正式路径（`agent-transformer.cpp`）会先注册，所以不受影响。

## 4. 修复前后对比

### 变异检查（`mutation/`）

- **做法**：在临时 worktree 里检出 `e299cd3f`，打上 `summry/jetson` 的 `syscall/x86-hook-before-fix-mutation.patch`（`source-state.txt`、补丁副本、`patch-source-commit.txt`），用同样的选项构建 agent、syscall server、transformer、bpftimetool 和测试（`build.log`）。这个 worktree 只在本地用，用完已删除。
- **新测试**：同样是 2/3，失败原因也是 capstone（`tests-transformer.log`）。所以它现在分辨不出修复前后。
- **regcheck**（`regcheck-mutation.log`）：
  - 不挂程序：rdx 被改；
  - 挂 syscount 程序：rdi、rsi、rdx、r10、r8（= 0x28）、r9 和 xmm0、5、7 被改。

  和修复前的 `59e5e154` 完全一样，说明修复前确实会出错。

### `e299cd3f` 的 regcheck（`regcheck-e299cd3f.log`，`regcheck.sh` 加了 `BUILDS` 参数）

不挂程序、挂 syscount 程序两种情况都是 **0 个寄存器改变，0 个 xmm 改变**。

## 5. nginx 与审计（`audit-e299cd3f.jsonl`，容器 `docker/start-container.sh`，工作目录 `view-e299cd3f`）

`view-e299cd3f` 的 `build` 指向 `build-e299cd3f`，`benchmark`、`example` 链接到上一级的 `view/`。

| 方式 | 5 秒内的请求数 | 数到的系统调用 | 每个请求 |
|---|---:|---:|---:|
| 内核 syscount `-p <nginx>` | 511,266 | 4,609,012 | 9.015 |
| 官方方式 | 535,080 | 0 | 0 |
| 跳板，nginx 先启动 | 562,837 | 0 | 0 |
| **先加载程序，再用跳板启动 nginx** | 457,244 | 4,122,297 | **9.016** |

nginx 能起来，bpftime 的计数和内核一致（各系统调用的分布也相同），这次没有产生 core 文件。

## 6. 正式测量：`run_correct.py --rounds 10`（`correct/`）

**设置**：
- 驱动脚本是 `../session/correct.sh`：CPU 0–7 钉 1.7 GHz、关 turbo，方法同官方脚本那次；`vm.mmap_min_addr` 设为 0，测完恢复为 65536。
- 前后的忙检查都 PASS（1718–1725 MHz），记录到的忙时频率 1639–1710 MHz，中位 1710。时间 19:28–19:47。
- 开机后 `plymouthd` 一直占约 2.5% 的 CPU，测量期间把它的亲和性临时设为 8–15，测完改回 0–15（`plymouthd-affinity-*.txt`）。
- 恢复：测完核对过，turbo 已开，CPU 0–7 是 400000–4400000，空闲状态全开。

**结果**：60 次全部有效，nginx 一次都没退出（`summary-correct.md`）：

| 配置 | 每秒请求数 | 标准差 | 相对不追踪 | 每次 nginx 系统调用多花 | 每个请求数到的调用 |
|---|---:|---:|---:|---:|---|
| 不追踪 | 58,371 | 229 | – | – | – |
| 内核，追踪 nginx | 55,273 | 235 | −5.31% | +107 ns | 9.015 |
| 内核，追踪别的进程 | 56,251 | 318 | −3.63% | +72 ns | 0.000–0.001 |
| **bpftime，追踪 nginx** | **47,466** | 262 | **−18.68%** | **+437 ns** | **9.015**（= 内核） |
| bpftime，追踪别的进程 | 48,383 | 335 | −17.11% | +392 ns | 0 |
| 只加载跳板，不挂程序 | 51,410 | 145 | −11.92% | +257 ns | – |

"每次 nginx 系统调用多花"：把每个请求多花的时间除以 9.015 次调用（内核的计数），即把全部差值都算到系统调用头上。

- 不追踪时的绝对吞吐（58,371）比之前官方脚本那次（53,884）高。两次的脚本、运行顺序、access.log 的增长都不同，而且中间重启过，所以两组数字不直接对比，各自只和本组的"不追踪"比。
- 只加载跳板、不挂程序就要 −11.9%（每次调用 +257 ns ≈ 440 个周期），Jetson 是 −3.4%（+139 ns）。可能的原因，**都没有单独测过**：
  - **zpoline 的 nop 滑道**：每个请求的 9 次调用是 setsockopt（54）×2、close（3）、recvfrom（45）、sendfile（40）、write（1）、fstat（5）、openat（257）、writev（20）。平均调用号约 53，平均每次要走约 460 个 nop；写循环里测过 write（511 个 nop）约 130 个周期；
  - **新加的保存与恢复**：修复后每次系统调用都做一次 XSAVE/XRSTOR（掩码 0xe7），并保存和恢复参数寄存器，即使没有程序也是如此；
  - **缓存**：Jetson 的 perf 里看到的那种内核变慢。

  可以用写循环拆层（`../../x86-nuc-breakdown-20261029` 的 `run_layers.py`）在 `build-59e5e154` 和 `build-e299cd3f` 上对比 rewrite 一层，量出 XSAVE 修复本身的成本。
