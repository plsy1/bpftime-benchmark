# x86 跳板寄存器修复（0a602133）在 x86 上的验证（2026-10-29 夜，NUC）

按 `summry/jetson` 的 `syscall/x86-nuc-x86-hook-registers-handoff-20261029.md` 做。开发分支 `0a602133`（"text segment
transformer: keep every register across the x86 hook"，在 `59e5e154` 之后）。

**结果：在 x86 上编不过（链接失败），按交接第 1 节第 1 步停下。** 后面需要新构建的步骤（测试、regcheck、nginx、
`run_correct.py`）都没有做。

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
