# Typed hash delete helper (72aebadd, 2026-10-23)

Commit `72aebadd` (local) on top of `dffb2fce`/`af10949b` (shrinking probe
bound, `../hash-bound-shrink-20261022`). Hash delete used to go through the
generic `bpftime_map_delete_elem_helper`, a ~9.7 KB function with every map
type's delete inlined, whose code generation changed with any hash map edit.
Now `bpftime_hash_map_delete_elem_helper` (816 bytes) is selected at load
time with the same rule as the typed lookup/update helpers; it re-checks
the map type and otherwise uses the generic handler path. Bound programs
treat it as a standard delete helper. New tests in `[typed-helper-selection]`
(selection log and behavior in plain and JIT-bound programs; equivalence with
the generic helper for array, empty and out-of-range handles). Regression
filter: 77 cases / 239634 assertions (`tests.log`).

## A/B (same run)

Official 17-case victim, 24 rounds, all six orders of f26cc335 / dffb2fce /
72aebadd (`run-screen3.py`, `summary.json`), median ns/op and median paired
ratio vs f26cc335:

| Case | f26cc335 | dffb2fce | 72aebadd |
|---|---:|---:|---:|
| ordinary hash delete | 36.64 | 37.81 (1.033) | **27.88 (0.755)** |
| ordinary hash lookup | 23.78 | 23.78 (1.001) | 24.73 (1.000) |
| ordinary hash update | 27.74 | 27.84 (1.013) | 30.60 (1.061) |
| array delete (unsupported, error path) | 20.18 | 20.31 (1.009) | 20.78 (1.028) |
| uprobe | 198.48 | 197.82 (0.999) | 204.55 (1.028) |
| uprobe + uretprobe | 361.78 | 324.68 (0.901) | 326.46 (0.905) |

Ordinary hash update is bimodal (~27.5 and ~31 ns per process) in all three
builds; the slow mode occurred in 6, 10 and 14 of 24 samples. Its code did not
change. Other cases within +-1.5%.

`mutate6/` (hash update/delete in three program shapes, kernel + f26cc335 +
72aebadd, 6 rounds): delete + insert 74.8-74.9 -> 66.4-67.3 ns (-10 to -11%,
0.31-0.32x kernel); update unchanged (0.31-0.34x).

`shape6/` (lookup shapes, kernel + f26cc335 + 72aebadd, 6 rounds): pure misses
on a half-full table after churn 46.68 (1.62x) -> 29.06 ns (1.01x); all other
cases within +-2%.

## Full ten rounds, userspace only (`full10-72aebadd/`)

Exit 0, activation audit passed. Against f26cc335 (different run):
ordinary hash delete 36.86 -> 28.22 ns (-23.4%, 0.26x the baseline-run
kernel); every other map row within +-0.7%. Non-map rows: uprobe +3.0%
(also +2.8% in the screen; no changed code on that path, layout), uprobe +
uretprobe 358.08 -> 322.47 ns and probe write back to ~4281 ns without the
slow mode (layout effects of f26cc335 gone in this build).
