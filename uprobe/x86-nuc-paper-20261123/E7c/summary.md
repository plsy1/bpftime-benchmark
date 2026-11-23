# x86 E7c: cache and TLB counters on the nginx worker (counterpart of Jetson E7c)

Session E7c-cache-20261124-try1, exit 0, guards ok. Policy M, 16 B, wrk -d 20, perf stat on the nginx worker for t=2..18 s, 3 reps x 8 legs x 2 counter groups, no multiplexing. Per-request values use the access.log window. Not part of the throughput statistics. Raw rows: raw/E7c-cache-20261124-try1/rows.jsonl on the NUC (summary below).

Interference note: the Jetson session ran one read-only ssh command on the NUC at 08:48:55 JST, which took less than 1 s. The nearest leg is D@S1 round 1 rep 1, whose wrk ran from 08:48:35 to about 08:48:55; its perf window ended at about 08:48:53. Its values are 36,117 cycles/req, against 36,474 and 36,081 for its siblings. No rep was affected, and nothing was rerun.

Event mapping: L1I misses = cpu_core/L1-icache-load-misses/; L2 misses = l2_rqsts.miss (code + data); L2 fills = l2_lines_in.all; iTLB and dTLB = page walks (STLB misses) on Intel. Arm's iTLB/dTLB refill events count L1 TLB misses, so the TLB rows are not comparable across the two machines. Arm's L2D_CACHE_REFILL counts all refills of the unified L2.

## Per request (median of reps; n per event in parentheses)

| leg | L1-dcache-load-misses | L1-icache-load-misses | cycles | dTLB-load-misses | iTLB-load-misses | icache_data.stalls | instructions | l2_lines_in.all | l2_rqsts.code_rd_miss | l2_rqsts.miss |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| B | 860 (3) | 4,398 (3) | 31,821 (6) | 0 (3) | 58 (3) | 2,194 (3) | 60,281 (6) | 471 (3) | 13 (3) | 476 (3) |
| K@S1 | 1,025 (3) | 5,421 (3) | 48,649 (6) | 16 (3) | 68 (3) | 2,637 (3) | 69,466 (6) | 511 (3) | 15 (3) | 504 (3) |
| D@S1 | 1,032 (3) | 4,864 (3) | 36,099 (6) | 1 (3) | 62 (3) | 2,217 (3) | 65,409 (6) | 486 (3) | 14 (3) | 489 (3) |
| D@S4 | 1,177 (3) | 5,295 (3) | 38,926 (6) | 1 (3) | 65 (3) | 2,537 (3) | 71,384 (6) | 512 (3) | 15 (3) | 515 (3) |
| Odiag@S4+NO_PIN | 1,184 (3) | 5,603 (3) | 44,662 (6) | 1 (3) | 59 (3) | 2,676 (3) | 77,543 (6) | 514 (3) | 15 (3) | 518 (3) |
| Odiag@S4 | 1,381 (3) | 6,663 (3) | 56,849 (6) | 1 (3) | 71 (3) | 3,307 (3) | 105,640 (6) | 531 (3) | 17 (3) | 532 (3) |
| Odiag@S1 | 1,022 (3) | 4,823 (3) | 35,489 (6) | 1 (3) | 59 (3) | 2,236 (3) | 64,828 (6) | 482 (3) | 13 (3) | 491 (3) |
| K@S4 | 1,181 (3) | 6,353 (3) | 59,580 (6) | 16 (3) | 93 (3) | 3,035 (3) | 79,549 (6) | 619 (3) | 16 (3) | 604 (3) |

max spread (max-min)/median over all legs and events: 81.5%

## Deltas per request

| step | L1-dcache-load-misses | L1-icache-load-misses | cycles | dTLB-load-misses | iTLB-load-misses | icache_data.stalls | instructions | l2_lines_in.all | l2_rqsts.code_rd_miss | l2_rqsts.miss |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| K@S1 - B | +166 | +1,023 | +16,828 | +15 | +11 | +443 | +9,185 | +39 | +2 | +27 |
| Odiag@S1 - B | +163 | +425 | +3,668 | +0 | +1 | +42 | +4,548 | +11 | +0 | +15 |
| D@S1 - B | +172 | +466 | +4,278 | +0 | +5 | +23 | +5,128 | +14 | +1 | +13 |
| K@S4 - B | +321 | +1,955 | +27,759 | +15 | +35 | +842 | +19,268 | +148 | +4 | +128 |
| Odiag@S4 - B | +522 | +2,265 | +25,028 | +0 | +13 | +1,113 | +45,359 | +60 | +5 | +56 |
| D@S4 - B | +317 | +897 | +7,105 | +1 | +7 | +343 | +11,103 | +41 | +2 | +39 |
| Odiag@S4 - Odiag@S4+NO_PIN | +197 | +1,060 | +12,187 | -0 | +12 | +630 | +28,097 | +17 | +2 | +15 |

Per pin sequence (2 per request): L1-dcache-load-misses +99, L1-icache-load-misses +530, cycles +6,093, dTLB-load-misses -0, iTLB-load-misses +6, icache_data.stalls +315, instructions +14,048, l2_lines_in.all +8, l2_rqsts.code_rd_miss +1, l2_rqsts.miss +7

The 81.5% max spread is on near-zero counts (dTLB 0-1); cycles and instructions vary by 3% or less across reps.

## Cross-platform, per pin sequence (Odiag@S4 - Odiag@S4+NO_PIN, /2)

| counter | x86 | Orin (Jetson E7c) |
|---|---:|---:|
| instructions | 14,048 | 13,168 |
| L1I misses | 530 | 642 |
| L1D misses | 99 | 90 |
| L2 misses (x86 l2_rqsts.miss) / L2 refills (Orin L2D_CACHE_REFILL) | 7 (fills 8) | 576 |
| cycles | 6,093 | about 12,500 (Jetson E7) |
| cycles per instruction | 0.43 | about 0.95 |

## Cross-platform, output (S4 - B), instructions per request

| | x86 | Orin |
|---|---:|---:|
| K | 19,268 | 23,982 |
| Odiag | 45,359 | 50,440 |
| D | 11,103 | 11,440 |

## Reading

- Both machines execute nearly the same instructions for the pin sequence (14.0k vs 13.2k). Both miss L1I in nginx about as often (530 vs 642 per pin), because the path is cold there; in the pinbench loop it stays cache-hot.
- The difference is the next level. On x86 the misses hit the 1.25 MB P-core L2: about 8 lines per pin come from beyond L2. On the Orin, 576 lines per pin are refilled into L2 from L3 or DRAM. That fits the Orin's in-nginx premium (about 2.3x the idle pin cost; CPI 0.95 vs 0.43), and it explains why idle microbenchmarks give the same cost on both machines. Miss latency itself was not measured. The comparison rests on two vendors' events with similar but not identical definitions.
- The output stage runs similar instruction counts on both machines (within 5-25%), so the cross-platform cycle gap is microarchitectural, not a different code path.
- Hook (S1 - B): x86 bpftime adds about 4.5-5.1k instructions and 425-466 L1I misses per request, against the kernel's 9.2k and 1,023. TLB page walks are near zero for bpftime on x86. On the Orin, bpftime's L1 TLB refills exceed the kernel's (Jetson E7c), but the two TLB metrics are not comparable.
