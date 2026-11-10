# 给 NUC 会话的交接（续二）：测试注册问题已修，复查测试并量出寄存器修复的成本（2026-10-29 夜）

接着你的 `x86-nuc/bpftime-x86-nuc-x86-hook-registers-findings-2-20261029.md`。**回复用户用中文**，NUC 上不改开发分支，推送前问用户。

## 0. 第 1 节（测试必然失败）已修：开发分支 `c23442ac`

- 提交名："text segment transformer: register capstone x86 in setup_syscall_tracer"。
- 改法：把 `cs_arch_register_x86()` 挪进 x86 的 `setup_syscall_tracer()` 本身，`agent-transformer.cpp` 里原来那次调用删掉。这样测试和其他调用方都不用记得注册。
- 验证情况：Jetson 上只做了 x86 的语法检查；aarch64 的测试 8/8 连跑 3 次通过。变异补丁 `syscall/x86-hook-before-fix-mutation.patch` 在 `c23442ac` 上仍然能打上。

## 1. 要做的事

1. **测试复查**：
   - 构建 `c23442ac`，选项同前。
   - root 加 `vm.mmap_min_addr=0` 跑 `bpftime_text_segment_transformer_tests`，应当 **3/3 通过，连跑 3 次**。
   - 在临时 worktree 里打上变异补丁再跑，新用例的两个 SECTION 都应当**因为寄存器或 red zone 检查而失败**，不能再是因为 capstone 的退出码 1。把子进程打印的寄存器名记下来。
   - 每日过滤器、`bpftime_syscall_trace_attach_tests`（root）。
2. **量出寄存器修复的成本**（你的第 2 节建议）。在写循环上用 Jetson 的 `run_layers.py`，设置和你 10/29 下午的拆层一样：CPU 2，1.7 GHz，关 turbo，5 轮。对比两个构建：
   - `build-59e5e154`（修复前，跳板不保存寄存器）；
   - `build-c23442ac`（修复后：恢复 6 个参数寄存器、XSAVE/XRSTOR 掩码 0xe7、跳过 red zone）。
   - 至少跑 none、rewrite、passthrough、agent-other、agent-exit 这几层。看 rewrite − none 和 agent-exit − none 修复前后差多少周期。
   - 可选：对修复后的 rewrite 一层做一次 perf（`perf record -g`，2000 万次 write），看 `xsave`/`xrstor` 两条指令和 nop 滑道各占多少。
   - 可选：nginx 的 9 个系统调用编号不同，nop 滑道的长度也就不同。可以按 `audit.py` 记录的调用分布，加权估算 nginx 里平均每次的滑道成本。
3. **先只测量，不要改代码**。修复是为了正确性，必须保留。如果 XSAVE 确实很贵，后面怎么改由 Jetson 会话和用户决定，可选的方向有：
   - 用 `xsavec`（带 init 优化，glibc 的延迟绑定跳板就是这么做的）；
   - 在汇编里先判断"这个调用号有没有程序"，没有就在 XSAVE 之前直接发起系统调用。

## 2. 结果放哪里

- 原始数据：`benchmark-results/x86-nuc` 的 `syscall/x86-nuc-syscount-nginx-20261029/regs-fix/`，可以加 `tests-c23442ac/`、`layers-cost/` 之类的子目录。
- 结论写进 `summry/x86-nuc`，问题写发现文件。推送前先问用户。
