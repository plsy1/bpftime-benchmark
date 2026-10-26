# bpftime 在 ARM64 上的性能研究：阶段总结（截至 2026-10-26）

本文按时间整理课题从开始到现在的全部工作，作为汇报大纲。每个阶段只列要点，详细数据见各阶段链接的文档和
`benchmark-results/jetson` 分支的结果。依据是本仓库各分支的提交记录（最早为 2026-06-26）。
本文随阶段推进持续更新（原文件名 `bpftime-research-overview-20261023.md`）。

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
- 详见 [2026-10-11 优化推进状态](archive/bpftime-uprobe-optimization-progress-20261011.md)。

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
- 第 5、6 阶段详见 [接力文档历史部分](archive/bpftime-uprobe-handoff-20261021-history.md) 和 [接力文档](uprobe/bpftime-uprobe-handoff-20261021.md) 第 0.1 节。

## 7. 回到 ssl-nginx 端到端（10/23 – 10/24）

- **10/23 同轮对照**：无追踪、kernel、官方、官方 + 去绑核、开发分支。map 优化对端到端吞吐没有可测影响（+0.2% / +0.4%）：
  sslsniff 每个事件只有几次 map 操作，bpftime 每个事件约 10 µs 的开销在 uprobe 进出和 perf 输出路径。去绑核仍是主要收益（+21.6% / +19.4%）。
- **两个正确性修复**：默认 array JIT 后程序在 fork 子进程里不执行，nginx worker 的事件全部丢失（`a90fa447`）；
  256 KB 大响应时 perf 缓冲区满了静默丢掉约一半事件（`db3d475f`，官方也有）。修复后 7 种 payload 都快于 kernel（+3.4% 到 +7.6%）。
- 复核 7 月的 Tailscale/host netns 结论：共享的约 8% 环境开销成立，"bpftime 额外受损"在当前版本不成立。
- **10/24 四段路径消融**（锁频重做）：输给 kernel 的是 helper/map/元数据一段，一半来自 `bpf_get_current_uid_gid` 每次两个系统调用，
  加缓存（`7ed339ad`）；再用 perf 找到 perf 输出每次 `getpid`/`gettid`，加缓存（`bca2ebce`）。
  16 B：相对 kernel +17.4%，相对不追踪 −19.2%（kernel −31.1%；官方论文 x86 上为 −7.41%）。
- **payload 扫测**（16 B–256 KB）：相对 kernel 从 +16.7% 收窄到 +11–12%。copy-size 实验说明这是分母变大造成的稀释，每事件绝对优势一直约 4.5–6 µs。
- **官方 benchmark 覆盖**：10 项中 ARM64 上只有 uprobe 和 ssl-nginx 能跑；syscall/syscount-nginx/fuse 缺 aarch64 系统调用改写跳板，
  deepflow 缺内核 `FTRACE_SYSCALLS`。
- **BTF 内核**：编好 L4T R39.2.1 内核 `6.8.12-1021-tegra-btf`（BTF、syscall tracepoint、kprobe、function tracer），A 段作为非默认启动项装好，
  B 段（启动验证）需本人在场。当晚内核崩溃（疑似 memcg 内核 bug），memtester 9 遍 4.5 GB 无错误，NVMe 健康。
- 详见 `benchmark-results/jetson` 分支 `uprobe/ssl-nginx-2x2-20261023/README.md` 及 10/24 各结果目录。

## 8. 真实程序：兼容性审计与修复（10/25 – 10/26）

- **审计**：7 个真实 uprobe 工具（libbpf-tools 的 funclatency、gethostlatency、memleak、bashreadline，bpftime 的 funclatency/malloc/usdt 示例），
  比较 kernel、官方 177a1dc 和开发分支。memleak、funclatency（libbpf-tools）在官方上就不能用；没有"官方能用、我们改后不能用"的工具。
- **修复**：三批共 18 个提交，分布在 uprobe 挂钩层、syscall-server、运行时、map 实现、helper 和 llvmbpf JIT 几乎所有组件
  （此前的优化集中在 map、JIT 和 perf 输出热路径，这次是边角路径的正确性）。逐项见 [真实程序兼容性报告](realprog/bpftime-realprog-compatibility-20261026.md)。
- **结果**：7 个工具的计数全部与 kernel 一致；吞吐相对 kernel：gethostlatency 1.45×、memleak 1.28×、funclatency 3.1–3.6×、malloc 示例 0.99×
  （每次 malloc 都 printk，printk 改为不写宿主 stdout 后变慢）。memleak 从 2 万提到 41 万次分配/秒（kernel 32 万）。
- **上游情况**：GitHub 上几乎没有真实工具的问题报告；上游合并外部小 PR 积极，计划分批提交（见接力文档 0.6）。
- **BTF 内核后扩大范围（10/26）**：切到带 BTF 和 syscall tracepoint 的新内核，工具改用原版 CO-RE 编译，审计扩到 18 个工具
  （新增 sslsniff、bashreadline、get_stack_id 示例和 6 个 bpftrace 用法，含 JVM 里的 USDT），开发分支全部与 kernel 精确一致，没有新问题；
  javagc（要注入运行中的进程）和 syscall tracepoint 工具（aarch64 缺跳板，静默无事件）在 bpftime 上跑不了。
  重测 kernel 基线：新内核配置不改变 kernel 侧成本（±3%），之前的倍率都成立（`results/uprobe/btf-kernel-20261026`）。

## 9. 总体结果：最初 vs 现在（官方 10 项，相对 kernel）

| 操作 | 最初（10/07） | 10/23（`72aebadd`） | 现在（`a7d54d7e`） |
| --- | ---: | ---: | ---: |
| Ordinary array lookup | 4.22× | 0.54×（consuming 测试） | 未变（consuming 测试；官方测法 0.31×） |
| Ordinary array update | 1.78× | 0.88× | 0.88× |
| Ordinary hash lookup | 2.04× | 0.85× | 0.81× |
| Ordinary hash update | 0.67× | 0.32× | 0.30× |
| Ordinary hash delete | 0.40× | 0.26× | 0.26× |
| Per-CPU array lookup | 6.51× | 0.74×（consuming 测试） | 未变（官方测法 0.93×） |
| Per-CPU array update | 4.55× | 0.87× | 0.87× |
| Per-CPU hash lookup | 4.97× | 0.94× | 0.94× |
| Per-CPU hash update | 3.26× | 0.66× | 0.65× |
| Per-CPU hash delete | 6.50× | 0.65× | 0.66× |

"最初"为 10/07 优化开始前对官方版本的严格复现；后两列为完整十轮，倍率借用 10/21 基线那一轮的 kernel。
端到端：ssl-nginx 16 B 相对 kernel +19.1%、256 KB +12.4%（`2448aeaf`，10/25）；官方 177a1dc 在 7 种 payload 上 −7.3% 到 −11.8%。

## 10. 方法论与工程沉淀

- **测量纪律**：同轮测 kernel、固定 CPU 与频率（重启后先 `jetson_clocks`）、配对 A/B 与 AB/BA、每项改动跑完整十轮、异常时比对机器码和
  `perf stat`（每次调用指令数 + IPC，用来区分真实开销与代码布局效应，见 `realprog-next-20261025/malloc-ab`）。
- **每个修复配一个"不修就失败"的测试**，并在官方原版上确认问题是否已存在。
- **自设计 benchmark**：consuming 测试、hash 各种形态测试、update/delete 测试、真实程序审计与吞吐测量脚本。
- **可维护性**：每日补丁栈（73 个提交，每次改动先做 LLVM 15 回放验证，已到第 12 轮）、接力文档、结果仓库，频繁同步 GitHub。

## 11. 局限与下一步

- **局限**：
  - 微基准仍是单线程、小 map；hash map 是打过补丁的线性探测，内存翻倍、整表一把锁；
  - 代码布局效应：uprobe 等路径和个别工具会随构建在 1–6% 内波动；
  - 真实工具只覆盖 uprobe/USDT 类（18 个）；syscall tracepoint 类在 ARM64 bpftime 上不支持，运行中注入（`bpftime attach`）未测。
- **下一步**：
  1. ~~启动 BTF 新内核，扩大真实程序范围，重测 kernel 基线~~（10/26 完成）；
  2. 扩大 ARM64 支持（aarch64 系统调用改写跳板，解锁 syscall/syscount-nginx/fuse 和 syscall tracepoint 工具；deepflow；`bpftime attach`）；
  3. 分批向上游提交修复；
  4. 视结果再做多维度 map benchmark 和 hash map 重写。

## 12. x86 NUC（10/26 起）

- 用户的 x86 NUC（i5-1240P，Debian 13，内核 6.12）上由另一个会话跑 bpftime，两边通过本仓库交接（[交接说明](https://github.com/plsy1/bpftime-benchmark/blob/summry/x86-nuc/x86-nuc/bpftime-x86-nuc-handoff-20261026.md)）；
  NUC 上不改开发分支，发现的问题交给 Jetson 会话（[x86 发现](uprobe/bpftime-x86-nuc-findings-20261026.md)）。
  x86 的结论和数据放在 [`summry/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/summry/x86-nuc) 和 [`benchmark-results/x86-nuc`](https://github.com/plsy1/bpftime-benchmark/tree/benchmark-results/x86-nuc) 分支。
- **构建与回归（10/26）**：官方 177a1dc 和 `a7d54d7e` 用 LLVM 19 都能编；开发分支每日过滤器 110 个用例、Frida 19 个用例全过，
  含 x86 走 gum 回溯器的 uretprobe 取栈。交给 Jetson：`bpf_get_stackid` 的错误在 64 位返回值里仍是正数（`d766e6ad` 的三元表达式类型提升，官方同样），
  诊断 benchmark 的 clang 查找让只有 clang-19 的机器配置失败。详见 [x86 报告](https://github.com/plsy1/bpftime-benchmark/blob/summry/x86-nuc/x86-nuc/bpftime-x86-nuc-build-regression-20261026.md)。
- **uprobe 完整十轮（10/26，4.4 GHz）**：开发分支 10 项 map 操作全部快于同轮 kernel（0.25–0.81×），官方只有 hash update/delete 快于 kernel；
  hash 三项倍率与 Jetson 几乎相同。x86 kernel uprobe 约 1.1 µs/次，bpftime 124 ns。见 x86 报告第 5 节。
- **ssl-nginx（10/26）**：a7d54d7e 相对同轮 kernel +48.0%（16 B）/ +27.9%（256 KB），Jetson 为 +17.5% / +10.4%；官方在 x86 上也快于 kernel（+16.2%），
  但 256 KB 时丢约 70% 事件。7 档扫测：a7d54d7e 领先从 16 B 的 +50% 收窄到 256 KB 的 +26%（Jetson +17% → +12%），
  官方从 1 KB 起就丢事件。见 x86 报告第 6 节。
