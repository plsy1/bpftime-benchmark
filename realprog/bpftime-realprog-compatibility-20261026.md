# bpftime 跑真实 uprobe 工具：兼容性问题与修复（2026-10-25 – 10-26）

## 1. 目的与方法

微基准和 ssl-nginx 之外，用真实工具检查 bpftime 是否"完美兼容 BPF 程序"：工具和 BPF 程序原样不动，
问题一律在 bpftime 里修。

- **平台**：Jetson Orin，内核 6.8.12-1021-tegra（无 BTF），`jetson_clocks` 锁频。
- **工具**（7 个）：libbpf-tools v0.37.0 的 funclatency、gethostlatency、memleak、bashreadline；
  bpftime 自带示例 funclatency、malloc、usdt_minimal。
- **编译**：内核没有 BTF，工具用 `-DBPF_NO_PRESERVE_ACCESS_INDEX`、去掉 `.BTF.ext` 编译，libbpf-tools 的
  `ensure_core_btf` 在无 BTF 时不报错。只改编译方式和用户态 BTF 检查，不改 BPF 源码；kernel 与 bpftime 用同一份产物。
  BTF 内核启用后这套补丁作废。
- **审计**：每个工具配一个工作负载（`callloop`、`resolveloop`、`mallocloop`、pty 驱动的 bash、usdt victim），
  工作负载打印精确调用总数，比较工具记到的事件数与实际次数。
- **对照**：kernel、官方 177a1dc（`builds/official`，未打补丁）、开发分支各版本。
- 脚本与原始数据：`benchmark-results/jetson` 分支 `uprobe/realprog-audit-20261025`（审计与脚本源码）、
  `realprog-measure-20261025`、`realprog-fixes-20261025`、`realprog-next-20261025`、`realprog-rest-20261026`。

## 2. 起点（10/25 审计）

| 工具 | kernel | 官方 177a1dc | 开发分支 bca2ebce |
|---|---|---|---|
| funclatency（libbpf-tools） | 正常 | 被追踪程序崩溃 | 崩溃 |
| memleak | 正常 | 0 个泄漏、48 次 map 错误 | 同官方 |
| funclatency 示例、gethostlatency、bashreadline、usdt_minimal | 正常 | 正常 | 正常 |
| malloc 示例 | 正常 | 偶尔多计 10–20% | 同官方（概率不同） |

没有任何工具是"官方能用、开发分支不能用"。

## 3. 发现并修复的问题（开发分支 `codex/uprobe-official-177a1dc465ff`）

"官方"列：官方 177a1dc 是否也有该问题。每个修复都带一个在修复前失败、修复后通过的测试。

| # | 提交 | 暴露它的工具 | 问题与根因 | 修复 | 官方 |
|---|---|---|---|---|---|
| 1 | `97a837bc` | memleak | uretprobe 里 `bpf_get_stackid` 全部失败：`current_thread_gum_cpu_context` 只在 on_enter 设置 | 返回点也能取到 CPU 上下文 | 有 |
| 2 | `28637b6b` | memleak | 修好 1 后进程退出时崩溃：退出析构函数拆掉共享内存，探针仍挂着 | 共享内存释放后跳过程序执行（`-ENODEV`） | 有（潜伏） |
| 3 | `a43f6b85` | memleak `-c` | "Expected fd 4 to be a map fd"：工具在首个 `bpf()` 前 fork 子进程，子进程又启动一个 syscall-server，新会话清掉父进程的 map | 服务启动前 fork 的子进程直通 libc | 有 |
| 4 | `d0e40e41` | malloc 示例 | hash map 忽略 `BPF_EXIST`/`BPF_NOEXIST`，被读者删除的 key 带旧计数写回 | 按内核 htab 语义（`EEXIST`/`ENOENT`/`E2BIG`） | 有 |
| 5 | `86f98862` | funclatency（libbpf-tools） | JIT 崩溃：llvmbpf 只为 pc > 1 的 bpf-to-bpf 调用登记返回块，pc 0 的调用写入空指针 | 父仓库补丁 `local-call-return-block.patch` | 有 |
| 6 | `df865b3e` | funclatency（libbpf-tools） | JIT 编译失败后 LLVM VM 每次调用都重编并打 3 行日志（1122 次/秒） | 编译失败的程序一次性禁用（`-ENOEXEC`） | 有 |
| 7 | `2448aeaf` | （完整十轮） | #1 在返回点设线程局部变量，agent 里每次 TLS 访问是 `__tls_get_addr`，uretprobe +1.9% | 取栈时从当前 gum invocation 取上下文 | 我们引入 |
| 8 | `64da197c` | memleak | gum 精确回溯每次重读 ELF 展开信息（2 万次分配/秒），栈只有一帧 | aarch64 按帧指针取栈，结果同内核 `perf_callchain_user` | 有 |
| 9 | `d766e6ad` | memleak | 栈 map 查询越界读（栈尾乱值）、等长栈只比哈希、冲突返回旧 id、`bpf_get_stackid` 错误被转成正 id | 按 `__bpf_get_stackid`：补零、截断、`-EEXIST` | 有 |
| 10 | `f9d2d9fc` | funclatency（libbpf-tools） | 缺 `BPF_MAP_TYPE_CGROUP_ARRAY` 和 37 号 `bpf_current_task_under_cgroup` | 写入时把 cgroup 目录 fd 解析成路径；按线程读 `/proc/thread-self/cgroup` | 缺 |
| 11 | `597d89f0` | malloc 示例、usdt | `bpf_printk` 直接 `vprintf` 到宿主 stdout（违反上游 #634 的宿主契约），被杀时缓冲区内容丢失 | 默认写 bpftime 日志；`BPFTIME_TRACE_PRINTK_OUTPUT` 可选 stdout/stderr/文件/none | 有 |
| 12 | `4341a60b` | memleak | 共享内存不够时 `bpf()` 异常被转给真内核，内核建了 map，工具加载"成功"却记不到数据 | 返回 `ENOMEM`；默认 50 → 128 MB（boost 预占，常驻内存） | 有 |
| 13 | `da50a34c` | funclatency 等 | 每事件的出错日志每次都打 | 同一位置只在第 1、10、100…次打 | 有 |
| 14 | `71ce81b7` | （分析发现） | 服务启动后 fork 的子进程关闭继承 fd，删掉所有进程共享的对象 | 子进程只关自己的 fd | 有 |
| 15 | `b00be207` | （补全） | 缺 80 号 `bpf_get_current_cgroup_id` | 返回 cgroup v2 目录的 inode 号 | 缺 |
| 16 | `f2596ce8` | memleak | 栈帧带 PAC 签名解析不出符号：`gum_strip_code_address` 在 Linux 上不去签名 | 用 XPACI（检查 `HWCAP_PACA`） | 有（gum） |
| 17 | `4d9b8a61` | memleak | 探针看到 agent 自己的分配（约 250 个、两个 8 MB 线程栈） | agent 挂载、建线程、setenv 和后台线程 `gum_interceptor_ignore_current_thread` | 有 |
| 18 | `a7d54d7e` | （完整十轮） | #13 的日志内联进热函数，`bpf_probe_read` 每次都加载栈保护金丝雀，probe read +31% | 日志改为冷的非内联函数模板 | 我们引入 |

组件分布：uprobe 挂钩层（1、7、8、16）、运行时（2、6、13、18）、syscall-server（3、12、14）、
map 实现（4、9、10）、helper（9、10、11、15）、llvmbpf JIT（5）、agent（17）。

## 4. 不是 bpftime 的问题

- **gethostlatency 少记 0.5–0.9%**：测试脚本用"每秒中位数 × 秒数"估算期望值，误差约 1%。改用精确总数后，
  工具记到的数 + 它打印的 "lost N events" = 实际调用数；kernel 也同样会丢（工具每事件打印一行，perf 缓冲溢出）。
- **审计脚本自身的问题**（已修）：工具输出管道 64 KB 满后阻塞（gethostlatency 只记到约 1200）；usdt 计数正则匹配到 libbpf 调试行；
  malloc 示例取单秒最大值；memleak `-c` 的 shell 包装（曾误判为原因，真正原因是 #3）。
- **memleak 少 1 个 4096 字节分配**：agent 在 `__libc_start_main` 才接管，之前的分配看不到（注入式的固有限制，未验证）。

## 5. 结果（`a7d54d7e`）

功能：7 个工具的计数与 kernel 一致；memleak 1,026 个泄漏（kernel 1,027），栈 main → libc → `__libc_start_main` → `_start`；
usdt_minimal 8/8；funclatency 的 cgroup 过滤与 kernel 一致。

吞吐（被追踪程序每秒操作数，5 轮中位数；`realprog-rest-20261026/measure-run-2`）：

| 工具 | 不追踪 | kernel | 官方 177a1dc | `a7d54d7e` | 相对 kernel |
|---|---:|---:|---:|---:|---:|
| gethostlatency | 112,563 | 59,881 | 55,252 | 86,820 | 1.45× |
| funclatency（bpftime 示例） | 9,000 万 | 506,997 | 1,678,052 | 1,826,687 | 3.60× |
| funclatency（libbpf-tools） | 9,000 万 | 480,148 | 崩溃 | 1,499,178 | 3.12× |
| memleak | 4,240 万 | 324,340 | 不工作 | 414,798 | 1.28× |
| malloc 示例 | 4,250 万 | 531,086 | 1,921,304 | 524,523 | 0.99× |

官方列来自 `realprog-measure-20261025`（官方含每事件绑核）。funclatency 与 malloc 追踪的函数只要几纳秒，测的基本是探针本身；
malloc 示例每次 malloc 都 printk，#11 之后 printk 走日志，从约 200 万降到 52 万（`stdout` 65 万、`none` 300 万）。
完整十轮相对修复前在 ±2% 内；1–2.5% 的差值经 `perf stat` 确认每次调用指令数不增、周期相同（布局/噪声）。

## 6. 观察

- 18 个问题里 16 个官方也有，且几乎都与 ARM 无关；只有 #8、#16（PAC）与 ARM 有关。这些路径（返回点取栈、先 fork 后加载、
  退出时探针仍在、首条指令就是函数调用、边读边删）是 bpftime 自带示例和论文 benchmark 没走过的。
- GitHub（10/25）：309 个 issue 中几乎没有真实工具的报告（memleak 仅 #450，最终跑的是示例）；
  `bpf_get_stackid`（#140）、ARM64 Frida 支持（#304）至今未关闭。
- 上游合并外部小修复积极（393 个 PR 合并约 85%）；第一批建议：#5（先提 llvmbpf）、#4、#9、#1+#7。
  提交前对照：上游 #705（加载时拒绝未注册 helper）与 #6 重叠，#703/#701 在 #2 附近。

## 7. 遗留

- printk 默认路径慢：内核式共享内存 trace 环形缓冲区（有界、无系统调用、工具读取）是更快的默认方案。
- cgroup 过滤 327k 次/秒（kernel 469k）：每次一次 `pread`。
- ~~当前只覆盖 7 个工具~~：10/26 已扩到 18 个，见第 8 节。

## 8. BTF 内核上的扩大审计（10/26）

新内核 `6.8.12-1021-tegra-btf` 有 BTF，第 1 节的无 BTF 编译方式作废：libbpf-tools v0.37.0 和 bpftime 示例原样 CO-RE 编译，
另加 bpftrace 0.20.2 和 OpenJDK 17。数据与脚本：`benchmark-results/jetson` 分支 `uprobe/btf-kernel-20261026`（README 有全表）。

| 工具 | kernel | 官方 177a1dc | `a7d54d7e` |
|---|---|---|---|
| 原 7 个（CO-RE 产物） | 正常 | 同第 2 节的老问题 | 全部精确一致 |
| sslsniff 示例（`sslloop`：BIO 对上的 SSL 读写） | 精确 | 精确 | 精确 |
| bashreadline 示例 | 200/200 | 200/200 | 200/200 |
| get_stack_id 示例 | 栈全是 `-EEXIST`（示例追踪全部进程，map 满） | 记到 agent 自己的分配，栈 2 帧 | 栈 6 帧 |
| bpftrace：uprobe 计数、uprobe+uretprobe 直方图、`ustack`、bashreadline.bt、gethostlatency.bt | 精确 | 精确（`ustack` 慢 11 倍） | 精确 |
| bpftrace：JVM 里的 USDT `hotspot:gc__begin` | 50/50 | 50/50 | 50/50 |
| javagc（libbpf-tools） | 50/50 | 未测 | 未测 |
| opensnoop（syscall tracepoint） | 99.99% | 0，不报错 | 0，不报错 |

- 开发分支上没有发现新的 bpftime 问题；官方的差别都是已修过的问题：取栈与 agent 自身分配（第 3 节 #8、#16、#17），perf 输出静默丢事件（10/24 的 db3d475f）。
- `ustack` 符号：两边都是 `target_func → __libc_start_call_main → __libc_start_main_impl → _start`（入口处缺直接调用者，同内核），
  bpftime 多一帧 agent 的 `bpftime_hooked_main`（agent 包装了 main）。
- **跑不了的**：javagc 必须先有 JVM 的 pid 再加载，bpftime 只能靠 `bpftime attach`（frida CLI）注入运行中的进程，冻结构建没有编；
  syscall tracepoint 工具在 bpftime 上显示 "Attach successfully" 却一个事件都没有（aarch64 没有系统调用改写跳板），应在挂载时报错。
- **不是 bpftime 的**：libbpf-tools v0.37 opensnoop 的 `-n` 在 kernel 上也会以 EPERM 退出（过滤掉的事件返回 -1，被 `ring_buffer__poll` 当成错误）；
  bpftime 示例 opensnoop 挂 `sys_enter_open`，arm64 没有这个 tracepoint。

吞吐（5 轮中位数，相对 kernel）：

| 工具 | kernel | 官方 | `a7d54d7e` | `a7d54d7e`/kernel |
|---|---:|---:|---:|---:|
| gethostlatency | 62,086 | 56,258 | 88,226 | 1.42× |
| funclatency 示例 | 505,050 | 1,681,368 | 1,847,435 | 3.66× |
| funclatency（libbpf-tools） | 484,264 | 崩溃 | 1,473,208 | 3.04× |
| memleak | 331,007 | 不工作 | 419,432 | 1.27× |
| malloc 示例 | 537,594 | 1,940,265 | 519,392 | 0.97× |
| bpftrace uprobe 计数 | 795,350 | 3,946,528 | 4,290,175 | 5.39× |
| bpftrace uprobe+uretprobe 直方图 | 468,611 | 1,162,976 | 1,233,130 | 2.63× |
| bpftrace gethostlatency.bt | 61,412 | 87,401 | 85,162 | 1.39× |

kernel 侧与旧内核相差 −0.4%～+3.7%；新内核配置不改变 kernel 成本（完整十轮 kernel 腿逐项 ±3%，ssl-nginx kernel 12,163 req/s，旧 11,967–12,027）。
