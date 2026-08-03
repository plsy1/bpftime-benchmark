# Jetson hash lookup 顶层残差定位

## 技术结论

原先计算出的约 `12.011 ns/helper`“顶层残差”已经被收紧到一个明确边界：

> 它不在 empty uprobe、1000 次循环或 key/stack 准备路径中；差异只在实际
> hash-map helper 被调用的完整 BPF 程序进入 BPFtime production agent 后出现。

matched 顶层实验使用同一个 victim、同一个 loader 和三个 probe：

```text
A: empty
B: loop-control（1000 次循环 + key/stack 准备，无 helper）
C: full hash lookup-hit（1000 次 bpf_map_lookup_elem）
```

6 个独立进程、三种执行顺序各出现两次后的结果：

| 顶层分段 | kernel | BPFtime | BPFtime − kernel |
|---|---:|---:|---:|
| `B − A`：非 helper 循环 | 1.204 ns/helper | 0.802 ns/helper | **−0.402 ns/helper** |
| `C − B`：包含 helper 的路径 | 25.537 ns/helper | 53.705 ns/helper | **+28.167 ns/helper** |
| `C − A`：完整顶层路径 | 26.741 ns/helper | 54.507 ns/helper | **+27.766 ns/helper** |

闭合关系为：

```text
27.766 = -0.402 + 28.167 ns/helper
```

因此此前“残差可能是普通 JIT loop/key 准备成本”的假设被否定。BPFtime 的
loop-control 反而比 kernel 少约 `0.402 ns/helper`。完整性能差异全部集中在
`full hash lookup − loop-control` 这一侧。

但还不能把 `+28.167 ns/helper` 全部称为 hash-map C++ 实现本体。这个分段同时包含：

- production BPF 程序中的 map relocation/fd 参数；
- JIT 生成的 helper call 和调用 ABI；
- production agent 中实际链接的 map helper 与 map implementation；
- helper 在真实 uprobe/agent 上下文中的交互。

也就是说，位置已经从“helper 前后所有顶层路径”缩小到“实际 map-helper 调用所在
的 production agent 路径”，尚未缩小到其中某一个函数。

## PMU 证明差异来自额外指令，而不是随机 wall-time 波动

对三个 case 分别运行 `perf stat`，每次只采一个 event；cycles 和 instructions
各 3 轮，所有数据均为 `100% running`，没有 multiplex。

| 顶层分段 | kernel cycles | BPFtime cycles | 差额 | kernel instructions | BPFtime instructions | 差额 |
|---|---:|---:|---:|---:|---:|---:|
| 非 helper 循环 | 2.074 | 1.395 | −0.679 | 10.992 | 4.001 | −6.991 |
| 包含 helper 的路径 | 44.072 | 93.646 | **+49.574** | 121.472 | 361.994 | **+240.523** |
| 完整顶层路径 | 46.145 | 95.041 | +48.895 | 132.464 | 365.995 | +233.532 |

单位均为每个 helper。instruction 差额非常稳定：包含 helper 的路径三轮差额标准差
只有 `0.0015 instructions/helper`。因此这不是调度噪声，也不是相同指令在 ARM
上偶然执行得慢；production BPFtime 路径确实每次 lookup 多执行约 240 条指令。

## 与原顶层 benchmark 相互验证

| 数据 | kernel | BPFtime | 差额 |
|---|---:|---:|---:|
| 原官方 5-process 顶层结果 | 27.808 | 54.608 | +26.800 |
| 本轮 matched `C − A` | 26.741 | 54.507 | +27.766 |

BPFtime 绝对结果只相差 `0.101 ns/helper`；kernel 相差约 `1.067 ns/helper`。新诊断
复现了原问题的方向和量级，不是换了程序后产生的新现象。

执行顺序也没有改变结论。三个顺序下的两轮平均总差额分别为 `27.907`、`27.668`
和 `27.723 ns/helper`，全部稳定为 BPFtime 更慢。

## 为什么原来的 12.011 ns 不能叫一个独立 runtime 阶段

旧闭合使用了三种 control：

- 官方顶层：`full hash lookup − empty probe`；
- standalone BPFtime JIT：`real helper − no-op helper`；
- kernel runtime：`real BPF program − loop-control BPF program`。

它们的程序形状和最终链接边界不同。尤其是 standalone 诊断 executable 与
production agent 虽然来自同一 runtime 源码和构建配置，但 LTO 后并不是同一份
机器码：

| 最终产物 | `bpftime_map_lookup_elem_helper` symbol size |
|---|---:|
| standalone `hash-helper-jit-layers` | `0x1518` bytes |
| production `libbpftime-agent.so` | `0x38` bytes |

agent 中该 symbol 是一个很小的 wrapper/tail-call，后续进入 const-propagated
函数；standalone executable 中则形成较大的 generic body。symbol size 不能用于
判断谁快，但足以证明两者不是 byte-identical A/B。

所以更准确的修正表述是：

> `12.011 ns/helper` 是跨 harness、跨 control、跨最终链接产物相减得到的整合项，
> 不是已经观测到的一个独立函数或阶段。本轮 matched 顶层实验证明该项属于
> helper-containing production path，而不是普通 loop/key-preparation path。

## 与 standalone helper 数据的关系

既有 standalone/direct 数据为：

| 路径 | ns/helper | cycles/helper | instructions/helper |
|---|---:|---:|---:|
| BPFtime direct L3 | 41.261 | 71.250 | 285.908 |
| kernel matched runtime | 26.769 | 44.441 | 122.410 |

本轮 production 顶层的 helper-containing 分段为：

| 路径 | ns/helper | cycles/helper | instructions/helper |
|---|---:|---:|---:|
| BPFtime `C − B` | 53.705 | 93.646 | 361.994 |
| kernel `C − B` | 25.537 | 44.072 | 121.472 |

相对 standalone/direct 边界：

- BPFtime production 分段多 `12.444 ns`、`22.396 cycles`、约 `76.086 instructions`；
- kernel production 分段少 `1.232 ns`、`0.369 cycles`、约 `0.938 instructions`。

这些是独立实验间的方向性比较，不是同步 A/B，但说明旧 residual 主要由 BPFtime
production agent 上下文相对 standalone diagnostic 的差异产生。kernel 的两个
边界则相当接近。

## 测量定义与实验控制

- Jetson Orin Nano，CPU5，无 SMT sibling；
- MAXN_SUPER，`jetson_clocks`，CPU 1.728 GHz；
- host 运行，不使用 Docker；
- loader 和 victim 均固定 CPU5、均为 root；
- LLVM/Clang 15，victim 使用 GCC 13 `-g -O3`；
- BPFtime 使用与官方顶层结果相同的 agent/syscall-server 二进制；
- 每个 BPF program invocation 执行 1000 次循环/helper；
- wall-time：6 个独立 victim，每个 case 100000 invocations；
- PMU：3 个独立进程组，每个 case 100000 invocations；
- hash map 在计时前插入 key `0..999`，kernel 已通过 bpftool 验证 1000 entries；
- 三个 victim 函数具有相同的 12-byte AArch64 函数体，地址各自独立；
- 结束后 kernel BPF links 为 0、`kernel.bpf_stats_enabled=0`、BPFtime shm 不存在。

## 稳健性与限制

1. `B − A` 与 `C − B` 在同一 victim 进程中配对，且顺序轮换，因此 wall-time
   分解不依赖固定执行顺序。
2. PMU 三个 case 是独立进程；agent 启动、map setup 和三项 warm-up 在每个进程中
   相同，通过 case subtraction 抵消。固定成本仍可能留下极小误差，但无法解释
   240 instructions/helper 的稳定差额。
3. loop-control 保留 bounded loop、key stack store 和指针准备，但不能在不调用
   helper 的同时保留有意义的 map relocation。因此 `C − B` 仍是一个包含多个
   子环节的边界。
4. 本轮没有修改 kernel 或 BPFtime runtime。新增内容仅为独立诊断 BPF program、
   loader、victim 和运行/汇总脚本。

## 下一步只需要在 production helper-containing 路径内继续切分

要进一步定位约 76 条 BPFtime production-only instructions，需要在**同一个 agent
最终链接产物**中形成 matched A/B，而不是继续使用 standalone executable 相减。
优先级如下：

1. 在不改变 production helper 机器码的条件下，获得同一 BPF 指令流的
   no-op-helper/real-helper agent A/B；
2. 若不能动态替换 helper，则在 agent 内对 JIT function entry、helper entry、
   map-handler entry 分别采集硬件计数边界；
3. 对比这些边界后，再决定剩余指令属于 JIT call trampoline、fd/handler dispatch
   还是 hash implementation。

在完成这个同 binary A/B 之前，不能仅凭旧 standalone profile 把 12 ns 指认给
某个 C++ helper 函数。

## 文件

- `decomposition.csv`：wall-time 三段闭合和 BPFtime-kernel 差额；
- `pmu-summary.csv`：cycles/instructions 三段结果；
- `wall-raw.csv`、`pmu-raw.csv`：逐进程原始派生值；
- `raw/`、`raw-pmu/`：victim、loader、`time -v` 和 `perf stat` 原始输出；
- `run.sh`、`run-pmu.sh`：精确执行脚本；
- `summarize.py`：全部 CSV 的生成与闭合校验；
- `codegen-audit.txt`：BPF 指令数量与最终链接 symbol 审计。
