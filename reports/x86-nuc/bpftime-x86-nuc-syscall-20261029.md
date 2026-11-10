# x86 上的系统调用追踪对照（2026-10-29，NUC）

按 `summry/jetson` 的 `syscall/x86-nuc-syscall-handoff-20261029.md` 第 1 节做。原始数据在 `benchmark-results/x86-nuc` 的
`syscall/` 下：

- 构建与回归：`x86-nuc-build-20261029`
- 拆层测量：`x86-nuc-breakdown-20261029`
- 官方 benchmark/syscall：`x86-nuc-official-syscall-20261029`
- 驱动脚本与频率记录：`x86-nuc-syscall-session-20261029`

需要 Jetson 会话看的点写在 [发现文件](bpftime-x86-nuc-syscall-findings-20261029.md)。

## 1. 结论

1. **`7a7d29b8` 在 x86 上没问题**：
   - 用 10/26 的选项（gcc-13、LLVM 19.1.7、LTO）构建成功；`bpftime_text_segment_transformer` 能编译，两个源文件没有警告。
   - 每日过滤器 113/113 通过：10/26 的 110 个，加 3 个新测试。
   - zpoline 路径没被改坏：计数程序在 3 组 × 5 轮里都数到 1,000,000 / 1,000,000，45 × 3 次运行全部有效。
2. **agent 分发在 x86 上不是 200 ns，但仍然是最大的一层**：
   - 没有匹配程序时，分发约 **220 个周期**：1.7 GHz 下 130 ns，4.4 GHz 下 51 ns，两个频率下周期数一致。
   - Jetson 是 349 个周期（202 ns，1.728 GHz）。
   - 在同频（1.7 GHz）下，分发占 bpftime 单程序总开销的 57%，Jetson 是 72%。
   - 分发是两种架构共用的同一段 C++ 代码，同频下 ARM64 多出约 130 个周期，这部分和架构有关。
3. **跳板：zpoline 比 ARM64 逐处跳板贵**：
   - zpoline 约 **110 个周期**：1.7 GHz 下 66 ns，4.4 GHz 下 25 ns。
   - ARM64 跳板 **65 个周期**（37.5 ns）。
   - x86 只保存 7 个参数寄存器，ARM64 要保存 x0–x30 和 q0–q31，但 x86 反而多约 45 个周期。
   - 推测原因（未单独测）：zpoline 跳到"地址 = 系统调用号"处，再顺着 nop 滑到第 512 字节。write 的调用号是 1，每次要走 511 个 nop，外加两次间接跳转。
   - 照这个推测，x86 上跳板成本随调用号变化：号越小越贵。
4. **官方 benchmark/syscall**：
   - 1.7 GHz：kernel +57.8 ns，bpftime +224.2 ns，3.9×。
   - 4.4 GHz：kernel +20.9 ns，bpftime +87.4 ns，4.2×。
   - Jetson：kernel +84.8 ns，bpftime +286.6 ns，3.4×。
   - 两次 x86 的结果都和拆层的 kernel-exit / agent-exit 一致。
5. **坑：官方 benchmark 不能只绑一个 CPU**：
   - 加载器 `syscall.c` 在整个 leg 期间一直忙等，绑在同一个 CPU 上会和 victim 平分 CPU，被追踪的两个 leg 测出来翻倍。
   - 第一次就是这样，那次已作废并保留记录；后来改成给脚本两个 P 核。

## 2. 方法

- 机器：i5-1240P，Debian 13，内核 6.12.113。开发分支 `7a7d29b8`，构建目录 `build-7a7d29b8`。
- `vm.mmap_min_addr` 原值 65536：测量时设成 0，测完恢复（三组都核对过）。
- 频率：沿用 10/26 的做法，min = max、powersave、关 C6 以上，并用 APERF/MPERF 做忙检查和每秒记录。三组：
  - **1.7 GHz、关 turbo**：和 Jetson 的 1.728 GHz 基本同频，可以直接比 ns。
  - **4.4 GHz、开 turbo**：和 10/26 的 x86 uprobe 数据同设置。
  - **4.4 GHz 第一次**：拆层部分有效，官方部分无效。
- 绑核：拆层测量绑 CPU 2；官方脚本绑 CPU 2 和 4，两个核钉在同一频率。两核同时钉 4.4 GHz 时实测中位是 4.36 GHz。
- 拆层用的是 Jetson 原样的 `run_layers.py` 和 `src/`，每组 5 轮，leg 顺序轮换。
- 官方脚本不改，用 Jetson 的软链接视图方式跑，并设 `HOME` 为空目录，走 LD_PRELOAD。

## 3. 拆层对比（中位 ns/write；括号里是相对下一层的增量和换算的周期数）

| Leg | Jetson 1.73 GHz | x86 1.7 GHz | x86 4.4 GHz |
|---|---:|---:|---:|
| none（不追踪） | 352.6 | 234.0 | 91.7 |
| rewrite（改写+跳板） | +37.5 ns, 65 cyc | +65.8 ns, 112 cyc | +24.5 ns, 107 cyc |
| passthrough（跨命名空间调用） | +0.4 ns | +0.8 ns | +0.0 ns |
| agent-other（分发，无匹配程序） | **+202.1 ns, 349 cyc** | **+129.6 ns, 221 cyc** | **+51.0 ns, 222 cyc** |
| agent-exit（一个空程序） | +42.2 ns, 73 cyc | +29.5 ns, 50 cyc | +11.1 ns, 48 cyc |
| agent-count（两个计数程序） | +47.0 ns, 81 cyc | +30.4 ns, 52 cyc | +12.0 ns, 52 cyc |
| kernel-other（开了一个 syscall tracepoint） | +14.8 ns, 26 cyc | +36.7 ns, 63 cyc | +14.9 ns, 65 cyc |
| kernel-exit（一个空程序） | +69.3 ns, 120 cyc | +15.3 ns, 26 cyc | +7.1 ns, 31 cyc |
| kernel-count（两个计数程序） | +86.9 ns, 150 cyc | +143.1 ns, 245 cyc | +53.2 ns, 232 cyc |

各列的 bpftime 合计（agent-exit 相对 none）：

- Jetson：+282.2 ns
- x86 1.7 GHz：+225.7 ns（386 个周期）
- x86 4.4 GHz：+86.6 ns

4.4 GHz 第一次的拆层数字和第二次相差不到 1.5 ns，完整表格见结果目录的 `compare.md`。

## 4. 对 Jetson 优化方向的意义

- **分发层是两边共同的大头**，优化它对 x86 同样有效。按同频算，x86 上分发约 220 个周期，占 bpftime 单程序开销的一半以上。
- **ARM64 多出的约 130 个周期值得单独找**：同一段代码，在 ARM64 上贵了 60%，可能和 TLS 访问、`std::function` 的构造和析构、异常保护在两种架构上的代价有关。这些只是可以去看的方向，没有测过。建议 Jetson 做 perf 时，在 x86 上也做一次同样的 perf 作对照，NUC 可以配合。
- **单个程序的成本**：bpftime 每个程序约 50 个周期（Jetson 73–81 个），x86 内核跑一个空程序只要 24–31 个周期。所以在 x86 上，bpftime 的差距几乎全部来自固定成本，也就是跳板加分发，共约 330 个周期。
- **ARM64 跳板（65 个周期）已经比 zpoline（110 个周期）便宜**，跳板这一层不是 ARM64 的短板。
