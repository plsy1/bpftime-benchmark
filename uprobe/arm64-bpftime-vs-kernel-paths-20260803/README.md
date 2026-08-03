# Jetson ARM64：BPFtime 与 kernel map 路径差额闭合

## 技术结论

本轮已经把 Jetson 顶层 uprobe benchmark 的三个差额闭合到三个边界：
**helper 直接路径差额、BPFtime JIT 上下文增量、顶层整合残差**。计算闭合误差均为
`0.000 ns/helper`。

| 操作 | kernel 顶层 | BPFtime 顶层 | 总差额 | helper 直接差额 | JIT 上下文 | 顶层整合残差 |
|---|---:|---:|---:|---:|---:|---:|
| array lookup-hit | 2.563 | 12.901 | **+10.338** | +7.536（72.9%） | +1.143（11.1%） | +1.659（16.1%） |
| array update-existing | 11.858 | 16.426 | **+4.568** | +2.226（48.7%） | +1.329（29.1%） | +1.013（22.2%） |
| hash lookup-hit | 27.808 | 54.608 | **+26.800** | +14.492（54.1%） | +0.297（1.1%） | **+12.011（44.8%）** |

单位均为 `ns/helper`。百分比只是对总差额的代数分摊，不表示三个量来自同一次
同步测量。

结论分三层理解：

1. **array lookup 的主要原因已经定位到 BPFtime userspace helper 的通用路径。**
   array 本体只用 0.228 ns / 14 instructions；generic handler 和
   shm/fd/variant dispatch 分别再增加 4.760 ns 和 4.959 ns。kernel 对应路径只有
   1.384 ns、2.422 cycles、10.997 instructions。
2. **array update 的 BPFtime 直接 helper 确实更重，但 kernel 自身的 update
   工作也很重。** BPFtime L3 为 13.178 ns，kernel runtime 为 10.952 ns，直接
   差额只有 2.226 ns；其余主要来自 JIT 上下文和约 1 ns 的顶层差异。不能把
   BPFtime helper 内所有 13.178 ns 都称为相对 kernel 的额外开销。
3. **hash lookup 尚未完全定位。** helper 直接路径解释 14.492 ns，但另有
   12.011 ns 顶层差异没有在现有 direct/JIT 边界内出现。现有证据足以确认 hash
   本体、无竞争锁和通用分发都重，但不足以把这 12.011 ns 指认给某一个函数。

因此，本阶段不是“ARM 上全部由 kernel 分母过小造成”，也不是“所有差异都已归因
到 BPFtime map 实现”。array lookup 明显是 BPFtime 通用 userspace 路径占主导；
array update 同时包含 BPFtime 分子和 kernel 固有工作；hash lookup 仍保留一个
较大的顶层待定位项。

## 三项差额如何闭合

闭合定义为：

```text
顶层 BPFtime-kernel 差额
  = (BPFtime direct L3 - kernel runtime)
  + (BPFtime JIT helper A/B - BPFtime direct L3)
  + [(BPFtime 顶层 - BPFtime JIT) - (kernel 顶层 - kernel runtime)]
```

最后一项称为“顶层整合残差”。它是两个环境在 helper 边界之外的**差分残差**，
不是 BPFtime 单侧的绝对 uprobe 成本，也不能仅凭减法确定内部函数。

### Array lookup：10.338 ns 中 7.536 ns 来自 helper 直接差额

| 边界 | BPFtime | kernel | BPFtime-kernel |
|---|---:|---:|---:|
| 顶层 empty-subtracted | 12.901 | 2.563 | +10.338 |
| direct helper/runtime | 8.920 | 1.384 | +7.536 |
| BPFtime JIT 相对 direct | 1.143 | — | +1.143 |
| 顶层整合残差 | — | — | +1.659 |

BPFtime helper 内部的累计/相邻层结果：

| 路径层 | 累计 ns | 相邻增量 ns | 增量 cycles | 增量 instructions |
|---|---:|---:|---:|---:|
| array 实现 | 0.228 | +0.228 | +0.393 | +14 |
| generic handler | 4.989 | +4.760 | +8.219 | +50 |
| shm/fd/variant dispatch | 9.947 | +4.959 | +8.562 | +34 |
| LTO 后生产 helper | 8.920 | -1.027 | -1.774 | -9 |

L3 比 L2 小是 whole-path LTO 重塑后的结果，不是一个物理 wrapper 的负成本。
它不能被强制改写成严格可加的源代码计时。

### Array update：kernel 自身工作抵消了大部分 BPFtime helper 毛成本

| 边界 | BPFtime | kernel | BPFtime-kernel |
|---|---:|---:|---:|
| 顶层 empty-subtracted | 16.426 | 11.858 | +4.568 |
| direct helper/runtime | 13.178 | 10.952 | +2.226 |
| BPFtime JIT 相对 direct | 1.329 | — | +1.329 |
| 顶层整合残差 | — | — | +1.013 |

| 路径层 | 累计 ns | 相邻增量 ns | 增量 cycles | 增量 instructions |
|---|---:|---:|---:|---:|
| array 实现（含 8-byte update） | 2.771 | +2.771 | +4.785 | +48 |
| generic handler | 8.937 | +6.166 | +10.647 | +60 |
| shm/fd/variant dispatch | 14.878 | +5.941 | +10.257 | +41 |
| LTO 后生产 helper | 13.178 | -1.701 | -2.936 | -18 |

PMU 对照也显示两边都执行了实质工作：BPFtime L3 是 22.753 cycles / 131
instructions；kernel runtime 是 19.034 cycles / 82.029 instructions。BPFtime 多执行
约 49 条指令，但 wall-time 的 direct 差额只有 2.226 ns。

### Hash lookup：约一半在 helper 内，约一半仍在顶层

| 边界 | BPFtime | kernel | BPFtime-kernel |
|---|---:|---:|---:|
| 顶层 empty-subtracted | 54.608 | 27.808 | +26.800 |
| direct helper/runtime | 41.261 | 26.769 | +14.492 |
| BPFtime JIT 相对 direct | 0.297 | — | +0.297 |
| 顶层整合残差 | — | — | **+12.011** |

| 路径参考 | 累计 ns | 方向性增量 ns | 增量 cycles | 增量 instructions |
|---|---:|---:|---:|---:|
| 无竞争 spin lock | 11.451 | +11.451 | +19.813 | +43 |
| hash 本体（扣 lock 参考） | 36.414 | +24.963 | +43.083 | +126.908 |
| generic handler | 39.971 | +3.557 | +6.130 | +79 |
| shm/fd/variant dispatch | 41.415 | +1.444 | +2.483 | +34 |
| LTO 后生产 helper | 41.261 | -0.154 | -0.259 | +3 |

这里 lock-only 和 L0 hash 是独立测量参考，因此 `L0-lock` 只能作为方向性拆分，
不能声称锁和 map body 在同一次执行中严格可加。新补的 kernel PMU 是 44.441
cycles / 122.410 instructions；BPFtime L3 是 71.250 cycles / 285.908 instructions。
这确认了 helper 层 BPFtime 分子较重，同时 kernel hash lookup 本身也不是一个很小
的分母。

## 新补 PMU 数据

原归档缺少 kernel array/hash lookup 的 matched control/real PMU。本轮没有修改
kernel 或 BPFtime runtime，只重建并运行既有 `kernel-map-runtime` 诊断对象。

| 操作 | kernel ns/helper | cycles/helper | instructions/helper | PMU 状态 |
|---|---:|---:|---:|---|
| array lookup-hit | 1.384 | 2.422 | 10.997 | 新补；无 multiplex |
| array update-existing | 10.952 | 19.034 | 82.029 | 既有归档 |
| hash lookup-hit | 26.769 | 44.441 | 122.410 | 新补；无 multiplex |

新数据每个 control/real 程序均记录 80,000 次运行；每个 JSON 都满足
`enabled == running`。wall time 换算的名义 cycles 与 PMU 的差别，对 array lookup
约 1.3%，对 array update 约 0.6%，对 hash lookup 约 3.9%，量级一致。

## 数据兼容性与不确定性

- 官方顶层值来自提交 `8ed291e` 的 5 个独立进程中位数；direct/JIT/kernel
  runtime 是独立诊断实验的均值。它们使用相同 map 形状、key/value、hit/existing
  语义、CPU5、工具链和生产 runtime 源码，但不是同一时间同步采样。
- `8ed291e..176eb29` 只增加 value-size 诊断；没有修改生产 runtime。因此新 PMU
  可与已有 kernel-runtime harness 直接对照。
- `ead56c9..8ed291e` 只修改 benchmark/诊断语义并增加诊断文件，没有修改生产
  runtime。array/hash direct 与 JIT 数据可用于独立边界估计。
- 表中 uncertainty 是来源样本标准差的平方和传播，用于表示跨实验波动量级，
  **不是置信区间**，也不包含系统性 harness 差异。
- 三个总差额的传播样本标准差约为 0.063、0.060、0.278 ns/helper；闭合残差的
  传播量级约为 0.064、0.063、0.281 ns/helper。hash 的 12.011 ns 远大于该波动，
  因而不是简单随机抖动，但其内部来源仍未定位。
- 8-byte value-size sweep 使用不同 fixed-struct BPF/control 模板，只作为趋势证据，
  没有被拿来替换本报告的 kernel runtime 分母。

完整兼容性判定见 `data-compatibility.csv`，全部精确值和公式见 `closure.csv`；表格
适合精确审计，因此本报告没有另画会掩盖小量差额的图。

## 方法与复现

新 PMU 采集保持 Jetson host、CPU5、MAXN_SUPER、1.728 GHz、1000 helper/program、
20,000 invocations × 5 rounds、1,000 warm-up。一次只采一个 metric，control 和 real
同时挂 profiler，各采 30 秒。

```bash
cd /home/jetson/src/benchmark-results/uprobe/arm64-bpftime-vs-kernel-paths-20260803
./build-kernel-map-runtime.sh
./run-kernel-lookup-pmu.sh
python3 summarize.py
```

关键文件：

- `profile-kernel-lookup-pmu.sh`：单操作、单 PMU metric 采集和清理；
- `run-kernel-lookup-pmu.sh`：array/hash lookup 的 cycles/instructions 矩阵；
- `raw-kernel-pmu/`：原始 PMU JSON、BPF program IDs、xlated/JIT dump 和频率；
- `kernel-pmu.csv`：新旧 kernel PMU 的统一表；
- `layer-attribution.csv`：BPFtime 累计层、相邻增量、kernel 分母和闭合项；
- `summarize.py`：从原始归档重新生成全部派生 CSV。

## 后续只需继续定位 hash 顶层残差

如果继续调查，最有价值的下一步不是再拆 array map，而是对 hash lookup 做一个
**同一顶层 harness 内的 matched boundary A/B**，区分：

1. JIT helper 返回后的 BPF program/agent 调度；
2. uprobe 触发、上下文准备与回到 victim；
3. top-level victim 计时中 kernel/BPFtime 不对称的固定工作。

目标量就是 12.011 ns/helper。任何新增诊断都应保持 hash 预填充、lookup-hit、
1000 helper 和 production JIT program shape，不应修改 runtime 后再解释旧差额。

## 验证状态

结论可在注明限制后共享：闭合计算已经独立重算；新 PMU 无 multiplex；运行结束时
`kernel.bpf_stats_enabled=0`、kernel BPF link 为 0、BPFtime shared memory 已清除。
唯一实质性未决项是 hash lookup 的顶层整合残差，报告中没有把它错误归因。
