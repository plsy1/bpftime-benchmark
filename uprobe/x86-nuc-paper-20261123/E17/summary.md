# E17: official bpftime at upstream master 70f7bc1f (O-latest), x86 NUC

Session E17-olatest-20261124-try1, 2026-11-24 14:17–15:00 JST. 168 rows, all valid, exit 0. 16 B payload, policy M unless marked `#S` (nginx on one core).

## Builds

| leg | source | checks |
|---|---|---|
| O | official 4708f409 (2026-11-08), the paper's O (frozen `builds/official`) | off |
| Opc | same | on |
| Ol | upstream eunomia-bpf/bpftime master 70f7bc1f (2026-11-23), local branch `ladder/olatest-70f7bc1f` | off |
| Olpc | same | on |

Ol and Olpc use the same `build-arm.sh` command as official: RelWithDebInfo, LTO, gcc-13, LLVM 19, SPDLOG_ACTIVE_LEVEL=INFO, CUDA off. The only cmake-options differences are BPFTIME_BUILD_EXECUTABLE, BPFTIME_BUILD_KERNEL_BPF, BPFTIME_ENABLE_CCACHE and BPFTIME_VERBOSE_OUTPUT, which upstream #866 removed as non-functional (no CMake condition consumed them), and the new BPFTIME_ENABLE_FUZZING=OFF. Build provenance is in `builds/`. The sslsniff binaries are the frozen paper ones.

Why: after the paper base, upstream merged #836 (pin removed), #837 (tid cache), #812 (pid_tgid cache), #834 (comm cache), #835 (probe read-ahead), #730 (per-CPU lock around every program execution) and more. See paper-drafts/upstream-drift-20261124.md.

## Round 0: throughput (10 reps, rotated, paired)

| ratio | GMR [95% CI] |
|---|---|
| Ol / K | **1.3347 [1.3294, 1.3400]** |
| Ol / K_wb32 | 1.1777 [1.1719, 1.1836] |
| Olpc / K | 1.2352 [1.2248, 1.2457] |
| Olpc / K_wb32 | 1.0899 [1.0814, 1.0986] |
| O / K | 1.0423 [1.0380, 1.0466] (E2o: 1.039) |
| O / K_wb32 | 0.9197 [0.9155, 0.9239] |
| Opc / K | 0.9871 [0.9801, 0.9942] |
| Opc / K_wb32 | 0.8710 [0.8652, 0.8769] |
| Ol / O | 1.2806 [1.2746, 1.2866] |
| Olpc / Opc | 1.2513 [1.2391, 1.2637] |
| K_wb32 / K | 1.1333 [1.1291, 1.1375] (E15: 1.1318) |
| Ol#S / K#S | 1.3244 [1.3073, 1.3418] |
| O#S / K#S | 1.1661 [1.1609, 1.1713] (E2o: 1.169) |

TOST Ol vs Ol#S, margin ±1.5%: ratio 1.0087, 90% CI [1.0007, 1.0168], so equivalence is NOT shown. The policy difference for upstream is below 1.7%; for O it is 1.169/1.039, about 12%.

Paired extra time per event (per request / 2):
- x(K) − x(Ol) = 2.162 µs [2.131, 2.192]
- x(K_wb32) − x(Ol) = 1.148 µs [1.113, 1.183]
- x(O) − x(Ol) = 1.812 µs [1.781, 1.843]
- x(Olpc) − x(Ol) = 0.520 µs [0.468, 0.573]: upstream's probe-read check
- x(Opc) − x(Olpc) = 1.754 µs [1.679, 1.829]

All bpftime legs deliver every event: del/exp ≥ 0.99977, and the differences are tail events still in flight at stop. Lost = 0.

## Round 1: system calls per request on the nginx worker (perf stat, 3 reps, medians)

| leg | all | getuid | getgid | getpid | gettid | sched_getaffinity | sched_setaffinity | process_vm_readv |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| B | 8.02 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| K@S4 | 11.02 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| O@S4 | 30.01 | 6 | 6 | 2 | 2 | 2 | 4 | 0 |
| Ol@S1 | 8.02 | 0 | 0 | 0.01 | 0 | 0 | 0 | 0 |
| Ol@S4 | 22.01 | 6 | 6 | 2 | 0 | 0 | 0 | 0 |
| Olpc@S4 | 24.01 | 6 | 6 | 2 | 0 | 0 | 0 | 2 |

Upstream makes 14 extra system calls per request (7 per event) instead of O's 22: getuid 6, getgid 6, getpid 2. Code reading predicted exactly this. The pin (#836) and gettid (#837) are gone.

## Round 2: hook-only stage S1 (10 reps)

- x(Ol@S1) − x(O@S1) = 0.106 µs per event [0.037, 0.175]. Upstream's hook stage is about 370 cycles slower at 3.5 GHz. The likely cause is #730's per-CPU interprocess mutex around every program execution, though this was not isolated.
- x(K@S1) − x(Ol@S1) = 1.805 µs per event. The hook saving is 6,320 cycles instead of 6,690 (O: 1.910 µs).

## Reading

- Current upstream is 1.33× the stock kernel and 1.18× the kernel with batched wakeups. That is about the paper's L1 rung (1.333, measured in another session), consistent with the gettid cache and the per-CPU lock roughly cancelling (not isolated).
- With upstream's default probe-read checks, it is 1.24× and 1.09×. The paper's exact, checked runtime (Dx+EXACT, E16) is 1.12× and 0.99×. Upstream stays ahead of the batched kernel because it caches the thread ID, pid_tgid and comm with atfork resets. These caches are not exact under vfork and raw clone (§5). It still pays getuid/getgid on every call and getpid in the shard lookup.
- Without the pin, upstream's lead hardly depends on the core policy (Ol#S/Ol 0.991). O's 17% vs 4% policy effect belongs to the pinned 4708f409 only.
