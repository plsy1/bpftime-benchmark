# BPFtime `benchmark/uprobe` 后续调查执行计划

日期：2026-08-16

执行状态：阶段 A–F 已于 2026-08-16 完成；阶段 G 的缺失语义作为下一独立实验，不自动混入本轮 workload sweep。

## 目标

现有调查已经定位当前官方 workload 下的主要高成本路径。下一步不直接优化，而是验证这些结论在不同 hash workload 下是否仍然成立，并补齐 benchmark 尚未覆盖的操作语义。

核心问题：

1. Ordinary hash lookup 中的通用比较和 probing 成本是否随 load factor、key size 变化？
2. Per-CPU hash lookup/update 中 Boost container remainder、key vector 和 value-vector copy 是否随 key/value size 按预期增长？
3. Jetson 上现有 `BPFtime − kernel` 方向是否只由当前 `1000 keys / 1024 max entries / 4B key / 8B value` 配置触发？
4. Lookup-miss、update-insert、delete-miss 是否表现出与现有 hit/existing 路径不同的成本结构？

本计划只做诊断和稳健性验证；不修改生产 runtime 算法，不把诊断开关当作正式优化。

## 实验原则

- Host 直接运行，不使用 Docker。
- Jetson Orin Nano，MAXN_SUPER，`jetson_clocks`，loader/victim 固定 CPU5。
- Kernel 与 BPFtime 对每个配置使用同一份 BPF object、相同 victim 和相同运行顺序。
- BPFtime 使用既有正式 agent/syscall-server 构建，不为 sweep 修改 runtime。
- 每个 BPF invocation 执行 1000 次 helper。
- 状态准备位于计时区外；lookup 测 hit，update 测 existing。
- 每个 real case 配 operation-specific control，保留相同循环、key/value 初始化和指针准备。
- Wall-time 每个配置、每个 engine 运行 5 个独立 victim；PMU 对代表性端点运行 3 轮。
- 结果同时报告 raw、mean、median、sample SD、min/max。
- 只在同 binary、同配置、同 harness 内做差；不同 harness 不构造代数“残差”。

## 阶段 A：环境与基线冻结

记录：

- source branch、commit、dirty status；
- agent、syscall-server、loader、victim、BPF object 的 SHA-256；
- GCC、Clang/LLVM、Boost、kernel、CPU topology；
- power mode、CPU5 频率；
- LLVM JIT、LTO、probe read/write checks 配置；
- 正式运行前后 kernel BPF link 和 `/dev/shm/bpftime_maps_shm` 清理状态。

先复跑当前基线：

```text
active_keys=1000
max_entries=1024
key_size=4B
value_size=8B
```

验收：方向和量级应与 `helper-hash-closure-arm64-20260812` 一致；如果偏差超过 10%，先停止 sweep 并审计环境。

## 阶段 B：Load-factor sweep

固定 `max_entries=1024`、`key_size=4B`、`value_size=8B`：

| 配置 | active keys | 名义占用率 |
|---|---:|---:|
| `lf0064` | 64 | 6.25% |
| `lf0256` | 256 | 25.00% |
| `lf0512` | 512 | 50.00% |
| `lf1000` | 1000 | 97.66% |

每个配置测量：

- ordinary hash lookup-hit；
- ordinary hash update-existing；
- per-CPU hash lookup-hit；
- per-CPU hash update-existing。

主要判断：

- Ordinary hash lookup 的 BPFtime 成本是否随 probing/collision 增长；
- Per-CPU Boost unordered-map 路径是否对 load factor 同样敏感；
- Kernel 分母和 BPFtime 分子各自如何变化；
- per-CPU-specific cost 是否随负载变化。

## 阶段 C：Key-size sweep

固定 `active_keys=512`、`max_entries=1024`、`value_size=8B`：

```text
key_size = 4B, 16B, 64B
```

主要判断：

- Ordinary hash 的 generic comparison/hash 是否随 key size 增长；
- Per-CPU hash 的 `key_vec.assign()`、Boost hash/equality 和 container remainder 如何变化；
- 当前 4-byte fixed-key 快路径结论是否仅适用于官方配置。

## 阶段 D：Value-size sweep

固定 `active_keys=512`、`max_entries=1024`、`key_size=4B`：

```text
value_size = 8B, 64B, 256B
```

lookup 和 update 都运行，但重点分析 update：

- Ordinary hash update 的 value copy/现有 BPFtime 优势是否随 value size 改变；
- Per-CPU hash update 的 shared-memory value-vector slot copy 是否随 value size 增长；
- Kernel 与 BPFtime 的 value-size slope 是否不同。

## 阶段 E：PMU 代表点复核

为控制总时长，不对所有配置做 PMU。选择：

- load factor：`lf0064`、`lf1000`；
- key size：4B、64B；
- value size：8B、256B。

每个端点采集：

```text
cycles
instructions
```

每个 real/control 配对运行 3 轮。若硬件事件不可用，保留 perf stderr，不用 task-clock 替代后静默混入。

## 阶段 F：结果分析

对每个 engine/config/case 计算：

```text
net ns/helper = (real ns/invocation - control ns/invocation) / 1000
BPFtime-kernel gap = BPFtime net - kernel net
per-CPU-specific = per-CPU hash net - ordinary hash net
```

输出：

- `wall-raw.csv`
- `wall-summary.csv`
- `gaps.csv`
- `pmu-raw.csv`
- `pmu-summary.csv`
- `README.md`
- `environment.txt`
- 精确构建/执行脚本和解析脚本

报告必须分别描述：

- 绝对成本；
- 平台内部 BPFtime/kernel 差额；
- 相对当前 4B/8B/高负载基线的 slope；
- 哪些原归因得到支持，哪些需要修正。

## 阶段 G：补齐缺失语义

在 workload sweep 完成并确认 harness 稳定后，再新增独立 case：

1. `lookup-miss`：计时区内查询稳定不存在的 key；
2. `update-insert`：使用 `BPF_NOEXIST`，每个计时样本前在计时区外清理目标 key；
3. `delete-miss`：查询并删除稳定不存在的 key。

不得修改现有 lookup-hit、update-existing、delete-hit 的语义。新增语义先做 ARM64 顶层 matched 对照；只有出现新的高额差距时才继续 L0–L3/叶子 A/B。

## 决策门

完成阶段 A–F 后：

- 若主要路径在不同 workload 下保持，当前根因结论升级为稳健结论；
- 若成本只在高 load、长 key 或大 value 出现，则在报告中限定适用范围；
- 若出现新的胜负翻转，先分离 BPFtime 分子与 kernel 分母，再决定是否新增叶子实验；
- 不自动进入优化。只有研究目标明确切换为优化后，才评估 wrapper、container representation 或 reclamation 修改。

## 代码与结果位置

- 代码分支：`codex/official-no-btf`
- 计划文档：`summry/jetson`
- 结果分支：`benchmark-results/jetson`
- 计划结果目录：`uprobe/hash-workload-robustness-arm64-20260816/`

## 完成标准

1. 阶段 A–F 全部完成并通过清理、频率、语义和结果完整性检查；
2. 基线与既有结果方向一致，或已给出可验证的环境差异解释；
3. 所有结论可由原始 CSV 和执行脚本重现；
4. 综合报告与 12 项调查矩阵更新；
5. 代码、结果和总结分别提交并推送到对应分支。
