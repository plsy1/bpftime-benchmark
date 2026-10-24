# 7ed339ad: cache bpf_get_current_uid_gid, invalidate on credential changes (2026-10-24)

Motivation: the stage 2 split of `../ssl-nginx-ablation-20261024` found `bpf_get_current_uid_gid`
(getgid + getuid on every call, 4 calls per 16 B request) costing 5.0 us/request, half of
bpftime's helper/map/metadata stage.

Change: the agent interposes libc's eight uid/gid setters (call the real function, keep result and
errno, invalidate); the cache is enabled only when all eight interposers are bound (LD_PRELOAD);
the atfork child handler invalidates; fast path is one atomic load. Raw-syscall credential changes
are not observed (documented in `CLAUDE.md`).

| Check | Result |
|---|---|
| `[uid_gid]` | 4 cases / 27 assertions (cache hit, invalidation, fork child, interposer keeps result and errno, off unless bound) |
| Daily filter + `[uid_gid]` (`tests.log`) | 91 cases / 254742 assertions |
| LD_PRELOAD binding | all eight setters resolve to the agent's hooks; without preload none (cache stays off) |
| ssl-nginx UID audit (`sslsniff -x`, 16 B) | kernel, db3d475f and 7ed339ad all record UID 65534 (the nginx worker after it drops root) for every event, 2.000 events/request |
| Full ten rounds (`full10-7ed339ad`) | all 15 items within +/-1.5% of db3d475f (the suite does not call the helper) |

The UID audit shows correct values, but the cache is first filled in the worker after its `setuid`
(the master runs no SSL probes), so the fill-then-invalidate path is covered by the unit tests only.

The first full-ten attempt stopped at its clock check: the Jetson had rebooted at 05:54 and
`jetson_clocks` was not applied (`full10-failed-no-jetson-clocks`). It was re-applied before the
full ten rounds and the ssl-nginx runs below.

## ssl-nginx, fair scope, 2 rounds x 10 (`bench/summary.md`)

| Leg | 16 B req/s | vs kernel | us/req over baseline | 256 KB req/s | vs kernel | us/req over baseline |
|---|---:|---:|---:|---:|---:|---:|
| baseline | 17,157 | +45.3% | 0 | 1,749 | +48.9% | 0 |
| kernel | 11,808 | 0 | 26.4 | 1,174 | 0 | 280 |
| db3d475f | 12,751 | +8.0% | 20.1 | 1,255 | +6.8% | 225 |
| 7ed339ad | 13,751 | **+16.5%** | 14.4 | 1,326 | **+12.9%** | 182 |

7ed339ad vs db3d475f, paired per rep: 16 B +7.8% (20/20 faster), 256 KB +6.0% (13/20; CV up to
6.6% in this payload). bpftime's loss against no tracing drops from 25.7% to 19.9% at 16 B. The
saving, 5.7 us per 16 B request and ~43 us per 256 KB request (about 36 helper calls), matches the
ablation estimate of about 0.6 us per system call.
