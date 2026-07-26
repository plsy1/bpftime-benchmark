# 上游 issue 草稿（eunomia-bpf/bpftime）

状态：待 master 验证 run（30221575655 / 30221579383）出数后填入 `<MASTER-*>` 占位符即可发布。
发布顺序建议：先 Issue 1（主发现），隔天 Issue 2；对齐 bug 单独成篇后续再发。

---

## Issue 1

**Title:** `bpf_perf_event_output`: per-event `sched_setaffinity` pinning provides no protection and dominates tracing overhead (removing it: up to +40% throughput)

```markdown
## Summary

`bpf_perf_event_output` pins the calling thread to its current CPU around
every event (`sched_getaffinity` + 2× `sched_setaffinity`, at
`runtime/src/bpf_helper.cpp:503-552` on current master). We traced a large
uprobe-tracing performance regression to this sequence, verified by code
reading that it provides no correctness guarantee, and measured large
throughput gains on both arm64 and x86-64 from removing it.

## Why the pinning has no effect

1. **The per-CPU map lookup key is snapshotted *before* the pin.**
   `current_cpu = my_sched_getcpu()` (bpf_helper.cpp:497) is captured first
   and used as the lookup key (:527); migrating later does not change the
   lookup. Pinning never affects which per-CPU slot is used.
2. **The ring write goes to a per-(pid, tid) producer shard, not a per-CPU
   buffer.** `software_perf_event_data::output_data` →
   `get_current_thread_shard().buffer.output_data(...)`
   (runtime/src/handler/perf_event_handler.cpp). The same thread writes the
   same shard regardless of which CPU it runs on; no cross-thread writer
   exists for a shard.
3. **Pinning is not a lock.** Two threads may pin concurrently (even to the
   same CPU); if shared state needed mutual exclusion, pinning would not
   provide it.
4. For `BPF_MAP_TYPE_KERNEL_USER_PERF_EVENT_ARRAY`, CPU attribution is
   decided kernel-side by the transporter program (`BPF_F_CURRENT_CPU`), so
   userspace pinning is irrelevant on that path too.

Empirically, with the pinning removed we observe zero event loss
(2.001 events per request, matching 2 × completed wrk requests) and zero
nginx-worker migrations over 200k events.

## Two additional bugs in the same code

- The error paths at :520 and :532 return **without restoring the original
  mask**, leaving the traced application thread permanently pinned to one
  CPU after a transient map error.
- The restore at :552 writes back a mask captured at event entry; it races
  with the application's own `pthread_setaffinity_np`/`taskset` and can
  silently revert it.

## Measured impact (ssl-nginx benchmark: nginx + wrk, sslsniff uprobes)

`sched_setaffinity` costs ~1.4 µs/call on Jetson Orin (5× a plain syscall;
the cost is the cpumask bookkeeping, not the trap), i.e. 3–7 µs per event
on the nginx worker's critical path.

| Platform | Scope | bpftime vs kernel eBPF, before | after removal |
|---|---|---:|---:|
| Jetson Orin (arm64, bare metal) | equal-scope attach, 3 rounds × 30 samples | **−9.6%** (16B) | **+7.6%** (16B) / **+9.8%** (256KB) |
| x86-64 (hosted VM), same-VM A/B | stock scope | +42.6% (16B) | **+96.8%** (16B); bpftime throughput itself **+37–41%** |
| current master (<MASTER-SHA>), arm64 runner | stock scope | <MASTER-ARM-BEFORE> | <MASTER-ARM-AFTER> |
| current master (<MASTER-SHA>), x64 runner | stock scope | <MASTER-X64-BEFORE> | <MASTER-X64-AFTER> |

In the same-VM A/B the kernel-eBPF leg differed by ≤0.7% between the two
builds (it does not contain the changed code), confirming the control.
bpftime's variance also converges to the kernel leg's level once the
per-event affinity churn is gone.

## Proposed fix

Delete the pin/restore sequence and keep the existing `my_sched_getcpu()`
snapshot (rseq-backed, ~3.5 ns). The patch cherry-picks cleanly onto
current master: <FIXED-BRANCH-LINK>. Happy to open a PR if this analysis
looks right to you.

Full benchmark data and methodology:
https://github.com/plsy1/bpftime-benchmark (branches `summry/jetson`,
`benchmark-results/jetson`).
```

---

## Issue 2

**Title:** `bpftime_probe_read`/`probe_write_user` reinstall the SIGSEGV handler on every call (uninitialized-variable test; 2 extra `rt_sigaction` syscalls per call)

```markdown
## Summary

With `ENABLE_PROBE_READ_CHECK`, the "do we need to install the handler"
test at `runtime/src/bpf_helper.cpp:181` (and :268 for the write path)
compares against `original_sa.sa_sigaction`, but `original_sa` is a local
variable that is only filled in on the very first call (inside the
`exist_read == NOT_CHECKED` branch, :171-183). Every subsequent call reads
it **uninitialized** (undefined behavior); the comparison effectively
always fails, so the SIGSEGV handler is reinstalled — two `rt_sigaction`
syscalls — on **every** `bpf_probe_read`/`bpf_probe_write_user` invocation
on the tracing hot path.

## Fix

Track installation with an explicit `thread_local bool` (one per path).
The handler genuinely needs to be installed once per thread; the fix
preserves behavior exactly and removes the UB read. Cherry-picks cleanly
onto master: <FIXED-BRANCH-LINK>.

## Impact

Same pattern as the affinity pinning issue (#<ISSUE1>): redundant
per-event syscalls on the hot path. In a quick ssl-nginx sample the
bpftime throughput impact drops from 24.5% to 22.9% of baseline with the
fix; happy to provide more rigorous numbers on request.
```

---

## 后续再发（占位）

- **对齐 bug（最重要的正确性问题）**：master 的 software perf record 仍未按
  8 字节对齐（record header 可跨环边界 → libbpf 读到 size=0 → 消费者空转 +
  隐藏丢事件 + 双峰吞吐）。需要把 `0fcdb0e` 手工移植后连 patch 一起发。
- `bpf_perf_event_output` 忽略 `flags`（`BPF_F_INDEX_MASK`）。
- ring 满静默丢弃、全 runtime 无 `PERF_RECORD_LOST`。
- aarch64 `text_segment_transformer` 蹦床未实现但静默构建成功（.so 带未定义
  符号、capstone 写死 `CS_ARCH_X86`）——建议加 `#error` 或 CMake 平台判断。
