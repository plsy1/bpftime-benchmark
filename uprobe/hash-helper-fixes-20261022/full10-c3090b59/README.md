# c3090b59 full ten rounds and AB/BA screen (2026-10-22)

Runtime: `c3090b59` (fixes 1 and 2 on `codex/uprobe-official-177a1dc465ff`),
frozen artifacts = the incremental dev build also used in `../dev2-selection`,
plus the embedded benchmark built from the same tree (`source-state.txt`).
Workloads: byte-identical copies of the 5466f0d baseline workloads.

## Full ten rounds, userspace + embedded only (`full10/`)

No kernel arm (unchanged; `run-userspace-benchmark.py` replaces it with a skip
marker). 604 s, exit 0, activation audit passed. `full10/comparison.md` compares
with the 5466f0d and bdd9835 complete runs (different runs). All map operations
within +-1.2% except ordinary hash update +2.4% (known 33/36 ns bimodality);
probe write mean +5.9% with 4 of 10 samples at ~4848 ns instead of ~4273 ns.

## AB/BA screen (`abba-screen/`)

`run-screen.py` (copy of the bdd9835 hash-update screen): 20 rounds alternating
AB/BA, 5466f0d baseline vs c3090b59, full 17-case victim with 10000
iterations per arm, fresh processes, CPU 5 at 1.728 GHz, no kernel run.

| Case | 5466f0d median | c3090b59 median | Ratio of means | Median paired ratio | c3090b59 slower |
|---|---:|---:|---:|---:|---:|
| probe write (ns/inv) | 4272.28 | 4273.10 | 1.0063 | 1.0001 | 13/20 |
| per-CPU array update (ns/op) | 14.410 | 14.575 | 1.0108 | 1.0108 | 20/20 |
| ordinary array update | 11.575 | 11.529 | 0.9935 | 0.9969 | 5/20 |
| ordinary hash update | 34.758 | 35.828 | 1.0024 | 1.0014 | 11/20 |
| ordinary hash lookup | 27.166 | 27.050 | 0.9927 | 0.9928 | 3/20 |
| per-CPU hash update | 44.623 | 44.460 | 0.9963 | 0.9965 | 1/20 |
| probe read (ns/inv) | 4577.87 | 4568.25 | 0.9971 | 0.9978 | 0/20 |
| uprobe (ns/inv) | 198.33 | 198.35 | 0.9993 | 0.9989 | 9/20 |

- Probe write: the typical value is unchanged (paired median 1.0001); one
  c3090b59 sample was in the ~4804 ns slow mode. Across both runs the slow
  mode occurred in 5 of 30 c3090b59 samples and in none of 40 baseline/bdd9835
  samples, so a build-dependent occasional slow mode is not excluded. Neither
  fix touches the probe write path.
- Per-CPU array update: +1.1% (+0.16 ns/op), slower in 20/20 pairs, i.e. a
  real but small shift. Neither fix touches this path; likely code layout or
  LTO inlining. Not yet confirmed by machine-code comparison.

## Machine-code comparison (5466f0d vs c3090b59 agent)

`objdump -d` of `bpftime_percpu_array_map_update_elem_helper` (45
instructions), its callee `per_cpu_array_map_impl::elem_update` (69) and
`bpftime_probe_write_user` (31): instruction sequences identical; only data
addresses (log strings) differ. What changed is code placement:

| Function | 5466f0d addr mod 64 | bdd9835 | baf0cc85 | c3090b59 |
|---|---:|---:|---:|---:|
| percpu array update helper | 0 | 0 | 32 | 48 |
| per_cpu_array_map_impl::elem_update | 0 | 20 | 52 | 4 |
| probe write helper | 0 | 32 | 0 | 16 |

The per-CPU array update shift (+1.1%, 20/20 pairs) is consistent with a code
alignment effect: the 180-byte helper starts 64-byte aligned in 5466f0d and
bdd9835 (both 14.37-14.38 ns) but at offset 48 in c3090b59, spanning one more
cache line. This is an explanation, not a proof (no controlled realignment
test). The probe write slow mode is per process with identical helper code;
it is not explained by this comparison. Both are side effects of the two
fixes changing code size elsewhere in the agent, not of their logic.
