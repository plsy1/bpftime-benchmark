# 加强后的 x86 跳板测试（1aa74c4c）在 x86 上的确认（2026-10-29 夜，NUC）

按 `summry/jetson` 的 `syscall/x86-nuc-x86-hook-test-handoff-4-20261029.md` 做，只做测试确认，不做性能测量。

开发分支 `1aa74c4c`（"text segment transformer tests: clobber the argument registers after the x86 system call"）只改了
`test/test_syscall_rewrite_x86.cpp`：`clobbering_hook` 在 `next_hook` 返回之后改写 rdi…r9（`0x0bad0001`…`0x0bad0006`）和 xmm0、7、15。

## 1. 构建与测试

- **构建**：`run-build.sh`，选项同前（gcc-13 / LLVM 19.1.7 / LTO），新目录 `build-1aa74c4c`，只构建 `bpftime_text_segment_transformer_tests`，退出码 0（`build.log`，源码状态见 `source-state.txt`）。
- **运行**：root，`vm.mmap_min_addr` 65536 → 0 → 65536（`mmap_min_addr.txt`）。
- **结果**：**连跑 3 次都是 3/3**（`tests-transformer-{1,2,3}.log`），新用例的两个 SECTION 都通过。

## 2. 变异检查（`mutation/`）

- **做法**：在临时 worktree 里检出 `1aa74c4c`，打上 `summry/jetson` 的 `syscall/x86-hook-before-fix-mutation.patch`（副本和来源提交在本目录），只把跳板换回修复前的写法。用同样的选项构建测试，连跑 3 次。worktree 只在本地用，用完已删除。
- **结果**：3 次都是 2/3，失败的是新用例的两个 SECTION，3 次报告的内容相同（`tests-transformer-{1,2,3}.log`）：

| SECTION | 子进程退出码 | 报告 |
|---|---:|---|
| no hook | 9 | red zone rsp−32 到 rsp−96，共 9 项 |
| hook that uses registers | 24 | **rdi = 0xbad0001，rsi = 0xbad0002，rdx = 0xbad0003，r10 = 0xbad0004，r8 = 0xbad0005，r9 = 0xbad0006**；xmm0、1、2、7、15；red zone rsp−32 到 rsp−128，共 13 项 |

和 `c23442ac` 那次对比（`../mutation-c23442ac/`）：当时"hook that uses registers"只报了 xmm 和 red zone，没有 rdi…r9。现在测试能抓到参数寄存器没有恢复的情况，也就是当初让 nginx 崩溃的那种。
