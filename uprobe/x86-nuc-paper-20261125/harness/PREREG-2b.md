# PREREG-2b — amendment of PREREG-2 §1/§2 (M6 build) and the Orin M6 session (2026-11-26, before C2)

Written before the Dn730f E4 check (E4f-20261126-try1, scheduled 02:15) and before any sslsniff measurement of Dn730f.
PREREG-2, PREREG-2a and every rule not named here are unchanged.

## 1. Why
- PREREG-2 §1 defines Dn730 = c71a9987 + `git revert 24999e3c` (Tern with upstream's #730 per-CPU exec lock), so that
  M6 (Dn730 vs OlL3) compares two builds that both hold the lock ("同等正确性").
- x86 E4-20261125-try1 showed Dn730 is not that: in variant b1 (PERCPU_ARRAY load, helper call, store) at T = 16 and 64
  threads on 6 CPUs it lost 1–3 and 28–54 updates per run in 5/5 reps, where Ol (upstream with the lock) lost 0.
- Cause (confirmed in source, e7a5b79f): `bpftime_resolve_array_cpu_query_v1` gives the JIT's inlined per-CPU lookup
  `::sched_getcpu`, while the lock is taken per `current_bpf_cpu`; after a migration lock and slot disagree. The helper
  path already uses `my_sched_getcpu()` (returns `current_bpf_cpu` while the lock is held). This is a defect of our
  ladder build (Tern itself has no lock), not of Tern.

## 2. Change
- New build **Dn730f** = ladder/dn730-c71a9987 1a2d42ec (Dn730 + `hooks->read_cpu = my_sched_getcpu` for every
  platform), x86 `builds/dn730f`, same options as Dn730 (checks off).
- Gate (decided before seeing it): E4f, legs Dn / Dn730 / Dn730f, variants b1 and c at T = 16, 64, 5 reps each.
  - Dn730f loses 0 updates in all 20 counting runs → Dn730f replaces Dn730 everywhere PREREG-2 names Dn730: M6 becomes
    **Dn730f vs OlL3**, P4 becomes Dn730f − Dn ≈ Ol − Ol730x, and the C2 leg list uses Dn730f.
  - Otherwise M6 leaves the confirmatory family (Holm over 5 per platform) and is reported exploratory with the defect
    stated.
- E4f's timing runs (variant a, T = 1, 4, 16, 64) report Dn730f / Dn730 for information; they do not change the gate.
  Expected: Dn730f slightly slower than Dn730 per call, because `my_sched_getcpu` reads a thread_local in the agent
  (a TLS descriptor call in the dlmopen-ed agent) where `::sched_getcpu` is a vDSO read.
- C1's Dn730 rows stay as recorded (exploratory, now with the defect noted). Nothing from C1 enters the paper.

## 3. Orin
- The J13 leg list (eval-plan.md line 281) did not include OlL3, so Orin M6 could not be computed from J13 (planning
  error, found 2026-11-26 after J13).
- Orin M6 comes from a separate supplement session **J13s**: legs B, Kb, OlL3, Dn730f (Orin port of the same change),
  OlL3x730; 16 B, policy and loader as J13, n = 40 (2 rounds × 20, as J13), one warm-up rep, own randomization.
  M6 is paired within J13s only; M1–M5 stay from J13. The Orin family is still 6 (M1–M5 from J13, M6 from J13s);
  margin ±0.634 us/event (PREREG-2a).
- Same gate on the Orin as on x86 is not required (E4 is x86 only), but J13s does not start before the x86 gate passes.
