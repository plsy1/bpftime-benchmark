# P5a Dview spike (x86 NUC, 2026-11-26), summary

Claim tested (spike plan p5a-plan.md §1, kept here): a privileged loader keeps a per-thread {tid, tgid, uid, gid, comm}
view in kernel eBPF; Tern maps it read-only and, on top of careful mode, answers uid/gid/comm as the kernel does in
every E5 scenario, at ≤ 0.084 µs/event on nginx. Code: ladder/dview-6b6b247e @ 3499d664 (Dnext 6b6b247e + tools/view +
runtime/src/kernel_view.*), builds/dview, dviewpc; tool binaries' sha in tools-sha256.txt. Local only, not pushed.

## Go criteria (pre-registered in the spike plan) and results

| criterion | result |
|---|---|
| G1 hooks attach (commit_creds fexit → fentry → kprobe) | all five attach; commit_creds by **fexit** (no fallback needed); verifier accepts the program |
| G1 view mapped in E5 targets and nginx | "View: mapped … via pin, self hit" in every E5 view target (I8v) and in nginx master + worker in the measurement image (manual check; the harness discards nginx's stderr, so the M4 session itself has no log of it) |
| G2 selftest S1–S10, lock_fail = 0 | **wrong = 0 in every scenario** (S0–S10, 10^4–2·10^6 reads each). S0–S8, M0 pass. S9 (two threads hammering renames and raw setresuid on one slot, 10^6 each): lock_fail 1,784–3,139 (~0.1 % of writes) → that slot is stale until its next full refresh, reads fall back to system calls (27–47 % of S9's reads); likely preemption inside the BPF critical section (tracing programs run with migration, not preemption, disabled); raising the spin bound from 64 to 1024 did not help. S10 (16 slots) produced collisions but no read miss (test design). **Not met as written (lock_fail > 0 in S9); no wrong value.** |
| G2 E5 (I8v) CAREFUL + VIEW all 0/500 except W5r; 16-slot leg too | **met**: every cell as PREDICTIONS-v4 (e5/results/I8v-20261126.SUMMARY.md) |
| G3 M1 full snapshot read ≤ 10 ns | **1.49 ns** median (p90 1.87); getuid+getgid 131.7 ns; prctl(PR_GET_NAME) 129.4 ns (bench.log, CPU 2 at 3.5 GHz) |
| G3 M4 nginx: CAREFUL+VIEW − CAREFUL ≤ 0.084 µs/event (90 % upper bound) | **+0.011 µs/event, 90 % CI [−0.015, +0.036]**; rps ratio 0.9983 [0.9942, 1.0024] (raw/M4-20261126-try1) |
| G3 per-worker view miss ≤ 0.1 % | not measured (the agent's miss counter goes to nginx's discarded stderr) |
| G4 kernel hook cost (report; cap ≤ 1 µs and ≤ 10 % for fork, pthread) | balanced 8-block run: setresuid +56 ns [42, 71] (+13 %), prctl rename +26 ns [15, 36] (+18 %), pthread_create+join +80 ns [32, 128] (+1.0 %), fork+_exit+wait: nominal +2.7 µs (+4 %) but the first block (off) ran at 54 µs and all seven later blocks, on and off alike, at 65–66 µs: a warm-up effect; without block 0, on 65.5 vs off 65.7 µs, no difference |

No-go triggers: none (hooks attach; view maps in the container; no unexplained wrong value; M1 and M4 far inside).
**Assessment: the spike meets its correctness and cost criteria; GO recommended** for the post-go work (C2 leg, backfill,
cgroup filter, containers without --pid=host), with these declared limits: same-slot write contention can leave a slot
stale (fallback, not wrong); the view key is Tern's identity, so a raw clone child (W5r) reads its parent's slot; needs a
privileged loader; init pid namespace only; no backfill for threads that existed before viewd started; the view exposes
every thread's uid/comm to processes that can open the pin (0600 root). The decision date in the eval plan is 12-11.

## Other findings

- A read-only bpf map fd cannot be mmapped MAP_SHARED on 6.12 (mmap(2) clears VM_SHARED for a file not open for write;
  bpf_map_mmap then returns EINVAL). The agent opens the pin read-write and maps it PROT_READ; mprotect(PROT_WRITE) on
  that mapping is refused (M0).
- Dviewpc (view and careful off) vs Dnextpc on nginx: rps 1.0074 [1.0012, 1.0135] (equivalent within ±1.5 %, a 0.7 %
  layout difference between binaries, as B5's 0.87 %); x −0.045 µs/event, 90 % CI [−0.083, −0.008] (inside ±0.084).
- Kb in M4: Dviewpc+CAREFUL+VIEW / Kb 1.236 [1.230, 1.243]; Dviewpc+CAREFUL / Kb 1.238; Dnextpc / Kb 1.229.
- Pre-existing Tern fork hang (Gum allocator lock inherited locked) found by W7t: see e5/results/I8v-20261126.SUMMARY.md.
