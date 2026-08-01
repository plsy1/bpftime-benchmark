# BPFtime 普通 array map 的 ARM64/x64 差异诊断

日期：2026-07-31

## 技术摘要

当前数据不支持“BPFtime 普通 array map 在 ARM64 上实现得特别慢，而在 x64
上基本都比 kernel 快”这一概括。

更准确的结论是：

1. **array lookup 在 ARM64 和 x64 上都是 BPFtime 更慢。** Linux 会把
   kernel array lookup helper 替换成一小段内联 BPF 指令；BPFtime 仍经过
   通用的 helper、共享内存 fd、handler、variant 和 map-type 分发路径。
2. **array delete 不是有效的删除 benchmark。** kernel 和 BPFtime 的 array
   delete 都只返回 `EINVAL`。原始 x64 表中看似 BPFtime 更快，减掉各自的
   Empty Probe 后只剩约 3% 差异，而且这个错误返回项本来就不能代表删除性能。
3. **真正发生方向翻转的只有 array update。** 最新 Actions run 中，ARM64
   kernel update 为 6.39 ns/helper，BPFtime 为 9.37 ns/helper；x64 kernel
   却为 41.47 ns/helper，BPFtime 为 10.81 ns/helper。方向翻转是 x64 kernel
   update 变重造成的，不是 BPFtime ARM64 的绝对成本异常。
4. **x64 Actions runner 存在两档明显不同的 kernel helper 成本。**
   `centralus` 的一次 run 中，x64 kernel update 只有 7.55 ns/helper；后续
   两次 `eastus` run 均约 41.5 ns/helper。runner image 相同，BPFtime 源码的
   相关 map 路径也未改变。该变化与物理宿主、CPU/微码和 x86 kernel
   call/return mitigation 配置相关的可能性很高，但现有 artifact 没有保存
   vulnerability、kernel config、JIT dump 和 x64 perf counters，尚不能把
   具体机制认定为已证实根因。

一句话总结：

> ARM64 上 BPFtime 普通 array 慢，主要因为 ARM kernel 的 array 路径极轻，
> 而 BPFtime 承担通用 userspace map 分发成本；x64 上 update 的优势则主要
> 来自某些 GitHub runner 上 kernel helper call/return 路径显著偏重。

## 测量定义：必须先减掉 Empty Probe

`benchmark/uprobe/uprobe.bpf.c` 的每个 map 程序在一次函数触发中执行 1000 次
map 操作。原始 `avg` 同时包含一次 uprobe 触发成本。

本文使用：

```text
net ns/helper
  = (map operation avg ns - 同环境 __bench_uprobe avg ns) / 1000
```

kernel 和 BPFtime 必须分别减掉自己的 Empty Probe，不能直接拿原始表中的两个
`avg` 相除。尤其在 x64 上，kernel Empty Probe 明显比 BPFtime 更重，不扣除会
把基础 uprobe 优势错误归到 map 操作上。

## 最新 Actions run：x64 不是三项都更快

来源：GitHub Actions run
[`30340475614`](https://github.com/plsy1/bpftime-benchmark/actions/runs/30340475614)，
每个平台各 10 轮。单位均为减去 Empty Probe 后的 `ns/helper`。

| Operation | ARM Kernel | ARM BPFtime | ARM B/K | x64 Kernel | x64 BPFtime | x64 B/K |
|---|---:|---:|---:|---:|---:|---:|
| array lookup | 1.186 | 6.454 | 5.44× | 0.950 | 7.022 | 7.39× |
| array update | 6.394 | 9.369 | 1.47× | 41.469 | 10.811 | 0.26× |
| array delete/error | 1.246 | 6.443 | 5.17× | 7.169 | 6.930 | 0.97× |

从这张表可以直接看出：

- lookup 在两边都由 kernel 获胜；
- update 只有 x64 由 BPFtime 明显获胜；
- delete 在 x64 净成本基本相等，但它只是错误返回路径。

BPFtime 本身的 ARM64/x64 绝对时间处在同一数量级：

- lookup：6.45 vs 7.02 ns/helper；
- update：9.37 vs 10.81 ns/helper；
- delete/error：6.44 vs 6.93 ns/helper。

因此不能把平台内 `BPFtime / Kernel` 倍率的变化解释为 BPFtime ARM64 runtime
突然低效。主要变化来自 kernel 分母。

## 三次 workflow 显示 x64 kernel 出现两档路径

下表仍是减掉 Empty Probe 后的净成本：

| Run | x64 region | Operation | Kernel ns/helper | BPFtime ns/helper | B/K |
|---|---|---|---:|---:|---:|
| `30260879794` | `centralus` | lookup | 1.251 | 4.023 | 3.22× |
| `30260879794` | `centralus` | update | 7.551 | 6.687 | 0.89× |
| `30260879794` | `centralus` | delete/error | 0.811 | 4.169 | 5.14× |
| `30333737528` | `eastus` | lookup | 1.025 | 6.873 | 6.70× |
| `30333737528` | `eastus` | update | 41.548 | 10.625 | 0.26× |
| `30333737528` | `eastus` | delete/error | 7.182 | 6.902 | 0.96× |
| `30340475614` | `eastus` | lookup | 0.950 | 7.022 | 7.39× |
| `30340475614` | `eastus` | update | 41.469 | 10.811 | 0.26× |
| `30340475614` | `eastus` | delete/error | 7.169 | 6.930 | 0.97× |

值得注意：

- 两次 `eastus` 结果高度一致，不是单轮抖动；
- 相比 `centralus`，`eastus` kernel update 重约 **5.5×**；
- kernel delete/error 重约 **8.8×**；
- 内联的 kernel lookup 没有一起变重，反而略快；
- 同期 BPFtime 虽然也受 runner 绝对性能影响，但变化远小于 kernel update。

三次 ARM64 run 分布在 `northcentralus` 和 `westus2`，普通 array 数据基本稳定：

| Operation | ARM Kernel 范围 | ARM BPFtime 范围 |
|---|---:|---:|
| lookup | 1.183–1.186 ns/helper | 6.454–6.470 ns/helper |
| update | 6.394–6.401 ns/helper | 9.369–9.377 ns/helper |
| delete/error | 1.207–1.246 ns/helper | 6.443–6.455 ns/helper |

这说明当前跨架构比较还混入了 GitHub x64 宿主池差异。不能把某次 x64
runner 的结果直接上升为普适的 ISA 结论。

## array lookup：kernel 内联，BPFtime 经过通用分发

Linux 6.17 的 `array_map_ops` 注册了：

```c
.map_lookup_elem = array_map_lookup_elem,
.map_gen_lookup = array_map_gen_lookup,
```

verifier 在 JIT 模式下发现 map 提供了 `map_gen_lookup` 后，会用
`array_map_gen_lookup()` 生成的 BPF 指令替换原来的
`bpf_map_lookup_elem()` 调用。该内联路径主要只有：

```text
读取 key
  -> 范围检查
  -> index mask / shift
  -> 与 array value 基址相加
```

它不再执行一个通用的 map lookup helper 函数调用。

BPFtime 当前没有做同等的 map-type specialization。一次普通 array lookup
仍经过：

```text
LLVM JIT helper call
  -> bpftime_map_lookup_elem_helper()
  -> bpftime_shm::bpf_map_lookup_elem()
  -> try_get_map_handler()
       -> is_map_fd()
       -> handler_manager::get_handler()
       -> std::holds_alternative
       -> 再次 get_handler()
       -> std::get<bpf_map_handler>
  -> bpf_map_handler::map_lookup_elem()
       -> should_lock 判断
       -> map type switch
       -> map_impl_ptr
  -> array_map_impl::elem_lookup()
       -> key 范围检查
       -> Boost.Interprocess vector 地址计算
```

普通 array 的 `should_lock=false`，所以这里的差距不是锁竞争造成的。底层
`elem_lookup()` 本身也很短，主要差异来自 kernel lookup 已被内联，而 BPFtime
保留了通用 userspace helper/handler 分发。

Jetson focused `perf stat` 与该解释一致：

| array lookup | Kernel | BPFtime |
|---|---:|---:|
| ns/helper | 3.815 | 14.353 |
| instructions/helper | 23.19 | 121.76 |

BPFtime 每个 lookup 多执行约 99 条指令。它不是低 IPC 或 cache miss 主导，而是
执行了更多软件层。

## array update：ARM kernel 很轻，x64 runner 的 call 路径发生分档

kernel verifier 不会像 lookup 那样把完整 array update 展开为普通 BPF
算术指令，而是把 helper 定向到 `array_map_update_elem()`。

对本 benchmark 的 8 字节 value、`BPF_ANY`、合法 key，核心工作是：

```text
flags/key 检查
  -> 计算 value 地址
  -> copy_map_value() 复制 8 字节
  -> bpf_obj_free_fields()
  -> return
```

BPFtime update 则继续经过与 lookup 类似的共享内存 fd/handler/type 分发，
最后由 `array_map_impl::elem_update()` 做 flag/key 检查和 8 字节
`std::copy`。

Jetson focused counters：

| array update | Kernel | BPFtime |
|---|---:|---:|
| ns/helper | 13.087 | 18.093 |
| instructions/helper | 93.20 | 168.76 |

BPFtime 多约 76 条指令/helper，符合通用分发路径的额外成本。

但最新 x64 Actions 的 kernel update 达到约 41.5 ns/helper，明显不是简单的
8 字节 copy 本身能够解释。Linux x86 JIT 在 helper call 前会执行
`x86_call_depth_emit_accounting()`，再发出直接 `call`；kernel 函数返回还会
受到具体 CPU vulnerability mitigation、kernel config、boot 参数和微码状态
影响。ARM64 JIT 则根据距离发出直接 `BL` 或间接 `BLR`。

当前数据呈现出很强的调用路径特征：

- lookup 被内联，没有随 eastus x64 runner 一起变重；
- update 和 delete/error 都需要进入 kernel map op，二者同时显著变重；
- runner image 版本相同，但物理 worker 和 Azure region 不同。

因此，**x86 kernel call/return mitigation 或宿主差异是目前最强的原因候选**。
不过 Actions artifact 没有保存以下信息：

- `/sys/devices/system/cpu/vulnerabilities/*`；
- `/proc/cmdline`；
- kernel config 中的 `RETHUNK`、SRSO、call-depth tracking 等选项；
- `net.core.bpf_jit_*`；
- JIT 后的机器码；
- x64 `perf stat/record`。

所以这一层目前应标记为“高可信候选”，还不能写成最终已证实的具体机制。

另外，Actions 使用的是 Ubuntu Azure `6.17.0-1020-azure` kernel，而下面引用的
是 upstream Linux `v6.17` 源码。核心的 array map/verifier/JIT 结构能够解释
现象，但 Azure kernel 可能包含额外补丁；最终归因仍应以该 runner 的 JIT dump、
kernel config 和 perf 结果为准。

## array delete：不能用于评价 map 删除性能

Linux 的 `array_map_delete_elem()` 直接：

```c
return -EINVAL;
```

BPFtime 的 `array_map_impl::elem_delete()` 同样设置 `errno=EINVAL` 并返回
`-1`。array 元素是预分配的，不支持删除。

因此该项实际测量的是：

```text
uprobe/hook
  + BPF/JIT dispatch
  + map-op call/通用分发
  + EINVAL 返回
```

它可以作为 helper/call 分发的诊断载体，但不能拿来声称“BPFtime array delete
比 kernel 快”。

## 已证实、可能原因和未解决部分

### 已证实

- BPFtime array lookup 在 ARM64 和 x64 上都慢于 kernel；
- kernel array lookup 会通过 `map_gen_lookup` 被内联；
- BPFtime 普通 array 仍执行通用 userspace map 分发；
- Jetson 上 lookup/update 的 BPFtime 指令数分别比 kernel 多约 99/76 条；
- x64 只有 update 显示明确的 BPFtime 优势；
- x64 delete 的 raw 优势主要受 Empty Probe 影响，且 delete 本身无效；
- x64 kernel update 在不同 Actions worker/region 上存在约 5.5× 的两档成本；
- 该 regime change 远大于单次 10 轮内部方差。

### 高可信但尚未最终证实

- x64 kernel update/delete 的分档与物理宿主、CPU/微码以及 x86 kernel
  call/return mitigation 有关；
- BPFtime x64 的相对优势被 x64 kernel 侧额外成本放大。

### 仍需验证

- 第一轮 `centralus` x64 artifact 没有保存 CPU model，region 与性能档位目前
  只是相关性，不能单独证明 region 是原因；
- 两档 x64 runner 的 CPU vendor/model 是否完全一致；
- vulnerability mitigation 和 kernel config 到底差在哪一项；
- x64 update 的 41.5 ns/helper 中，JIT call、`array_map_update_elem()`、
  `bpf_obj_free_fields()` 和 return thunk 各占多少。

当前报告的验证等级为：**可带 caveat 分享**。关于 lookup 内联、BPFtime 通用
分发、delete 语义和 kernel 分母效应的结论已由原始结果与源码交叉验证；具体
x86 mitigation 仍是待下一轮 focused workflow 验证的原因候选。

## 最小后续验证

下一次 x64/ARM64 focused workflow 不需要跑完整 uprobe benchmark，只跑：

```text
Empty Probe
array lookup
array update
array delete/error
```

同时保存：

```text
lscpu
/proc/cpuinfo
/proc/cmdline
/sys/devices/system/cpu/vulnerabilities/*
kernel config 中的 BPF_JIT、RETHUNK、SRSO、CALL_DEPTH、RETPOLINE
net.core.bpf_jit_enable
net.core.bpf_jit_harden
net.core.bpf_jit_kallsyms
bpftool prog dump jited
perf stat: task-clock, cycles, instructions, branches, branch-misses
perf record/report: array update
```

判断规则：

1. 若 x64 update 变重时 instructions/helper 同步大增，优先查 JIT 展开和
   mitigation 指令；
2. 若 instructions 接近但 cycles 大增，优先查 return thunk、barrier、虚拟化
   和宿主调度；
3. 若只在 `array_map_update_elem()` / `bpf_obj_free_fields()` 中变重，继续拆
   kernel map-op 内部；
4. 若同一 runner 上复测能在两档之间切换，再查动态 mitigation 或频率；否则
   先按宿主池异构处理。

## 来源

- Actions：
  [`30260879794`](https://github.com/plsy1/bpftime-benchmark/actions/runs/30260879794)、
  [`30333737528`](https://github.com/plsy1/bpftime-benchmark/actions/runs/30333737528)、
  [`30340475614`](https://github.com/plsy1/bpftime-benchmark/actions/runs/30340475614)
- BPFtime：
  `benchmark/uprobe/uprobe.bpf.c`、
  `runtime/src/bpf_helper.cpp`、
  `runtime/src/bpftime_shm_internal.cpp`、
  `runtime/src/handler/map_handler.cpp`、
  `runtime/src/bpf_map/userspace/array_map.cpp`
- Jetson focused counters：
  `benchmark-results/uprobe/percpu-arm-diagnosis-20260729/`
- Linux v6.17：
  [`kernel/bpf/arraymap.c`](https://github.com/torvalds/linux/blob/v6.17/kernel/bpf/arraymap.c)、
  [`kernel/bpf/verifier.c`](https://github.com/torvalds/linux/blob/v6.17/kernel/bpf/verifier.c)、
  [`arch/x86/net/bpf_jit_comp.c`](https://github.com/torvalds/linux/blob/v6.17/arch/x86/net/bpf_jit_comp.c)、
  [`arch/arm64/net/bpf_jit_comp.c`](https://github.com/torvalds/linux/blob/v6.17/arch/arm64/net/bpf_jit_comp.c)
