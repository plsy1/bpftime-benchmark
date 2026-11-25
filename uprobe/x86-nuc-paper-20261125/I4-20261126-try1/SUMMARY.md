# I4-20261126-try1 — perf buffer size sweep at 256 KiB (x86 NUC), summary

Session ended 04:28:38, exit 0. 90 measured rows (+ 18 warm-up), all valid; noisy_core rows 0. Policy M, payload 256 KiB,
stage-4 counting loader `sslsniff-s4pb` (H3: the s4 loader plus SSLSNIFF_PB_PAGES; BPF object instruction-identical to
s4; build scripts/build-sslsniff-pb.sh), pages per perf buffer 16 (the default everywhere else), 64, 256; legs K, Kb
(SSLSNIFF_PB_WAKEUP=32), O, L3port, Ol, Dn; n = 5 paired reps, all 18 legs shuffled within each rep. Kernel buffers are
per CPU, and the single nginx worker stayed on one CPU (0 migrations in 83/90 rows; every K loss message names one
CPU), so the kernel had 1 × pages; in all three bpftime trees the producer writes a per-thread shard sized to the
consumer's mapping and the drain copies it into a consumer buffer of the same size, so bpftime had ≥ 2 × pages. Same
page count for every leg, i.e. bpftime got at least twice the kernel's capacity. Tables: delivery.txt, ratios.txt.

## Delivery (write bytes delivered / expected, median) and loss

| leg | 16 pages | 64 pages | 256 pages | lost reported (% of events, median) |
|---|---|---|---|---|
| K | 0.9969 | 0.9996 | 0.9999 | 0.28 → 0.04 → 0.005 |
| Kb | 0.9969 | 0.9985 | 1.0000 | 0.27 → 0.13 → 0.000 |
| Dn | 0.9966 | 0.9983 | 0.9997 | 0.31 → 0.15 → 0.02 |
| O | 0.1163 | 0.4164 | 0.9997 | 0 at every size |
| Ol | 0.1047 | 0.3594 | 0.9999 | 0 at every size |
| L3port | 0.1007 | 0.3450 | 0.9993 | 0 at every size |

O/Ol/L3port at 256 pages still miss events they never report: decomposing each byte deficit exactly into 16 KiB
records and 252-byte header writes gives O 0–521, Ol 0–728, L3port 20–712 per run. The same decomposition gives exactly
0 for every Dn row and a constant 18 for every K/Kb row (the readiness-probe request: 1 read + 16 records + 1 header).

## Throughput (GMR of rps, 95 % CI, n = 5)

| ratio | 16 pages | 64 pages | 256 pages |
|---|---|---|---|
| Dn / K | 1.251 [1.218, 1.285] | 1.293 [1.257, 1.330] | 1.296 [1.263, 1.329] |
| Dn / Kb | 1.179 [1.168, 1.191] | 1.198 [1.156, 1.241] | 1.196 [1.188, 1.205] |
| Ol / Kb | 1.167 [1.151, 1.183] | 1.127 [1.091, 1.164] | 1.093 [1.059, 1.128] |
| Dn / Ol | 1.010 [0.999, 1.023] | 1.063 [1.043, 1.083] | 1.094 [1.054, 1.136] |
| L3port / Dn | 1.068 [1.055, 1.081] | 0.999 [0.969, 1.031] | 0.979 [0.932, 1.029] |
| O / K | 1.031 [0.999, 1.064] | 1.019 [1.005, 1.032] | 1.010 [0.978, 1.043] |

Same leg, 256 vs 16 pages: Dn 1.032 [1.013, 1.050] (64: 1.038), K 0.996, Kb 1.017 [1.001, 1.034], Ol 0.953
[0.937, 0.968], O 0.976 [0.950, 1.002], L3port 0.946 [0.914, 0.979].

## Reading (corrected after the independent check)

1. Two things must be kept apart. Whether loss is reported does not depend on the buffer: official, upstream head and
   L3port never report it (in all three trees `software_perf_event_buffer::output_data` ignores the failed append,
   returns 0 and writes no PERF_RECORD_LOST: official perf_event_handler.cpp:385, olatest :486, l3port :412), and a
   residue still goes missing unreported at 256 pages. How much is lost does depend on the buffer: at 256 KiB they
   deliver 10–12 % of the data with sslsniff's default 16 pages, 35–42 % with 64, ≥ 99.93 % with 256. At the same 16
   pages (and despite ≥ 2 × the kernel's capacity) the kernel loses 0.28 % and reports it while upstream loses ~88 %,
   so the size of the loss belongs to upstream's drain design (I3 suspected the 1 ms poll), not to a mis-set parameter.
   Tern delivers ≥ 99.66 % (median; ≥ 99.44 % per run for K/Kb) at every size and reports its loss.
2. Once they deliver (256 pages) the lossy legs are slower: Ol 0.953 [0.937, 0.968], L3port 0.946 [0.914, 0.979],
   O 0.976 [0.950, 1.002] (not significant) vs 16 pages. Page count also changes buffer footprint (64 KiB → 1 MiB) and
   speeds Dn up, so this is not a pure "cost of delivering".
3. Tern vs upstream head at 256 KiB: Dn / Ol 1.010 at 16 pages (I3's Dn / Ol at 256 KiB, from Dn/Kb ÷ Ol/Kb: 1.016)
   and 1.094 [1.054, 1.136] at 256 pages. L3port vs Tern at 256 pages: no difference detected (0.979, CI −7 % to +3 %;
   L3port bimodal across reps).
4. Kernel: no buffer effect detected for K (0.996 [0.973, 1.020]); Kb +1.7 % [0.1, 3.4] at 256 vs 16 pages. Tern +3.2 %
   (256) / +3.8 % (64); mechanism not measured (its delivered work rises by only ~0.3 % of events).
5. Tern's lead over the kernel holds at every size: Dn / Kb 1.18–1.20, Dn / K 1.25–1.30 (with Tern's capacity
   advantage noted above).
6. For the paper (256 KiB only): with sslsniff's default buffer upstream silently drops most large records; with 256
   pages it delivers, Ol and L3port then ~5 % slower, and it still reports no loss (a code defect, not a setting).
   Whether larger buffers fix delivery at 1–64 KiB (up to 10× the event rate) was not measured.

## Flags

- n = 5 per cell (CIs wide, e.g. Dn/Ol at 256 pages ±4 %).
- Page counts are equal across legs, not equal record capacity (kernel 1 × pages on the worker's CPU vs bpftime shard +
  consumer buffer ≥ 2 × pages; see the header).
- missing_ev − lost (−18…0 for Tern, 0…18 for K) is an artifact of missing_ev's 16 KiB ceiling (see the exact
  decomposition above); not loss.
- 29 ratios on n = 5, one session, no multiplicity correction. Order effect negligible (×1.004 ± 0.012 first to last
  slot); nginx core 2 vs 6 ×1.003 ± 0.008.
- Tern at 256 pages uses ~32–34 MiB of the 50 MiB shm (consumer + overflow shard for 16 CPUs): near the limit.
- Independent check (subagent, own scripts): every table number reproduces; readings corrected as above.
- Payload 256 KiB only; smaller payloads with larger buffers were not measured.
