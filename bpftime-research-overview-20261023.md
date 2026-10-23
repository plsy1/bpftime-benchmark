# bpftime 在 ARM64 上的性能研究：阶段总结（截至 2026-10-23）

本文按时间整理课题从开始到现在的全部工作，作为汇报大纲。每个阶段只列要点，详细数据见各阶段链接的文档和
`benchmark-results/jetson` 分支的结果。依据是本仓库各分支的提交记录（最早为 2026-06-26）。

## 0. 课题背景与目标

- **对象**：bpftime，一个在用户态运行 eBPF 的 runtime。
- **平台**：Jetson Orin（ARM64），并与 x64 对照。
- **问题**：bpftime 在 ARM64 上相对 kernel eBPF 表现如何，慢在哪里，能否优化到不输 kernel。

## 1. 测试基础设施（6/26 – 7/11）

- 搭建 ARM64 / x64 的 benchmark runner 和 GitHub Actions 工作流（smoke、full 两档）。
- 覆盖的测试：uprobe 微基准、syscall、ssl-nginx（sslsniff）、syscount-nginx、MPK。
- 对比官方 v0.2.0 与 master 两个版本。
- Jetson 内核没有 BTF，为此构建了 no-BTF 的镜像和构建流程。
- 对应分支：`main`、`codex/official-no-btf`；运行手册见
  [ssl-nginx 运行手册](ssl-nginx/bpftime-official-no-btf-ssl-nginx-benchmark-runbook.md)。

## 2. ssl-nginx 端到端调查与修复（7/12 – 7/27）

- **现象**：Jetson 上 bpftime 吞吐落后 kernel 3.8–12.6%，方差大、偶尔双峰；同一套软件在 x64 上大幅领先 kernel。
- **根因，三层逐层剥出**：
  1. **测量假象**：software perf record 没有按 8 字节对齐，导致隐藏丢事件和双峰（正确性 bug）。
  2. **主因（约 72%）**：`bpf_perf_event_output` 每个事件都做一次 CPU 亲和性绑核，多出 4 次 syscall，而这个操作没有任何保护作用。
  3. **次因**：`bpf_probe_read_user` 每次调用都重装 SIGSEGV 处理函数，判断条件读了未初始化的变量。
- **修复后**：
  - Jetson 上 bpftime 比 kernel 快 7.6%（16 B）/ 9.8%（256 KB），方差（CV）从 8.7–16.6% 降到 1–3%；
  - 同一台 x64 虚拟机上 A/B 实测，去掉绑核后吞吐提升 37–41%。
- **其他产出**：上游 issue 草稿、源码阅读指南（`docs/source-reading-guide` 分支）、性能分析方法手册、网络环境（Docker bridge 与 host 网络）对照调查。
- 详见 [ARM 性能根因总览](ssl-nginx/bpftime-arm-performance-root-cause-20260727.md)、
  [上游 issue 草稿](guides/upstream-issue-drafts.md)、[性能分析方法手册](guides/performance-analysis-playbook.md)。

## 3. uprobe map 操作的诊断（7/30 – 8/16）

- **修正 benchmark 语义**：原 hash delete 测试几乎测的都是删除不存在的 key，已修正；array 不支持 delete，该项没有性能意义。
- **ARM64 与 x64 顶层对照**，以及严格匹配条件下的 bpftime − kernel 差额。
- **逐层定位成本**：从 runtime 的四层分层（L0–L3），到同一二进制里切换生产路径做 A/B，再到源码最底层的具体操作。
  成本定位在通用分派、`std::function` 包装、Boost 共享内存容器、value 复制、节点析构和回收。
- 修复 per-CPU hash delete 的语义 bug。
- hash 的负载与 key/value 大小扫描，验证归因在不同负载下都成立。
- 详见 [uprobe 综合调查报告](uprobe/bpftime-uprobe-comprehensive-investigation-report-20260813.md)、
  [12 项操作调查状态表](uprobe/map-operation-investigation-matrix.md)。

## 4. C++ 实现层优化（10/11 – 10/19）

- **per-CPU 路径**：per-CPU array 的 `std::function` 改为模板转发；per-CPU hash 减少复制、优化 key 比较、
  4 字节 key 用整数 hash、复用节点和 value 存储。
- **加载时按 map 类型选专用 helper**，覆盖 lookup 和 update。
- **hash 查找路径**：hash 计算、探测、key 比较、取模优化。
- **结果**：10 项 map 操作中 8 项快于 kernel，只剩两项 array lookup。
- **工程**：建立每日自动化（10/14 起），跟踪官方新提交，自动重放补丁栈、构建、测试。
- 详见 [2026-10-11 优化推进状态](uprobe/bpftime-uprobe-optimization-progress-20261011.md)。

## 5. array lookup 的 JIT 内联（10/19 – 10/21）

- **思路**：参照 kernel 的 `map_gen_lookup`，在 LLVM IR 层把 array lookup 展开成直接的内存读取；
  llvmbpf 中已有相关代码但从未被启用。
- **扩展成通用**：支持 per-CPU array；默认开启；允许和 hash、其他 helper、全局变量出现在同一个程序里。
- **自设计 consuming benchmark**：解决官方测法不使用返回值、测不准内联后 lookup 的问题。
- **结果**：array lookup 从官方原版的 2.5–4.4× kernel 降到 0.53–0.83×。
- **真实程序普查**（libbpf-tools）：93% 的 lookup 是 hash，这决定了下一阶段转向 hash。

## 6. hash map 优化（10/21 – 10/23）

- 修复 JIT 对混用 map 程序的副作用（helper 选择、绑定程序的分派）。
- 改进 hash 数据结构：桶数加倍、删除时回收墓碑、限制探测长度、探测上限可随删除降低。
- 给 hash delete 增加专用 helper。
- **结果**：hash lookup 在各种情况下（含 miss、插删之后）都在 0.77–1.01×；hash update / delete 在 0.26–0.34×。
- 第 5、6 阶段详见 [研究进度与工程接力](uprobe/bpftime-uprobe-handoff-20261021.md) 第 0 节。

## 6.1 回到 ssl-nginx 验证（10/23）

- 同轮对照：无追踪、kernel、官方、官方 + 去绑核、开发分支（map 优化 + 去绑核）。
- **map 优化对端到端吞吐没有可测影响**（+0.2% / +0.4%）：sslsniff 每个事件只有几次 map 操作，bpftime 每个事件约 10 µs
  的开销主要在 uprobe 进出和 perf 输出路径。去绑核仍是主要收益（+21.6% / +19.4%），开发分支比 kernel 快 7.2% / 10.8%。
- 顺带发现并修复一个正确性回归：默认 array JIT 后，程序在 fork 子进程里不执行，nginx worker 的事件全部丢失（`a90fa447`）。
- 又发现并修复（官方也有）：大响应（256 KB）时 bpftime 的 perf 缓冲区满了会静默丢掉约一半事件（`db3d475f`）。
  修复后在 7 种 payload（16 B–256 KB）上都快于 kernel：+3.4% 到 +7.6%；官方 177a1dc 为 −7.3% 到 −11.8%。
- 复核 7 月的 Tailscale/host netns 结论：共享的环境开销成立，“bpftime 额外受损”在当前版本不成立。
- 详见 `benchmark-results/jetson` 分支 `uprobe/ssl-nginx-2x2-20261023/README.md`。

## 7. 总体结果：最初 vs 现在（官方 10 项，相对 kernel）

| 操作 | 最初（10/07） | 现在 |
| --- | ---: | ---: |
| Ordinary array lookup | 4.22× | 0.54×（consuming 测试） |
| Ordinary array update | 1.78× | 0.88× |
| Ordinary hash lookup | 2.04× | 0.85× |
| Ordinary hash update | 0.67× | 0.32× |
| Ordinary hash delete | 0.40× | 0.26× |
| Per-CPU array lookup | 6.51× | 0.74×（consuming 测试） |
| Per-CPU array update | 4.55× | 0.87× |
| Per-CPU hash lookup | 4.97× | 0.94× |
| Per-CPU hash update | 3.26× | 0.66× |
| Per-CPU hash delete | 6.50× | 0.65× |

“最初”为 10/07 优化开始前对官方版本的严格复现；“现在”为 `72aebadd` 的完整十轮（倍率借用 10/21 基线那一轮的
kernel）。两项 array lookup 内联后官方测法测不准，用 consuming 测试的全命中结果。

## 8. 方法论与工程沉淀

- **测量纪律**：同轮测 kernel、固定 CPU 与频率、配对 A/B 与 AB/BA、每项改动跑完整十轮、遇到异常比对机器码。
- **自设计 benchmark**：consuming 测试、hash 各种形态测试、update/delete 测试。
- **可维护性**：每日补丁栈（共 50 个提交，可自动迁移到官方新版本）、接力文档、结果仓库。

## 9. 局限与下一步

- **局限**：
  - 评估仍限于微基准：单线程、小 map；
  - hash map 仍是打过补丁的线性探测，内存翻倍、整表一把锁；
  - 代码布局效应：uprobe 等路径会随构建在 1–10% 内波动。
  - 端到端的 tracing 开销不在 map 操作上（见 6.1）。
- **下一步**：扩大测试覆盖（多维度 benchmark + 真实程序），之后视结果重写 hash map，并考虑整理成上游 PR。
