# uprobe suite and ssl-nginx on dev a4ac0d31 (2026-11-02 18:47-20:38 JST)

Dev `a4ac0d31`: 68f19c1f plus the per-CPU hash without the map lock and the ordinary hash's
`flatten` (`../percpu-hash-lockfree-20261102`). Same methods and runners as
`../dev-68f19c1f-20261102`; BTF kernel, cpu5 at 1728000 before and after, outside the daily job.
`run-all.sh`, `run-all.log` (first the gate in `../percpu-hash-lockfree-20261102/final`).
Container removed. The sslsniff captures (`ssl-nginx/*.sslsniff.txt`, up to 545 MB) stay on the
host and are not committed.

## ssl-nginx (`ssl-nginx/`, 2 rounds x 10 reps; 68f19c1f measured again in the same run)

| Leg | 16 B req/s | vs kernel | 256 KB req/s | vs kernel |
|---|---:|---:|---:|---:|
| baseline (no tracing) | 17,544 | +44.8% | 1,751.8 | +38.8% |
| kernel | 12,114 | 0 | 1,261.9 | 0 |
| official 177a1dc | 10,782 | -11.0% | 1,136.1 | -10.0% |
| dev 68f19c1f | 14,153 | +16.8% | 1,387.1 | +9.9% |
| dev a4ac0d31 | 14,216 | +17.4% | 1,399.4 | +10.9% |

a4ac0d31 vs 68f19c1f, paired: +0.1% (16 B, 12/20) and +1.6% (256 KB, 14/20): within noise
(sslsniff uses a per-CPU array and a perf event array, no hash). a4ac0d31 vs kernel: 20/20 faster
at both sizes. Audit, events per request: 16 B 2.000 for every tracing leg (official 2.001);
256 KB kernel 17.99, 68f19c1f 18.00, a4ac0d31 17.72, official 9.56.

## Complete ten-round uprobe suite (`full10-a4ac0d31/full10/comparison.md`)

ns per operation (value per call / 1000 for the map cases); kernel and 68f19c1f from the 11/02
morning run (kernel arm measured again here: within -0.7%..+2.5%).

| Operation | kernel | 68f19c1f | a4ac0d31 | change | a4ac0d31 / kernel |
|---|---:|---:|---:|---:|---:|
| uprobe | 1237.1 (per call) | 199.2 | 201.2 | +1.0% | 0.16x |
| read | 17.6 | 4.57 | 4.85 | +6.3% | 0.28x |
| write | 17.8 | 4.27 | 4.32 | +1.0% | 0.24x |
| hash update | 95.1 | 29.5 | 29.1 | -1.4% | 0.31x |
| hash lookup | 29.7 | 22.3 | 22.3 | -0.3% | 0.75x |
| hash delete | 108.7 | 37.5 | 33.9 | -9.7% | 0.31x |
| array update | 13.1 | 11.5 | 11.5 | 0.0% | 0.88x |
| array lookup | 3.8 | 1.18 | 1.18 | +0.2% | 0.31x |
| per-CPU hash update | 68.3 | 54.3 | 36.3 | -33.1% | 0.53x |
| per-CPU hash lookup | 29.8 | 33.5 | 25.3 | -24.4% | **0.85x** (was 1.14x) |
| per-CPU hash delete | 151.1 | 104.6 | 58.3 | -44.3% | 0.39x |
| per-CPU array update | 16.6 | 14.8 | 14.9 | +0.3% | 0.90x |
| per-CPU array lookup | 4.5 | 4.11 | 4.11 | 0.0% | 0.91x |

uretprobe, uprobe+uretprobe and embed within +-0.6%. `read` +6.3%: code layout of
`bpftime_probe_read` (unchanged code), stable in every run; not chased. Activation audit
(17 cases) and sample validation passed. Every tracked map operation is now below the kernel's.
