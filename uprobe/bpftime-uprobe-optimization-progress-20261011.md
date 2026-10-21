# Uprobe 优化推进状态（2026-10-11）

> 历史阶段记录（正文截至 2026-10-13）。最新状态请读
> [2026-10-21 研究进度与模型接力](bpftime-uprobe-handoff-20261021.md)。
> 本文中的“正在执行/后续”不是当前待办，不要据此重跑或覆盖现有结果。

## 研究边界

此前的操作语义修正、ARM64/x64 对照、matched 差额、L0–L3 和叶子路径归因、hash workload 扫描已有结论；当前转入正式实现优化。
本轮不重开 ssl-nginx，也不推进 syscall、syscount-nginx、MPK 或单独补齐 miss/insert/delete-miss 的研究项目。
优化必需的正确性测试仍然保留。

推进顺序：per-CPU array 包装 → per-CPU hash 复制/遍历 → per-CPU hash 删除回收 → 普通 map 分派/探测。
每项单独验证后再叠加，避免混淆收益来源。

## 第一项：per-CPU array 包装

源码分支：`codex/uprobe-map-optimization`，基于 `1a1dc0d6c2026a5e4b0e4faa964e3eea8ccdd241`；本项提交为 `0572f81`。

`runtime/src/bpf_map/map_common_def.hpp` 中 `ensure_on_current_cpu` 由接收 `std::function` 改为模板 callable 转发。
生产路径的两个调用者为 per-CPU array lookup/update；不改变取当前 CPU、检查 flags/key、数据布局或复制语义，也不打开诊断 shortcut。
旧 update 的机器码包含 32 B 堆分配、间接调用和 function 析构；新版本移除了这些包装开销。

已完成：

- 独立构建 `build-uprobe-opt-20261011`，原 `build-uprobe-strict-20260910` 保留，校验哈希未变。
- 沿用 GCC 13 / LLVM 15 / RelWithDebInfo / LTO ON / checks OFF / INFO；测试开关没有改变 runtime 编译参数。
- 7 项相关 map 回归测试全部通过，共 3,514 条断言；新增错误返回/内容保持、多核隔离写入和 8/24/256 B value 检查。
- MAXN_SUPER、CPU5 固定 1.728 GHz，kernel/原版/优化版交替的 5 轮 matched A/B 完成。

| 操作 | 原版净耗时 ns/helper | 优化版 | 耗时变化 |
|---|---:|---:|---:|
| per-CPU array lookup | 26.686 | 20.634 | -22.68% |
| per-CPU array update | 75.690 | 27.142 | -64.14% |
| 普通 array lookup（对照） | 14.828 | 15.014 | +1.25% |
| 普通 array update（对照） | 23.076 | 23.121 | +0.20% |

以上为 5 个独立 victim 的中位数，使用相同 BPF、操作专属 control，净耗时为 `(operation-control)/1000`。
这不是应用吞吐提升，也不是完整官方 benchmark 的 raw 值。普通 array lookup 的小幅退步保留为待核查项，不预先归为噪声。

## 第二项：per-CPU hash 复制路径

提交：`3ba5d73 optimize per-cpu hash copy paths`。

实现只处理已经归因明确的复制开销，没有改 map 类型、共享内存布局、hash/equality 或命中语义：

- lookup 复用构造时已定长的 key scratch vector，以 `memcpy` 覆盖内容，省去每次 `vector::assign` 的范围处理。
- update 不再把 value 先写入单 value scratch vector，再从共享内存 iterator 复制到 map value；命中时从调用方 value 直接复制到当前 CPU 槽。
- insert 仍从全零 value template 建立完整 per-CPU value，只把调用方 value 直接写入当前 CPU 槽。
- 诊断用 `no_find` 模式仍保留输入复制；生产 base 模式没有 shortcut。

新增 1/4/17 B key 与 1/8/24/257 B value 的 12 组内容和 CPU 槽隔离测试。最终匹配构建中新测试 180 条断言、原有 per-CPU hash 测试 1,232 条断言均通过。

MAXN_SUPER、CPU5 1.728 GHz、GCC 13、RelWithDebInfo、LTO ON、probe read/write checks OFF、编译期日志 INFO。五轮 matched A/B（每项 20,000 次 victim invocation，每次 1,000 helper operations）的中位数如下：

| 操作 | 阶段一基线 ns/helper | 第二项候选 | 耗时变化 |
|---|---:|---:|---:|
| per-CPU hash lookup | 136.253 | 131.107 | -3.78% |
| per-CPU hash update | 220.951 | 140.365 | -36.47% |
| 普通 hash lookup（对照） | 58.282 | 58.295 | +0.02% |
| 普通 hash update（对照） | 62.061 | 62.454 | +0.63% |

结果目录：`/home/jetson/src/results/uprobe/percpu-hash-copy-optimization-matched-arm64-20261012/`。其中保存 source diff、两套 CMake cache、二进制哈希、每轮原始 CSV、频率采样和汇总。

曾有两批候选误用 checks ON / TRACE，与既定基线不匹配；普通 hash 控制组随之明显漂移。它们已在各自结果目录用 `INVALID.md` 标记，不纳入结论。修正后 fresh baseline 的关键函数地址和机器码尺寸与旧基线一致，普通 hash 两项变化回到 1% 内。

## 第三项：per-CPU hash key equality

提交：`ed4aeb7 optimize shared-memory key equality`。

`bytes_vec_equal` 先检查长度；常见的 4 B BPF key 改为两个固定宽度 `uint32_t` 的比较；其他非空尺寸改为一次 `memcmp`。这不改变 hash 值、bucket、碰撞分布、共享内存布局或查找结果。普通 hash 使用的是 `std::equal_to<bytes_vec>`，因此也是本项的未改控制。

正确性覆盖 1/4/17 B key 与 1/8/24/257 B value、原有 per-CPU hash、多种 LRU hash，以及 subprocess 共享内存 hash；合计分别通过 180、1,232、4,135 和 204 条断言。

相同正式配置下，五轮 matched A/B 中位数如下：

| 操作 | 复制路径基线 ns/helper | equality 候选 | 耗时变化 |
|---|---:|---:|---:|
| per-CPU hash lookup | 130.906 | 99.231 | -24.20% |
| per-CPU hash update | 140.038 | 114.320 | -18.36% |
| 普通 hash lookup（对照） | 56.714 | 56.911 | +0.35% |
| 普通 hash update（对照） | 63.337 | 63.349 | +0.02% |

这一项从 lookup/update 中分别减少 31.675/25.718 ns/helper，与 2026-08-13 叶子归因中通用 equality 的约 27 ns 成本一致。结果目录：`/home/jetson/src/results/uprobe/percpu-hash-equality-optimization-arm64-20261013/`。

## 第四项：4 B key hash

提交：`f9cf67f optimize four-byte shared-memory key hashing`。

先验证了保持原 hash 精确同值的循环展开版本。它只让 per-CPU lookup/update 改善 0.19%/1.41%，说明旧归因中的约 13–15 ns 主要来自五次串行 `boost::hash_combine`，并非循环控制。该中间结果保存在 `percpu-hash-exact-hash-optimization-valid-arm64-20261013/`，不作为最终优化。

正式实现对 4 B `bytes_vec` key 使用与仓库已有 `uint32_hasher` 相同的直接整数 hash；其他 key 尺寸仍用原逐字节 Boost hash。五轮 matched A/B 中位数：

| 操作 | equality 基线 ns/helper | fast4 候选 | 耗时变化 |
|---|---:|---:|---:|
| per-CPU hash lookup | 99.378 | 67.140 | -32.44% |
| per-CPU hash update | 114.473 | 79.314 | -30.71% |
| 普通 hash lookup（对照） | 56.917 | 56.933 | +0.03% |
| 普通 hash update（对照） | 63.113 | 63.124 | +0.02% |

11,000 次 successful find 的计数中，hash 调用均为 11,000 次，equality 从 14,696 次降为 11,000 次。收益既来自去掉五次 hash-combine，也来自顺序整数 key 的额外碰撞遍历消失。

负载/尺寸矩阵进一步验证：4 B key 在 64/512/1000 个活跃 key 时，lookup 分别改善 28.04%/30.43%/32.27%，update 改善 25.28%/28.97%/30.44%；16 B 和 64 B 未改路径的 lookup/update 变化均在 1.4% 内。结果目录分别为 `percpu-hash-fast4-optimization-arm64-20261013/` 和 `percpu-hash-fast4-workload-validation-arm64-20261013/`。

正确性覆盖 1/4/16/17/64 B key、1/8/24/257 B value、ordinary/LRU/subprocess shared-memory maps。4 B hash 改变 bucket placement，因此共享同一已填充 map 的进程必须运行同一 runtime build；map API 语义与 key/value bytes 不变。

## 正在执行与后续

第一项的完整 10 轮官方 uprobe 回归于 21:57 JST 启动，并于 23:15 JST 成功结束。17 项 kernel、17 项 BPFtime 和 1 项 embedded VM 均取得 10 个有效样本；运行期间二进制哈希未变，CPU5 保持 1.728 GHz。
结果目录：`benchmark-results/jetson` 分支的 `uprobe/percpu-array-optimization-arm64-20261011/`。
本机对应 `/home/jetson/src/results/uprobe/percpu-array-optimization-arm64-20261011/`。

- `validation/environment.json`：实时/最终状态。
- `validation/summary.json`、`wall-raw.csv`：已完成的 matched A/B。
- `validation/full/benchmark.log`：完整回归进度，成功后同目录归档 JSON、报告和日志。
- `unit-tests.log`、`source.diff`（位于各 run 中）、反汇编和 CMake cache：正确性、改动与构建证据。

第二项的完整 10 轮官方 uprobe 回归于 2026-10-12 22:37 JST 启动，并于 23:52 JST 成功结束，耗时 4,530.86 秒。17 项 kernel、17 项 BPFtime 和 1 项 embedded VM 均取得 10 个有限正数样本；运行状态为 success，退出码为 0，全部频率采样均为 CPU5 1.728 GHz。结果目录为 `/home/jetson/src/results/uprobe/percpu-hash-full-regression-arm64-20261012/`。

相对第一项完整回归，官方 userspace raw 均值中 per-CPU hash lookup 从 137,364.24 降至 132,346.63 ns/inv（-3.65%），update 从 221,234.15 降至 141,286.33 ns/inv（-36.14%）。普通 hash lookup/update 分别为 +1.09%/-0.39%，per-CPU array lookup/update 分别为 -0.99%/-1.09%；未改控制项没有出现同量级漂移。第二项因此已完成正确性、matched A/B 和完整官方回归三层验证。

第三、第四项组合候选接下来运行完整 10 轮官方 uprobe 回归，确认 17 项 kernel、17 项 userspace、embedded VM 以及未改 map 操作没有回归。matched A/B 中 fast4 后 per-CPU lookup/update 已降至 67.140/79.314 ns/helper；hash/equality 已不再是下一阶段主目标。完整回归通过后，剩余 lookup 重点是 Boost unordered 的 bucket/node/`offset_ptr` traversal，随后才是 per-CPU hash delete 的同步析构/allocator reclaim。
删除回收必须同时验证内存占用和完整生命周期成本；不能只把析构挪出计时区就视为完成优化。

组合候选的完整回归已于 2026-10-13 17:34 JST 在 tmux `uprobe-hash-final-full-20261013` 启动。进入 full 前的一轮 stage-2 → final preflight 成功：per-CPU lookup 下降 48.57%，update 下降 42.90%。完整结果目录为 `/home/jetson/src/results/uprobe/percpu-hash-equality-fast4-full-regression-arm64-20261013/`，当前状态以其中 `environment.json` 和 `full/benchmark.log` 为准。
