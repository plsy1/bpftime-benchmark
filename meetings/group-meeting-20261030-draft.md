# 组会材料草稿（2026-10-30，周五）

上次组会（10/23）讲到：C++ 实现层收尾、array lookup JIT 内联、hash map 优化（截至 10/23 中午）。
本周内容从 10/23 下午起。边做边更新；最后定稿成翻页的幻灯片（形式和语言待定）。

## 本周一句话

单线程和端到端上 bpftime 已普遍快于 kernel；本周把**正确性和覆盖面**补齐（真实工具、ssl-nginx、DeepFlow、多维度、
ARM64 系统调用），同时找到了 bpftime 的主要短板：**多线程**。

---

## 一、已完成

### 1. ssl-nginx 端到端（10/23–24）
- map 优化对端到端吞吐没有可测影响（+0.2% / +0.4%）：每事件约 10 µs 开销在 uprobe 进出和 perf 输出路径。
- 修两个正确性问题：fork 子进程里程序不执行（nginx worker 事件全丢）；256 KB 时 perf 缓冲区满静默丢一半事件（官方也有）。
- 四段路径消融：`bpf_get_current_uid_gid` 每次两个系统调用、perf 输出每次 `getpid`/`gettid`，都加缓存。
- 结果：16 B 相对 kernel **+17.4%**，相对不追踪 −19.2%（kernel −31.1%）；payload 扫测 16 B–256 KB 全部快于 kernel。

### 2. 真实工具：兼容性审计与修复（10/25–26）
- 先审计 7 个、换 BTF 内核后扩到 18 个真实 uprobe/USDT 工具（libbpf-tools、bpftrace、sslsniff、JVM USDT 等）。
- 修复 18 个提交，分布在几乎所有组件（挂钩层、syscall-server、运行时、map、helper、llvmbpf JIT）。
- 结果：全部与 kernel **计数精确一致**；吞吐 gethostlatency 1.45×、memleak 1.28×、funclatency 3.1–3.6× kernel。
- 上游官方版本上 memleak、funclatency 根本不能用。

### 3. BTF 内核（10/26）
- 自编 L4T 内核（BTF、syscall tracepoint、kprobe），工具改用原版 CO-RE 编译；重测 kernel 基线，之前的倍率都成立。

### 4. DeepFlow（daemon 模式，10/26–27）
- 修 6 个 daemon 模式问题后在 ARM64 上跑通（官方同样有这些问题）。
- 结果：bpftime 比 kernel **慢 1–28%**（小请求最慢）。原因：daemon 模式下 map 留在内核，搬到用户态的 uprobe 程序每次访问 map
  都要一次系统调用（只有可 mmap 的 array 例外）；perf 显示这部分占服务端 CPU 的 21%。
- 论文 "至少快 1.5 倍" 的场景是挂在没有系统调用的 Go 函数上，和本周测的路径不同。

### 5. 多维度对比（10/27–28）
- 先用计数选项统计 16 个真实工具的 map 用法（入口插入/出口查删最常见，key 4/8 B，**全部单线程**），再按这些用法做受控测试，
  1–6 线程、同轮 kernel、每次校验结果。
- **单线程**：bpftime 的 map 操作与 kernel 持平或更快。
- **多线程是短板**：6 线程时一次调用 bpftime 15.5 µs 对 kernel 6.7 µs。约 10.7 µs 来自 hash 整表一把自旋锁（查找也拿）；
  约 4.8 µs 来自 Frida（每次命中对同一个计数器原子加减，perf 定位）。
- **正确性**：per-CPU hash 多线程插入让被追踪程序崩溃（官方同样）→ 已修；另记录两条（持锁进程被杀后锁卡死、
  不绑核时 per-CPU map 丢计数 1.4–2.3%）。

### 6. ARM64 系统调用跳板（10/28–29）
- bpftime 在 ARM64 上一直拦不到系统调用（上游 2024-01 起是 TODO）。x86 用 zpoline，ARM64 不能照搬。
- 参考 svc-hook（Middleware 2025）的思路自己实现：每处 `svc` 改成 `b` 到专属跳板；改进了它会改坏寄存器、
  clone 依赖栈帧的问题。6 个测试，每个都确认"去掉对应处理就失败"。
- 端到端：100 万次 write，bpftime 的 `sys_enter`/`sys_exit` 程序计数与 kernel **完全一致**。
- 开销：每次 write bpftime +327 ns、kernel +172 ns（官方 x86 数据方向相同：bpftime 也比 kernel 慢）。
- 官方 `benchmark/syscall`（10 轮，每次 write 一个空程序）：不追踪 353 ns，kernel 438 ns（+85），bpftime 640 ns（**+287**），
  bpftime 的额外开销是 kernel 的 3.4 倍。官方 x86 数据方向相同（2.3 倍、1.5 倍）。
- 线索：每次 write 从一个空程序加到两个计数程序，kernel 开销 +85 → +172 ns，bpftime 只从 +287 → +327 ns。
  bpftime 的开销大部分是**每次系统调用固定**的（跳板、保存寄存器、分发），不在程序本身，下一步拆层测量。

### 7. 工程
- 每日补丁栈 86 个提交，第 14 轮验证；x86 NUC 交叉验证（10/26）。

---

## 二、下一步

1. 系统调用追踪：拆层测量 bpftime 多出的开销（跳板 / 保存寄存器 / 分发 / 程序），`syscount-nginx`、opensnoop 等工具审计。
2. 多线程性能（大课题，排在后面）：hash 查找不加锁、写锁拆分。
3. 分批向上游提交修复（per-CPU hash 崩溃、ARM64 跳板是较有分量的候选）。

---

## 待定

- 形式：和上次一样做成翻页幻灯片？语言（中文 / 英文 / 日文）？
- 重点取舍：内容较多，可能挑 3–4 条主线讲（建议：多维度的多线程结论、ARM64 跳板、真实工具兼容性），其余放附页。
