# 根因报告：bpftime 在 ARM（Jetson）上为什么输给 kernel eBPF

一页纸总结。详细数据与方法见文末索引。

## 现象

Jetson Orin 上 ssl-nginx benchmark（nginx + wrk，比较 baseline / kernel eBPF
sslsniff / bpftime sslsniff 吞吐）：bpftime 全 payload 落后 kernel eBPF
3.8%～12.6%，且方差偏大、偶发双峰；而同一软件在 x64 上大幅领先 kernel。

## 根因：三层，逐层剥出

### 第 0 层（测量假象）：software perf record 未按 8 字节对齐

record header 可落在环缓冲最后一字节，libbpf 读到跨界 `size=0` → 消费者空转、
隐藏丢事件、吞吐双峰（CV 高达 16.6%），并使早期数据中 bpftime 的"高吞吐"
部分为丢事件换来的假象。**这不是性能问题，是正确性 bug**（另有共享内存残留
加剧双峰）。修复（`0fcdb0e`）后测量才可信：bpftime 稳定落后 kernel——
真正的性能问题这才显形。

### 第 1 层（主因，~72%）：`bpf_perf_event_output` 每事件的 affinity 绑核序列

每个 perf 事件执行 getcpu + getaffinity + 2×setaffinity（Jetson 实测序列
3.1 µs 无竞争、6.7 µs 负载下；单次 setaffinity ~1.4 µs = 普通 syscall 的 5 倍，
贵在内核改 cpumask 的簿记而非陷入本身）。经代码走读证明该序列**无任何保护
作用**：cpu 值在绑核前已快照、ring 写入走 per-(pid,tid) shard、绑核不是锁；
还附带两个子 bug（错误路径漏恢复 mask 会把被追踪线程永久钉死单核；restore
使用陈旧 mask 与应用自身 setaffinity 竞态——二者也是方差偏大的来源）。
消融定量：此序列独占 event-output 段成本的 72%（+12.6 pp）。修复 = 直接删除
（`076e3e4`）。

### 第 2 层（次因）：`bpf_probe_read_user` 每次调用重装 SIGSEGV handler

"是否需要安装信号处理器"的判断读了**未初始化局部变量**（UB，首次调用后
恒真），导致每次 probe_read 多 2 个 `rt_sigaction` syscall。修复 = thread_local
安装标志（`ead56c9`）。

## 为什么偏偏 ARM 翻车、x64 没事

统一框架：**kernel uprobe 是 trap 计价**（每命中 2+ 次内核异常，成本对环境
敏感：虚拟化 x86 + 安全缓解下极贵，裸机 ARM 反而便宜）；**bpftime 本应是
代码计价**（纯用户态，开销与业务代码同比例缩放）。但上述两处 bug 给 bpftime
的热路径**混入了每事件 6 个冗余 syscall**，使它部分退化为 trap 计价——
在每请求预算小（低主频 + 单 nginx worker，~60 µs/请求）、事件数随 payload
增长（`ssl_buffer_size` 16KB 分块，256KB ≈ 17 事件/请求）的 Jetson 上，
这笔账足以吃光 bpftime 在 probe 分发上的结构性优势（免陷入，比 kernel 便宜
8.4 pp）并倒亏；而 x64 上 kernel uprobe 自身太贵（impact 47%～62%），bpftime
拉着同样的手刹也照样领先，问题因此长期隐身。
（注：手刹在 x64 同样昂贵——同 VM A/B 实测删除后 x64 吞吐 +37%～41%，
"仅 ARM 受害"的最初判断只对了"谁先露馅"。）

## 修复后状态

| 口径 | 修复前 | 修复后 |
|---|---:|---:|
| Jetson fair（等范围挂载，30 样本/腿） | −9.57%（16B） | **+7.58%（16B）/ +9.81%（256KB）** |
| Jetson 方差 | CV 8.7–16.6%（双峰） | 1–3%，与 kernel 同水平 |
| x64 同 VM A/B（vs kernel） | +42.6%（16B） | **+96.8%（16B）/ +60.6%（256KB）** |
| payload 趋势 | 越大越差 | 越大优势越大（事件数放大镜反向工作） |

## 一句话

**bpftime 的先天优势是把 tracing 从 trap 计价改成代码计价；它在 ARM 上输，
不是因为 ARM 慢，而是自己的热路径里藏着每事件 6 个无效 syscall（绑核 4 个 +
重装信号处理器 2 个），把优势全部漏光——拔掉之后，两个平台全面反超 kernel。**

## 详细文档索引（summry 分支）

- 消融方法与全部数据：`archive/bpftime-official-no-btf-ssl-nginx-path-ablation-20260726.md`
- 绑核为何冗余（代码级论证）：`archive/bpftime-perf-event-output-affinity-redundancy-20260727.md`
- 对齐 bug 定位与修复：`archive/bpftime-software-perf-record-alignment-fix-20260722.md`
- 修复清单与验证：`bpftime-official-no-btf-change-log.md`
- 操作手册：`bpftime-official-no-btf-ssl-nginx-benchmark-runbook.md`
