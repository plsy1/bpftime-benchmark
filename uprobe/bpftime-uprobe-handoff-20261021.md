# Uprobe 优化：研究进度与模型接力（2026-10-21）

本文件是当前接力入口（文件名保留 10/21，根目录 `HANDOFF.md` 指向这里）。
最后核对：2026-10-26 13:45 JST 左右。以下分支、进程和文件状态均为本机快照；
后续模型行动前需重新核实。**新会话先读第 0 节**；第 1–3 节是工作区、JIT 设计边界和易踩坑。
课题全过程见 [研究总览](../bpftime-research-overview.md)；10/21 写成的原第 1–9 节已移到
[archive](../archive/bpftime-uprobe-handoff-20261021-history.md)。

## 0. 现状一页（2026-10-26 13:45 JST，新会话从这里继续）

**目标**：继续优化 `codex/uprobe-official-177a1dc465ff`（开发 worktree
`/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate`，HEAD `a7d54d7e`）。
下一阶段重点：扩大测试覆盖（见 0.4），hash map 重写暂缓。

### 0.1 map 与 ssl-nginx 阶段的提交（10/22–24，均已推送并加入补丁栈）

| 提交 | 改动 | 效果（相对同轮 kernel） |
|---|---|---|
| `baf0cc85` | 被 array JIT 绑定的程序里，非内联 map 调用改为按 FD 平表分派 | hash + array 程序 lookup 2.41x → 1.95x |
| `c3090b59` | 放宽 typed helper 选择（全局变量、事件类 map、array + 一种 hash 都用专用 helper） | hash + 全局变量 1.78x → 1.19x |
| `1cb20807` | **改 hash 数据结构**：桶数 `next_prime(2×max_entries)`（内存翻倍）、删除时回收以空桶结尾的墓碑段、按最大偏移距离限制探测；已有元素从不移动 | 满表 miss 19x → 0.94x；随机命中 1.17x → 0.77x |
| `f26cc335` | 绑定程序里常量非 array 句柄的 lookup/update/delete 直接调用原 helper（overlay + 私有 helper 槽） | hash + array 程序与只有 hash 的一样快 |
| `af10949b` | 探测上限按“每种距离的元素数”维护，最远元素删光后上限下降（每 map 多 256 字节） | 插删后半满表纯 miss 1.62x → 1.01x |
| `dffb2fce` | 删除时只在最远一档被删空时才重算上限 | 无可测收益，逻辑保留 |
| `72aebadd` | hash 专用 delete helper（加载时按类型选，与 lookup/update 同规则） | 官方 hash delete 36.9 → 28.2 ns（0.26x）；delete+insert -10% |

10/23 下午到晚上又加 3 个提交（均已推送；已加入 `config.json`）。10/24 又加 `7ed339ad`、`bca2ebce`，补丁栈共 55 个提交；LLVM 15 重放第 6–9 轮见 `round6-affinity-fork`、`round7-perf-lost`、`round8-uid-gid`、`round9-perf-identity`：

| 提交 | 改动 | 效果 |
|---|---|---|
| `20870a03` | 去掉 `bpf_perf_event_output` 每个事件的 CPU 亲和性绑核（7 月 ssl-nginx 修复，官方至今没有） | ssl-nginx 吞吐 +21.6%（16 B）/ +19.4%（256 KB），相对官方 177a1dc |
| `a90fa447` | **正确性修复**：fork 子进程可以执行继承的 array JIT 程序。atfork prepare 为子进程给每个绑定多拿一份共享引用 | 修复前 nginx worker 里 sslsniff 一个事件都收不到（见 0.3 第 8 条） |
| `bca2ebce` | 缓存 perf 输出用来找分片的 pid/tid（原来每次输出都调 `getpid` 和 `gettid` 两个系统调用，用 perf 找到）；fork 子进程重置 | ssl-nginx 16 B 相对 kernel +14.8% → +17.4%，256 KB +9.1% → +11.7% |
| `7ed339ad` | 缓存 `bpf_get_current_uid_gid`：agent 挂上 libc 的 8 个 set*id 函数，调用后让缓存失效；只有 8 个挂钩都生效（LD_PRELOAD）才启用；fork 子进程失效。每日过滤器加 `[uid_gid]` | ssl-nginx 16 B 相对 kernel +8.0% → +16.5%，256 KB +6.8% → +12.9% |
| `db3d475f` | **正确性修复**：perf 输出丢事件时按 kernel 方式报告（`PERF_RECORD_LOST`、`-ENOSPC`）；生产者写到缓冲区半满时用 futex 叫醒消费者（模拟的 `epoll_wait` 原来固定每 1 ms 轮询一次）。每日过滤器加 `[perf_event],[epoll]` | 256 KB 时每请求送到事件 9.5 → 18（kernel 17.9）；ssl-nginx 256 KB 相对 kernel +6.7%（同样的事件数） |

另：`ccb0ade1`（距离 0 不计数）使 hash delete 慢 17%，推送前已撤销。原因：hash 删除被内联进 ~9.7 KB 的通用
delete 大函数，源码小改动就会重排整函数的代码生成；专用 delete helper 后不再受影响。
每日补丁栈：LLVM 15 第 5 轮重放（`results/uprobe/daily-jit-stack-validation-20261021/round5-hash-bound-delete`）
50/50 无冲突，树与 `72aebadd` 相同，候选 77 cases、官方 26 cases 通过。

### 0.1a 真实程序（10/25–26）：审计、测量、18 个修复提交（已推送，已加入补丁栈，共 73 个提交）

逐项汇总（现象、根因、修复、官方是否也有）见 [真实程序兼容性报告](../realprog/bpftime-realprog-compatibility-20261026.md)。

审计（`realprog-audit-20261025`）：无 BTF 构建 7 个 uprobe 工具，比较 kernel / 官方 177a1dc（`builds/official`，**未去绑核**）/ `bca2ebce`。
没有“官方能跑、我们改后不能跑”的工具。测量（`realprog-measure-20261025`，5 轮中位数，被追踪程序吞吐）：gethostlatency `bca2ebce` 是 kernel 的 1.49x
（相对不追踪 −21%，kernel −47%）；funclatency 示例 3.71x、malloc 示例 4.00x（被追踪函数只有几 ns，测的是探针本身；malloc 示例每次调用 `bpf_printk`，bpftime 写宿主 stdout）。

修复（`realprog-fixes-20261025`，每个都有不修就失败的单测）：

| 提交 | 修了什么 |
|---|---|
| `97a837bc` | uretprobe 里栈 helper 全部失败（memleak 拿不到栈） |
| `28637b6b` | 退出时 shm 已解除映射、probe 还挂着，之后的 hook 调用崩溃（栈修好后暴露，官方同样）。`[shm_release]` |
| `a43f6b85` | memleak `-c` 的 “Expected fd 4 to be a map fd”：fork 出的子进程启动了第二个 syscall-server，新会话 `reset_server_state` 清掉父进程刚建的 map。子进程（父进程尚未启动 server 时 fork）改为直通 libc |
| `d0e40e41` | hash update 按 kernel 处理 `BPF_NOEXIST/BPF_EXIST`、满表 `E2BIG`、删除不存在返回 `ENOENT`（malloc 示例官方多计 10–20%） |
| `86f98862` | JIT 崩溃真因是 llvmbpf 差一错误：pc 0 的 bpf-to-bpf 调用没登记返回块。父仓库补丁 `vm/compat/llvm-vm/local-call-return-block.patch`，子模块不改。`[jit_load]` |
| `df865b3e` | LLVM JIT 编译失败后每次调用都重编并打日志（1122 次/秒），改为一次性禁用该程序（`-ENOEXEC`） |
| `2448aeaf` | `97a837bc` 在 on_leave 设线程局部变量使 uretprobe +1.9%（agent 里每次 TLS 访问是 `__tls_get_addr`）；改为栈 helper 需要时从当前 gum invocation 取上下文 |

验证：每日过滤器 97 cases 通过；完整十轮（`full10-2448aeaf`）各项相对 `bca2ebce` ±2.1% 内；ssl-nginx 2 × 10 配对 +0.9%（16 B）/ +0.6%（256 KB），
相对 kernel +19.1% / +12.4%。修复后：memleak `-c` 能报 1024 个泄漏、栈 `main+0xf0` 正确；funclatency（libbpf-tools）不再崩溃，但缺 helper 37，事件数 0。

当晚又加 3 个（`realprog-next-20261025`，第 11 轮回放 65/65，候选 103 cases、官方 34 cases）：

| 提交 | 修了什么 |
|---|---|
| `64da197c` | aarch64 取栈改为按帧指针走（结果同 kernel `perf_callchain_user`：先 pc，再 x29 帧记录，去 PAC，限定线程栈范围）。memleak 2 万 → 42 万次分配/秒（kernel 32 万），栈完整 |
| `d766e6ad` | 栈 map 按 `__bpf_get_stackid`：补零（原来查询越界读，memleak 栈尾的乱值）、截断、冲突返回 `-EEXIST`；`bpf_get_stackid` 错误保持负数 |
| `f9d2d9fc` | 新增 CGROUP_ARRAY 和 37 号 `bpf_current_task_under_cgroup`（写入时把 fd 解析成 cgroup v2 路径；每线程缓存 `/proc/thread-self/cgroup` 的 fd，用 pread 重读，fork 后重开）。funclatency（libbpf-tools）计数和 cgroup 过滤与 kernel 一致 |

七个工具在 bpftime 上全部正常。吞吐（5 轮中位数，相对 kernel）：gethostlatency 1.49x、funclatency 示例 3.68x、funclatency（libbpf-tools）3.21x、
memleak 1.31x、malloc 示例 3.74x；完整十轮 ±2%。malloc 示例相对 2448aeaf −6% 已证实是代码布局（每次调用指令数相同、IPC 2.93 → 2.74；
同逻辑加无用 nop 的对照构建回到 −0.9%），不靠填充修。

10/26 凌晨再加 8 个（`realprog-rest-20261026`，第 12 轮回放）：

| 提交 | 修了什么 |
|---|---|
| `597d89f0` | `bpf_trace_printk` 默认不再写宿主 stdout（上游契约 #634）：默认写 bpftime 日志；`BPFTIME_TRACE_PRINTK_OUTPUT`=stdout/stderr/none/文件（一次 write）；返回长度 |
| `4341a60b` | 用户态处理 `bpf()`/`perf_event_open()` 出错时返回 `ENOMEM`/`EINVAL`，不再转给内核（原来内核建了 map，memleak 的程序引用不到、静默无数据）；默认 shm 128 MB（boost 用 posix_fallocate 预占，所以是常驻内存） |
| `da50a34c` + `a7d54d7e` | 每事件的出错日志限流（第 1、10、100…次）；日志放到冷函数里，否则热函数多出栈保护金丝雀（`4d9b8a61` 上 probe read +31%） |
| `71ce81b7` | 已启动 server 后 fork 的子进程关闭继承 fd 不再删掉全局对象 |
| `b00be207` | `bpf_get_current_cgroup_id`（80） |
| `f2596ce8` | 栈帧用 XPACI 去 PAC（`gum_strip_code_address` 在 Linux 上不去签名） |
| `4d9b8a61` | agent 的挂载、建线程、setenv、后台线程不再触发探针：memleak 1026（kernel 1027，原约 1275） |

gethostlatency 少记不是 bpftime 的问题：用精确总数（工作负载打印 `total_ops`）后，工具计数 + 它报告的 "lost N events" = 调用数，kernel 也同样丢（perf 缓冲溢出）。
吞吐（a7d54d7e，相对 kernel）：gethostlatency 1.45x、funclatency 示例 3.60x、funclatency（libbpf-tools）3.12x、memleak 1.28x、malloc 示例 0.99x
（每次 malloc 都 printk，默认写日志后从约 200 万降到 52 万；共享内存 trace 环形缓冲区是更快的默认方案，未做）。完整十轮 ±2%；相对 f9d2d9fc 的 1–2.5% 差值经 perf stat 确认
每次调用指令数不增（2367 vs 2373）、周期数相同，属布局/噪声。

遗留：cgroup 过滤 327k 次/秒（kernel 469k）；printk 默认路径较慢；bpftime 看不到 agent 接管前（`__libc_start_main` 之前）的分配。上游 PR 计划见 0.6。

### 0.1b BTF 内核（10/26）：CO-RE 审计扩到 18 个工具，kernel 基线重测

10/26 11:03 起默认启动 `6.8.12-1021-tegra-btf`（检查脚本 14 项全过，见 0.4）。结果：`results/uprobe/btf-kernel-20261026`（README 有全部表格）。

- **编译**：无 BTF 补丁作废。libbpf-tools v0.37.0 原版源码 CO-RE 编译（`/home/jetson/src/realprog-20261026-btf/libbpf-tools`），
  bpftime 示例原样编译在官方树的 `*-core` 副本里；另装 bpftrace 0.20.2、OpenJDK 17（libjvm 自带 hotspot USDT）。
- **审计 18 个工具 × kernel / 官方 / `a7d54d7e`**（`realprog-20261026-btf/harness/audit.py`，新工作负载 `sslloop`、`openloop`、`GcLoop.java`）：
  原 7 个用 CO-RE 产物结论不变；新增 sslsniff、bashreadline 示例、get_stack_id 示例、bpftrace 6 个（uprobe 计数、uprobe+uretprobe 直方图、
  `ustack`、JVM 里的 USDT `gc__begin`、bashreadline.bt、gethostlatency.bt）在 `a7d54d7e` 上全部与工作负载精确一致；**没有发现新的 bpftime 问题**。
  官方的差别都是已修过的老问题（栈、agent 自身分配、perf 静默丢事件）。
- **不能跑的**：javagc 要先有 JVM 的 pid 再加载，bpftime 要靠 `bpftime attach`（frida CLI）注入运行中的进程，冻结构建没编；
  opensnoop/syscount（syscall tracepoint）在 bpftime 上“Attach successfully”然后一个事件都没有（aarch64 缺系统调用跳板），应在挂载时报错。
- **不是 bpftime 的**：libbpf-tools v0.37 的 opensnoop `-n` 在 kernel 上也会 EPERM 退出（过滤的事件返回 -1，ring buffer 当错误）；
  get_stack_id 示例在 kernel 上追踪全部进程，栈 map 满后 victim 的栈全是 `-EEXIST`。
- **kernel 基线**：新内核的配置（BTF、tracing）不改变 kernel 侧成本：完整十轮 kernel 腿逐项 −3.1%～+2.2%（同一份 benchmark 程序），
  `a7d54d7e` 相对 kernel 的倍率变化 ≤0.2；真实工具 kernel 吞吐与旧内核差 −0.4%～+3.7%；ssl-nginx kernel 12,163 req/s（旧 11,967–12,027）。
  同轮：ssl-nginx `a7d54d7e` 相对 kernel +17.5%（16 B）/ +10.4%（256 KB），官方 −10.8% / −9.4%；真实工具 `a7d54d7e` 相对 kernel：
  gethostlatency 1.42x、funclatency 示例 3.66x、funclatency（libbpf-tools）3.04x、memleak 1.27x、malloc 示例 0.97x、bpftrace 计数 5.39x、直方图 2.63x。

### 0.1c x86 NUC（10/26，另一个会话）

用户的 x86 NUC 上另有一个会话在跑 bpftime，结果在 `summry/x86-nuc`、`benchmark-results/x86-nuc` 分支。它不改开发分支，
发现的问题交给这里处理：[x86 发现](bpftime-x86-nuc-findings-20261026.md)（`bpf_get_stackid` 的错误在 64 位返回值里仍是正数；
开发分支在只有 clang-19 的机器上配置失败；官方单测 `Test attach replace with ebpf` 在 gum 初始化前调用 gum）。

### 0.2 测了什么（结果目录 `/home/jetson/src/results/uprobe/`）

| 目录 | 内容 |
|---|---|
| `hash-gap-history-20261022` | 6 个版本同轮：三种程序（只有 hash / + 全局变量 / + array）的 hash lookup |
| `hash-shape-20261022` | 自写 workload：顺序/随机 key、是否用返回值、填充 1024/1000/512、插删后；纯 miss |
| `hash-mutate-20261022` | 三种程序里的 hash update、delete+insert |
| `hash-helper-fixes-…`、`hash-probe-bound-…`、`direct-map-helpers-…`、`hash-bound-shrink-20261022`、`hash-typed-delete-20261023` | 每个提交的回归测试、同轮 A/B、只跑 bpftime 的完整十轮（`full10-*`）、配对短测 |
| `direct-map-helpers-20261022/layout-check` | 代码布局验证（全函数 64 字节对齐对照构建） |
| `perf-output-no-affinity-20261023`、`fork-child-binding-20261023` | `20870a03`、`a90fa447` 的回归测试、完整十轮；后者另有 AB/BA 短测 `screen/` |
| `ssl-nginx-2x2-20261023` | ssl-nginx 同轮对照：无追踪 / kernel / 官方 / 官方 + 去绑核 / 开发分支；16 B 和 256 KB，各 2 轮 × 10 次；`db3d475f` 的 2 × 10 对照和 7 种 payload 扫测（含图 `bench/absolute_performance.png`、`bench/relative_to_kernel.png`）（`README.md`、`README-perf-lost.md`） |
| `perf-lost-wakeup-20261023` | `db3d475f` 的测试汇总和完整十轮 |
| `ssl-nginx-ablation-20261024` | 在 `db3d475f` 上重做四段路径消融（16 B、256 KB）和第 2 段的逐项拆分；含论文数据对照。注意：这两次是在动态调频下跑的（机器 10/24 05:54 重启后没执行 `jetson_clocks`） |
| `ssl-nginx-ablation2-20261024` | 在 `7ed339ad` 上锁频重做四段消融、拆第 1 段（只挂入口/只挂返回）、perf record/stat（`perf/`） |
| `perf-output-identity-20261024` | `bca2ebce` 的测试、完整十轮、ssl-nginx 2 × 10 |
| `uid-gid-cache-20261024` | `7ed339ad` 的测试、UID 审计、完整十轮、ssl-nginx 2 × 10 |
| `realprog-audit-20261025`、`realprog-measure-20261025` | 真实工具功能审计（7 个工具 × kernel / 官方 / `bca2ebce`）和吞吐测量（5 轮）；测试脚本源码在 `realprog-audit-20261025/src` |
| `realprog-rest-20261026` | printk、shm 耗尽、日志限流、fork 关闭、cgroup id、PAC、agent 噪声 8 个修复的审计、两次完整十轮、两轮 5 工具测量、perf stat |
| `realprog-next-20261025` | 帧指针取栈、栈 map、cgroup array 的审计、cgroup 过滤检查、5 工具吞吐（`measure-run/`）、完整十轮、malloc 布局对照（`malloc-ab/`） |
| `realprog-fixes-20261025` | 7 个修复提交的审计、两次完整十轮（`full10-df865b3e`、`full10-2448aeaf`）、ssl-nginx 2 × 10（`bench/`） |
| `ssl-nginx-netns-recheck-20261023` | 复核 7 月 Tailscale/host netns 结论：共享的约 8% 环境开销成立，“bpftime 额外损失”在当前版本不成立 |

### 0.3 结论

1. 官方 hash lookup 0.95x 与自设计 ~1.2x 的差别几乎全来自 key 顺序；是否使用返回值、1000/1024 填充对命中无影响。
2. hash lookup 在所有测过的形态（命中、约半数 miss、满表纯 miss、插删后纯 miss）都低于或接近 kernel（0.77–1.01x）。
3. hash update/delete 在三种程序里都远低于 kernel（0.31–0.34x）。
4. 官方十项（`72aebadd`，只跑 bpftime，对基线 `5466f0d`）：hash lookup -10%（0.85x）、hash update -13%（0.32x）、hash delete -22%（0.26x）、其余 map 项 ±1%。
5. 代码布局效应：uprobe、uprobe + uretprobe、per-CPU array update、probe write 会随构建在 1–10% 内波动，已证实是布局而非逻辑（同源码全函数对齐对照）；全函数对齐不采用。热点未定位。
6. ~~已知语义问题：hash update 忽略 `BPF_NOEXIST/BPF_EXIST` 且表满时仍返回 0，delete 不存在的 key 也返回 0。~~ 10/25 由 `d0e40e41` 修复（见 0.1a）。
7. 现有 hash map 仍是“打过补丁的线性探测”：内存翻倍、插删后表内无空桶（靠探测上限兜底）、整表一把锁、只对 4 字节 key 有快速路径。重写方案（元素池 + 带 hash 标签的紧凑索引表、只在索引上做回移删除、分段锁）已讨论，**等测试证明是瓶颈再做**。
8. **ssl-nginx 端到端（`ssl-nginx-2x2-20261023`）**：
   - map 优化对端到端吞吐没有可测影响（开发分支对“官方 + 去绑核”：+0.2% / +0.4%，噪声内）。sslsniff 每个事件只有几次 map 操作，
     而 bpftime 每个事件的开销约 10 µs，省下的几十 ns 不到 1%；开销在 uprobe 进出和 perf 输出路径。
   - 去绑核仍是主要收益：+21.6%（16 B）/ +19.4%（256 KB）；开发分支比 kernel 快 7.2% / 10.8%。
   - 发现并修复默认 array JIT 引入的正确性回归（`c0939bd` 起）：绑定程序在 fork 子进程里返回 ENOTSUP，
     nginx 这种 master 加载、worker 处理请求的程序静默丢掉全部事件。修复为 `a90fa447`。
   - 发现并修复（官方也有）：256 KB 时 bpftime 静默丢掉约一半 perf 事件。根因是模拟的 `epoll_wait` 每 1 ms 才轮询一次，
     一个 256 KB 响应在约 0.7 ms 内产生约 17 个 8 KB 事件，64 KB 的分片放不下。修复为 `db3d475f`，之后 256 KB 才是公平对比。
   - 7 种 payload 公平扫测（16 B–256 KB）：`db3d475f` 在每种大小都快于 kernel（+3.4% 到 +7.6%），官方 177a1dc 都慢于 kernel（−7.3% 到 −11.8%）。
   - 复核 7 月“host netns / Tailscale 让 bpftime 额外受损”：共享的约 8% 下降成立；bpftime 的额外损失在当前版本不成立（交互项跨 0），
     7 月的机制（reader 占约一个核）是次日查出的 perf 记录对齐 bug。
9. `a90fa447` 完整十轮：除 hash lookup / update 外都在 ±1%；AB/BA 短测 hash update +2.5%、hash lookup +1.2%、uprobe +1.2%。
   hash 相关函数指令完全相同，只是地址移动（hash update helper 从 64 字节边界移到偏移 48），属代码布局效应。
10. **四段消融（`db3d475f`，16 B，每请求 µs，bpftime / kernel）**：probe 分发 6.2 / 11.1，helper/map/元数据 11.1 / 5.8，probe_read 1.7 / 1.5，perf 输出 3.6 / 8.7。
   第 2 段是唯一明显输给 kernel 的一段，其中一半是 `bpf_get_current_uid_gid` 每次两个系统调用（5.0 µs/请求），已由 `7ed339ad` 修掉；
   bpftime 的 hash map 操作比 kernel 便宜。所有构建都是 `ENABLE_PROBE_READ_CHECK=OFF`（probe_read 是 memcpy）。
11. 官方论文（OSDI'25）：x86 48 线程机器上 bpftime 比不追踪低 7.41%、kernel 低 28.06%；没有任何 ARM 数据。我们 kernel −31%，bpftime 现为 −20%（16 B）。
12. **机器重启后要先执行 `sudo jetson_clocks`**：10/24 05:54 重启后频率没有锁定，完整十轮的频率检查会拦下，但 ssl-nginx 的 runner 不检查。
13. **perf 定位（本机已装 perf：`/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf`）**：挂空 probe 时 IPC 不变（0.76），每次命中多执行约 1.2–1.5 千条指令，
   开销是 Frida 拦截器 + 分发的指令本身，不是缓存失效；微基准 IPC 高所以看起来便宜十倍。修掉 perf 输出里的两个系统调用（`bca2ebce`）后，
   agent + JIT + vDSO 约占 worker 周期的 9%（约 6.7 µs/请求），分散在 Frida 拦截器、hash map、clock_gettime、perf 输出通用查表、分发，没有单项突出。
   当前 16 B：bpftime 每请求多 13.8 µs（相对不追踪 −19.2%），kernel 26.3 µs（−31.1%）。
14. **7 种 payload 公平扫测（`bca2ebce`，`ssl-nginx-sweep-bca2ebce-20261024`）**：相对不追踪 bpftime −17.4%～−22.7%，kernel −27.6%～−30.3%，
   官方 177a1dc −34.7%～−40.0%；相对 kernel 从 16 B 的 +16.7% 收窄到 128–256 KB 的 +11–12%（早在 7 月 host 测试中就有此趋势，这次在修完全部问题后复现）。
15. **收窄原因（`ssl-nginx-copysize-20261024`）**：固定 256 KB 只改每事件复制量，kernel 每 KB 的成本（+0.33 µs/KB）不低于 bpftime（+0.22），
   每事件绝对优势一直约 4.5–6 µs；百分比收窄是因为大 payload 时每事件附带的 TLS 工作和两边都要付的复制成本把分母变大。
   bpftime 自己多出来的只有大事件触发的“半满唤醒” futex（每请求约 4 次，共享内存上的 futex 每次约 2 µs，约 0.5 µs/事件）。暂不优化。

### 0.4 下一阶段：扩大测试覆盖（用户定的重点）

官方 benchmark 太理想化，自设计的 consuming benchmark 也不够。两条线并行：
1. **多维度、多参数的 benchmark**：key 大小（libbpf-tools 普通 hash 仅约 46% 是 4 字节 key，27% 是 8 字节，27% 是 12–192 字节）、value 大小（多为 16–144 字节）、操作语义（插入新 key、删除不存在的 key）、表的状态、并发（多线程/多进程；hash map 目前整表一把锁），每项与同轮 kernel 对比。
2. **真实程序**：在 bpftime 上跑 uprobe 类工具（如 memleak、funclatency、bashreadline、sslsniff），统计实际 map 使用特征（操作次数、key/value 大小、miss 率、插删频率、并发度），从结果找问题；优化后回到真实程序验证。

次要：布局效应热点定位（本机有 nsys，10/16 用过）、hash 返回值语义、上游 PR。

**扩大支持（10/24 确认在计划内，ssl-nginx 优化告一段落后做）**：官方 `benchmark/` 10 项中，ARM64 上目前只有 uprobe 和
ssl-nginx 能跑并已分析。其余各项的现状（10/24 读源码核对，未改系统）：

| benchmark | 现状 | 解锁条件 |
|---|---|---|
| `syscall`、`syscount-nginx`、`fuse` | 跑不了：bpftime 拦截系统调用靠 `attach/text_segment_transformer`，aarch64 分支只有 `// TODO: implement syscall trace trampoline`，capstone 写死 x86。7 月 ARM64 CI 上 syscount-nginx 的“成功”并没有真正拦截到系统调用（日志已过期） | 实现 aarch64 系统调用改写跳板（`svc #0` 改写 + 指令重定位） |
| `deepflow` | 旧内核上连 kernel 那一组都跑不了：没开 `CONFIG_FTRACE_SYSCALLS`（只有 `raw_syscalls`；10/26 起的 BTF 内核已开），DeepFlow v6.1.1 的 socket 追踪器（`TPPROG(sys_enter_read)` 等）和 `bpftime_daemon`（`tracepoint/syscalls/sys_enter_bpf` 等）都依赖按调用名的系统调用 tracepoint；另需在 ARM64 上装 Rust/Go 重编 agent 和服务 | 内核条件 10/26 已满足；还需在 ARM64 上重编 agent 和服务，bpftime 侧仍受 aarch64 系统调用跳板限制 |
| `redis-durability-tuning` | 未试：uprobe 加 io_uring 扩展，BPF 程序里写死了 x86 路径 | 移植路径，验证 io_uring 扩展在 ARM64 上可用 |
| `gpu` | 未试：CUDA 挂钩，与 CPU 侧 uprobe 研究关系远 | 暂不考虑 |
| `mpk` | 不适用：MPK 是 x86 专有 | — |
| `trap-uprobe` | 不适用：针对 riscv64（9/8 新增） | — |

任何一项跑起来后，先审计 bpftime 那一组确实收到了事件，再采信吞吐数字。

**新内核（10/26 起为默认内核）**：L4T R39.2.1 内核 `6.8.12-1021-tegra-btf`（BTF、`FTRACE_SYSCALLS`、`KPROBES`、`FUNCTION_TRACER`，
全部 NVIDIA 树外驱动重编），10/26 设为 extlinux 默认项并启动验证：`sudo check-btf-kernel.sh` 14 项全过（622 个 syscall tracepoint，
WiFi、GPU、显示正常）。原内核保留为启动项 `primary`；启动菜单在本机显示器上看不到但会等 30 秒（硬件计数器算出内核前多等了 26 秒），
开机约 20 秒盲按 `0` 回原内核，配置备份 `/boot/extlinux/extlinux.conf.before-default-btf`。`nvpmodel.service` 每次开机失败（原内核也是），模式已是 MAXN_SUPER。
操作指南、检查脚本、配置和构建日志在 `benchmark-results/jetson` 分支 `kernel/l4t-r39.2.1-btf/`（本机 `/home/jetson/src/l4t-r39.2.1/`；检查脚本要 sudo 跑）。
deepflow 与 `bpftime_daemon` 的内核条件已满足，尚未尝试。arm64 6.8 上 `FPROBE`（kprobe_multi）开不了，属架构限制。

### 0.5 用户定下的工作方式

- 每次改动：增量构建 → 回归测试（`config.json` 的 `test_filter` 清单）→ 同轮 kernel / 改动前 / 改动后的配对短测 → 完整十轮（不跑 kernel，倍率借用基线那轮 kernel 并注明）。
- 补丁栈攒几轮再加；推送前问用户。
- 汇报用提交号和描述性名称，自起的标签首次出现要解释。
- 10/26 已切到 BTF 新内核：不再做受限于旧内核的工作（无 BTF 编译补丁作废）；CO-RE 审计和 kernel 基线已重做（0.1b）。
- 教训：自选测试过滤器会漏测；SHA 用 `git rev-parse` 完整值；核对系统时间；测量期间不要在其他核上跑 CPU 密集任务。

### 0.6 上游 PR（用户 10/25 定：过几天再提）

上游 eunomia-bpf/bpftime 合并外部小修复很快（393 个 PR 合并约 85%，"fix:" 类 0–15 天），大功能会搁置。规则：一个 issue 一个 PR（Fixes #N）、改动小、带测试、`.clang-format`。
第一批建议：llvmbpf pc 0 调用（先给 eunomia-bpf/llvmbpf 提）、hash 标志 `d0e40e41`、栈 map `d766e6ad`、uretprobe 取栈（`97a837bc`+`2448aeaf` 合一）。
提交前对照：上游 #705（加载时拒绝未注册 helper）与 `df865b3e` 重叠；#703/#701（多进程退出回收 shm）在 `28637b6b` 附近；#685 与 `bca2ebce` 相关；
#678 说明上游仍每事件绑核，`20870a03` 去绑核应先开 issue 附 ssl-nginx 数据。用用户账号 plsy1 提交，先给用户看草稿。

## 1. 工作区与分支（10/26）

| 用途 | 本机路径 / 分支 |
|---|---|
| **开发源码** | `/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate`，分支 `codex/uprobe-official-177a1dc465ff`，HEAD `a7d54d7e`（官方 177a1dc + 73 个提交） |
| 开发构建 | 上述目录的 `build-jit`（LLVM 16，RelWithDebInfo + LTO）；增量构建 `cmake --build build-jit --target bpftime-agent bpftime-syscall-server bpftime_runtime_tests` |
| 冻结的构建产物 | `results/uprobe/ssl-nginx-2x2-20261023/builds/<提交号>/`（agent、syscall-server、bpftimetool），说明见同目录 `README.txt` |
| 官方对照树 | `/home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023`（177a1dc + 去绑核补丁，工作树里还有 no-BTF 示例） |
| 每日自动化 | `/home/jetson/src/bpftime-jetson-daily`，分支 `automation/jetson-daily`，补丁栈在 `tools/jetson-upstream/config.json` |
| 回放验证 worktree | `worktrees/177a1dc465ff/jit-stack-validation-N`（detached，只用于验证，不建分支） |
| 结果仓库 | `/home/jetson/src/results`，分支 `benchmark-results/jetson` |
| 研究文档 | `/home/jetson/src/summry`，分支 `summry/jetson` |
| 真实程序测试 | `/home/jetson/src/realprog-20261025`（libbpf-tools 拷贝、工作负载、审计/测量脚本；源码备份在结果仓库 `realprog-audit-20261025/src`） |
| BTF 内核 | `/home/jetson/src/l4t-r39.2.1/`（B 段指南 `B段-启动新内核操作指南.md`，检查脚本 `check-btf-kernel.sh`） |
| 旧优化源码 | `/home/jetson/src/bpftime`，不要误当成当前开发树 |

开发分支根目录的 `CLAUDE.md` 记录了所有运行时约束和每个修复的规则，改代码前先读。
llvmbpf 子模块保持干净，JIT 修改都是 `vm/compat/llvm-vm/*.patch` 在构建目录生成的 overlay。

## 2. JIT 设计与不能破坏的边界（10/21 写成，仍有效）

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

其余运行时约束（宿主契约、fork、shm、printk、日志限流、取栈、cgroup 等）以开发分支 `CLAUDE.md` 为准，每个修复都在那里写了规则。

## 3. 复现实验与构建易踩坑

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

- **10/24–26 新增的教训**：
  - 机器重启后频率不锁定，测量前先 `sudo jetson_clocks` 并检查 cpu5 为 1728000（完整十轮脚本会拦，其他脚本不拦）。
  - 测量期间不要在其他核上跑编译等 CPU 密集任务；docker 测量容器跑完就删（`run_fair.py` 的清理是全机 `pkill`，两组测量不能同时跑）。
  - 怀疑性能回退时先用 `perf stat` 看每次调用的指令数和 IPC：指令数不变、IPC 变低就是代码布局效应（`realprog-next-20261025/malloc-ab`
    用"加无用 nop 的对照构建"证实过），不靠填充去修。
  - 热函数里不要内联任何日志/格式化代码：会引入栈保护金丝雀（`a7d54d7e`，probe read +31%）；异常路径用冷的非内联函数，参数按值传。
  - 不要用 naked 汇编函数传参：会被过程间优化删掉参数（`f2596ce8` 的第一版）。
  - 真实程序审计用工作负载打印的精确总数（`total_ops`），不要用"中位数 × 秒数"估算；工具输出写文件，不要用没人读的管道。
  - 每个修复先写"不修就失败"的测试，并在官方原版上确认问题是否已存在。
  - 补丁栈每次加提交前做 LLVM 15 回放验证（`daily-jit-stack-validation-20261021/roundN`），推送前问用户。

本机总入口为 `/home/jetson/src/HANDOFF.md`，该根目录文件不在 Git worktree 中；
GitHub 上的接力入口就是本文件。跨分支结果链接使用明确的 GitHub 分支地址。
