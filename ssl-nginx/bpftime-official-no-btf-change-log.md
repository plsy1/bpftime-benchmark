# bpftime official-no-btf bug 修复记录

分支 `codex/official-no-btf`（远端 plsy1/bpftime-benchmark）。

## 已修复

| # | Commit | 日期 | 文件 | Bug | 修复 | 验证 |
|---|---|---|---|---|---|---|
| 1 | `0fcdb0e` | 07-22 | `runtime/src/handler/perf_event_handler.cpp` + 单测 | software perf record 未按 8 字节对齐：record header 可落在环最后一字节，libbpf 读到跨界 `size=0` → 消费者空转、producer shard 填满、隐藏丢事件、吞吐双峰（CV 8.7–16.6%） | record 总长向上对齐 8B 并写入 `header.size`、padding 清零、u16 上限拒绝、消费侧拒绝非对齐记录、新增环边界回归测试 | 单测 8,358 断言；reader task-clock/指令 −95.5%/−96.1%；drop 0%；七档 CV 14.2%→2.6%；2026-07-27 代码复核确认（含不变量归纳） |
| 2 | `076e3e4` | 07-27 | `runtime/src/bpf_helper.cpp` | `bpf_perf_event_output` 每事件执行 affinity 绑核序列：无任何保护作用（cpu 值先于绑核已快照、ring 写入走 per-(pid,tid) shard、绑核非锁），Jetson 上 3.1–6.7 µs/事件 = event-output 段成本的 72%；附带两个子 bug——错误路径（原 `:520/:532`）漏恢复 mask 将被追踪线程永久钉死单核、restore 用陈旧 mask 与应用自身 setaffinity 竞态 | 无条件删除绑核序列，cpu 值保留 `my_sched_getcpu()` 快照（rseq，3.5 ns） | 单测通过；16B 短测 bpftime 由 −8% 变 +8.8% vs kernel；**fair 口径 3 轮 ×2 payload：16B +7.58% / 256KB +9.81%，六轮全同向**；事件守恒 2.001/请求零丢失；CV 收敛至 kernel 水平；**x64 同 VM A/B（run 30217974408）：删除带来 +37~41% 吞吐，kernel 腿 Δ≤0.7% 证明控制干净——跨平台收益** |
| 3 | `ead56c9` | 07-27 | `runtime/src/bpf_helper.cpp` | `probe_read`/`probe_write_user` 的"是否需装 SIGSEGV handler"判断读**未初始化局部变量**（UB，首个调用后恒真）→ 每次调用重装 handler，热路径每次多 2 个 `rt_sigaction` syscall | `thread_local bool *_handler_installed` 标志替换坏判断（read/write 两路径） | 单测（既有失败 23/24 不变，A/B 确认与本改动无关）；单样本 16B：bpftime impact 24.5%→22.9%（n=1 方向性，定量待 10-rep） |

## 已知未修

严重度：🔴 静默错误结果 / 🟡 性能或行为偏差 / ⚪ 无运行时影响（整洁性）。
下表每条均经代码走读 + 独立对抗式复核（2026-07-27）确认，并附实际触发面。

| 严重度 | Bug | 精确表述与影响 |
|---|---|---|
| 🔴 | `per_cpu_hash_map::elem_delete` 语义错误<br>`per_cpu_hash_map.cpp:96-107` | eBPF 程序对 PERCPU_HASH 调 `bpf_map_delete_elem` 时：`fill` 清的是切片 `0..cpu-1`（**别的 CPU 的数据**），本 CPU 数据保留；entry 从不 `erase`；key 不存在也返回 0。内核语义应为整条 entry 删除 + 未命中返回 `-ENOENT`，故修法是 `impl.erase(itr)` 而非调整 fill 区间。后果：静默数据损坏（日志仅 DEBUG）、delete 后 lookup 仍非空、槽位不释放。syscall 侧（`elem_delete_userspace`）实现正确、不补偿此路径。该路径零测试覆盖，常见示例用不到 PERCPU_HASH+程序内 delete，属潜伏 bug |
| 🟡 | `bpf_perf_event_output` 忽略 `flags` | `BPF_F_INDEX_MASK` 显式索引被当作 current-cpu 处理 |
| 🟡 | ring 满时静默丢弃<br>`perf_event_handler.cpp:385-386` | `output_data` 忽略 `append_sample` 返回值恒返回 0；全 runtime 无 `PERF_RECORD_LOST`，丢弃对消费者不可见 |
| 🟡 | aarch64 syscall tracing 静默构建失败 | `text_segment_transformer` 蹦床未实现（仅 TODO 注释）却构建成功，产出带未定义符号的 `.so`，运行时才失败；反汇编器写死 `CS_ARCH_X86` |
| 🟡 | `attach_at` 互斥检查不对称<br>`frida_uprobe_attach_impl.cpp:75-79` | 只查 `has_override()`，`has_uprobe_or_uretprobe()` 定义了却无调用者。同地址"先 uprobe 后 override"→ override **静默 no-op**（返回正常 attach id 但程序永不执行）；且该幽灵条目使 `has_override()` 从此为真，**污染该地址**：后续合法 uprobe 被误拒 `-EEXIST`。反方向仅 DEBUG 日志，双向静默 |
| 🟡 | 定时 perf event 周期快 1000 倍<br>`perf_event_array_kernel_user.cpp:453` | `sample_period = duration_ms*1000` 但 CPU_CLOCK 单位为 ns：名义 10ms 实际 **10µs**（恰为内核 hrtimer 下限，无法更快）。仅影响 kernel-user 共享 perf array 路径（daemon / RUN_WITH_KERNEL）；内核采样节流封顶开销，且事件 task-bound，故为性能偏差非正确性问题 |
| 🟡 | `handle_mmap64` 兜底路径重复 mmap<br>`runtime/syscall-server/syscall_context.cpp:890-892` | 三类 mock 均不命中时连调两次 `orig_mmap64_fn`，第一次返回值丢弃。无 `MAP_FIXED` 时两段落在不同地址 → 第一段永久残留（泄漏虚拟地址空间 + VMA，非 RSS）；`MAP_FIXED` 或首次失败时不泄漏。仅注入了 syscall-server 的加载端进程受影响，量级为每进程几十~几百次 |
| ⚪ | `ensure_on_certain_cpu` 泛型主模板是死代码<br>`map_common_def.hpp:33-49` | `func(currcpu)` 对零参 `std::function<T()>` 传参，**定义体一旦被实例化即编译失败**（T=void 走显式特化不受影响）；且 `currcpu` 初始化后从不赋值，快路径永假且语义错误。全仓仅单测以显式 `<void>` 调用，生产无调用者 → 运行时影响为零，但为未来维护者埋了编译期陷阱，建议整段删除 |
| ⚪ | `*probe*` 单测既有失败 | mocked `get_global_attach_ctx` 异常，23/24；与本轮三个修复无关（A/B 已确认） |

## 更新记录

- 2026-07-27：首版（三项修复 + 未修清单）。
- 2026-07-27：按要求精简为纯 bug 记录（移除基础设施与非修复类提交）。
- 2026-07-27：源码深读新增 5 条待修项，全部经独立对抗式复核确认（含表述纠正与实际影响分级）；"已知未修"表改为带严重度与精确影响的形式。
