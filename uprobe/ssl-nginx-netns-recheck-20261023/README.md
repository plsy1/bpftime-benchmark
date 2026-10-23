# Re-check: does the host network namespace (Tailscale/netfilter) cost bpftime extra? (2026-10-23)

Claim checked: `summry/archive/bpftime-v020-jetson-host-network-validation.md` (2026-07-21, v0.2.0):
host netns lowers all modes, but bpftime loses 3.5-4.3 pp more than kernel, attributed to a bpftime
reader that occupied about one CPU core (0.93-0.95) and amplified the host network tax; Tailscale
on/off was associated with the extra loss.

## Method

`run_recheck.py`, host side. Two identical containers from `bpftime-ssl-nginx:20261023`:
`netns-private` (Docker bridge) and `netns-host` (`--network host`); both `--privileged --pid=host`.
wrk -> nginx over 127.0.0.1 inside the container, 1 KB payload (July's main case), `-c 100 -d 10`,
fair scope (kernel sslsniff `-p <worker>`). Per rep the leg order rotates and each leg runs in both
containers back to back, container order alternating. Legs: baseline, kernel, official 177a1dc
(affinity pinning kept), opt-noaff-fork (dev branch `a90fa447`). Builds and sslsniff from
`../ssl-nginx-2x2-20261023`. Reader/nginx CPU from `/proc` while wrk runs; host `INPUT`/`ts-input`
counters read only. Tailscale stayed on (the user connects through it). Legs wait while any foreign
build or benchmark process exists and are repeated if one appeared; rep 2 still had one leg
overlapping a build and is excluded (`analyze.py`; 9 reps kept). Run 15:48-16:17 JST.

## Result (`summary.md`)

| Leg | private req/s | host req/s | host vs private (95% CI) | reader cores |
|---|---:|---:|---:|---:|
| baseline | 16,518 | 15,238 | -7.7% [-8.3, -7.0] | - |
| kernel | 11,458 | 10,418 | -9.1% [-10.8, -7.4] | 0.11 |
| official | 10,194 | 9,644 | -6.5% [-8.3, -4.8] | 0.05 |
| opt-noaff-fork | 12,346 | 11,409 | -8.1% [-9.8, -6.5] | 0.06 |

Interaction (tracer change minus kernel change, private -> host): official +2.6 pp [-0.1, +5.3],
opt-noaff-fork +1.0 pp [-0.9, +2.8]. July (v0.2.0): -3.5 pp [-5.4, -1.5] and -4.3 pp [-6.3, -2.4].
In host netns, benchmark loopback traverses `INPUT -> ts-input` 2.03 times per request.

## Verdict

1. **Holds:** the host netns costs every mode about 7-9%, as in July (-6.3 to -6.8%). This is a
   shared environment cost; compare only within one netns.
2. **Does not hold for current bpftime:** there is no extra bpftime loss; if anything it loses
   slightly less than kernel. bpftime is ahead of kernel by 8-9% in both netns.
3. **Why July saw it:** the mechanism it relied on, a reader using about one core, was the software
   perf record alignment bug found the next day (`summry/archive/bpftime-software-perf-record-alignment-fix-20260722.md`):
   a misaligned header made the consumer spin, the fix cut reader task-clock by 95%. Readers now use
   0.05-0.06 cores. v0.2.0 itself was not rerun, so this is the most likely explanation, not a
   re-measurement of v0.2.0.
4. **Tailscale specifically:** not separable here. Turning it off would disconnect the user, so only
   the whole host netns (including the ts-input traversal) was compared. July's own rule replay
   already found the ts-input chain to be a shared cost without a significant bpftime-specific part.
