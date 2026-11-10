# Jetson kernel BPF 普通 array/hash 路径成本（2026-08-01）

## 结论

第二阶段 ARM64 侧已经完成。使用 kernel run_time_ns/run_cnt 对 matched
loop-control 和 real map BPF program 做 A/B，得到：

| 操作 | 状态 | kernel 净 ns/helper |
|---|---|---:|
| array lookup | hit | **1.384** |
| array update | existing | **10.952** |
| hash lookup | hit | **26.769** |
| hash update | existing | **90.823** |

每项执行 5 轮，每轮 20M helper。CV 为 0.043%–0.149%，本次 kernel runtime
统计非常稳定。

这组数字测的是 kernel BPF program 本体，不包含 uprobe hook 和 victim wall
time。real-control 仍包含 map pseudo-load、helper call 及极少量编译后 loop
差异，因此是 matched-program 净成本，不应宣称为纯 helper C 函数体时间。

## 当前能回答什么

这确认了 Jetson kernel 普通 map 四项的绝对路径成本，并建立了可在 x64 原样复现
的测量基线。当前只有 ARM64 数据，尚不能单独证明 x64 kernel 路径更重。

x64 下一步必须使用相同源码和参数补齐：

    20000 BPF invocations/round
    1000 helpers/invocation
    5 rounds
    1000 warm-up invocations
    lookup-hit
    update-existing

补齐后直接计算：

    x64 kernel net ns/helper / ARM64 kernel net ns/helper

如果 array update、hash lookup、hash update 在 x64 上仍分别显著高于 ARM64，
即可直接支持“顶层趋势翻转主要来自 x64 kernel map 路径更重”。

完整 raw 数据、环境、公式和测量边界位于：

benchmark-results/uprobe/kernel-map-runtime-arm64-20260801/。
