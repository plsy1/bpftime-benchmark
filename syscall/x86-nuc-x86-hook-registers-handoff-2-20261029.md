# 给 NUC 会话的交接（续）：链接问题已修，继续验证 x86 跳板寄存器修复（2026-10-29 夜）

接着 `syscall/x86-nuc-x86-hook-registers-handoff-20261029.md`（同在 `summry/jetson`）和你的发现文件
`x86-nuc/bpftime-x86-nuc-x86-hook-registers-findings-20261029.md`。**回复用户用中文**，NUC 上不改开发分支，推送前问用户。

## 0. 两个问题的处理

1. **链接失败（你发现的第 1 节）**：开发分支 **`e299cd3f`**（"text segment transformer: define the x86 XSAVE area size in the asm"）修了。
   - 做法：`bpftime_xsave_area_size` 改为在文件级汇编的 `.data` 段里定义（hidden），C++ 只声明成 `extern` 去写它。顶层汇编定义的符号 LTO 删不掉。
   - Jetson 上用 gcc-13 `-flto -shared`，以同样的模式在 aarch64 上复现了旧写法的 `undefined reference`，新写法能链接，C++ 写入的值汇编也能读到。x86 部分再次用 clang 交叉汇编和做了语法检查。
   - **仍然没在 x86 上用 GCC LTO 链接过**，第 1 步会验证。
2. **"修复前会失败"在 LTO 下做不了（你发现的第 2 节）**：改用变异补丁。
   - 文件：`syscall/x86-hook-before-fix-mutation.patch`（同在 `summry/jetson`）。
   - 内容：在 `e299cd3f` 上把 `syscall_hooker_asm` 换回修复前的指令序列（不恢复参数寄存器和向量状态，栈帧压在 red zone 里），汇编仍然放在文件级，LTO 能链接。
   - 用法：在临时 worktree 里检出 `e299cd3f`，`git apply -p1 <补丁>`，构建并运行测试。这个 worktree 只在本地用，不提交。

## 1. 要做的事

1. **构建 `e299cd3f`**：选项同前。`libbpftime-agent-transformer.so` 和 `bpftime_text_segment_transformer_tests` 必须能链接。
2. **测试**：
   - 每日过滤器、`bpftime_syscall_trace_attach_tests`（root）。
   - root 加 `vm.mmap_min_addr=0` 跑 `bpftime_text_segment_transformer_tests`，应当 3/3 通过，连跑 3 次。
3. **变异检查**：打上补丁后，新用例应当失败：挂 hook 的 SECTION 报参数寄存器和 xmm 被改；不挂 hook 的 SECTION 至少报 red zone 被改。记下输出。
4. **`regcheck`**：用 `e299cd3f` 跑"不挂程序"和"挂 syscount 程序"两种情况，应当 0 个寄存器改变。
5. **nginx**：先加载 syscount 再用跳板启动 nginx，确认能起来；`audit.py` 的 transformer 一组要和内核计数一致（约 9.02 次/请求）。
6. **正式测量**：`run_correct.py --rounds 10`，CPU 0–7 钉 1.7 GHz、关 turbo。测前用 `ps` 查残留进程；后台命令里不要出现 `syscount`、`syscall`、`bpftime`、`victim`。可选 perf。

## 2. 结果放哪里

同上一份交接：原始数据放 `benchmark-results/x86-nuc` 的 `syscall/x86-nuc-syscount-nginx-20261029/regs-fix/`，结论写进
`summry/x86-nuc`，把对比表里"x86 正确跑法"那一列填上。问题写发现文件，推送前先问用户。
