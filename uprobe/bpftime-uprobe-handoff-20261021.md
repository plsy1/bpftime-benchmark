# Uprobe 优化：研究进度与模型接力（2026-10-21）

本文件是当前接力入口，替代 2026-10-11/13 文档的“当前状态”，但不覆盖历史实验。
最后核对：2026-10-23 12:00 JST 左右。以下分支、进程和文件状态均为本机快照；
后续模型行动前需重新核实。**新会话先读第 0 节**，再按需读其余章节。

## 0. 现状一页（2026-10-23 12:00 JST，新会话从这里继续）

**目标**：继续优化 `codex/uprobe-official-177a1dc465ff`（开发 worktree
`/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate`，远端 HEAD `72aebadd`）。
下一阶段重点：扩大测试覆盖（见 0.4），hash map 重写暂缓。

### 0.1 改了什么（7 个新提交，均已推送并加入每日补丁栈，共 50 个提交）

| 提交 | 改动 | 效果（相对同轮 kernel） |
|---|---|---|
| `baf0cc85` | 被 array JIT 绑定的程序里，非内联 map 调用改为按 FD 平表分派 | hash + array 程序 lookup 2.41x → 1.95x |
| `c3090b59` | 放宽 typed helper 选择（全局变量、事件类 map、array + 一种 hash 都用专用 helper） | hash + 全局变量 1.78x → 1.19x |
| `1cb20807` | **改 hash 数据结构**：桶数 `next_prime(2×max_entries)`（内存翻倍）、删除时回收以空桶结尾的墓碑段、按最大偏移距离限制探测；已有元素从不移动 | 满表 miss 19x → 0.94x；随机命中 1.17x → 0.77x |
| `f26cc335` | 绑定程序里常量非 array 句柄的 lookup/update/delete 直接调用原 helper（overlay + 私有 helper 槽） | hash + array 程序与只有 hash 的一样快 |
| `af10949b` | 探测上限按“每种距离的元素数”维护，最远元素删光后上限下降（每 map 多 256 字节） | 插删后半满表纯 miss 1.62x → 1.01x |
| `dffb2fce` | 删除时只在最远一档被删空时才重算上限 | 无可测收益，逻辑保留 |
| `72aebadd` | hash 专用 delete helper（加载时按类型选，与 lookup/update 同规则） | 官方 hash delete 36.9 → 28.2 ns（0.26x）；delete+insert -10% |

另：`ccb0ade1`（距离 0 不计数）使 hash delete 慢 17%，推送前已撤销。原因：hash 删除被内联进 ~9.7 KB 的通用
delete 大函数，源码小改动就会重排整函数的代码生成；专用 delete helper 后不再受影响。
每日补丁栈：LLVM 15 第 5 轮重放（`results/uprobe/daily-jit-stack-validation-20261021/round5-hash-bound-delete`）
50/50 无冲突，树与 `72aebadd` 相同，候选 77 cases、官方 26 cases 通过。

### 0.2 测了什么（结果目录 `/home/jetson/src/results/uprobe/`）

| 目录 | 内容 |
|---|---|
| `hash-gap-history-20261022` | 6 个版本同轮：三种程序（只有 hash / + 全局变量 / + array）的 hash lookup |
| `hash-shape-20261022` | 自写 workload：顺序/随机 key、是否用返回值、填充 1024/1000/512、插删后；纯 miss |
| `hash-mutate-20261022` | 三种程序里的 hash update、delete+insert |
| `hash-helper-fixes-…`、`hash-probe-bound-…`、`direct-map-helpers-…`、`hash-bound-shrink-20261022`、`hash-typed-delete-20261023` | 每个提交的回归测试、同轮 A/B、只跑 bpftime 的完整十轮（`full10-*`）、配对短测 |
| `direct-map-helpers-20261022/layout-check` | 代码布局验证（全函数 64 字节对齐对照构建） |

### 0.3 结论

1. 官方 hash lookup 0.95x 与自设计 ~1.2x 的差别几乎全来自 key 顺序；是否使用返回值、1000/1024 填充对命中无影响。
2. hash lookup 在所有测过的形态（命中、约半数 miss、满表纯 miss、插删后纯 miss）都低于或接近 kernel（0.77–1.01x）。
3. hash update/delete 在三种程序里都远低于 kernel（0.31–0.34x）。
4. 官方十项（`72aebadd`，只跑 bpftime，对基线 `5466f0d`）：hash lookup -10%（0.85x）、hash update -13%（0.32x）、hash delete -22%（0.26x）、其余 map 项 ±1%。
5. 代码布局效应：uprobe、uprobe + uretprobe、per-CPU array update、probe write 会随构建在 1–10% 内波动，已证实是布局而非逻辑（同源码全函数对齐对照）；全函数对齐不采用。热点未定位。
6. 已知语义问题（官方即如此，未改）：hash update 忽略 `BPF_NOEXIST/BPF_EXIST` 且表满时仍返回 0，delete 不存在的 key 也返回 0。
7. 现有 hash map 仍是“打过补丁的线性探测”：内存翻倍、插删后表内无空桶（靠探测上限兜底）、整表一把锁、只对 4 字节 key 有快速路径。重写方案（元素池 + 带 hash 标签的紧凑索引表、只在索引上做回移删除、分段锁）已讨论，**等测试证明是瓶颈再做**。

### 0.4 下一阶段：扩大测试覆盖（用户定的重点）

官方 benchmark 太理想化，自设计的 consuming benchmark 也不够。两条线并行：
1. **多维度、多参数的 benchmark**：key 大小（libbpf-tools 普通 hash 仅约 46% 是 4 字节 key，27% 是 8 字节，27% 是 12–192 字节）、value 大小（多为 16–144 字节）、操作语义（插入新 key、删除不存在的 key）、表的状态、并发（多线程/多进程；hash map 目前整表一把锁），每项与同轮 kernel 对比。
2. **真实程序**：在 bpftime 上跑 uprobe 类工具（如 memleak、funclatency、bashreadline、sslsniff），统计实际 map 使用特征（操作次数、key/value 大小、miss 率、插删频率、并发度），从结果找问题；优化后回到真实程序验证。

次要：布局效应热点定位（本机有 nsys，10/16 用过）、hash 返回值语义、上游 PR。

### 0.5 用户定下的工作方式

- 每次改动：增量构建 → 回归测试（`config.json` 的 `test_filter` 清单）→ 同轮 kernel / 改动前 / 改动后的配对短测 → 完整十轮（不跑 kernel，倍率借用基线那轮 kernel 并注明）。
- 补丁栈攒几轮再加；推送前问用户。
- 汇报用提交号和描述性名称，自起的标签首次出现要解释。
- 教训：自选测试过滤器会漏测；SHA 用 `git rev-parse` 完整值；核对系统时间；测量期间不要在其他核上跑 CPU 密集任务。

## 1. 先读结论

- 研究重点是 Jetson ARM64 上 uprobe benchmark 的 map-operation hot paths。
  ssl-nginx 已完成当前阶段工作，不重开；syscall、syscount-nginx、MPK 不在本阶段范围。
- 此前进行了 callable、分配/复制、key/hash/equality、probe loop、分派、update、
  per-CPU node/value 回收等 C++ 实现层优化；八项非 array-lookup 主表操作已低于同轮 kernel。
- 最近已进入 JIT 优化，不再只是 C++ 微调：ordinary 和 per-CPU array lookup
  在 LLVM IR 层展开，随后由 LLVM 优化并生成机器码。
- 用户要求取消 enable 开关，内联应成为符合条件程序的默认行为；这已实现，
  并支持 ordinary/per-CPU array、hash、update 和其他 helper 混用。
- 默认化改动曾使 ordinary hash delete 从 37.06 升到 43.69 ns；根因已定位，
  修复已落在正式工作区，完整十轮恢复为 36.64 ns，没有发现其他明显回退。
- 正确性、配对短测、完整十轮、默认内联生效审计均已结束并通过。
  但实验自选的测试过滤器比每日过滤器窄，漏跑了上游用例
  `Hash maps with colliding public names remain independent`；集成每日补丁栈时
  发现它在 JIT 代码上失败，已由只改测试的 `2eea622` 修复（见第 7 节）。
  **本轮 per-CPU JIT、默认化/泛化及 delete 修复源码为 `c0939bd`，连同 `9ffbffb`、
  `2eea622` 已加入每日补丁栈并推送。**
  接力文档发布在 `summry/jetson`，选定结果发布在 `benchmark-results/jetson`；
  本机完整源码归档、构建产物和中间实验不在本次发布范围。
- **总体评价口径**（与用户确认，2026-10-21）：在 Jetson ARM64 uprobe map 微基准中优化是成功的——
  八项可比操作全部低于同轮 kernel；array lookup 在自设计 consuming 测试中从官方原版的
  2.5–4.4x kernel 降到整体 0.53–0.83x，正确性与可迁移性已验证。不足：评估场景只覆盖小 map、
  单线程微基准；JIT 只覆盖符合条件的 array lookup；lookup 本身的 miss 路径仍约 2x 慢于 kernel。
  详见第 4.1–4.2 节。
- 当前没有本轮 benchmark 在跑；tmux 中遗留的 shell / tail 窗口不代表实验仍在运行。

## 2. 工作区与分支：不要改错目录

| 用途 | 本机路径 / 分支 |
|---|---|
| **后续开发源码** | `/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate` |
| **后续开发分支** | `codex/uprobe-official-177a1dc465ff`：远端 `f24dc9b`（官方 177a1dc + 补丁栈 42 个 + 诊断 `f24dc9b`）；新增 `baf0cc85` … `72aebadd` 共 7 个（已推送，见第 0 节）；`5466f0d` 为基线，`bdd9835` 为全局变量支持 |
| 本轮实测源码（冻结） | `/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate` |
| 本轮实测分支（冻结） | `codex/uprobe-official-8da867d541f8` |
| 已提交并推送 HEAD | `2eea622f97688b82a5c39402f631e79b5eb538f8`，map-name 用例适配 binding mutex（只改测试） |
| 本轮实测源码提交 | `c0939bdf8e3ece38f56d964cac3d333468b8bf23`，默认 ordinary/per-CPU array JIT + delete 修复 |
| 独立热构建源码 | `/home/jetson/src/bpftime-percpu-array-jit-20261020` |
| 热构建目录 | 上述目录中的 `build-jit` |
| 每日脚本工作区 | `/home/jetson/src/bpftime-jetson-daily`，`automation/jetson-daily` |
| 原始结果工作区 | `/home/jetson/src/results`，`benchmark-results/jetson` |
| 研究文档工作区 | `/home/jetson/src/summry`，`summry/jetson` |
| 旧优化源码 | `/home/jetson/src/bpftime`，不要误当成当前 candidate |

本地 `official-latest` 已是 `177a1dc...`，另有对应 candidate；本轮正在优化的仍是
`8da867d541f8` 快照，并不因为分支叫 official 就自动变为最新代码。
`9ffbffb` 是本轮改动的父提交（ordinary opt-in JIT）。最新源码提交
[`c0939bd`](https://github.com/plsy1/bpftime-benchmark/commit/c0939bdf8e3ece38f56d964cac3d333468b8bf23)
已在 GitHub 的同名优化分支；其 35 个归档源码文件精确匹配实测版本。
其后的 `2eea622` 只改 `runtime/unit-test/maps/test_shm_hash_maps.cpp`，
不影响实测运行时源码和性能结果。
工作区仍有无关 hash workload 诊断改动，未带入此提交，不是遗漏本轮优化。

正式工作区、热构建源码的 35 个归档文件均已核对最新 `source-sha256.json`，无差异。
LLVM 子模块 `vm/llvm-jit` 干净，SHA 为
`87bac39ad879cb1388c0fa13095779cd09ed9027`；修改是父仓库维护的 overlay patch，
不是靠未提交的子模块改动。主目录先读 `AGENTS.md`（指向 `CLAUDE.md`）。

## 3. 最新实验：已经完成，勿按旧的 pending 状态重跑

最新结果根目录，后文简称 R：
`/home/jetson/src/results/uprobe/array-jit-default-delete-fix-20261021`。

| 检查 | 结果 / 证据 |
|---|---|
| 构建 | `build-exit-status.txt = 0` |
| Map / JIT / lifetime 正确性 | `tests.log`：42 cases / 110422 assertions（过滤器未含 `[maps][map_name]`，见第 7 节） |
| Host-process contract | `host-contract.log`：18 cases / 201 assertions |
| 默认行为 | `default-tests.log`：5 cases / 138 assertions |
| 原始完整 workload 短 A/B | 12 个 AB/BA 配对，24 个 fresh processes；`short-exit-status.txt = 0` |
| Consuming array 短测 | ordinary/per-CPU 各 6 轮，包含同轮 kernel、checksum 和 hit/miss 校验 |
| 完整 uprobe | 17 kernel + 17 userspace + 1 embedded，全部各 10 样本 |
| 完整运行状态 | `full10/exit-status.txt = 0`，2026-10-21 14:59:07 JST 结束 |
| 完整计时时长 | 1480.87 秒，约 24 分 41 秒，不是一个 case 的耗时 |
| 样本/二进制 | 全部有限正数样本，SHA 检查通过 |
| 默认内联审计 | `full10/activation-audit.log`，17 cases，通过；启用变量不存在 |

R 内保存脚本、日志、raw CSV、summary JSON、原始完整 benchmark JSON、
source/、source-sha256.json、changes-vs-9ffbffb.diff，以及冻结的 agent/server/CLI/embedded 产物。
产物用于本机复核，不要把大型二进制及中间构建全部提交。本次 GitHub 发布的是
结果说明、完整十轮表/原始 JSON、两类 consuming 的 raw/summary/环境记录、
正确性及审计证据等选定文本文件；脚本、source/ 和 artifacts/ 完整集合仍在本机。

配置：MAXN_SUPER，CPU5 固定 1.728 GHz；GCC 13，当前 JIT 热构建 LLVM 16，
RelWithDebInfo / LTO ON / probe read-write checks OFF / 生产诊断 OFF。
每日脚本仍配置 LLVM 15，不能把它与本轮构建不加说明地视为同配置。
Kernel 6.8.12-1021-tegra，无 `/sys/kernel/btf/vmlinux`；复现 no-BTF 构建要沿用已有生成流程。

## 4. 十项结果：必须区分历史纯操作与 consuming JIT 测试

完整对比表：[最新十轮与 consuming 表](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/uprobe/array-jit-default-delete-fix-20261021/full10/ten-operation-comparison.md)。

下表单位 ns/op；倍率为 BPFtime / kernel，越小越好。“上次”指发生 delete 回退的
`array-jit-default-20261021/full10`，而不是回退前好版本。初始为 2026-10-07 严格复现。

| 操作 | 优化状态 | 最初 | 最初/kernel | 上次 | 当前 | 同轮 kernel | 当前/kernel | vs. 最初 | vs. 上次 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 已做 C++ + JIT；另表 | 15.94 | 4.22x | 不作同口径比较 | 见 consuming 表 | — | — | — | — |
| Ordinary array update | 已优化 C++ | 23.51 | 1.78x | 11.53 | 11.55 | 13.04 | 0.89x | -50.9% | +0.2% |
| Ordinary hash lookup | 已优化 C++ | 58.47 | 2.04x | 27.19 | 26.90 | 29.00 | 0.93x | -54.0% | -1.1% |
| Ordinary hash update | 原本更快；共享路径受益 | 63.18 | 0.67x | 35.07 | 34.21 | 94.95 | 0.36x | -45.9% | -2.5% |
| Ordinary hash delete | 原本更快；共享路径/回退修复 | 42.64 | 0.40x | 43.69 | 36.64 | 108.06 | 0.34x | -14.1% | -16.1% |
| Per-CPU array lookup | 已做 C++ + JIT；另表 | 27.73 | 6.51x | 不作同口径比较 | 见 consuming 表 | — | — | — | — |
| Per-CPU array update | 已优化 C++ | 75.93 | 4.55x | 14.39 | 14.41 | 16.58 | 0.87x | -81.0% | +0.1% |
| Per-CPU hash lookup | 已优化 C++ | 141.25 | 4.97x | 27.74 | 27.74 | 29.37 | 0.94x | -80.4% | 约 0% |
| Per-CPU hash update | 已优化 C++ | 220.96 | 3.26x | 44.50 | 44.42 | 68.11 | 0.65x | -79.9% | -0.2% |
| Per-CPU hash delete | 已优化 C++ | 986.68 | 6.50x | 99.81 | 98.77 | 149.54 | 0.66x | -90.0% | -1.0% |

原始 array lookup 不使用返回值，内联后 LLVM 可以消除部分指针/读值工作，
不能把原始结果里的约 1.17 / 4.12 ns 当成真实 consuming lookup 性能，
也不能据此计算相对 15.94 / 27.73 ns 的优化百分比。

独立 consuming 实验的 6 轮中位数如下；包含 probe、运行时 key 生成、读取 value 和结果累计：

| 额外测试 | 默认化前好版本 INLINE | 当前默认 INLINE | 同轮 kernel | 当前/kernel |
|---|---:|---:|---:|---:|
| Ordinary 全命中 | 3.0954 | 3.0942 | 5.8827 | 0.53x |
| Ordinary 约半数 miss | 9.6749 | 8.4503 | 12.3114 | 0.69x |
| Per-CPU 全命中 | 5.1875 | 5.1189 | 6.8282 | 0.75x |
| Per-CPU 约半数 miss | 11.5377 | 10.6386 | 12.7457 | 0.83x |

“约半数 miss”是新增诊断 workload，不是原始 benchmark 就有的设定。
不可把两种口径拼起来声称“原始十项统一口径全胜”，也不代表所有应用或 key/value/load 都更快。
177a1dc 新基线（见第 8 节）上述数字均在 ±1% 内复现。

### 4.1 consuming 测试的性质与官方原版对照

consuming workload 是**我们自己设计的诊断测试**（`benchmark/uprobe/diagnostics/array_jit_poc*`，
由 `9ffbffb` / `c0939bd` 加入，上游没有）。与官方 benchmark 的 lookup 相比：官方用固定顺序 key 0..999、
丢弃返回值、全命中、无结果校验；我们用运行时 LCG key、读出 value 累加校验和、有全命中/约半数 miss
两种、loader 逐项比对校验和与 hit/miss，并带不访问 map 的 control case。它只在“结果必须被使用”
这一点上更严格；map 规模（1024 项、u32/u64、8 KB）、单线程、热 cache、CPU5 与官方相同，仍是微基准。
官方 benchmark 在无 JIT 内联时测 helper 成本仍有效（官方原版 4.22x kernel，与下表 3.78x 一致）；
其余七项操作走 helper、不被 DCE，数字有效。

同轮三臂对照（[`consuming-official-vs-optimized-20261021`](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/uprobe/consuming-official-vs-optimized-20261021/README.md)；
官方 = 未打补丁 `177a1dc`，与基线相同 LLVM 16 选项；优化 = `5466f0d`；6 轮中位数，ns/元素）：

| 测试 | kernel | 官方原版 | 优化版 | 官方/kernel | 优化/kernel | 优化 vs 官方 |
|---|---:|---:|---:|---:|---:|---:|
| ordinary 全命中 | 5.77 | 21.81 | 3.10 | 3.78x | 0.54x | -85.8% |
| ordinary 约半数 miss | 12.25 | 30.17 | 8.44 | 2.46x | 0.69x | -72.0% |
| per-CPU 全命中 | 6.89 | 29.95 | 5.12 | 4.35x | 0.74x | -82.9% |
| per-CPU 约半数 miss | 12.78 | 39.25 | 10.64 | 3.07x | 0.83x | -72.9% |
| control_hit（无 map） | 3.76 | 1.61 | 1.60 | 0.43x | 0.42x | -1.1% |
| control_mixed（无 map） | 9.30 | 2.52 | 2.50 | 0.27x | 0.27x | -0.7% |

- 官方原版在此测试中明显慢于 kernel，因此“低于 kernel”不是测试偏向 bpftime 造成的。
- control 部分官方与优化版相同，且约为 kernel 的 0.27–0.43x：差距和收益都在 lookup 路径；
  但 raw 低于 kernel 有相当部分来自 map 以外代码的 LLVM 生成优势。
- 扣除 control 的粗算（control 无 helper 调用、分支结构不同，仅作参考，不与 raw 混用）：
  官方原版 lookup 约为 kernel 的 9–11x；优化版 ordinary 全命中约 0.75x、per-CPU 全命中约 1.13x、
  两种约半数 miss 约 2.0–2.3x。**不能说“array lookup 本身快于 kernel”，miss 路径仍慢。**
- 分层参考（ordinary 全命中，非同轮）：官方 21.8 → 仅 C++ 优化 7.9（10-19 POC）→ C++ + JIT 3.1 ns。

### 4.2 结论与不足（对外表述口径）

可表述为：在 Jetson ARM64 的 uprobe map 微基准中，优化是成功的——八项可比操作全部低于同轮 kernel；
array lookup 在 consuming 测试中从官方的 2.5–4.4x kernel 降到整体低于 kernel，且正确性和可迁移性
都经过验证。不足需同时写明：

1. **评估覆盖不足**（未知，不是已知不行）：小 map、u32/u64、单线程、热 cache、CPU5 微基准；
   更大 map、多线程、不同 key/value（仅 hash 在 8 月做过）、真实应用端到端均未测。
2. **功能覆盖有限**（有意收窄）：JIT 只覆盖 constant-handle 的 ordinary/per-CPU array lookup；
   其余 map/操作走已优化 C++ helper；direct map-value relocation、CALLX、CUDA、MPK、AOT、解释器保守回退。
3. **已知性能缺口**：lookup miss 路径约 2x 慢于 kernel，per-CPU 全命中略慢（以上均为扣 control 粗算）。
4. 工程层面：每日构建为 LLVM 15，性能实测为 LLVM 16；未提交上游审查；per-CPU 无 kernel 式禁抢占保证。

### 4.3 全局变量支持（`bdd9835`，2026-10-21）

普查（[`jit-eligibility-survey-20261021`](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/uprobe/jit-eligibility-survey-20261021/README.md)）：bcc v0.37.0
libbpf-tools 的 lookup 调用点 93% 是 hash、5% per-CPU array、无 ordinary array；96% 有 lookup 的程序
因引用全局变量（`const volatile` 的 `.rodata`、`.bss`）被 JIT 整个拒绝，现有 JIT 一个都覆盖不到。

`bdd9835`：direct map-value LDDW 按内核规则（单元素 ordinary array、偏移在 value 内）绑定并持有，
加载副本把 LDDW 改写为持有地址常量；原指令保留重定位；不改 llvmbpf/overlay。其余重定位、多元素/
per-CPU/hash 目标仍走原路径；只有全局变量、没有 array lookup 的程序不建 binding。
结果（[`array-jit-global-data-20261021`](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/uprobe/array-jit-global-data-20261021/README.md)）：

| 带 `.rodata` 配置 + `.bss` 计数的 consuming workload | kernel | `5466f0d` | `bdd9835` | 新/kernel |
|---|---:|---:|---:|---:|
| ordinary 全命中 | 5.89 | 21.99 | 3.09 | 0.53x（-85.9%） |
| ordinary 约半数 miss | 12.28 | 30.83 | 8.53 | 0.69x（-72.3%） |
| per-CPU 全命中 | 6.91 | 24.79 | 5.17 | 0.75x（-79.1%） |
| per-CPU 约半数 miss | 12.82 | 32.48 | 10.65 | 0.83x（-67.2%） |

- 正确性：完整每日 filter 69 cases / 128422 assertions；新增 `[array-jit-global]` 3 例。
- 无全局变量的原 workload 不变；完整十轮 ordinary hash update 曾显示 +3.8%，经机器码逐条比对一致、
  样本双峰（约 33/36 ns）加一个 43 ns 离群点、12 轮 AB/BA 短测中位数 33.43 vs 33.45 ns 确认为波动。
- 覆盖面（静态）：libbpf-tools 通过 JIT 程序门槛 8 → 179/183，可内联 array 调用点 0 → 19
  （可在用户态运行的 1 → 38/38、0 → 9）。直接收益覆盖面有限，主要价值是为后续通用化铺路。
- **新发现**：`5466f0d` 跑带全局变量 workload 与官方原版几乎同速（21.99 vs 21.81 ns），因为
  `bpf_attach_ctx.cpp` 的 `uses_only_maps_of_type_for_helper` 要求程序内所有 map 同类型且无
  direct map-value，否则退回通用 helper。libbpf 风格程序中的 hash lookup 因此多半拿不到 typed helper；
  差距测量见 `hash-lookup-helper-gap-20261021`（进行中/见第 8 节）。
- 构建过程中三次脚本错误（skeleton `name`、skeleton 类型 `u64`、runner 断言）的记录保留在
  `formal/failed-attempt-*`；运行时行为在三次中都正确。

## 5. JIT 当前设计与不能破坏的边界

流程：Clang 先编译 C → eBPF；运行时加载并绑定具体 map；LLVM 后端把可证明的
array lookup call site 展开为 IR，再优化/codegen。这不是 verifier 把 C 编译成 eBPF。
kernel 的 map_gen_lookup 则通常生成替换 eBPF 指令后交给 kernel JIT；两者都是加载期优化。

- `BPFTIME_ARRAY_JIT_INLINE` 已过时，不再被 runtime 读取；不是要求用户设置开关。
- LLVM 按 constant-handle lookup call site、按 map kind 选择优化；支持混合 ordinary/per-CPU
  arrays、hash、update、其他 helper，未匹配调用保持原 helper。
- 必须匹配已知标准 helper，不能绕过 custom/diagnostic helper。custom/missing CPU helper
  仅使 per-CPU 不能特化，不应阻止同程序里的 ordinary array。
- binding 拥有 map implementation 引用与精确 SHM mapping，不能只缓存可复用 FD。
  FD close/reuse/reset 后，lookup 命中、miss fallback、standard update/delete 均须访问同一旧绑定。
- per-CPU 每个有效 lookup 都查询 CPU 并检查分配 CPU 范围，不能缓存 CPU、标 pure、使用私有 TLS。
  它没有 kernel 风格禁抢占保证，不能宣称 caller 用返回指针期间线程迁移仍安全。
- bound program 保留 fork reload、signal 限制；interpreter/AOT 不走该 JIT binding。
  direct map-value/不支持的 relocation、CALLX、CUDA、MPK 等保守回退，不要当成已全支持。
- llvmbpf overlay 和 descriptor header 在 build 目录生成，所有 native consumer 必须使用同一定义。
  更新上游若 patch 冲突应停止并人工审查，不能静默忽略或直接脏改子模块。
- 注入 agent 是宿主的 guest：不能因失败终止宿主，未显式选择输出 sink 时不能碰宿主 stdio。

主要代码入口：正式源码中的 `runtime/src/bpftime_prog.cpp`、
`runtime/src/bpf_map/array_map_binding.{hpp,cpp}`、`runtime/src/bpf_helper.cpp`、
`runtime/src/attach/bpf_attach_ctx.cpp`、`vm/compat/llvm-vm/array-map-inline.patch`、
`array-map-descriptor.patch`、`vm/compat/include/bpftime_array_cpu_query.hpp`、
`runtime/unit-test/maps/test_array_jit_{lifetime,poc}.cpp`。完整改动清单以 R 的 source manifest 为准。

## 6. 本轮 delete 回退如何发现、修复

默认泛化增加 `array_jit_execution_scope::erase` → `array_map_binding::erase` →
generic `bpf_map_handler::map_delete_elem`，形成通用分派的新调用点。
GCC 13 + LTO 改变内联决策，使普通 hash delete 虽不走 array JIT 也受到影响。

证据链：完整十轮 +17.9%；12 对单项 AB/BA 重现 +18%；
只在独立构建改 binding erase 的 12 个平衡 ABC block 证明恢复；正式应用后再做全操作短测、
两类 consuming 测试与完整十轮。标准 delete helper 从 196 B + 外部分派，
恢复为内含分派的 9460 B 函数；尺寸是整个 all-map 函数，不是每操作执行指令数。
不能把“多一个跳转”当作全部微架构成本已独立测清。

修复仅令 binding erase 按 is_per_cpu() 调用具体 array implementation 的 elem_delete。
两种 array 都不需要该锁，原有 -1 / EINVAL 语义保留；没有修改 ordinary hash delete 算法。
短测好版本 37.066 → 修复版 36.309 ns；完整回归坏版本 43.69 → 修复版 36.64 ns。
恢复已确认，不宜把相对好版本约 1–2% 变化包装成新的重大优化。

证据目录：旧 R 同级 `array-jit-default-20261021/hash-delete-recheck`、
`hash-delete-investigation`，以及新 R 的 `native-comparison.json` 和反汇编。
旧 default 目录冻结的 source/artifacts 是发生回退的版本，不能为“统一”而覆盖。

## 7. 每日自动化：补丁栈已包含默认 array JIT

入口是 `/home/jetson/src/bpftime-jetson-daily/tools/jetson-upstream/`，不是 automation/ 或 scripts/。
维护 official-latest 镜像；SHA 变化后构建官方、跑十轮并归档，再在单独 worktree
重放配置的补丁栈、构建测试并按配置推送 candidate。不要把顺序误说成只跑打补丁版本。

`official-latest` 只能跟踪官方，不加本地优化提交；`automation/jetson-daily` 保存脚本和配置；
`codex/uprobe-official-<SHA>` 是对应上游快照的优化分支。

2026-10-21 起 config（`automation/jetson-daily`，最新 `8cd91f4`，已推送）补丁栈共 42 个提交：
`8da867d541f8..2eea622` 按序全部提交（在 `fefe2f6` 之后新增 `9ffbffb` ordinary JIT、
`c0939bd` 默认 ordinary/per-CPU JIT + delete 修复、`2eea622` 测试修复），再加 `bdd9835` 全局变量支持。
诊断提交 `f24dc9b`（hash 变体用的结果 map 类型宏与 loader 预写槽位）尚未加入补丁栈。
测试 filter 新增 `[array-jit-lifetime]`、`[array-jit-poc]`、`[array-offset-branch]`
和 host-contract 一组（`[allocation]`、`[maps][map_in_map]`、runtime_config、external maps、
Select Frida*）。README 已同步。

集成验证（[结果目录](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/uprobe/daily-jit-stack-validation-20261021)）：在官方 `177a1dc` 的独立 detached worktree 中重放、
用 `tracker.py build`（GCC 13 / LLVM 15 / LTO）构建并跑同一 filter：

| 轮次 | 结果 |
|---|---|
| 第 1 轮（38 + 9ffbffb + c0939bd） | 40/40 无冲突；LLVM 15 构建成功、全部 JIT 用例通过；48 cases 中 1 个失败 |
| 失败原因 | `9ffbffb` 令 `set_handler`/`clear_*` 取 `map_binding_mutex`，首次调用惰性创建一个 segment 级命名对象。`bpftime_shm` 构造时已创建，但上游用例 `Hash maps with colliding public names remain independent`（`[maps][map_name]`）直接构造裸 segment，命名对象数 1→2。JIT 前候选通过，LLVM 16 热构建同样失败，与 LLVM 版本无关 |
| 修复 | `2eea622` 让用例在取基线前创建该 mutex，与生产初始化顺序一致；只改测试，不影响性能结果 |
| 第 2 轮（41 个提交） | 41/41 无冲突，候选 `0fb1fd5`；候选 66 cases / 128293 assertions、官方 26 cases / 17913 assertions 全部通过 |
| 第 3 轮（42 个提交，加 `bdd9835`） | 42/42 无冲突，树与 `bdd9835` 一致；LLVM 15 候选 69 cases / 128422 assertions、官方 26 cases 全部通过 |

**为何之前没发现**：各 JIT 实验的 `run-build.sh` 使用手选 filter，不含 `[maps][map_name]` 等每日 filter 用例；
全量套件因官方 `Test attach replace with ebpf` 在本机 SIGSEGV 不可用；JIT 此前从未进入每日补丁栈。
**今后对维护栈的实验至少跑完整每日 filter，再加实验专属标签。**

注意：
- 每日任务只在官方 SHA 变化时迁移。`codex/uprobe-official-177a1dc465ff` 原为 10-19 生成的
  37 个提交，已于 10-21 20:31 用 `tracker.py migrate` 补齐 4 个提交到 `5466f0d` 并推送；
  其树与第 2 轮已验证候选 `0fb1fd5` 完全一致。
  集成验证 worktree `worktrees/177a1dc465ff/jit-stack-validation{,-2}` 为 detached，未建分支、未推送。
- 每日构建用 LLVM 15，JIT 性能实测是 LLVM 16 热构建；两者正确性已验证，但每日 candidate 不跑 benchmark，
  不能拿每日结果代表 JIT 性能。
- llvmbpf overlay patch 冲突会让 CMake 配置失败，需要人工处理，不能视作偶发失败。

user timer 在每天 03:30 附近运行（有随机延迟、persistent）；最近 service Result=success、
ExecMainStatus=0，查询时 inactive。后续希望迁到独立 Jetson，还没迁移。
本机共享 CPU/SHM，跑手工实验前需检查是否与定时服务重叠，不要擅自停用或改动定时任务。

## 8. 接手后的行动顺序

1. 先读本文件和正式源码 CLAUDE.md，核对 git status / HEAD、最新结果退出码和实际日志。
   本次测试已结束，不要无理由再跑 25 分钟。需要重新构建才生成新结果目录。
2. 本轮源码已在 `c0939bd` 提交并推送，共 24 个变更文件，包括 per-CPU JIT、direct CPU query、
   默认泛化、delete 修复、新 header/overlay 和正确性测试；随后的 `2eea622` 为测试修复，同样已推送。
   不要把它们重复当作待提交工作。新机器复现应取 `2eea622` 并初始化子模块，
   核对 source manifest 和原实验 toolchain。
3. 保留无关诊断修改：`benchmark/uprobe/Makefile`、`compare_runtime_builds.py`、
   `hash_workload_sweep.bpf.c`、未跟踪 `hash_candidate_screen.py` / `hash_distribution_validation.py`
   不能未经审查一并提交或清理。结果工作区也有大量历史未提交内容。
4. 源码、自动化补丁栈、接力文档和选定结果均已同步。补丁栈已在 177a1dc 上验证可重放；
   下次官方 SHA 变化时由定时任务自动迁移。若 cherry-pick 或 overlay patch 冲突，
   按 README 在保留的 candidate worktree 中人工解决，再更新补丁栈。
5. **后续优化在 `codex/uprobe-official-177a1dc465ff` 上开发**；`8da867d541f8` 分支冻结为已归档实验的
   源码快照。用户选定 LLVM 16。**新基线已完成**（10-21 20:37–21:11，全部退出码 0）：
   [`baseline-177a1dc-jit-llvm16-20261021`](https://github.com/plsy1/bpftime-benchmark/blob/benchmark-results/jetson/uprobe/baseline-177a1dc-jit-llvm16-20261021/README.md)，
   本机 `/home/jetson/src/results/uprobe/baseline-177a1dc-jit-llvm16-20261021`。
   干净 worktree `/home/jetson/src/bpftime-baseline-177a1dc-20261021`（detached `5466f0d`）+ `build-jit`，
   CMake 选项与 delete-fix 构建逐项相同；完整每日 filter 66 cases / 128293 assertions 通过；
   完整十轮八项可比操作 0.34x–0.95x kernel，与 8da 版均在 ±1% 内；consuming 同轮三臂
   （kernel / c0939bd / 5466f0d）差异 ≤0.2%（ordinary hit 3.10 ns 0.53x、mixed 8.46 ns 0.69x，
   per-CPU hit 5.18 ns 0.76x、mixed 10.65 ns 0.83x）。**以后“上次”对照用这个目录**，
   新实验可复用其 `run-build.sh` / `run-consuming.py` / `run-full.sh`（有防覆盖假设，需新目录）。
   8da 工作区的未提交 hash 诊断改动不随迁移。新提交验收后按序追加到 `config.json` 补丁栈。
6. 当前进行中的工作见第 0 节（hash 差距第 2 条已修、第 1 条待做、第 3 条待用户决定）。普查显示真实程序 93% 是 hash，
   array miss 路径在真实程序中少见，优先级已下调。其他候选：扩大评估覆盖（大 map、多线程、真实应用）、
   上游提交、独立测试机迁移。不要擅自扩大为全 map JIT 或修改 map 数据结构/语义。

## 9. 复现实验与构建易踩坑

- 长测放 tmux，短 A/B 用户要求盯到完成；长测启动后可让用户稍后查，不必不断轮询消耗 token。
- 对照需区分最初未优化、上次（回归检查）与回退前好版本。报告倍率使用同轮 kernel，
  不拿旧 kernel 分母冒充新一轮，也不把 net-control 差额与 raw ns 混用。
- 保持同 BPF/loader/victim、CPU5、频率、fresh map、binary SHA；不在计时期间编译。
- R 的 `run-build.sh` / `run-screen.py` / `run-consuming.py` / `run-short.sh` / `run-full.sh`
  是本轮确切流程；它们对输出目录有防覆盖假设，不能直接在已有结果上重跑。
- 独立 target build.make 必须先运行该 target 的 depend，否则头文件依赖可能陈旧。
  多个顶层 CMake target 可重复安装 libbpf、触碰头文件引起全量重编。
- 不在构建期间带保留旧 mtime 的方式同步改过的源文件，否则可能漏重编；编译前核对源 manifest。
  不忽略新 class-layout ODR 警告。已有 ebpf_inst native/compat ODR 警告另有历史背景。
- runner 必须显式处理 INT/TERM 为非零退出；不能只凭旧 EXIT trap 产生的 0 宣称成功。
- 黄金比例单乘法 4-byte ordinary hash 路线已因特殊 stride/high load 不稳而取消，
  不要重新把它当成正式优化。当前保留既定 hash/table 设计上的已验证实现优化。

本机总入口为 `/home/jetson/src/HANDOFF.md`，该根目录文件不在 Git worktree 中；
GitHub 上的接力入口就是本文件。跨分支结果链接使用明确的 GitHub 分支地址。
