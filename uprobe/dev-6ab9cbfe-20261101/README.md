# uprobe suite and ssl-nginx on dev 6ab9cbfe (2026-11-01 03:58-05:07 JST)

Dev `6ab9cbfe` (after the fourth review; ringbuf submit back to one exchange) against the last
complete measurements on the BTF kernel (10/26, `../btf-kernel-20261026`, dev `a7d54d7e`).
Same methods and runners. Kernel `6.8.12-1021-tegra-btf`, `jetson_clocks`, cpu5 at 1728000
before and after, no other runs, daily job finished at 03:33. `run-all.sh`, `run-all.log`.
Build frozen in `../ssl-nginx-2x2-20261023/builds/dev-6ab9cbfe` and `full10-6ab9cbfe/artifacts`.
Containers removed after.

## ssl-nginx (`ssl-nginx/`, 2 rounds x 10 reps, wrk -c 100 -d 10, same CO-RE sslsniff)

a7d54d7e is measured again in the same run, so the comparison does not depend on the day.

| Leg | 16 B req/s | vs kernel | 256 KB req/s | vs kernel |
|---|---:|---:|---:|---:|
| baseline (no tracing) | 17,600 | +44.5% | 1,743.8 | +38.5% |
| kernel | 12,183 | 0 | 1,258.9 | 0 |
| official 177a1dc | 10,776 | -11.5% | 1,139.3 | -9.5% |
| a7d54d7e (10/26 dev) | 14,255 | +17.0% | 1,397.3 | +11.0% |
| dev 6ab9cbfe | 14,019 | +15.1% | 1,387.9 | +10.3% |

- 6ab9cbfe vs kernel, paired per rep: +15.4% (16 B) and +10.0% (256 KB), 20/20 faster.
- 6ab9cbfe vs a7d54d7e, paired: -1.6% (16 B, 4/20 faster) and -0.8% (256 KB, 6/20).
  At 16 B that is about 1.2 us more per request (14.5 vs 13.3 us over no tracing).
- Audit: 2.000 events per request for every tracing leg at 16 B; at 256 KB kernel 17.82,
  a7d54d7e 17.84, 6ab9cbfe 17.98, official 9.64 (its perf output drops about half).
- Kernel and a7d54d7e are within 0.6% of their 10/26 values.

## Complete ten-round uprobe suite (`full10-6ab9cbfe/full10/comparison.md`)

Same benchmark binaries as 10/26 (`artifact-sha256.txt`). Activation audit and sample
validation passed. ns per probe call, median of 10 rounds; each map case does 1,000 map
operations per call, so ns per operation is the value / 1000.

| Operation | kernel | a7d54d7e 10/26 | 6ab9cbfe | change | 6ab9cbfe / kernel |
|---|---:|---:|---:|---:|---:|
| uprobe | 1240.5 | 201.5 | 201.5 | 0 | 0.16x |
| uretprobe | 1658.7 | 273.2 | 269.7 | -1.3% | 0.16x |
| uprobe + uretprobe | 1781.5 | 327.9 | 327.6 | 0 | 0.18x |
| read / write | 17,596 / 17,825 | 4,570 / 4,274 | 4,575 / 4,276 | 0 | 0.26x / 0.24x |
| array update / lookup | 13,088 / 3,800 | 11,516 / 1,176 | 11,543 / 1,178 | 0 | 0.88x / 0.31x |
| per-CPU array update / lookup | 16,609 / 4,520 | 14,457 / 4,115 | 14,879 / 4,109 | +2.9% / 0 | 0.90x / 0.91x |
| **hash update** | 95,284 | 28,265 | 42,913 | **+51.8%** | 0.45x |
| **hash lookup** | 28,544 | 23,296 | 27,553 | **+18.3%** | 0.97x |
| **hash delete** | 108,268 | 28,477 | 43,832 | **+53.9%** | 0.40x |
| **per-CPU hash update** | 68,248 | 44,463 | 58,053 | **+30.6%** | 0.85x |
| **per-CPU hash lookup** | 30,363 | 27,848 | 35,041 | **+25.8%** | **1.15x** |
| per-CPU hash delete | 151,070 | 99,731 | 105,726 | +6.0% | 0.70x |
| embed | | 155.9 | 155.3 | 0 | |

The kernel arm moved -4.9% to +4.9% from 10/26 (the two array deletes and per-CPU array
lookup at the ends, uprobe and hash within ±0.3%).

## Reading

- Probe paths (uprobe, uretprobe, read/write helpers), arrays and embed are unchanged.
- Hash maps are slower single-threaded than a7d54d7e: update and delete +15 ns per
  operation, lookup +4 ns; per-CPU hash update +14 ns, lookup +7 ns, delete +6 ns. Per-CPU hash
  lookup is now slower than the kernel (35 vs 30 ns).
- The hash map work since a7d54d7e was gated on its own runners (churn, tracking table):
  striped writers cost +7.4% on single-thread churn (`../hash-striped-writers-20261030`),
  about +14 ns per call over three map operations, and per-CPU hash took the map lock on every
  operation for correctness (`../percpu-hash-fix-20261028`). This suite's map loops, 1,000
  operations on 1,000 keys per call, show a larger share of that cost. Which commits account
  for how much is not measured here; the frozen builds of each step are in
  `../ssl-nginx-2x2-20261023/builds/`.
- ssl-nginx's -1.6% at 16 B is small but consistent (16/20 reps); not attributed.
