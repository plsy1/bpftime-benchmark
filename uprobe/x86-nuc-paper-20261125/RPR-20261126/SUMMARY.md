# R-PR — why Tern's unchecked probe read is slower than official's on x86 (2026-11-26), summary

Eval plan R-PR (reviewer W6): find the cause of the x86-only "Tern probe read 1.5× official" or drop the sentence.
Exploratory throughout (not preregistered). All runs held R/.measure.lock; victim on CPU 2, loader on CPU 4.

## Finding

Tern's AOT cache (since 2026-11-08, commits f37c9502/a73218ad, carried into c71a9987) compiles programs with LLVM
`CodeModel::Large` (vm/compat/llvm-vm/array-map-inline.patch, `create_large_code_model_target_machine_or_throw`, used when
`relocatable_map_addresses_`). Helpers are then called indirectly: the helper address is loaded once before the loop
(`movabs $helper,%rbp`) and each call is `call *%rbp`, where the JIT (and official, upstream) emit `call rel32`. Inside
the benchmark's loop that is the only difference besides register naming (gdb dumps: gdb-aot1.txt, gdb-aot0.txt,
verify/gdb-off.txt, verify/gdb-dnr1.txt).

On this CPU's P-core (i5-1240P, Golden Cove) an indirect call followed by a return costs ~4 cycles where a direct
call+ret pair costs ~1: the direct pair has a fast path the indirect call does not get. No mispredicts or BACLEARs are
involved (perf counters ≈ 0 per call; indirect jmp and call-without-ret cost the same as direct). The E-core (Gracemont)
shows no penalty. Whether the eIBRS / BHI_DIS_S mitigation state contributes cannot be tested without a reboot or MSR
write; BHI_DIS_S is described as kernel-mode only, and the E-core result points weakly to plain microarchitecture.

## Numbers (ns per bpf_probe_read_user of 5 bytes, 1,000 per hit; uprobe suite __bench_read)

| build | session 1 (r1.txt, 5 reps, median [range]) | rerun (verify/bench-rerun.txt, 3 reps) |
|---|---|---|
| official 4708f409 | 1.73 [1.72, 1.74] | 1.68–1.74 |
| upstream head 70f7bc1f | 1.74 [1.70, 1.75] | – |
| Tern c71a9987 (AOT cache, default) | 2.61 [2.55, 2.66] | 2.46–2.50 |
| Tern layout control (-falign-functions=64) | 2.61 [2.48, 2.67] | – |
| Tern, BPFTIME_AOT_CACHE=0 | 1.52–1.53 (3 runs) | 1.51–1.52 (dnew and dnr) |

- AOT − no AOT: 0.97–1.09 ns per call. Tern (AOT) − official: 0.75–0.88 ns. Official's JIT loop is byte-identical to
  Tern's without AOT; official is ~0.2 ns slower there because its `bpftime_probe_read` body spills to the stack (same C
  source, different codegen). So the Tern-vs-official gap = ~1.0 ns indirect-call penalty − ~0.2 ns leaner helper.
- Layout (function alignment) is ruled out: the layout control is as slow, and moving a replica loop over offsets
  0/16/32/48 does not remove the penalty.
- Replica microbenchmarks: icall.c direct 1.14 vs indirect 2.74 ns (+1.6) with a memcpy callee; exact-shape asm replica
  1.37 vs 2.97 (+1.6); with a ret-only callee +0.68. The penalty depends on the callee and is smaller in situ (~1.0).
- Unchecked probe read is the same `memcpy` in both sources, registered for helper 112 in both.

## History

The older E3 D build (9ccda7e8, 11-09; = builds/dev) contains the AOT cache. Pre-AOT Tern builds measured ~1.507 ns per
read (10-26 full10 dev results.md: read 1507.45; also 6ab9cbfe 11-01, 42595149/d5279469 11-03, 7ad5a152 11-04), so the
"regression" appeared between 11-04 and 11-09, with the AOT cache. (x86-facts §12.3's 1,507 is a read for the old Tern
build; official's E3 write is also 1,507.1, by coincidence.) On the Orin D/O was 1.002: aarch64 calls are `blr` either way
and no such penalty was seen.

## End to end (sslsniff on nginx, 16 B, policy M, n = 10; raw/RPRA-20261126-try1, plan RPR-AOT.txt)

| pair | Δx us/event (90 % CI) | reading |
|---|---|---|
| Dn − Dn+AOT0 | +0.018 [−0.006, +0.041] | within ±0.084 (TOST-style); no difference shown |
| Dnpc − Dnpc+AOT0 | −0.064 [−0.159, +0.031] | not shown equivalent; two low Dnpc+AOT0 reps (1, 2; nginx on core 2) drive it; without them +0.005 [−0.028, +0.039] |

The S4 sslsniff path makes ~15–16 helper calls per event (entry 5, exit 11; the per-CPU array lookup may be inlined), so
0.9–1.6 ns each predicts 0.014–0.026 us/event, consistent with Dn's +0.018 but not established (CI includes 0). The
AOT/no-AOT contrast also differs in more than the code model (an extra optimize pass after array inlining, array bases
through per-process symbols), so it is not a pure code-model test.

## For the paper and for Tern

- Replace "unexplained 1.5× slower unchecked probe read on x86" with: Tern's AOT cache calls helpers indirectly; on this
  x86 P-core an indirect call+ret costs ~1 ns more than a direct one, which is the whole per-read gap in the
  microbenchmark; end to end it is ≤ ~0.04 us/event (not detected), and absent on the Orin.
- Implication for M5/M6 (C2): Tern carries a small helper-call penalty that upstream does not; it biases against Tern.
- Tern dev (record, not fix; sent to the Jetson session): AOT objects could keep direct calls (small code model with a
  stub/PLT within ±2 GB, or patching rel32 at link time when the target is in reach).
- Independent check (2 subagents, own scripts and a rerun under the lock): mechanism and data reproduce; corrections
  applied here (gap ~1.0 not 1.6 ns; not misprediction; helper-body 0.2 ns; Dnpc pair not equivalent; history wording).
