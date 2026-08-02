# Jetson kernel array-update path profiling

## 结论

Jetson 与固定频率 x64 的 kernel BPF array-update 差异已经拆到 BPF program
计数器和内核机器码层面。Jetson 并不是因为 BPFtime userspace map 路径更重而
输掉 array update；真正发生翻转的是 Jetson 的 kernel array-update helper 路径
明显更便宜。

相同 control/real BPF program、每次 program 1000 次 helper、各 80000 次有效
program execution 的结果如下：

这里的每项净值定义为：

```text
net counter/helper
  = (real counter/program - control counter/program) / 1000
```

control 保留相同的 1000 次 loop、key/value stack 写入和指针准备，但不调用
`bpf_map_update_elem`。每个 metric 单独采集，ARM64 与 x64 都使用相同 BPF
源码、20000 invocations、5 rounds 和 1000 warm-up；比较基线是
`kernel-map-runtime-x64-20260803-fixedfreq`。

| 指标 | Jetson ARM64 | fixed x64 | ARM64/x64 | ARM64 减少 |
|---|---:|---:|---:|---:|
| cycles/helper | 19.034 | 69.220 | 0.275x | 72.5% |
| instructions/helper | 82.029 | 110.157 | 0.745x | 25.5% |
| L1D loads/helper | 24.003 | 25.041 | 0.959x | 4.1% |
| LLC misses/helper | 0.000073 | 0.000006 | 12.12x | — |

LLC miss 的相对倍数很大，但绝对值约为每 13640 次 helper 才一次 miss，不能解释
50 cycles/helper 的差距。L1D loads 也几乎相同。主要差异是 ARM64 少执行约
28.1 条净指令，并且这些净指令消耗的 cycles 更少。

## wall-time 与 cycles 闭合

| 平台 | 固定频率 | runtime A/B | runtime × GHz | profile cycles |
|---|---:|---:|---:|---:|
| Jetson ARM64 | 1.728 GHz | 11.037 ns/helper | 19.071 cycles | 19.034 cycles |
| x64 | 2.2 GHz | 32.877 ns/helper | 72.330 cycles | 69.220 cycles |

两种独立口径基本闭合，说明 `bpftool prog profile` 的差距能够解释此前
kernel-runtime wall time 的 3.0x 平台差异。由 real-control 差值计算的
instructions/cycle 是 ARM64 4.31、x64 1.59；这是差分指标，不应当当成整个 CPU
或整个程序的常规 IPC，但它说明差距不只是标称频率。

## JIT 入口不是 ARM64 更便宜的原因

| 平台 | control JIT 静态指令 | real JIT 静态指令 | real-control |
|---|---:|---:|---:|
| ARM64 | 38 | 48 | +10 |
| x64 | 19 | 25 | +6 |

ARM64 的 real JIT 在循环中准备参数并通过 `blr x10` 调用
`array_map_update_elem`。它相对 control 的静态机器指令增量反而比 x64 更大，
所以 ARM64 的优势不来自一个更短的 BPF JIT 调用桩；节省发生在 helper 进入后的
kernel map 实现及其执行效率。

## 内核路径差异

运行内核没有 BTF 和 `/proc/kcore`，但 `/boot/Image` 内嵌版本字符串与运行内核
均为 `6.8.12-1021-tegra`。使用 root `/proc/kallsyms` 的 `_text` 地址映射
`/boot/Image` 后，可以直接检查当前内核机器码。

Jetson 的 `array_map_update_elem`：

1. 没有调用独立的 `bpf_obj_memcpy` symbol；该包装逻辑已被内联。
2. 对普通 `u64` array value 的无 record 路径直接 `bl __memcpy`。
3. 随后仍然 `bl bpf_obj_free_fields`。
4. ARM64 `__memcpy` 对 8-byte 长度走短尺寸分支，执行 64-bit load/store 后返回。

x64 机器码则明确是：

```text
array_map_update_elem
  -> bpf_obj_memcpy
       -> memcpy_orig
  -> bpf_obj_free_fields
```

x64 的 `bpf_obj_memcpy` 保留完整函数序言、寄存器保存、record 检查、对
`memcpy_orig` 的调用和函数结尾；Jetson 编译器把这一层展开进
`array_map_update_elem`。这与 Jetson 少约 28 条 instructions/helper 的结果
一致，是目前最强、且有静态机器码支持的实现级原因。

但不能把全部 50 cycles/helper 差距严格归因于“少一次函数调用”。cycles 的降幅
显著大于 instructions 的降幅，还包含两套 CPU 对分支、短尺寸 memcpy、函数调用
和依赖链的不同执行效率。要把编译器与 ISA/微架构完全分离，需要让两端内核使用
相同源码和尽可能相同的编译器配置重新构建。

## perf 采样证据

`perf record -e cycles:k -g` 在完整四操作 diagnostic 中取得 61138 个样本，
丢失样本为零：

| symbol | 全局占比 | samples |
|---|---:|---:|
| `array_map_update_elem` | 4.04% | 2473 |
| array-update JIT program | 1.61% | 985 |
| `bpf_obj_free_fields` | 0.95% | 582 |
| `bpf_obj_memcpy` | 0 | 0 |
| `__memcpy`/`memcpy` | 0 | 0 |

静态反汇编已经证明 `__memcpy` 确实被调用，因此零 memcpy 样本只表示 8-byte
短路径太短、采样没有落在其内部，不能解释成“没有执行复制”。同理，perf 的百分比
覆盖完整 array/hash diagnostic，不是 array update 内部的独占百分比。

## no-BTF profiler 说明

系统 bpftool 虽然在 help 中列出 `prog profile`，实际没有编译 profiler skeleton。
重新构建完整 bpftool 后，原 profiler 仍因 Jetson 无 kernel BTF 而加载失败。其唯一
显式 CO-RE 依赖是本地三字段计数结构上的 `preserve_access_index`；字段布局固定为
三个 `u64`，无需 kernel type relocation。

`build-bpftool-profile-nobtf.sh` 在工具构建期间临时应用
`bpftool-profiler-no-core.patch`，构建完成立即恢复仓库源码。这个修改只影响测量工具，
没有修改目标 BPF program、kernel、BPFtime runtime 或 benchmark 语义。最终四项
profile 均验证：

- control/real run count 都是 80000；
- `enabled == running`，不存在 PMU multiplex；
- 每个 metric 单独运行，避免多个硬件事件互相影响；
- 结束后 `kernel.bpf_stats_enabled=0` 且 BPF links 为 0。

## 数据文件

- `profile-summary.csv`：ARM64 原始 counter、run count 和净 per-helper 数值。
- `comparison-x64.csv`：ARM64 与 fixed-frequency x64 counter 对比。
- `runtime-check.csv`：四次独立 profile run 中的五轮 runtime A/B。
- `derived-summary.csv`：wall time、名义 cycles 与 profile counters 闭合。
- `jit-static-summary.csv`：两端 control/real JIT 静态指令数量。
- `perf-symbol-summary.csv`：Jetson perf symbol 样本摘要。
- `raw-profile-*/`：四项 profile JSON、JIT/xlated dump、频率和 runtime raw CSV。
- `raw-perf-record/`：零丢失的 perf.data 和完整 call-graph report。
- `raw-kernel-disassembly/`：Jetson `/boot/Image` 机器码和 kallsyms 映射。
- `environment.txt`：源码、工具哈希、CPU/电源状态和清理验证。
- `*.sh`、`summarize.py`：精确构建、采集、反汇编和汇总步骤。

## 后续问题

目前可以确认差异位于 kernel helper 路径和生成的机器码，而不是 BPFtime map
实现；也可以确认 ARM 内核内联了 x64 未内联的 `bpf_obj_memcpy` 包装层。尚未完全
分开的因素是：

- 两端 kernel compiler 不同（Jetson GCC 13.2，x64 GCC 15.2）；
- 两个内核版本、配置和体系结构 memcpy 实现不同；
- 约 28 instructions/helper 的减少不能单独解释全部 50 cycles/helper 差距。

若需要把原因进一步收敛成“编译器”或“微架构”，下一步应使用同一 kernel
源码与尽量一致的 GCC 配置分别构建 ARM64/x64，检查
`array_map_update_elem` 是否仍只在 ARM64 内联 `bpf_obj_memcpy`。当前证据已经
足够解释 benchmark 的胜负翻转来源，但不应外推成所有 ARM64 内核都天然比 x64
更快。
