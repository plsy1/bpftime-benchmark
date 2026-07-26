# bpftime official-no-btf bug 修复记录

分支 `codex/official-no-btf`（远端 plsy1/bpftime-benchmark）。

## 已修复

| # | Commit | 日期 | 文件 | Bug | 修复 | 验证 |
|---|---|---|---|---|---|---|
| 1 | `0fcdb0e` | 07-22 | `runtime/src/handler/perf_event_handler.cpp` + 单测 | software perf record 未按 8 字节对齐：record header 可落在环最后一字节，libbpf 读到跨界 `size=0` → 消费者空转、producer shard 填满、隐藏丢事件、吞吐双峰（CV 8.7–16.6%） | record 总长向上对齐 8B 并写入 `header.size`、padding 清零、u16 上限拒绝、消费侧拒绝非对齐记录、新增环边界回归测试 | 单测 8,358 断言；reader task-clock/指令 −95.5%/−96.1%；drop 0%；七档 CV 14.2%→2.6%；2026-07-27 代码复核确认（含不变量归纳） |
| 2 | `076e3e4` | 07-27 | `runtime/src/bpf_helper.cpp` | `bpf_perf_event_output` 每事件执行 affinity 绑核序列：无任何保护作用（cpu 值先于绑核已快照、ring 写入走 per-(pid,tid) shard、绑核非锁），Jetson 上 3.1–6.7 µs/事件 = event-output 段成本的 72%；附带两个子 bug——错误路径（原 `:520/:532`）漏恢复 mask 将被追踪线程永久钉死单核、restore 用陈旧 mask 与应用自身 setaffinity 竞态 | 无条件删除绑核序列，cpu 值保留 `my_sched_getcpu()` 快照（rseq，3.5 ns） | 单测通过；16B 短测 bpftime 由 −8% 变 +8.8% vs kernel；**fair 口径 3 轮 ×2 payload：16B +7.58% / 256KB +9.81%，六轮全同向**；事件守恒 2.001/请求零丢失；CV 收敛至 kernel 水平；**x64 同 VM A/B（run 30217974408）：删除带来 +37~41% 吞吐，kernel 腿 Δ≤0.7% 证明控制干净——跨平台收益** |
| 3 | `ead56c9` | 07-27 | `runtime/src/bpf_helper.cpp` | `probe_read`/`probe_write_user` 的"是否需装 SIGSEGV handler"判断读**未初始化局部变量**（UB，首个调用后恒真）→ 每次调用重装 handler，热路径每次多 2 个 `rt_sigaction` syscall | `thread_local bool *_handler_installed` 标志替换坏判断（read/write 两路径） | 单测（既有失败 23/24 不变，A/B 确认与本改动无关）；单样本 16B：bpftime impact 24.5%→22.9%（n=1 方向性，定量待 10-rep） |

## 已知未修

| Bug | 说明 |
|---|---|
| `bpf_perf_event_output` 忽略 `flags` | `BPF_F_INDEX_MASK` 显式索引被当 current-cpu 处理 |
| ring 满时静默丢弃 | `output_data` 恒返回 0，且全 runtime 无 `PERF_RECORD_LOST`，丢弃对消费者不可见 |
| aarch64 syscall tracing 静默构建失败 | `text_segment_transformer` 蹦床未实现但构建成功（.so 带未定义符号，运行时才炸）；反汇编器写死 `CS_ARCH_X86` |
| `per_cpu_hash_map::elem_delete` 清零范围疑似有误 | fill 的是 `[begin, begin+cpu*value_size)` 而非本 cpu 切片，未验证 |
| `*probe*` 单测既有失败 | mocked `get_global_attach_ctx` 异常，23/24，与上述修复无关 |

## 更新记录

- 2026-07-27：首版（三项修复 + 未修清单）。
- 2026-07-27：按要求精简为纯 bug 记录（移除基础设施与非修复类提交）。
