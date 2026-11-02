# Perf event loss re-audit on the Jetson (2026-11-02)

Why: the NUC (x86, `benchmark-results/x86-nuc` 66aee24a) saw dev 6ab9cbfe report
"lost N events" in 2 of 4 16-byte ssl-nginx audits (~0.14%), a7d54d7e in 0 of 4, and asked
for a Jetson re-audit with sslsniff's stderr kept (`run_fair.py --audit` discards it).

How: `run.sh` (+ `run_more256k.sh`), same container image, frozen builds
(`../ssl-nginx-2x2-20261023/builds/`) and CO-RE sslsniff as `../dev-68f19c1f-20261102/ssl-nginx`.
`audit_lost.py` (from `../ssl-nginx-2x2-20261023/bench`) captures sslsniff's stdout and stderr and
sums libbpf's "lost N events on CPU #c" lines (bpftime publishes drops as PERF_RECORD_LOST since
10/23). Leg order rotated every round. BTF kernel 6.8.12-1021-tegra-btf, cpu5 1728000 before and
after. Raw: `out/<set>-r<n>.json`, sslsniff stderr next to them, logs `out/<set>.log`; the stdout
captures (23 GB) stay on the host, uncommitted.

## Result (`summarize.py` -> `summary.md`)

| Set | Leg | runs with loss | lost/request mean (min-max) | events/request mean | delivered |
|---|---|---:|---:|---:|---:|
| 16 B, -c 10 -d 3 | kernel | 1/8 | 0.0008 (0.0000-0.0063) | 2.000 | 99.9609% |
| 16 B, -c 10 -d 3 | a7d54d7e | 0/8 | 0.0000 (0.0000-0.0000) | 2.000 | 100.0000% |
| 16 B, -c 10 -d 3 | dev-6ab9cbfe | 0/8 | 0.0000 (0.0000-0.0000) | 2.000 | 100.0000% |
| 16 B, -c 10 -d 3 | dev-68f19c1f | 0/8 | 0.0000 (0.0000-0.0000) | 2.000 | 100.0000% |
| 16 B, -c 100 -d 10 | kernel | 1/3 | 0.0005 (0.0000-0.0014) | 2.001 | 99.9770% |
| 16 B, -c 100 -d 10 | a7d54d7e | 1/3 | 0.0004 (0.0000-0.0013) | 2.001 | 99.9787% |
| 16 B, -c 100 -d 10 | dev-6ab9cbfe | 0/3 | 0.0000 (0.0000-0.0000) | 2.001 | 100.0000% |
| 16 B, -c 100 -d 10 | dev-68f19c1f | 1/3 | 0.0004 (0.0000-0.0013) | 2.001 | 99.9773% |
| 256 KB, -c 10 -d 3 | kernel | 10/10 | 0.2017 (0.0230-0.5127) | 17.827 | 98.8746% |
| 256 KB, -c 10 -d 3 | a7d54d7e | 10/10 | 0.1869 (0.0457-0.4104) | 17.845 | 98.9648% |
| 256 KB, -c 10 -d 3 | dev-6ab9cbfe | 10/10 | 0.2195 (0.0759-0.3966) | 17.802 | 98.7749% |
| 256 KB, -c 10 -d 3 | dev-68f19c1f | 10/10 | 0.1818 (0.1225-0.2941) | 17.854 | 98.9875% |

- Not reproduced as a 6ab9cbfe regression. At 16 B, 6ab9cbfe lost nothing in 11 runs; the
  occasional reported loss (~0.1%, one run in a few) hits the kernel, a7d54d7e and 68f19c1f alike.
- At 256 KB every leg loses ~1% (the 64 KB per-CPU ring of a 17-event, 8 KB-per-event
  response), and the four legs are within each other's spread (kernel itself 0.02-0.51 per
  request). After the first 3 rounds 6ab9cbfe looked worse than a7d54d7e (0.20 vs 0.11); with 10
  rounds the means are 0.22 vs 0.19: noise.
- No silent loss: events per request are 2.000 at 16 B in every bpftime run, and at 256 KB
  delivered + reported is ~18.0 per request, as for the kernel.
- So the NUC's 2/4 vs 0/4 is most likely the same sporadic, reported loss (2 of 4 vs 0 of 4 is
  not significant: Fisher p = 0.43). On x86 it would need more rounds, with the kernel leg in the
  same audit, to tell.
