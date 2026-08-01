# Jetson kernel BPF 普通 array/hash runtime A/B（2026-08-01）

## 结论

Jetson 上普通 array/hash lookup/update 的 kernel BPF 程序本体已使用
run_time_ns/run_cnt 做 matched control/real A/B。5 轮结果：

| 操作 | 稳定状态 | kernel 净 ns/helper | 标准差 | CV |
|---|---|---:|---:|---:|
| array lookup | hit | **1.384** | 0.0011 | 0.078% |
| array update | existing | **10.952** | 0.0163 | 0.149% |
| hash lookup | hit | **26.769** | 0.0288 | 0.108% |
| hash update | existing | **90.823** | 0.0389 | 0.043% |

上述数字不包含 uprobe attach/陷入和 victim 函数 wall time。它表示 kernel 对每个
BPF 程序统计的平均执行时间，real 减去匹配 control 后再除以每次程序中的 1000
次 map helper。

仅有 ARM64 一侧时，还不能得出“x64 kernel 路径更重”的跨架构结论。x64 必须用
同一源码、参数和统计方法补齐后再计算 x64/ARM 比率。

## 环境

- 机器：Jetson Orin Nano
- CPU：6-core Cortex-A78AE
- kernel：6.8.12-1021-tegra
- 代码基线：ead56c9d9607c467cd45b11c17e6d6659da9bba0
- 工作树：包含诊断代码和此前未提交修改
- BPF Clang：LLVM/Clang 15.0.7
- loader/victim 编译器：GCC 13.3.0
- nvpmodel：MAXN_SUPER
- CPU/GPU/EMC 已锁频
- CPU：1.728 GHz
- victim 与 uprobe BPF 执行固定在 CPU 5
- kernel.bpf_stats_enabled=1 仅在采集期间开启，结束后恢复为 0

## 程序语义

每个 real BPF 程序执行 1000 次 map helper：

    array lookup       key 0..999，array 元素始终存在
    array update       key 0..999，BPF_ANY，覆盖既有 array value
    hash lookup        key 0..999，测量前预填充，全部 lookup-hit
    hash update        key 0..999，测量前预填充，全部 existing update

每个 control 保留相同的 1000 次 bounded loop、key/value BPF stack 写入和指针
准备，但不调用 map helper。反汇编已经检查 control 循环未被 Clang 删除。

## 测量方法

loader 加载并 attach 八个独立 uprobe BPF 程序：

    array_lookup_control / array_lookup_real
    array_update_control / array_update_real
    hash_lookup_control  / hash_lookup_real
    hash_update_control  / hash_update_real

每个程序先执行 1000 次 warm-up。正式采集每轮执行 20000 次 BPF program，每次
program 内含 1000 次循环，共 20M helper/round；5 轮共 100M helper/real
operation。奇数轮 control→real，偶数轮 real→control。

loader 直接对每个 program fd 调用 bpf_obj_get_info_by_fd()，在 victim 前后读取：

    run_cnt
    run_time_ns

计算：

    avg_program_ns
      = delta(run_time_ns) / delta(run_cnt)

    kernel_map_ns_per_helper
      = (real_avg_program_ns - control_avg_program_ns) / 1000

执行命令：

    sudo sysctl -w kernel.bpf_stats_enabled=1

    sudo benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime \
      benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim \
      20000 5 5 1000

    sudo sysctl -w kernel.bpf_stats_enabled=0

## A/B 边界

这比从完整 benchmark wall time 中减去空 uprobe 更干净：

- 不包含基础 uprobe hook 时间；
- 不包含 victim 函数时间；
- map 前置状态在计时外恢复；
- 只统计目标 BPF program 的 kernel runtime。

但它仍不是“字节级完全相同的 BPF 指令，只替换 helper 地址”。kernel 不允许像
BPFtime VM 一样注册任意 no-op helper。real 程序相对 control 还多出 map
pseudo-load、helper call，以及 Clang 形成的少量 loop branch 差异。因此结果应
称为 kernel map operation 的 matched-program 净成本，而不是 helper 函数体的
绝对纳秒数。

跨平台必须使用同一 BPF object 构建方式和 control 源码；这种少量边界差异在两端
一致，适合比较 x64/ARM 的相对趋势。

## 数据和诊断代码

- raw.csv：每轮 control/real 的原始 run_cnt、run_time_ns
- summary.csv：real-control 后的 5 轮派生结果
- BPF 程序：benchmark/uprobe/diagnostics/kernel_map_runtime.bpf.c
- loader：benchmark/uprobe/diagnostics/kernel_map_runtime.c
- victim：benchmark/uprobe/diagnostics/kernel_map_runtime_victim.c
- 构建目标：make -C benchmark/uprobe kernel-map-runtime-diagnostic

诊断只增加独立 target，没有修改 BPFtime runtime 生产逻辑。
