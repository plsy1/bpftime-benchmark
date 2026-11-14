# Step 0: pass 1 against pass 2 (2026-11-15)

Pass 1 ran 05:04-05:53 JST, pass 2 06:04-06:54 JST, same scripts and builds, clocks fixed in both.
apt's daily services ran only during check 4 of pass 2 (06:24:18-06:24:49 and 06:28:21-06:29:07).

## Check 2 (e2-cost): every resolved timing row, pass 2 / pass 1

- rows compared: 508; median ratio 0.998; min 0.711; max 1.168; rows off by more than 5%: 32; by more than 10%: 14
- rows off by more than 5% (delta us, pass 1 -> pass 2):
  - `tiny|ref_empty|none|f0|R1|k0`: 0.1 -> 0.1 (-28.9%)
  - `large|atom_warp_all|dev|f0|R1|k0`: 1950.0 -> 1562.0 (-19.9%)
  - `full|st_lane32|dev|f0|R16|k0`: 8.9 -> 10.4 (+16.8%)
  - `large|atom_warp_leader_sys|dev|f0|R1|k0`: 270.5 -> 230.5 (-14.8%)
  - `large|atom_warp_leader|dev|f0|R1|k0`: 270.4 -> 230.5 (-14.8%)
  - `large|atom_warp_leader|dev|f0|R2|k0`: 390.2 -> 333.4 (-14.6%)
  - `large|atom_warp_leader_sys|dev|f0|R2|k0`: 390.3 -> 333.5 (-14.6%)
  - `large|atom_warp_all|dev|f0|R2|k0`: 3297.0 -> 2830.0 (-14.2%)
  - `large|atom_warp_all|dev|f1|R1|k0`: 3741.0 -> 3254.0 (-13.0%)
  - `full|red_warp_leader|reg|f0|R16|k0`: 188.4 -> 163.9 (-13.0%)
  - `full|red_warp_leader|dev|f0|R1|k0`: 1.9 -> 1.7 (-12.8%)
  - `full|rec_lane0|reg|f1|R1|k0`: 4.4 -> 5.0 (+11.9%)
  - `full|atom_warp_all|reg|f1|R1|k0`: 962.7 -> 1063.0 (+10.4%)
  - `full|red_warp_leader|reg|f1|R16|k0`: 303.0 -> 271.6 (-10.4%)
  - `large|atom_warp_all|dev|f1|R2|k0`: 5784.0 -> 5251.0 (-9.2%)

| headline value | pass 1 | pass 2 | change |
|---|---:|---:|---:|
| today's 1512 op, anchor, ns/thread | 0.433 | 0.431 | -0.4% |
| record/warp lane 0 + membar.sys, anchor reg, ns/warp | 10.060 | 9.906 | -1.5% |
| record/warp lane 0, no fence, anchor reg, ns/warp | 1.612 | 1.519 | -5.8% |
| record/warp lane 0 + membar.sys, anchor dev, ns/warp | 7.976 | 7.937 | -0.5% |
| 8-B store/thread, anchor dev, no fence, ns/thread | 0.032 | 0.032 | -0.2% |
| 8-B store/thread, large dev, no fence, ns/thread | 0.060 | 0.060 | +0.0% |
| 8-B store/thread, full dev, no fence, ns/thread | 0.174 | 0.174 | -0.5% |
| 8-B store/thread, anchor reg, membar.sys, ns/thread | 0.258 | 0.258 | -0.1% |
| 32-B record/thread, anchor dev, no fence, ns/thread | 0.748 | 0.748 | +0.1% |
| atomic add own word, anchor dev, ns/thread | 0.057 | 0.056 | -1.0% |
| atomic add own word, anchor reg, ns/thread | 0.291 | 0.285 | -1.9% |
| one word per SM, 48 warps, reg, R=16, ns/counted op | 81.730 | 81.920 | +0.2% |
| one word per SM, 48 warps, dev, R=16, ns/counted op | 1.253 | 1.249 | -0.3% |

pass 1: reading verdict: {'primary': {'shape': 'anchor', 'record': 'rec_lane0 reg f1', 'reference': 'ref1512', 'ref_ns_per_thread': 0.4328624725341797, 'ref_ns_per_warp': 13.85159912109375, 'record_ns_per_warp': 10.062762451171874, 'gate_ratio_to_llama': 0.6971074501674107, 'gate_ok': True, 'ref_ns
pass 1: decisions: {"anchor": {"single_digit": "reachable only with batched publication (only unfenced registered variants < 10%: rec_lane0 4.7%, rec_lane4 4.1%)", "copy_out": "not required by this rule"}, "large": {"single_digit": "reachable only with batched publication (only unfenced registered variants < 10%: rec_lane0 3.9%, rec_lane4 3.6%)", "copy_out": "not required by this rule"}, "full": {"single_digit": "no

pass 2: reading verdict: {'primary': {'shape': 'anchor', 'record': 'rec_lane0 reg f1', 'reference': 'ref1512', 'ref_ns_per_thread': 0.43101253509521487, 'ref_ns_per_warp': 13.792401123046876, 'record_ns_per_warp': 9.906231689453126, 'gate_ratio_to_llama': 0.6941282009765625, 'gate_ok': True, 'ref_n
pass 2: decisions: {"anchor": {"single_digit": "reachable only with batched publication (only unfenced registered variants < 10%: rec_lane0 4.5%, rec_lane4 4.0%)", "copy_out": "not required by this rule"}, "large": {"single_digit": "reachable only with batched publication (only unfenced registered variants < 10%: rec_lane0 3.9%, rec_lane4 3.6%)", "copy_out": "not required by this rule"}, "full": {"single_digit": "no

## Check 6 (e6-drain)

| value | pass 1 | pass 2 | change |
|---|---:|---:|---:|
| drain array 40 B, records/s | 7,180,326 | 7,176,879 | -0.0% |
| drain array 4096 B, records/s | 2,505,771 | 2,469,644 | -1.4% |
| drain array 1048576 B, records/s | 9,947 | 9,943 | -0.0% |
| drain gpu1503 40 B, records/s | 6,715,297 | 6,719,145 | +0.1% |
| drain gpu1503 4096 B, records/s | 2,472,620 | 2,457,301 | -0.6% |
| drain gpu1503 1048576 B, records/s | 9,937 | 9,944 | +0.1% |
| GPU bandwidth slowdown, d40 | 1.48% | 1.48% | |
| GPU bandwidth slowdown, d1m | 11.55% | 11.51% | |
| GPU bandwidth slowdown, hog1 | 18.05% | 17.99% | |
| GPU bandwidth slowdown, hog4 | 39.91% | 39.02% | |
| pass 1: largest segment that registers | 2047 MiB | 2,048 MiB works: no | |
| pass 2: largest segment that registers | 2047 MiB | 2,048 MiB works: no | |
| pass 1: llama pp512 slowdown, d40 / hog4 (median %) | 0.09 | 7.85 | off-round spread 0.19% |
| pass 2: llama pp512 slowdown, d40 / hog4 (median %) | 0.03 | 8.02 | off-round spread 0.24% |

## Check 3 (e3-copyout)

- pass 1: overall yes; A fixable; layouts {'after': ('consistent', 0), 'before': ('consistent', 0), 'farafter': ('violates', 4530152), 'farbefore': ('consistent', 0), 'sepff': ('consistent', 0), 'seppf': ('violates', 4724471)}
- pass 2: overall yes; A fixable; layouts {'after': ('consistent', 0), 'before': ('consistent', 0), 'farafter': ('violates', 4417782), 'farbefore': ('consistent', 0), 'sepff': ('consistent', 0), 'seppf': ('violates', 4624668)}

| llama-bench with the copier | pass 1 t/s (loss) | pass 2 t/s (loss) |
|---|---:|---:|
| noshim pp512 | 2495.83 (noise 0.6%) | 2493.67 (noise 0.4%) |
| noshim tg128 | 67.80 (noise 0.6%) | 67.77 (noise 0.5%) |
| idle pp512 | 2492.03 (0.2%) | 2497.99 (-0.2%) |
| idle tg128 | 67.80 (0.0%) | 67.66 (0.2%) |
| c1m1ms pp512 | 2489.06 (0.3%) | 2487.67 (0.2%) |
| c1m1ms tg128 | 67.43 (0.6%) | 67.46 (0.5%) |
| c16m1ms pp512 | 2407.73 (3.5%) | 2403.94 (3.6%) |
| c16m1ms tg128 | 62.45 (7.9%) | 62.16 (8.3%) |
| cont pp512 | 2360.18 (5.4%) | 2364.69 (5.2%) |
| cont tg128 | 59.56 (12.2%) | 59.55 (12.1%) |
- pass 1: **B0 verdict (async copier safe next to relaxed-mode captures): pass**
- pass 1: **B: graphs keep working with the copier: yes; idle shim within noise: True; 1 MiB/1 ms loss bounded (<= 5.0%, both tests): yes.**
- pass 1: **C: tier 1 (direct CPU mapping) works: False; tier 2 (import, kernel and DtoH reads) works: True; verdict: works.**
- pass 1: | ctxsync-relaxed | - | crashed or no output (rc 139) | | | | | | | | |
- pass 2: **B0 verdict (async copier safe next to relaxed-mode captures): pass**
- pass 2: **B: graphs keep working with the copier: yes; idle shim within noise: True; 1 MiB/1 ms loss bounded (<= 5.0%, both tests): yes.**
- pass 2: **C: tier 1 (direct CPU mapping) works: False; tier 2 (import, kernel and DtoH reads) works: True; verdict: works.**
- pass 2: | ctxsync-relaxed | - | crashed or no output (rc 139) | | | | | | | | |

## Check 4 (e4-publish)

| run | pass 1 violating / verified | pass 2 violating / verified | rate p1 | rate p2 |
|---|---:|---:|---:|---:|
| t1_A | 0 / 1000000128 | 0 / 1000000128 | 16,301,463 | 16,011,406 |
| t1_A0_stress | 0 / 200000256 | 0 / 200000256 | 3,449,952 | 3,448,038 |
| t1_A_long | 0 / 100000128 | 0 / 100000128 | 12,907,045 | 12,701,222 |
| t1_A_stress | 0 / 1000000128 | 0 / 1000000128 | 3,446,511 | 3,441,168 |
| t1_Am | 0 / 10000128 | 0 / 10000128 | 16,080,365 | 15,910,301 |
| t1_Am0mix_stress | 0 / 10000128 | 0 / 10000128 | 3,911,041 | 3,924,213 |
| t1_Am_stress | 0 / 10000128 | 0 / 10000128 | 3,517,790 | 3,487,417 |
| t1_B | 0 / 100000128 | 0 / 100000128 | 16,320,977 | 16,066,858 |
| t1_B_stress | 0 / 200000256 | 0 / 200000256 | 3,440,315 | 3,491,058 |
| t1_C | 1370 / 200000256 | 1776 / 200000256 | 19,200,519 | 19,816,087 |
| t1_C0_stress | 17210 / 10000128 | 27969 / 10000128 | 3,404,418 | 3,341,324 |
| t1_C_long | 4 / 100000128 | 1 / 100000128 | 12,477,518 | 12,860,266 |
| t1_C_stress | 299982 / 200000256 | 465548 / 200000256 | 3,454,520 | 3,480,591 |
| t1_Cd | 2 / 200000256 | 0 / 200000256 | 13,369,278 | 13,551,522 |
| t1_Cd15 | 0 / 10000128 | 0 / 10000128 | 17,278,574 | 17,794,439 |
| t1_Cd15_stress | 17341 / 10000128 | 3379 / 10000128 | 3,334,744 | 3,379,314 |
| t1_Cd_stress | 0 / 200000256 | 0 / 200000256 | 3,485,639 | 3,521,327 |
| t1_D1 | 0 / 100000128 | 0 / 100000128 | 16,605,963 | 16,148,601 |
| t1_D1_stress | 0 / 200000256 | 0 / 200000256 | 3,439,403 | 3,446,596 |
| t1_D2 | 0 / 100000128 | 0 / 100000128 | 16,125,688 | 16,457,847 |
| t1_D2_stress | 0 / 200000256 | 0 / 200000256 | 3,493,113 | 3,469,423 |
| t1_E | 3320 / 10000128 | 2945 / 10000128 | 15,115,698 | 15,179,147 |
| t1_E_stress | 73799 / 10000128 | 68339 / 10000128 | 3,291,036 | 3,393,106 |
| t2_cas | stale 0 /  | stale 0 /  | 131,119 | 58,220 |
| t2_cas_stress | stale 0 /  | stale 0 /  | 60,739 | 107,437 |
| t2_casacq | stale 0 /  | stale 0 /  | 53,937 | 51,574 |
| t2_casfence | stale 0 /  | stale 0 /  | 58,458 | 54,648 |
| t2_control_cas | stale 253301 /  | stale 226096 /  | 46,957 | 42,124 |
| t2_control_casfence | stale 174 /  | stale 193 /  | 47,220 | 45,834 |
- pass 1: - verdict: **no violation observed in 1000000128 rounds of A; C failed 299982 times in 200000256 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the o
- pass 1: - attribution: the fence's duration: Cd (a fenceless wait of 28205 ns, A's median fence 28032 ns) showed 0 in 200000256 rounds where C-like failures would be 299982.0, so A is 'safe in practice here, not attributable to fence semantics'
- pass 1: - A_fence_ns_first_round: {'n': 2500224, 'p50': 28032, 'p90': 38848, 'p99': 41024, 'overflow': 0}
- pass 1: - verdict: **no violation observed in 1000000128 rounds of A; C failed 1370 times in 200000256 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the oth
- pass 1: - attribution: undecided: Cd failed 2 times in 200000256 rounds, too few to separate A's zero from a delay (needs >= 3 Cd failures and an expectation of >= 3 in A's rounds)
- pass 1: - A_fence_ns_first_round: {'n': 2500224, 'p50': 6400, 'p90': 7552, 'p99': 8448, 'overflow': 0}
- pass 1: - verdict: **no violation observed in 100000128 rounds of A; C failed 4 times in 100000128 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the other l
- pass 1: - verdict: **no violation observed in 200000256 rounds of A0 (the publishing lane wrote no payload) while C0 failed 17210 times in 10000128 (95% upper bound 1.50e-08 per round), but not separable from the fence's duration: a fence in a warp without payload too
- pass 2: - verdict: **no violation observed in 1000000128 rounds of A; C failed 465548 times in 200000256 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the o
- pass 2: - attribution: the fence's duration: Cd (a fenceless wait of 27948 ns, A's median fence 27776 ns) showed 0 in 200000256 rounds where C-like failures would be 465548.0, so A is 'safe in practice here, not attributable to fence semantics'
- pass 2: - A_fence_ns_first_round: {'n': 2500224, 'p50': 27776, 'p90': 37632, 'p99': 39424, 'overflow': 0}
- pass 2: - verdict: **no violation observed in 1000000128 rounds of A; C failed 1776 times in 200000256 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the oth
- pass 2: - attribution: the fence's duration: Cd (a fenceless wait of 6594 ns, A's median fence 6400 ns) showed 0 in 200000256 rounds where C-like failures would be 1776.0, so A is 'safe in practice here, not attributable to fence semantics'
- pass 2: - A_fence_ns_first_round: {'n': 2500224, 'p50': 6400, 'p90': 7232, 'p99': 8064, 'overflow': 0}
- pass 2: - verdict: **no violation observed in 100000128 rounds of A; C failed 1 times in 100000128 rounds of the same setup: consistent with one fence per warp, but A's run had too few rounds to expect 3 C-like failures (95% upper bound on A's rate: 3.00e-08 per round
- pass 2: - verdict: **no violation observed in 200000256 rounds of A0 (the publishing lane wrote no payload) while C0 failed 27969 times in 10000128 (95% upper bound 1.50e-08 per round), but not separable from the fence's duration: a fence in a warp without payload too

## Checks 1, 5, 7, 8, 9: verdict lines

### e1-ids summary.txt: identical

    S: max smid+1 = 8, %nsmid = [8]
    W: max warpid+1 = 48, %nwarpid = [48]
    (smid, warpid) exclusive among co-resident warps: yes (0 collisions in 33 proven launches; 0 not proven; oversubscribed control capped: True; (smid, tid/32) control collides: True; b pairs sharing an 
    %warpid: hardware slot (differs from tid/32 and exclusive per SM) (466 of 10752 warps have warpid == tid/32)
    entry->exit id changes in a/b: 0
    %gridid unique per launch (replays included): no (5994 duplicates; replay counters complete: True; missing records 0; parts with fewer launches []; graph node overflow 0)
    c1: launches 1000, duplicates 0, strictly increasing True
    c2: launches 1000, duplicates 0, strictly increasing True

### e7-warp summary.txt: differs

    p1: native: yes; violations (correct helper) 0; bad_rank violations {'early': 229573, 'lowlanes': 218626, 'nested': 253381, 'loops': 438247, 'memcap': 308489}; bad_leader0 {'early': 508296, 'lowlanes': 498736, 'nested': 5100
    p2: native: yes; violations (correct helper) 0; bad_rank violations {'early': 229467, 'lowlanes': 219451, 'nested': 252607, 'loops': 438172, 'memcap': 307379}; bad_leader0 {'early': 508296, 'lowlanes': 498736, 'nested': 5100
    p1: jit: yes; violations (correct helper) 0; bad_rank violations {'early': 229518, 'lowlanes': 219102, 'nested': 252722, 'loops': 437466, 'memcap': 310567}; bad_leader0 {'early': 508296, 'lowlanes': 498736, 'nested': 510030,
    p2: jit: yes; violations (correct helper) 0; bad_rank violations {'early': 229852, 'lowlanes': 219195, 'nested': 253570, 'loops': 438270, 'memcap': 308201}; bad_leader0 {'early': 508296, 'lowlanes': 498736, 'nested': 510030,

### e5-atomics summary.txt: differs

    p1: plain calls=122880 counter=90 gaps=122790 duplicate_indices=122790 cas_retries=0 cas_caphits=0 pass=0 valid=True
    p2: plain calls=122880 counter=99 gaps=122780 duplicate_indices=122780 cas_retries=0 cas_caphits=0 pass=0 valid=True
    p1: cas   calls=122880 counter=102 gaps=122778 duplicate_indices=122778 cas_retries=0 cas_caphits=0 pass=0 valid=True
    p2: cas   calls=122880 counter=94 gaps=122786 duplicate_indices=122786 cas_retries=0 cas_caphits=0 pass=0 valid=True
    p1: cas_run: unusable: 122778 duplicate indices of 122880 calls (counter 102)
    p2: cas_run: unusable: 122786 duplicate indices of 122880 calls (counter 94)

### e8-package summary.txt: identical

    base_ok=True negative_control_ok=True
    decision: {"valid": true, "per_N": {"294912": "yes", "2936016": "yes", "33554432": "yes", "268435456": "yes", "1073741824": "yes"}, "packaging_works_up_to_N": 1073741824, "constant_bank3_left_bytes": 

### e7-llama summary.txt: identical

    ctrl   ctrl   ctrl    valid=True launches=10/10 threads=30720/30720 calls=1280 mean_records_per_call=24.000 share_32=0.50000 anomalies=0/0
    ctrl   ctrl   ctrldiv valid=True launches=10/10 threads=40960/40960 calls=2560 mean_records_per_call=16.000 share_32=0.00000 anomalies=0/0
    ctrl   ctrl   ctrlmerge valid=True launches=10/10 threads=40960/40960 calls=1280 mean_records_per_call=32.000 share_32=1.00000 anomalies=0/0
    rms    pp512  rms     valid=True launches=45/45 threads=11273216/11273216 calls=352288 mean_records_per_call=32.000 share_32=1.00000 anomalies=0/0
    mmq    pp512  mmq     valid=True launches=21/21 threads=1892352/1892352 calls=59136 mean_records_per_call=32.000 share_32=1.00000 anomalies=0/0
    mmvq   pp512  mmvq    valid=True launches=2/2 threads=983040/983040 calls=30720 mean_records_per_call=32.000 share_32=1.00000 anomalies=0/0
    ropek  pp512  ropek   valid=True launches=22/22 threads=11534336/11534336 calls=360448 mean_records_per_call=32.000 share_32=1.00000 anomalies=0/0
    rms    tg128  rms     valid=True launches=5760/5760 threads=2949120/2949120 calls=92160 mean_records_per_call=32.000 share_32=1.00000 anomalies=0/0

### e9-hooks pp512.summary.txt: identical

    verdict: YES: every kernel's largest geometry was seen at a hook (graph kernels: their largest graph geometry at capture or a node hook)
    all geometries seen at some hook: True; strict (every graph launch's own geometry before replay): True
    largest threads per launch: nsys 4194304, hooks 4194304, hooks before replay 0

### e9-hooks tg128.summary.txt: identical

    verdict: YES: every kernel's largest geometry was seen at a hook (graph kernels: their largest graph geometry at capture or a node hook)
    all geometries seen at some hook: True; strict (every graph launch's own geometry before replay): True
    largest threads per launch: nsys 4096000, hooks 4096000, hooks before replay 4096000
    graphs (nsys): graph 2: 314 nodes, 127 replays, 314 nodes seen at capture/node hooks, 314 at any hook, max 720896 threads; graph 5: 110 nodes, 127 replays, 110 nodes seen at capture/node hooks, 110 at

