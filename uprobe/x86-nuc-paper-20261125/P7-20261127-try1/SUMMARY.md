# P7 x86 start-up, Jetson E13 protocol (x86 NUC, 2026-11-27; exploratory, no PREREG entry)

Session P7-20261127-try1, plan plans/P7.txt; check-after PASS.
- Hashes: the 11 helper binaries and /home/y/src/rp/stw checked OK at session start (console.log). The memleak
  binary was not hash-checked (the Jetson's external-sha256.txt covered it).

## Port (p7/; Jetson originals in p7/jetson-src, byte-identical to origin/benchmark-results/jetson 8c4ec8ee)

**Sources.** The helper sources (p7/src) are unchanged, built for x86 by p7/build.sh:
- gcc 14 and clang 19 (Jetson: gcc-13, clang-15);
- -D__TARGET_ARCH_x86;
- the official tree's static libbpf 1.7.0, the same version from a different build tree.

**Runner changes** (burst, n4, memleak; their measurement logic is unchanged):
- paths;
- build mapping via harness/config-x86.json (`bdir()`);
- the x86 memleak: bcc v0.37.0 libbpf-tools, a different build from the Jetson's realprog-20261026-btf. Program-set
  parity was not checked.
- stw, rebuilt from stampwrap.c (-static) and placed at /home/y/src/rp/stw: memleak -c copies at most 31 characters of
  the command.
- n4's tool stop timeout raised from 60 to 600 s. The kernel M = 1000 cells took 148 and 157 s in total; detach time
  was not measured separately. n4_many.py's header comment does not mention this change.

**Not ported or not run:**
- the Dpc burst point;
- seq (n2_startup);
- the no-shm agent (ST3);
- lib.sh's assert_clean, inter-step cleanup and budgets.

The x86 order is burst, n4, memleak (Jetson: burst, memleak, n4).

**Backends.** kernel; O (official 4708f409); Ol (upstream head 70f7bc1f); Dn (Tern c71a9987); Dn:noaot
(BPFTIME_AOT_CACHE=0 in the tool). `aot_cache_enabled()` is identical in c71a9987 and the Orin's 9ccda7e8: unset means
on, "0" means off.

**CPUs.**
- Helpers on 0–11 via taskset: 4 P cores with HT (0/1, 2/3, 4/5, 6/7) and 4 E cores (8–11), so starts can land on
  either microarchitecture.
- The session pins 2, 4, 6 at 3.5 GHz and 8–11 at 2.5 GHz; 0, 1, 3, 5 and 7 are not pinned, and 3, 5, 7 are HT
  siblings of pinned CPUs.
- many-M's call loop runs on CPU 5, the HT sibling of pinned CPU 4.
- The Jetson ran unpinned on 6 uniform cores.

**Smoke note** (smoke output not kept):
- An n4 smoke killed the kernel tool at the old 60 s stop timeout while it was still detaching 1000 uprobes. The O and
  Dn M = 1000 cells right after it died of SIGTRAP.
- After the change, all M = 1000 cells passed (n = 2 each).
- A plausible mechanism, not proven: the agents relocated the uprobes' int3 bytes into their trampolines.
- On the Jetson the same sequence (kernel M = 1000 tool SIGKILLed at 60 s, O and D cells about 0.3 s later) did not
  fail. Its O and D M = 1000 values were, however, measured right after that kill; the x86 values were measured after a
  full detach.

## Results (20 burst rows, 40 n4 rows, 50 memleak runs: all ok; every hit counted)

Median ms. Orin E13 values are in parentheses: orin-facts ST1–ST5; Table 5 (tab:startup) of the paper.

| | K | O | Ol | Dn (\sys) | Dn, no AOT |
|---|---|---|---|---|---|
| burst of 1: child p50 | 0.46 (0.51) | 13.63 (21.97) | 12.54 | 8.57 (12.98) | 12.84 (21.97) |
| burst of 16: round p50 | 1.54 (2.6) | 107.1 (215) | 94.7 | 36.8 (86) | 93.8 (220) |
| burst of 16: child p50 | 0.60 (1.29*) | 59.6 (115.1*) | 53.2 | 22.8 (47.3*) | 53.3 (117.5*) |
| 1 probe: start p50 | 0.44 (0.50) | 13.33 (22.2) | 12.72 | 8.66 (13.1) | – |
| 10 probes | 0.48 (0.52) | 14.92 (25.5) | 13.14 | 8.16 (15.0) | – |
| 100 probes | 0.72 (0.78) | 20.04 (58.0) | 15.58 | 9.73 (33.1) | – |
| 1,000 probes | 3.15 (3.39†) | 94.1 (391.5‡) | 48.6 | 15.7 (214.7‡) | – |
| memleak: workload start | 0.59 (0.69) | 130.1 (363) | 117.7 | 18.0 (38.1) | 114.2 (311) |
| memleak: tool set-up | 105.8 (167) | 231.1 (424) | 225.7 | 386.2 (769) | 282.1 (455) |

Footnotes:
- \* Recomputed from the Jetson raw burst.jsonl. ST2 and the paper's "burst of 16" are the round (whole-burst) p50,
  not the child p50 that its label suggests.
- † Orin kernel M = 1000 rows are ok = false: the tool stop timed out at 60 s, so hits were not verified. The start p50
  is measured before the stop.
- ‡ Measured right after the SIGKILLed kernel tool (see the smoke note).
- The 10-probe Orin values come from ST4 only; the paper table omits that row.
- The Orin D arm is dev 9ccda7e8, not rebased, a different build from Dn c71a9987.

Row definitions and extra observations:
- **Rows.**
  - Burst: 200 rounds × 2 reps, median over the reps.
  - n4: spawnloop start p50 over 50 starts, median of 2 reps; untraced 0.43–0.45 ms.
  - memleak: median of 10.
- **Burst of 16, per-rep child maximum** (median over the 2 reps): K 2.22, O 114.1, Ol 103.3, Dn 39.2, Dn:noaot
  100.0 ms. The largest child seen: 2.30, 117.4, 107.1, 39.5, 101.2.
- **Waiting.** Burst-16 child CPU p50 is 0.29 (K), 13.1 (O), 12.3 (Ol), 8.7 (Dn), 12.3 (Dn:noaot). Wall above CPU is
  waiting: the shared-memory lifecycle lock or no free CPU (16 children on 12 logical CPUs); the two were not separated.
- **many-M calls:** the kernel trap costs 1315–1374 ns per call. The bpftime builds cost 148–198 ns, with O lowest, then
  Dn, then Ol, at every M.
- **VmRSS:** Dn about 36–38 MB, O/Ol 47–49 MB, untraced or K 1.6–1.8 MB.

## Reading

**The Orin ordering holds on x86, at smaller absolute values.** One process with one probe (burst of 1):
- K 0.46 ms;
- \sys 8.6 (Orin 13.0);
- upstream head 12.5;
- official 13.6 (Orin 22.0);
- \sys without the AOT cache 12.8, which is about its base, upstream head. On Orin, noaot equalled official.

**Where \sys pulls ahead most:**
- many probes: 1,000 probes 15.7 ms against 94.1 (official) and 48.6 (upstream head);
- memleak's traced workload: 18.0 ms against 130 and 118, and 114 without AOT;
- 16 concurrent starts: round 36.8 against 94–107.

**The price.** The AOT cache moves compilation into the tool: memleak set-up 386 ms against 231 (official) and 282
(Dn without AOT); Orin 769 against 424 and 455.

**Probe-count scaling** (1 → 1,000 probes) is much flatter on x86 for every bpftime build:
- O ×7.1 (Orin ×17.6);
- \sys ×1.8 (Orin ×16.4);
- upstream head ×3.8.

Candidate causes, not separated:
- the platform;
- the build: Dn includes upstream head, while the Orin D is 9ccda7e8;
- the Orin M = 1000 cells following a killed kernel tool.

**Scope.** Exploratory, single session, mixed core types. Report as x86 numbers next to Orin's, not as a controlled
cross-platform comparison.
