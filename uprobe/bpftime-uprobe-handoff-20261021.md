# Uprobe 优化：研究进度与模型接力（2026-10-21）

本文件是当前接力入口，替代 2026-10-11/13 文档的“当前状态”，但不覆盖历史实验。
最后核对：2026-10-21 19:27 JST 左右。以下分支、进程和文件状态均为本机快照；
后续模型行动前需重新核实。用户随后要求推送本轮优化，源码已提交并推送为
`c0939bdf8e3ece38f56d964cac3d333468b8bf23`；每日自动化没有更新。

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
  **本轮 per-CPU JIT、默认化/泛化及 delete 修复源码已提交并推送为 `c0939bd`，
  但尚未加入每日补丁栈。**
  接力文档发布在 `summry/jetson`，选定结果发布在 `benchmark-results/jetson`；
  本机完整源码归档、构建产物和中间实验不在本次发布范围。
- 当前没有本轮 benchmark 在跑；tmux 中遗留的 shell / tail 窗口不代表实验仍在运行。

## 2. 工作区与分支：不要改错目录

| 用途 | 本机路径 / 分支 |
|---|---|
| 当前正式源码 | `/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate` |
| 正式源码分支 | `codex/uprobe-official-8da867d541f8` |
| 已提交并推送 HEAD | `c0939bdf8e3ece38f56d964cac3d333468b8bf23`，默认 ordinary/per-CPU array JIT + delete 修复 |
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
| Map / JIT / lifetime 正确性 | `tests.log`：42 cases / 110422 assertions |
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
可表述为八项历史可比较操作低于 kernel，加上额外 consuming array 测试也低于 kernel；
不可把两种口径拼起来声称“原始十项统一口径全胜”，也不代表所有应用或 key/value/load 都更快。

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

## 7. 每日自动化：已搭建，但尚未包含最新 JIT

入口是 `/home/jetson/src/bpftime-jetson-daily/tools/jetson-upstream/`，不是 automation/ 或 scripts/。
维护 official-latest 镜像；SHA 变化后构建官方、跑十轮并归档，再在单独 worktree
重放配置的补丁栈、构建测试并按配置推送 candidate。不要把顺序误说成只跑打补丁版本。

`official-latest` 只能跟踪官方，不加本地优化提交；`automation/jetson-daily` 保存脚本和配置；
`codex/uprobe-official-<SHA>` 是对应上游快照的优化分支。
当前 config 的补丁栈只到 `fefe2f6edf27`，连已提交 ordinary JIT `9ffbffb` 也尚未加入。
**不能声称每日任务已经自动重放当前全部 JIT 功能。**
user timer 在每天 03:30 附近运行（有随机延迟、persistent）；最近 service Result=success、
ExecMainStatus=0，查询时 inactive。后续希望迁到独立 Jetson，还没迁移。
本机共享 CPU/SHM，跑手工实验前需检查是否与定时服务重叠，不要擅自停用或改动定时任务。

## 8. 接手后的行动顺序

1. 先读本文件和正式源码 CLAUDE.md，核对 git status / HEAD、最新结果退出码和实际日志。
   本次测试已结束，不要无理由再跑 25 分钟。需要重新构建才生成新结果目录。
2. 本轮源码已在 `c0939bd` 提交并推送，共 24 个变更文件，包括 per-CPU JIT、direct CPU query、
   默认泛化、delete 修复、新 header/overlay 和正确性测试；不要把它重复当作待提交工作。
   新机器复现应取此提交并初始化子模块，核对 source manifest 和原实验 toolchain。
3. 保留无关诊断修改：`benchmark/uprobe/Makefile`、`compare_runtime_builds.py`、
   `hash_workload_sweep.bpf.c`、未跟踪 `hash_candidate_screen.py` / `hash_distribution_validation.py`
   不能未经审查一并提交或清理。结果工作区也有大量历史未提交内容。
4. 源码、接力文档和选定结果已同步；自动化没有改动。后续获得对应指令后，才更新补丁栈，
   纳入 LLVM overlay、generated header、测试 filter 和 toolchain 兼容验证，再迁移到新上游。
5. 本轮性能目标已完成验证。若继续开发，先与用户确认是完善兼容边界、上游移植、独立测试机，
   还是新性能研究；不要擅自扩大为全 map JIT 或修改 map 数据结构/语义。

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
