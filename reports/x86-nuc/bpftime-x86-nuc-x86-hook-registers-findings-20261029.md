# 交给 Jetson 会话：x86 跳板寄存器修复（0a602133）在 x86 上的验证（2026-10-29 夜，NUC）

来源：`summry/jetson` 的 `syscall/x86-nuc-x86-hook-registers-handoff-20261029.md`。原始记录在 `benchmark-results/x86-nuc` 的
`syscall/x86-nuc-syscount-nginx-20261029/regs-fix/`（README、`build/build.log`、`before-fix-test/build.log`）。NUC 上没有改开发分支。

## 1. 需要修：`0a602133` 在 x86 上链接失败（GCC LTO 删掉了 `bpftime_xsave_area_size`）

**构建**：选项与 `build-59e5e154` 相同，gcc-13 / LLVM 19.1.7 / LTO。
- 失败的只有 `libbpftime-agent-transformer.so` 和 `bpftime_text_segment_transformer_tests`，报错：
  `undefined reference to 'bpftime_xsave_area_size'`，`relocation R_X86_64_PC32 against undefined hidden symbol ... can not be used when making a shared object`。
- 其余目标（agent、syscall server、bpftimetool、runtime 测试）都链接成功。

**原因**
- 这个变量是 `extern "C" __attribute__((visibility("hidden"))) uint64_t bpftime_xsave_area_size = 0;`（`text_segment_transformer.cpp:69`）。
- C++ 代码只在 `setup_syscall_tracer` 里写它，读它的只有文件级 asm 里的 `subq bpftime_xsave_area_size(%rip), %rsp`。
- GCC 的 LTO 看不见顶层 asm 字符串里的引用，于是把这个"从没被读过"的隐藏变量删了：LTO 中间对象里还有它（`gcc-nm-13` 显示 `B`），ltrans 之后就没了。
- clang 交叉汇编和语法检查都不经过 GCC LTO 链接，所以 Jetson 上没发现。

**修法建议**（NUC 上没改，也没验证）
- 给这个变量加 `__attribute__((used))`。
- 或者在 C++ 里通过 asm 操作数引用它。
- 或者在 asm 里定义它，C++ 用 `extern` 引用。

## 2. 交接第 3 步（"修复前测试失败"）在 LTO 构建下做不了

- **做法**：检出 `0a602133`，只把 `text_segment_transformer.cpp` 换回 `59e5e154` 的版本，构建 `bpftime_text_segment_transformer_tests`。
- **结果**：同样链接失败，缺 `syscall_hooker_asm`、`syscall_addr`、`call_orig_syscall`。
- **原因**：旧版把 x86 汇编放在没人调用的 `__asm_holder()` 里，链接新的测试可执行文件时 LTO 丢掉了它。共享库里碰巧保留了下来。
- **建议**：要证明新测试"不修就失败"，有两条路，可以由 Jetson 选：
  - 不开 LTO，构建修复前和修复后两个版本；
  - 准备一个"旧逻辑但文件级 asm"的版本。

## 3. 还没做的（等第 1 节修好后在 NUC 上做）

- **测试**：3/3 连跑 3 次，再用 regcheck 复查。
- **nginx**：`repro-crash.sh` 方式启动，`audit.py` 的 transformer 一组要和内核计数一致。
- **正式测量**：`run_correct.py --rounds 10`，CPU 0–7 钉 1.7 GHz；可选 perf。
