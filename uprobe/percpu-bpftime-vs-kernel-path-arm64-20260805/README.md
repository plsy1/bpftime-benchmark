# ARM64 per-CPU helper path diagnosis

日期：2026-08-05

本轮只在 Jetson Orin Nano ARM64 上执行，目标是把“per-CPU 操作比普通 map
操作多出来的成本”与 kernel BPF 的同类增量配对比较。没有修改 BPFtime
runtime、kernel 或生产 benchmark；只增加诊断 BPF、victim、loader 和 LLVM-JIT
诊断程序。

## 结论先行

在同一 CPU、同一 key/value、同一 1000-helper 循环下，per-CPU 的额外成本主要
来自 BPFtime userspace map 实现的实际数据结构路径，而不是单独的
`sched_getcpu()`：

- array lookup：BPFtime 的 per-CPU 增量约 **+9.92 ns/helper**，kernel 约
  **+0.60 ns/helper**；
- array update：BPFtime 约 **+43.82 ns/helper**，kernel 约 **+3.38 ns/helper**；
- hash lookup：BPFtime 约 **+74.23 ns/helper**，kernel 约 **+1.17 ns/helper**；
- hash update：BPFtime 约 **+142.33 ns/helper**，kernel 该诊断 workload 中约
  **−25.73 ns/helper**（kernel 的 per-CPU update 反而低于 ordinary hash update）。

因此，当前 ARM64 证据支持的结论是：**BPFtime 的 per-CPU-specific map gap
是真实存在的，最大项是 per-CPU hash update，其次是 per-CPU hash lookup 和
per-CPU array update。** 这不是把 BPFtime 的内部热点直接等同于跨引擎差异；
上面的 gap 是先在每个引擎内部做 ordinary→per-CPU 配对，再相减得到的。

## 测量定义

每个 BPF 程序都执行 1000 次 map helper。控制程序保留相同的 uprobe 触发、
进程启动和循环结构，但不访问 map；kernel 的 net 值是：

```text
kernel net = (real BPF program profile - control BPF program profile) / 1000
```

BPFtime LLVM-JIT 诊断程序使用同一字节码和同一进程内的 noop/real 成对测量：

```text
BPFtime net = (real helper loop - noop helper loop) / 1000
```

对每个 ordinary/per-CPU 对再计算：

```text
perCPU-specific gap = (BPFtime_perCPU - BPFtime_ordinary)
                      - (kernel_perCPU - kernel_ordinary)
```

这一步避免把“两个平台的绝对 ns 不同”或“某个引擎的公共调用固定成本”误判成
per-CPU 特有成本。

## Wall-time 配对结果（ns/helper）

`wall-matched.csv` 是 5 轮结果的均值；完整 raw round 和均值/中位数/标准差在
`wall-raw.csv`、`wall-summary.csv`。

| map / operation | BPFtime ordinary | BPFtime per-CPU | kernel ordinary | kernel per-CPU | BPFtime per-CPU 增量 | kernel per-CPU 增量 | per-CPU-specific gap |
|---|---:|---:|---:|---:|---:|---:|---:|
| array lookup | 10.562 | 20.483 | 1.454 | 2.049 | +9.921 | +0.595 | **+9.326** |
| array update | 16.595 | 60.412 | 11.029 | 14.404 | +43.817 | +3.376 | **+40.442** |
| hash lookup | 41.464 | 115.696 | 25.958 | 27.129 | +74.232 | +1.170 | **+73.062** |
| hash update | 52.973 | 195.307 | 91.535 | 65.805 | +142.334 | −25.730 | **+168.063** |

这里的 kernel/BPFtime 数值来自匹配的 helper 诊断，不是 `benchmark/uprobe`
顶层的总 ns。它们用于定位 map helper 差异；顶层 benchmark 还会额外包含
hook、loader、agent 和 victim 路径。

## PMU 配对结果

### cycles/helper

| map / operation | BPFtime ordinary | BPFtime per-CPU | kernel ordinary | kernel per-CPU | per-CPU-specific gap |
|---|---:|---:|---:|---:|---:|
| array lookup | 18.597 | 35.717 | 2.454 | 3.507 | **+16.067** |
| array update | 28.615 | 104.272 | 18.709 | 25.121 | **+69.245** |
| hash lookup | 72.962 | 204.175 | 44.831 | 46.599 | **+129.446** |
| hash update | 94.145 | 344.417 | 157.944 | 113.706 | **+294.509** |

### instructions/helper

| map / operation | BPFtime ordinary | BPFtime per-CPU | kernel ordinary | kernel per-CPU | per-CPU-specific gap |
|---|---:|---:|---:|---:|---:|
| array lookup | 89.766 | 165.271 | 11.001 | 20.002 | **+66.503** |
| array update | 149.965 | 447.885 | 82.040 | 83.021 | **+296.940** |
| hash lookup | 276.396 | 719.689 | 122.248 | 127.481 | **+438.060** |
| hash update | 329.459 | 1271.642 | 403.571 | 276.537 | **+1069.218** |

PMU 明细见 `pmu-summary.csv` 和 `pmu-matched.csv`。kernel profile 每次只采集
一个 BPF 程序，且所有文件都通过 `enabled == running` 校验；第一版同时 profile
16 个程序造成了 multiplex，已废弃，没有进入这些表。

## 路径归因

本轮的 matched PMU 只回答“哪一类 map 操作造成差异”。具体 userspace 叶子路径
与之前同一提交的 per-CPU hash 分层结果相互印证，详见：
`../percpu-hash-path-arm64-20260805/README.md`。

### 各操作的主要成本、占比和 kernel 对应关系

下面的“路径额外开销”是相邻 A/B 层的差值；“占比”的分母是该操作
`生产 L0 − control` 的 BPFtime 内部增量。这个占比用于回答 BPFtime per-CPU
实现内部哪里最重，不等同于上文已经扣除 kernel 对应成本的
`per-CPU-specific gap`。

“双方都有”表示 kernel BPF 也必须完成相同语义，但 kernel 使用自己的专用 map
实现；“BPFtime 独有”表示 kernel 路径没有该 userspace/Boost/shared-memory
实现步骤；“混合”表示同一 A/B 层同时包含双方共有语义和 BPFtime 特有机制，
当前实验不能进一步把两者严格拆开。

#### per-CPU array lookup：主要是当前 CPU 获取和 `std::function`

生产 L0 相对 control 共增加 **10.090 ns/op**。

| 路径 | 路径额外开销 | 占 L0 额外开销 | kernel 是否也有 |
|---|---:|---:|---|
| key/null/bounds 检查和 slot 地址计算 | +0.393 ns | 3.9% | 双方都有；实现不同 |
| 获取当前 CPU：`my_sched_getcpu()` | +3.792 ns | 37.6% | 双方都有“确定当前 CPU”语义；userspace `sched_getcpu()` 路径为 BPFtime 特有 |
| `ensure_on_current_cpu(std::function)` 调用/构造 | **+5.555 ns** | **55.1%** | **BPFtime 独有** |
| synthetic 等价层到生产 L0 的剩余实现 | +0.351 ns | 3.5% | BPFtime 独有 |

#### per-CPU array update：`std::function` 与 value copy 占 83.3%

生产 L0 相对 control 共增加 **38.948 ns/op**。

| 路径 | 路径额外开销 | 占 L0 额外开销 | kernel 是否也有 |
|---|---:|---:|---|
| key/null/bounds 检查、slot 地址计算 | +2.926 ns | 7.5% | 双方都有；实现不同 |
| 获取当前 CPU：`my_sched_getcpu()` | +3.543 ns | 9.1% | 双方都有语义；userspace 调用路径为 BPFtime 特有 |
| `ensure_on_current_cpu(std::function)` 加 8-byte value copy | **+32.431 ns** | **83.3%** | **混合**：value copy 双方都有，`std::function` 为 BPFtime 独有 |
| synthetic 等价层到生产 L0 的剩余实现 | +0.048 ns | 0.1% | BPFtime 独有，但量级可忽略 |

这一层没有继续把 `std::function` 和 value copy 单独拆开，因此不能声称
32.431 ns 全部来自 `std::function` 或全部来自复制。

#### per-CPU hash lookup：Boost hash/find 占 80.5%

生产 L0 相对 control 共增加 **113.399 ns/op**。

| 路径 | 路径额外开销 | 占 L0 额外开销 | kernel 是否也有 |
|---|---:|---:|---|
| 获取当前 CPU：`my_sched_getcpu()` | +1.731 ns | 1.5% | 双方都有语义；userspace 调用路径为 BPFtime 特有 |
| `key_templates[cpu].assign(key)` | +9.011 ns | 7.9% | BPFtime 独有的 vector key 模板复制；kernel 读取 key，但没有该模板路径 |
| Boost.Interprocess unordered-map hash/find | **+91.293 ns** | **80.5%** | **混合**：hash lookup 双方都有，Boost 容器、`offset_ptr`、vector hash/compare 为 BPFtime 特有 |
| 选择当前 CPU 的 value slot | +1.017 ns | 0.9% | 双方都有；实现不同 |
| synthetic `value_select` 到生产 L0 的容器/iterator 剩余差异 | +10.346 ns | 9.1% | BPFtime 独有 |

PMU 同时显示 `key_copy → hash_find` 增加约 **489 instructions/op** 和约
**0.36 branch-miss/op**，与 wall-time 的大头一致。

#### per-CPU hash update-existing：hash/find 与 value copy 合计占 85.2%

生产 L0 相对 control 共增加 **178.991 ns/op**。

| 路径 | 路径额外开销 | 占 L0 额外开销 | kernel 是否也有 |
|---|---:|---:|---|
| 获取当前 CPU：`my_sched_getcpu()` | +1.769 ns | 1.0% | 双方都有语义；userspace 调用路径为 BPFtime 特有 |
| `key_templates[cpu].assign(key)` | +8.416 ns | 4.7% | BPFtime 独有的 vector key 模板复制 |
| `single_value_templates[cpu].assign(value)` | +8.335 ns | 4.7% | 混合：准备待写 value 双方都有，vector 模板路径为 BPFtime 特有 |
| Boost.Interprocess unordered-map hash/find | **+91.160 ns** | **50.9%** | **混合**：查找语义双方都有，Boost 容器实现为 BPFtime 特有 |
| 命中后复制到当前 CPU 的 value slot | **+61.351 ns** | **34.3%** | **混合**：value update 双方都有，userspace vector/shared-memory copy 路径为 BPFtime 特有 |
| synthetic `copy_existing` 到生产 L0 的剩余差异 | +7.959 ns | 4.4% | BPFtime 独有 |

因此 hash update 的两个绝对大头是 hash/find 和命中后的 per-CPU value copy，
两项合计约 **152.511 ns/op，占 85.2%**。`sched_getcpu()` 只有约 1%，不能解释
该操作的高成本。

#### per-CPU hash delete-hit：主要是 find/erase 和 allocator

生产 L0 相对 control 增加约 **970.801 ns/op**。CPU 选择和 key copy 合计只有
约 10.842 ns；其余约 **99%** 集中在 Boost hash `find + erase`、节点处理和共享
内存 allocator 回收。该百分比只是路径量级估算：synthetic 和生产对象的
allocator/bucket 状态不同，不能像 lookup/update 一样严格逐层相减。delete
语义双方都有，但 Boost erase/allocator 路径为 BPFtime 实现特有；本轮没有为
delete 建立 matched kernel PMU 对照，所以不把它列入上面的跨引擎 gap 表。

综合来看，双方都需要“获取当前 CPU、查找 map、选择 per-CPU slot、读取或更新
value”这些语义；真正只在 BPFtime 出现的是 `std::function`、key/value 模板
vector、Boost.Interprocess 容器、`offset_ptr` 和 shared-memory 分发。当前数据
表明主要额外成本来自这些 BPFtime 实现机制与共有语义的组合，而不是共有语义
本身。

kernel 的 `perf record` 也已归档到 `raw-perf/kernel-cycles-record/`。top symbols
包括：`htab_map_update_elem` 31.97%、`__htab_percpu_map_update_elem` 22.35%、
共享 hash lookup 的 `lookup_nulls_elem_raw` 13.97%、array update 的
`array_map_update_elem` 5.76%。这说明 kernel 侧也有自己的 map helper 成本；
本报告只用它做同口径对照，不把 kernel 的热点当成 BPFtime userspace 原因。

## 实验条件与复现

- 实验基线是 `1ee2eb6e2e2eacfc5a3ec7a3f8769adfdae6d492`；诊断文件随后以
  `e0240a1a81c461f758d3db9fcb8d159e2d9dcf98` 提交到 `codex/official-no-btf`，
  两者的诊断源码相同；LLVM/Clang 15，GCC 13，Boost 1.83，RelWithDebInfo，
  LLVM JIT/LTO 开启，probe read/write check 关闭；
- Jetson CPU5，MAXN_SUPER，1.728 GHz；loader/victim 均 root、taskset CPU5；
- kernel wall：5 轮、每轮 20,000 victim invocation、1000 warm-up；
- BPFtime wall：5 轮，同一进程交替 noop/real；array 100,000 invocation，hash
  20,000 invocation；
- PMU：BPFtime `perf stat -r 3`；kernel `bpftool prog profile` 每个程序独立
  8 秒；所有测试后 `kernel.bpf_stats_enabled` 恢复为 0、BPF links 清零、shared
  memory 清理。

精确命令在 `run-kernel-wall.sh`、`run-bpftime-wall.sh`、
`run-kernel-pmu.sh`、`run-bpftime-pmu.sh` 和 `run-kernel-perf-record.sh`；
`parse.py` 可重新生成所有 summary CSV。

## 边界

1. 这是 ARM64 单平台、匹配诊断程序的路径定位；不能单凭本目录宣称 ARM 特有，
   也不能直接替代 x64 同提交复测。
2. per-CPU-specific gap 是配对差额，不是把不同实验的绝对 ns 强行相减；PMU
   的 BPFtime 数值是 `perf stat -r3` 的每进程平均，kernel 数值是独立程序 profile
   的 `value / run_cnt`，因此 PMU 主要用于量级和路径归因。
3. 本轮没有优化实现；结果只用于确认高成本路径和下一步源码分析。

## 文件索引

- `wall-matched.csv`：wall 的 ordinary/per-CPU 配对与 gap；
- `pmu-matched.csv`：cycles/instructions 的配对与 gap；
- `wall-summary.csv`、`pmu-summary.csv`：逐引擎明细统计；
- `raw-kernel/`：kernel wall CSV、状态和 stderr；
- `raw-bpftime/`：BPFtime wall/PMU stdout 与 perf 输出；
- `raw-perf/kernel-cycles/`、`kernel-instructions/`：逐程序 profile JSON 和
  xlated/JIT dump；
- `raw-perf/kernel-cycles-record/`：kernel cycles call graph、perf.data 和报告；
- `parse.py`、`run-*.sh`：可重复解析与执行脚本。
