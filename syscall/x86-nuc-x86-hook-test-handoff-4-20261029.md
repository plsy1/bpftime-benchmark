# 给 NUC 会话的交接（续三）：确认加强后的 x86 跳板测试（2026-10-29 夜）

接着你的 `x86-nuc/bpftime-x86-nuc-x86-hook-registers-findings-3-20261029.md` 第 1 节。**回复用户用中文**，NUC 上不改开发分支，推送前问用户。
**只做测试确认，不做性能测量。** 用户已决定 x86 的性能先不管：XSAVE 的成本、`xsavec`、汇编里直接放行这些都不做。

## 0. 改了什么：开发分支 `1aa74c4c`

- 提交名："text segment transformer tests: clobber the argument registers after the x86 system call"。
- 只改了测试 `test/test_syscall_rewrite_x86.cpp`，功能代码和 `c23442ac` 相同。
- 改法：`clobbering_hook` 在 `next_hook` 返回**之后**，用 asm 改写 rdi、rsi、rdx、r10、r8、r9（写入 `0x0bad0001`…`0x0bad0006`）和 xmm0、xmm7、xmm15，模拟系统调用返回后运行的 exit 程序。
- Jetson 上只做了 x86 编译检查，确认生成的代码在 `next_hook` 调用之后执行这几条改写。变异补丁 `syscall/x86-hook-before-fix-mutation.patch` 在 `1aa74c4c` 上仍然能打上。

## 1. 要做的事

1. 构建 `1aa74c4c`，选项同前，只需要 `bpftime_text_segment_transformer_tests`。
2. root 加 `vm.mmap_min_addr=0`，连跑 3 次，应当 **3/3 通过**。原值记下来，测完恢复。
3. 在临时 worktree 里打上变异补丁，构建并运行。预期：
   - "hook that uses registers" 这个 SECTION 报出 **rdi、rsi、rdx、r10、r8、r9**（值是 `0xbad000N`），还有 xmm 和 red zone；
   - "no hook" 这个 SECTION 至少报 red zone。
   把子进程的输出记下来。worktree 只在本地用，不提交。
4. 如果第 3 步仍然报不出 rdi…r9，就停下，写进发现文件，不要自己改测试。

## 2. 结果放哪里

`benchmark-results/x86-nuc` 的 `syscall/x86-nuc-syscount-nginx-20261029/regs-fix/` 下加一个子目录，比如 `tests-1aa74c4c/`。
结论简短记进 `summry/x86-nuc`，推送前先问用户。
