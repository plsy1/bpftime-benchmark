# nginx request latency under sslsniff (2026-11-06)

Item 3 of the extra dimensions (`../README.md`): the traced program's latency, not only its
throughput. Kernel `6.8.12-1021-tegra-btf`; bpftime `fix-final` (dev d46ceb8f) and
`fix-final-probecheck` (probe reads checked, `../n1-probe-read`). cpu5 at 1728000 before and
after each run; no other runs.

## Setup (`bench/run_tail.py`, `bench/latency.lua`, `go.sh`)

nginx (one worker, TLS, 16-byte page) and libbpf-tools sslsniff in a container from
`bpftime-ssl-nginx:20261023`, as the earlier ssl-nginx runs: legs baseline (no tracing), kernel
(sslsniff `-p` worker), bpftime (sslsniff under the syscall server, nginx under the agent),
order rotated, every leg restarting its processes, 3 reps of 10 s. Loads: **light** = wrk
`-c 10`, each connection waiting 1 ms after every response (latency.lua's `delay`; an offered
load well below capacity, so the latency is the request's own and not queueing; the requests
per second it reaches only reflect that latency and wrk's millisecond timer); **closed** = wrk
`-c 100` back to back (the ssl-nginx benchmark's load). latency.lua prints wrk's percentiles.

Two runs:
- `tail.jsonl` (15:18-15:24): the CO-RE sslsniff built for this kernel
  (`realtools-20261104/ssl-nginx/sslsniff`, as every ssl-nginx run since the BTF kernel) and
  `realtools-20261104`'s container options (default bridge network). Audit first
  (`audit-16.json`): 2.000 events per request on kernel, fix-final and probecheck.
- `bench/tail-oldsniff.jsonl` (14:43-14:48): by mistake, `ssl-nginx-2x2-20261023`'s sslsniff
  from before the BTF kernel, and `--network host`.

`ab/`: realtools-20261104's exact throughput harness (`run_fair.py`, wrk `-c 100 -d 10`
without a script) over the builds since 11-04, 5 reps (15:05-15:11).

## Findings

**1. bpftime does not lengthen nginx's tail; its median is lower than the kernel's.** Light
load, us (3 reps):

| leg | p50 | p99 | p99.9 | p99.99 |
|---|---|---|---|---|
| baseline | 160-167 | 427-466 | 4,623-4,670 | 5,593-5,848 |
| kernel | 234-241 | 491-696 | 7,446-11,882 | 16,120-16,344 |
| fix-final | 189-193 | 457-781 | 4,602-11,293 | 5,736-16,086 |
| fix-final-probecheck | 216-246 | 474-731 | 4,801-11,378 | 5,740-15,786 |

Per request (two TLS calls, each with a uprobe, a uretprobe and a perf sample) the kernel adds
74 us at the median, bpftime 27 us, with checked reads 68 us. The tails (p99.9 and beyond, 5-16
ms) are scheduling and timer noise that every traced leg shows in some reps; no leg is
consistently worse.

**2. Throughput: bpftime +14% over the kernel, +5.5% with checked reads.** Closed load:
baseline 17,408-17,584 req/s, kernel 11,940-12,199, fix-final 13,404-14,116, probecheck
12,624-12,821 (p50 latency 5.4 / 7.8 / 6.8 / 7.4 ms). sslsniff reads each TLS buffer with
`bpf_probe_read_user`: the checked read's system call costs 7.5% of fix-final's throughput here.

**3. Builds since 11-04 (`ab/ab-16.json`).** kernel 12,064 req/s (median of 5); dev-7ad5a152
14,321 (+18.7% paired median, as on 11-04: +18.5%); dev-25f4134f (Gum slots, parking, grace
period) 14,308 (+19.0%); dev-b60b99c3 (before the fifth review's fixes) 14,112 (+17.9%);
fix-final 13,881 (+15.6%). fix-final is 3% below dev-7ad5a152, lower in each of the 5 reps; not
investigated (the fifth review's fixes changed Frida's override path and the grace counter's
stores, among others).

**4. Open: the older sslsniff takes bpftime's lead away.** With `ssl-nginx-2x2`'s sslsniff
(`bench/tail-oldsniff.jsonl`, closed load) kernel 11,006-11,337 req/s and fix-final
10,778-11,229: no lead, and fix-final's light-load p50 298-342 us against the kernel's 290-317.
Both binaries are libbpf-tools sslsniff; the older one was built before the BTF kernel
(`btf-kernel-20261026`). Not looked into; the network mode also differed (host there, bridge
here), so the two runs are not a clean A/B.

Raw: `bench/tail.jsonl`, `tail.log`, `bench/audit-16.json` and its captured tool output (`bench/audit-16.json.*.sslsniff.txt.xz`),
`audit-16.log`, `bench/tail-oldsniff.jsonl`, `tail-oldsniff.log`, `ab/ab-16.json`,
`ab/ab-16.log`, stamp and clock files.
