official      177a1dc unpatched (consuming-official-vs-optimized-20261021/official-artifacts)
official-noaff 177a1dc + no-affinity.patch only (/home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023/build-jit)
opt-aff       72aebadd: patch stack incl. map optimizations, affinity pinning kept
opt-noaff     20870a03: 72aebadd + affinity pinning removed
sslsniff      built from /home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023/example/tracing/sslsniff (same source as July)
opt-noaff-fork a90fa447: 20870a03 + fork children execute inherited array JIT programs (built from candidate build-jit, LLVM 16)
lost-wakeup   a90fa447 + kernel-style PERF_RECORD_LOST/-ENOSPC + futex consumer wakeup (candidate build-jit, LLVM 16; candidate.diff)
lost-report   lost-wakeup with notify_perf_event_consumers() stubbed out: losses reported, consumer still polls every 1 ms (candidate.diff)
lost-watermark lost-wakeup but the producer wakes the consumer only when its shard is >= half full after publishing or a sample was dropped (candidate.diff)
db3d475f      a90fa447 + perf loss reporting and half-full consumer wakeup (hard links to lost-watermark, same files)
