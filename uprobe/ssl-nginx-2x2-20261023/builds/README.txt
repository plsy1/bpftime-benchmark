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
7ed339ad      db3d475f + cached bpf_get_current_uid_gid with set*id interposers (candidate build-jit, LLVM 16)
bca2ebce      7ed339ad + cached producer pid/tid in software perf output (candidate build-jit, LLVM 16)
df865b3e      bca2ebce + uretprobe stack context, exit shm guard, fork-child pass-through, hash flags, JIT local call at pc 0, disabled failed JIT (candidate build-jit, LLVM 16)
2448aeaf      df865b3e + uretprobe CPU context read from the current gum invocation (on_leave keeps no TLS work) (candidate build-jit, LLVM 16)
f9d2d9fc      2448aeaf + frame-pointer user stacks on aarch64, kernel-style stack trace map, cgroup array + bpf_current_task_under_cgroup (candidate build-jit, LLVM 16)
layout-pad12  f9d2d9fc + 12 unused nops in bpf_helper.cpp (layout experiment only, not a commit)
layout-pad28  f9d2d9fc + 28 unused nops in bpf_helper.cpp (layout experiment only, not a commit)
4d9b8a61      f9d2d9fc + printk sink, ENOMEM on shm exhaustion (128 MB default), rate-limited runtime errors, fork-child inherited close, cgroup id helper, XPACI frame strip, agent work hidden from probes (candidate build-jit, LLVM 16)
a7d54d7e      4d9b8a61 + rate-limited logging out of line (candidate build-jit, LLVM 16)
mapusage      627a2201 + opt-in map usage counters (BPFTIME_ENABLE_MAP_USAGE_STATS=ON; array lookup JIT off; LLVM 15, no LTO). Counting only, not for timing.
09421773      dev branch HEAD (627a2201 + map usage counters compiled out); daily config: GCC 13, LLVM 15, RelWithDebInfo, LTO (candidate build-jetson-daily)
c98951a4      dev branch HEAD (09421773 + per-cpu hash: map lock on every operation, kernel update flags and size limit); daily config: GCC 13, LLVM 15, RelWithDebInfo, LTO (candidate build-jetson-daily)
9c3e0345      dev branch HEAD (1aa74c4c + hash map lookups take no lock: per-bucket generation, writer lock inside the implementation); daily config: GCC 13, LLVM 15, RelWithDebInfo, LTO (candidate build-jetson-daily)
cb3f29d2      dev branch HEAD (9c3e0345 + test fix + hash map writers lock stripes, per-CPU element tokens); daily config: GCC 13, LLVM 15, RelWithDebInfo, LTO (candidate build-jetson-daily)
stripe-tokens cb3f29d2 + element tokens held per stripe under its lock (no per-CPU slots, no sched_getcpu) (candidate.diff; candidate build-jetson-daily)
stripe-relaxed stripe-tokens + relaxed probe-bound loads + dmb ishst before writing a new key (candidate.diff; candidate build-jetson-daily)
stripe-view   stripe-tokens + shared vectors resolved to plain pointers once per operation (candidate.diff; candidate build-jetson-daily)
stripe-line   stripe-view + one-line stripe state holding the probe bound (11 tracked distances) (candidate.diff; candidate build-jetson-daily)
f4fdcbcd: dev/uprobe-official-177a1dc465ff commit f4fdcbcd (hash map: per-stripe tokens, one-line stripe state, resolved views); binaries identical to stripe-line.
hash-fmix     dev b9c849fb + hash map: MurmurHash3 finalizers for 4/8-byte keys, token batches (candidate.diff; candidate build-jetson-daily)
dev-b9c849fb  dev b9c849fb (review fixes A-E; B1 hash value alignment) (candidate build-jetson-daily)
hash-fmix4    dev b9c849fb + hash map: MurmurHash3 finalizer for 4-byte keys only, token batches (candidate.diff; candidate build-jetson-daily)
dev-8c108309  dev 8c108309 (all review fixes; candidate build-jetson-daily)
dev-7553abb8  dev 7553abb8 (second-review fixes; candidate build-jetson-daily)
cpucheck-unsigned dev f2d6ae7d's tree, built just before that commit (one unsigned CPU compare; candidate build-jetson-daily)
opt-identity  dev 7553abb8 + inline identity fast path (__thread), ringbuf owner slot from the tid (candidate.diff; candidate build-jetson-daily)
opt-identity2 opt-identity with the identity cache in one __thread struct (candidate.diff; candidate build-jetson-daily)
dev-f73a45f1  dev f73a45f1 (third-review fixes; candidate build-jetson-daily)
mix-s1        diagnostic: agent of 9aeb2863's tree with only ringbuf::submit from f73a45f1 (shows the cross-cluster ringbuf slowdown); server and bpftimetool of opt-identity2
dev-6ab9cbfe  dev 6ab9cbfe (ringbuf submit back to one exchange; review rounds ended; candidate build-jetson-daily)
dev-5b28dae5  dev 5b28dae5 (hash map home-stripe fast paths, inline spin locks, lookup acquire; candidate build-jetson-daily)
dev-68f19c1f  dev 68f19c1f (5b28dae5 + handler map lock out of line; candidate build-jetson-daily)
dev-e1677f0d  dev e1677f0d (68f19c1f + per-CPU hash on the striped table and a slot pool, no map lock; candidate build-jetson-daily)
dev-a4ac0d31  dev a4ac0d31 (e1677f0d + flatten of the ordinary hash's update/delete; candidate build-jetson-daily)
exp-flatten   experiment: same tree as dev-a4ac0d31 before the commit (built by target only)
dev-42595149  dev 42595149 (49bc1fd9 spin lock fix + hash map element count as one shared atomic, no tokens; candidate build-jetson-daily)
exp-llsc      experiment: 42595149 with the element count added by an ldxr/stxr loop (not committed)
dev-e5ba4aa7  dev e5ba4aa7 (no stack canary; 42595149 + hash map as preallocated elements in u32 chains, per-CPU free lists; candidate build-jetson-daily)
dev-b7ded27a  dev b7ded27a (e5ba4aa7 + delete unlinks before marking free, per-map hash seed, doc fixes; candidate build-jetson-daily)
exp-noseed    experiment: dev b7ded27a tree with the hash header of 199e23ec (delete fix, no seed) (candidate.diff; candidate build-jetson-daily)
exp-seed-noxor experiment: dev b7ded27a with the _seed member but no XOR for 4/8-byte keys (candidate.diff; candidate build-jetson-daily)
dev-f9b92cee  dev f9b92cee, never pushed, replaced by 518a6b99 (603c81f7 + updates of existing keys take a spare element; candidate build-jetson-daily)
exp-spare-stripe experiment: dev f9b92cee with one spare per stripe under its lock (no exchange), old head replaced by one head store (candidate.diff; candidate build-jetson-daily)
dev-518a6b99  dev 518a6b99 (603c81f7 + updates of existing keys take the stripe's spare element; binaries identical to exp-spare-stripe; candidate build-jetson-daily)
dev-7ad5a152  dev 7ad5a152 (d5279469 + updates put the new element in the old one's place, lookups re-check their link; candidate build-jetson-daily)
dev-ff5515d8  dev ff5515d8 (ea32ad8b + a thread's Gum context made inside an invocation before ignoring probes: memleak deadlock fix; candidate build-jetson-daily)
exp-c98951a4-gumfix experiment: c98951a4 (map code before the multi-thread work) + the ff5515d8 fix (cherry-picked, no CLAUDE.md), for real tools at 1-6 threads (jit-stack-validation-12 worktree, build-jetson-daily, daily cmake args)
dev-efe08cbd  dev efe08cbd (ff5515d8 + stack trace map: a stored trace is found without the map lock; candidate build-jetson-daily)
dev-25f4134f  dev 25f4134f (efe08cbd + Gum hooks never detached at run time: per-function slots, grace period, parking; candidate build-jetson-daily)
dev-6a15a271  dev 6a15a271 (f0954773 + ringbuf record tag keeps the map fd next to the owner slot: problem 9; with CLI and transformer; candidate build-jetson-daily)
dev-74d0ab8b  dev 74d0ab8b (6a15a271 + logger silenced and kept from exit onwards: problem 10 + libbpf-poisoned calls to a reporting helper: problem 11; with CLI and transformer; candidate build-jetson-daily)
exp-74d0ab8b-task158  experiment: dev-74d0ab8b's agent with helper 158 (bpf_get_current_task_btf) registered as a stub returning 0 (not committed); other files from dev-74d0ab8b
exp-74d0ab8b-missing  experiment: dev-74d0ab8b's agent with every call of an unregistered helper pointed at a stub returning -ENOSYS (not committed); other files from dev-74d0ab8b
dev-b60b99c3  dev b60b99c3 (74d0ab8b + map freeze recorded, calls of missing helpers in code frozen read-only constants make unreachable stubbed; with CLI and transformer; candidate build-jetson-daily)
  (dev-b60b99c3: the code of dev 94f9ba8f, which only adds CLAUDE.md lines)
fix-rbwake    working tree ce06c39c + ringbuf submit wakes a consumer that caught up (problem 13, not yet committed); with CLI and transformer; candidate build-jetson-daily
fix-rbwake2   working tree ce06c39c + ringbuf wakeup reading waiters before consumer_pos, M1 stale marker, M3 thunk slots, M4 per-CPU NOEXIST (not yet committed; candidate.diff); with CLI and transformer; candidate build-jetson-daily
exp-rbwake-nowake  experiment: fix-rbwake2 with the agent's ringbuf notify fencing and loading waiters but never waking (diagnosis only; server and tool from fix-rbwake2)
fix-final     dev d46ceb8f (94f9ba8f + the fifth review's fixes: H1, H2, ringbuf wakeup, M1-M4, lows L1-L10, docs; 15 commits); with CLI and transformer; candidate build-jetson-daily
fix-final-probecheck  fix-final (dev d46ceb8f) built with ENABLE_PROBE_READ_CHECK=ON and ENABLE_PROBE_WRITE_CHECK=ON (otherwise the daily cmake args, no unit tests; candidate build-probecheck); bpftimetool copied from fix-final (more-dims-20261106/n1-probe-read)
dev-f4d505b3  dev f4d505b3 (d46ceb8f + ringbuf: a consumer at the end padding has caught up; improve-20261106 item 1); with CLI and transformer; candidate build-jetson-daily (checks off)
dev-a48f6777-probecheck  dev a48f6777 (f4d505b3 + checked string reads in one system call per chunk, zeroed destination on faults; improve-20261106 item 2) built with ENABLE_PROBE_READ_CHECK=ON and ENABLE_PROBE_WRITE_CHECK=ON (daily cmake args otherwise; candidate build-probecheck); transformer from build-jetson-daily
dev-f5b388cd-probecheck  dev f5b388cd (a48f6777 + perf output wakes the consumer as its perf_event_attr asks; improve-20261106 item 3) built with probe read/write checks ON (daily cmake args otherwise; candidate build-probecheck); transformer from build-jetson-daily
dev-2a0ed6ff-probecheck  dev 2a0ed6ff (f5b388cd + reserve lock inline (26e7c994) + one handler read per reserve and submit; improve-20261106 item 4a+4c) built with probe read/write checks ON (candidate build-probecheck); transformer from build-jetson-daily
dev-d08ac155-probecheck  dev d08ac155 (2a0ed6ff + the reserve lock on a cache line of its own; item 4b) built with probe read/write checks ON (candidate build-probecheck); transformer from build-jetson-daily
dev-8a87aaed-probecheck  dev 8a87aaed (d08ac155 + reserve reads positions from the lock's line (cached consumer position); item 4d) built with probe read/write checks ON (candidate build-probecheck); transformer from build-jetson-daily
dev-b6da99bf-probecheck  dev b6da99bf (8a87aaed + a reserve waiter yields after spinning a while; item 4e) built with probe read/write checks ON (candidate build-probecheck); transformer from build-jetson-daily
dev-0f9c3654-probecheck  dev 0f9c3654 (b6da99bf + perf shard sweeps bounded and overflow shard (5e7eaf9f), agent looks for the segment once (dc10a160), Frida resolves each module once (0f9c3654); improve-20261106 items 6a-c, 5a-b) built with probe checks ON (candidate build-probecheck); transformer from build-jetson-daily
exp-0f9c3654-poll  experiment (not committed): dev 0f9c3654 + bpftime_epoll_wait polls for 200 us (BPFTIME_CONSUMER_POLL_US) after it last found events before sleeping on the futex; checks ON; improve-20261106 item 3 investigation
exp-0f9c3654-poll2  experiment (not committed): exp-0f9c3654-poll with sched_yield between polls instead of 64 yield instructions (exp-poll2.diff); checks ON
exp-0f9c3654-nap  experiment (not committed): dev 0f9c3654 + bpftime_epoll_wait naps 100 us (BPFTIME_CONSUMER_NAP_US) unregistered while it found events within the last 2 ms (BPFTIME_CONSUMER_NAP_WINDOW_US), then registers and sleeps as before (exp-nap.diff); checks ON
exp-0f9c3654-nap2  experiment (not committed): exp-0f9c3654-nap with the nap a futex wait registered in nappers: producers wake nappers only when a buffer fills (perf watermark or drop, a ring half full) or on BPF_RB_FORCE_WAKEUP (exp-nap2.diff); checks ON
exp-0f9c3654-nap3  experiment (not committed): exp-0f9c3654-nap2 with ring buffers keeping bpf's caught-up rule for napping consumers too (exp-nap3.diff); checks ON
dev-3f8981e4-probecheck  dev 3f8981e4 (d46ceb8f + improve-20261106 items 1-7: f4d505b3 .. 3f8981e4, 14 commits) built with probe read/write checks ON (daily cmake args otherwise; candidate build-probecheck), transformer from the same build
  (fix-final-probecheck: libbpftime-agent-transformer.so copied from fix-final on 11-06 23:55 for improve-20261106/final's opensnoop runs; the transformer has no probe read code)
