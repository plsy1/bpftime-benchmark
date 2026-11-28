# S8: measurement and correctness against today (rules, written before any S8 run, 2026-11-28)

Plan: `PLAN-stage1.md` S8. Code: proto/map-model-70f7bc1f at a3db63689 (dfbc4c603 plus one log line: the
delivery's per-hash-map summary at exit, the only code change S8 makes; committed before the
first run). Agent and server for every item: the worktree build
`/home/jetson/src/bpftime-proto-70f7/build-cuda` (host-path tools and 1533 tools on the same
build: the host-path tools hold no device-model map, so the routing pass leaves them alone, as on
the build hash-current measured).

## Set-up for every timed item

- Clocks: cpu5 1728000/1728000 and GPU 1020 MHz, checked by the harness before every item (it
  stops otherwise); apt's daily services idle (waited for).
- The GPU lock (`flock -o`), one GPU process at a time; every run under `timeout`; `ps` checked
  for leftovers before a pass; no containers; `/dev/shm` holds no `bpftime*` segment before a
  pass (run.sh refuses a name that exists).
- Item order shuffled per pass (seed = the pass label). Each pass's driver log keeps the board
  state before and after every item.

## A. pt_victim: 1533 against the host path (passes v1, v2, v3)

Sizes as hash-current: D = `pt_victim 1 0` (pt_kdiv, 100,096 threads), G = `pt_victim 0 2`
(pt_kflat through a graph, 4 launches x 14,400 = 57,600 threads), D4 = `4 0`, G8 = `0 8`. Every
item on all four sizes in every pass.

| item | host path (BPF_MAP_TYPE_HASH) | 1533 |
|---|---|---|
| empty | `empty2_test` | (same) |
| pre: lookup + add of a prefilled key (loader inserts keys 0, 1) | `hs_pre_thread_kc_test` | `hd_pre_thread_kc_test` |
| miss: lookup of a key never inserted | `hs_miss_thread_kc_test` | `hd_miss_thread_kc_test` |
| cnt-t: the counter idiom, every thread, one key per kernel | `hs_cnt_thread_kc_test` | `hd_cnt_thread_kc_test` |
| cnt-t-sm: the same, key per SM | `hs_cnt_thread_ksm_test` | `hd_cnt_thread_ksm_test` |
| cnt-b-blk: thread 0 of each block, key per block | `hs_cnt_block_kblk_test` | `hd_cnt_block_kblk_test` |
| cnt-b: thread 0 of each block, one key per kernel | `hs_cnt_block_kc_test` | `hd_cnt_block_kc_test` |
| cnt-l: the leader thread of each launch | `hs_cnt_leader_kc_test` | `hd_cnt_leader_kc_test` |

(`hd_cnt_block_kblk_test` and `hd_cnt_block_kc_test` are built for S8 by `build_hs.sh`, as the
other `hd_*` objects.)

**Validity of a run** (an invalid run is excluded and reported, not re-run in the same pass):
client_rc 0, server_rc 0; for a 1533 item the agent log has "Device hash maps in" with at least
one inline lookup and no "Unable to compile" / "requires call prototype"; for a host item no
"Device hash maps in" line with a non-zero count.

**Counts** (from `chk`, the device's own exact adds, and the map's dump):
- 1533 pre: value(k) = probe runs(k) (chk 4), no NULL (chk 1 = 0), companion EEXIST = E2BIG = 0.
  Expected exact; any difference is a failure of the build, reported before anything else.
- 1533 miss: map empty, hits 0, misses = runs.
- 1533 cnt-*: value(k) = hits(k) + successful inserts(k), and over the map:
  sum = runs - EEXIST - E2BIG, with the companion's EEXIST + E2BIG = the tool's failed inserts
  (chk 3). Expected exact.
- Host path: as hash-current (pre exact; cnt-* losses reported as numbers; a failed host update is
  counted by its non-zero result).

**Per operation by slope:** per call = (median ms(D4) - median ms(D)) / (calls(D4) - calls(D)),
and G8 against G, calls from chk 4 (per-thread items: one lookup per run; the counter idiom adds
an insert on a miss). Reported with the resolution the run-to-run spread of the D and D4 times
gives (ms spread / extra calls): for 1533 the per-thread cost is expected below that resolution
on these sizes (llama.cpp below measures it). A slope that differs by more than 10% between
passes is reported, not re-run.

## B. llama.cpp (llama-bench, TinyLlama Q4_K_M, -r 5, graphs on as built)

**B1. Cost on top of 1531** (tg128, mul_mat_vec_q probed, every thread, constant key; passes b1, b2;
`run_meas3.sh`): items `tg:base`, `tg:lat_mmvq` (the histogram in a 1531 sum map),
`tg:latd_mmvq` (the same tool with the histogram in a 1533 map: lookup, add on a hit, insert 1
with NOEXIST on a miss; companion `hist_st`), and the re-check of S0.2 on this build:
`tg:lat_mmvq@s8_dh_route`, `tg:lat_mmvq@s8_hd_t0`. S0.2's override files are keyed by the sha256 of
the patched PTX, which S3 changed (the trampoline's 1533 branches), so they are regenerated:
lat_mmvq's patched PTX dumped on this build (`BPFTIME_CUDA_PTX_DUMP_DIR`, one untimed run), then
`s0/make_s02.py` on it with this build's `devhash_ptx.h` (`s8_dh_route`: the hand-routed form;
`s8_hd_t0`: the dump unchanged, the override's control). An override item is valid only if the
agent logs "PTX override".
- Cost per thread = (s per run(item) - s per run(base)) / threads per run (3,137,339,392 + 1/128).
- **Expected:** latd - lat = S0.2's +0.036 to +0.038 ns per thread (the routed form; C2's
  0.034-0.037 was an inline form without calls).
- **Rule:** latd - lat more than 0.02 ns above S0.2's number (i.e. above 0.058) in both passes:
  compare latd's SASS (patched PTX dumped with `BPFTIME_CUDA_PTX_DUMP_DIR`, compiled for sm_87)
  with S0.2's `dh_route.mmvq.sass` before reporting the number.
- **Correctness:** lat: the hist sum = WANT (5 C + C/128, C = 3,137,339,392) and bucket 194 = 0;
  latd: the sum of hist's values + the companion's EEXIST + E2BIG = WANT, bucket 194 absent or 0;
  an item failing its check is invalid.

**B2. Against today's host path** (rms_norm, p32; passes l1, l2; the hash-current llama script with
the 1533 tools added, on the worktree build): `p32:base`, `p32:link` (hs pre on rms_norm, every
thread, key 0 prefilled), `p32:dlink` (hd pre, the same), `p32:blk` (hs cnt-b on rms_norm),
`p32:dblk` (hd cnt-b). Per call = (s per run - base s per run) / calls per run (chk 4 over 6
runs). Counts: link and dlink exact (map value = runs); blk losses reported; dblk:
sum = runs - EEXIST - E2BIG.

**B3. The per-launch counter on the four tg128 kernels** (same passes): `tg:base`, `tg:ldrf`
(hs_none_leader: the rate filter and the run count only), `tg:ldr` (hs cnt-l), `tg:dldr` (hd
cnt-l). Per call = (s per run(x) - s per run(ldrf)) / calls per run. Counts: ldr exact
(sum = runs); dldr: sum = runs - EEXIST - E2BIG (expected EEXIST = 0: one thread per launch).

## C. Delivery cost (passes c1, c2)

`hd_cnt_thread_kmod512_test` and `hd_cnt_thread_kmod1024_test` (built for S8: key = linear thread
index mod N, `max_entries` N, so all N rows go live in the first launch) on `pt_victim 4 0`; the
agent's exit summary gives each hash map's deliveries, total and longest snapshot time (the new
log line). Reported: µs per delivery (total / deliveries) and the longest, at 512 and 1024 rows.
- **Rule:** above 2 ms per delivery (a tenth of the 20 ms busy period): reported as a cost of the
  delivery schedule.
- Count check: live = N, count = N, sum = runs - EEXIST - E2BIG.

## D. Re-checks on the final build

- S0.2: the B1 override items above (same pass as latd).
- S2.12: `runtime/unit-test/maps/check_devhash_orders.sh` passes.
- S3.11 and every unit test of S1-S6: `[devhash-layout]`, `[hash-map-routing]` and the whole
  `bpftime_nv_attach_tests` (the known `[ptx-failure]` failure excepted, as on plain dev),
  `[gpu-devhash-cpu]`, `[gpu-devhash-device]` (all pass).

## Not claimed

- No x86 numbers; one GPU (Orin, sm_87).
- The pt_victim slopes for 1533 are a resolution bound, not a cost; B1 is the cost.

## Addendum (written after A and C, before their follow-up runs, 2026-11-28 15:35)

A and C showed the tools' own side counts dominating: every selected thread adds twice to `chk`, a
GPU array in registered host memory (1503, created as 1513), on a few shared words. Two pieces of
evidence:
- 1533's slope was about 1 µs per call even for a lookup of a missing key (no atomic in the hash
  path), and about 112 ms per extra launch of pt_kdiv;
- in-process deliveries took 40-110 ms while the exit delivery (application done) took 0.25 ms at
  512 rows. The snapshot waits for the probed kernel.

So A's 1533 slopes measure `chk`, not the hash map, and C's in-process times measure the wait
behind the application's kernel. Follow-up, passes n1 and n2:
- **Tools:** `-DHS_NOCHK` makes `count()` a no-op (`hdn_*`, `hsn_*` objects; otherwise as A).
- **Items:** empty, `hdn-pre`, `hdn-miss`, `hdn-cnt-t` and the host reference `hsn-pre`, on D, D4, G,
  G8; delivery cost `hdn-kmod512`, `hdn-kmod1024` on D4.
- **Quantities:**
  - per call by slope as in A, now without the side counts;
  - delivery cost from the per-hash exit summary: per delivery with the GPU free (the exit
    delivery's line) and per in-process delivery.
- **Counts** (no `chk`):
  - pre: value(k) = threads x launches (D 100,096; D4 400,384; G 57,600; G8 230,400);
  - miss: map empty;
  - cnt-t: sum + EEXIST + E2BIG = threads x launches.
- **Reported from A and C as they are:**
  - A's counts and its host-path slopes (side counts included);
  - the delivery's first set-up (about 0.62 s: the hash snapshot module's load and warm-up, once
    per process);
  - the exit delivery of a 262,144-entry map: 7.8 ms, above the 2 ms rule, so a cost of the
    schedule for large maps (the CPU passes over 524,288 slots).

## Addendum 2 (written after B1-B3, before their follow-up runs, 2026-11-28 16:25)

Two of B2/B3's numbers need splitting before they can be reported:
- **dlink includes the side counts.** It runs about 1.3 µs per call on rms_norm, every thread,
  and the side counts are included (as in A).
- **dldr minus ldrf can't be per-call hash cost.** It is +0.085 to +0.093 s per run, against
  15,616 hash calls per run. That is about 6 µs per call, too much for an inline lookup and add.
  The likely part is the routing pass, which runs on these modules only when a device-model map
  exists, acting on every thread's probe path (the rate filter's helper calls).

Follow-up, passes l3 and l4, `run_s8_llama.sh`:
- **Items:** `p32:base`; `p32:dlinkn` (`hdn_pre_thread_kc_rms`: dlink without the side counts);
  `p32:linkn` (`hsn_pre_thread_kc_rms`: the host path, likewise); `tg:base`; `tg:ldrf`; `tg:dldr`;
  `tg:dnone` (`hd_none_leader_kc_llama4`: the 1533 map and its companion declared, the same rate
  filter and run count, no hash operation; the routing pass runs, nothing is rewritten into a
  devhash call).
- **Quantities:**
  - dlinkn and linkn per call (no side counts);
  - dnone - ldrf = the routing pass's effect on the filter's path;
  - dldr - dnone = the hash operations, per call and per thread.
- **Counts:**
  - dlinkn: value = probe runs, from the map alone; the run count is not kept, so it is the
    rms_norm thread count of the run, taken from dlink's count in l1/l2: 4,233,216;
  - linkn: sum = 4,233,216;
  - dnone: the map stays empty.
