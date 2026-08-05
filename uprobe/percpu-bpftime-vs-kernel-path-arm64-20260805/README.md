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

该分层结果显示：

- per-CPU hash lookup：`sched_getcpu()` 约 +1.73 ns，key copy 约 +10.74 ns；
  `Boost.Interprocess`/unordered-map 的 hash `find` 约 +102.04 ns，是主体；
  per-CPU value slot 选择仅约 +1.02 ns；生产 L0 相对 synthetic 层还有约
  +10.35 ns 的完整容器路径差异；
- per-CPU hash update-existing：key/value 模板复制先增加约 +18.52 ns；hash
  find 后的命中 value copy 再增加约 +61.35 ns；生产 L0 相对 synthetic 层再
  增加约 +7.96 ns；
- 因而不能把整个 per-CPU 差异归因给 `sched_getcpu()`，它只是固定小项，主要
  成本是 key hash/find、容器访问和 per-CPU value copy。

kernel 的 `perf record` 也已归档到 `raw-perf/kernel-cycles-record/`。top symbols
包括：`htab_map_update_elem` 31.97%、`__htab_percpu_map_update_elem` 22.35%、
共享 hash lookup 的 `lookup_nulls_elem_raw` 13.97%、array update 的
`array_map_update_elem` 5.76%。这说明 kernel 侧也有自己的 map helper 成本；
本报告只用它做同口径对照，不把 kernel 的热点当成 BPFtime userspace 原因。

## 实验条件与复现

- source commit：`1ee2eb6e2e2eacfc5a3ec7a3f8769adfdae6d492`，branch
  `codex/official-no-btf`；LLVM/Clang 15，GCC 13，Boost 1.83，RelWithDebInfo，
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
