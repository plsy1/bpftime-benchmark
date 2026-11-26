# C4: the key directory's claim protocol by hand (2026-11-26)

Rules: `../RULES-round1.md`, section C4 (written before any run). Design: `results/gpu/hash-design-20261126/DESIGN.md`, "Operations (stage one)" and "Basis".
A standalone CUDA program (`src/c4.cu`), no bpftime code, builds or git touched. Orin Nano (sm_87, 8 SMs), CUDA 13.2 (V13.2.86), driver 595.78, kernel 6.8.12-1021-tegra-btf, cpu5 at 1728000. No other GPU work ran.

## Verdict under the C4 rule

- **The protocol: zero violations.**
  - The two rule runs: max_entries 128 and max_entries 48, each 10^6 threads x 10 launches.
  - 3,900 further trials of the protocol build (fresh zeroed memory, a new hash seed each), plus 1,300 trials of control 3, which differs only in the publish.
  - Per the rule, this reads as "consistent with the model", not as proof. C4 does not stop the build.
- **Control 1, claim by a plain `st.global.b64` instead of the CAS: fails**, in 1/1 rule run and 200/200 trials at 128.
  - It is caught by the sums, by several winners per key, and by orphaned adds.
  - It is **not** caught by "key in two slots", which the rules named as the expected signature: 0 times in 2,200 trials at max 128, 64 and 48.
  - So the test cannot see the two-slot form of this fault. It sees the fault through the sums and the winner counts.
  - An extra control shows the two-slot check can fire (plain store and no reservation together: 2,071 key-trials in 200 trials).
- **Control 2, no bound reservation at max_entries 48: fails** in 1/1 rule run and 200/200 trials. 64 slots were claimed, over the limit of 48.
- **Control 3, publish with `st.relaxed.gpu` instead of `st.release.gpu`: not observable by this test.**
  - The final scan cannot see it by construction: 0/1 rule run, 0/1,000 trials.
  - The concurrent reader did not see it either: 0/300 trials, while it overlapped every trial and saw all 64 claims turn live in each.
  - The reader is too slow to catch it. The first row sum it took after a header turned live already held at least 12 adds (the smallest over 300 trials).

## Findings beyond pass/fail

1. **E2BIG while the map is half empty.**
   - Setup: max_entries 128, 64 keys. Every trial ends with 64 slots claimed and `bound` = 64.
   - Yet every trial counted E2BIG: 1,000/1,000 trials, 7,061-11,695 per trial (rule run: 7,920).
   - It all comes in launch 1; per-launch tallies show 0 E2BIG and 0 EEXIST in launches 2-10.
   - The cause is in the protocol as written, which reserves at an empty slot *before* the CAS:
     - A refusal needs `bound >= 128`. Until `bound` first reaches 128, no refused thread has added anything, so at that moment 128 accepted reservations were held at once.
     - At most 64 of those can belong to winners (one per key). So at least 64 were held by threads that never won and gave theirs back later. This is inference from the protocol, consistent with the counters.
     - Counters from the rule run:
       - 252 reservations were accepted in all: 64 winners, and 188 given back, 185 of them by threads that lost the CAS to the same key.
       - Refused: 4,222 threads read `bound >= 128` before adding, and 3,698 more were refused after their own add (which they undo).
   - With max_entries 64 (exactly 64 keys), all 64 keys still get in, with 8,530-20,256 E2BIG per trial.
   - The equation still holds, because every E2BIG is counted and its increment is lost. But bpf returns E2BIG only when the map is full.
   - Whether this matters is a design question. I did not change the protocol.
2. **The bounded wait never ran out, but its bound does not limit elapsed time.**
   - In the bound's own units (requested nanosleep), the longest wait asked for 18,368 ns of the 100,000 ns bound (16,320 ns in protocol builds). 0 unstable claims in every protocol run.
   - Elapsed time (globaltimer) is much longer: up to 84.3 us. The re-reads dominate, not the sleeps.
   - In the build with the bound at 0 (no sleep at all), one wait still took up to 22.7 us.
   - So under this contention the wait is bounded in sleeps, not in time.
   - Not measured (a hypothesis): `bound` and `next_row` sit in one 32-byte sector here, and the winner's `atom.add(next_row)` may queue behind the reservation traffic.
3. **"EEXIST, then not NULL" held, and the wait is what holds it.**
   - Every thread that got EEXIST looked K up again at once. NULL came back 0 times in all protocol runs (millions of EEXIST).
   - With the bound at 0, the unstable path ran: 145,044 unstable EEXIST in 300 trials. Their counts matched the tallies and the sums held.
   - 28,827 of those (19.9%) were followed by NULL from the thread's own lookup. A non-unstable EEXIST was still never followed by NULL.
4. **Bookkeeping held in every protocol trial.**
   - `bound` = claimed slots, and `next_row` = live headers.
   - Exactly one winner per inserted key; 0 for keys never inserted and for prefilled keys.
   - Every probe chain from home to slot had no empty slot. Rows beyond `next_row` stayed zero.
5. **Keys inserted beforehand** (the design's CPU copy: header written live, value 5 in shard 0 of its row): 16 of the 64 keys, 300 trials. The sums including the prefill value held, and no GPU thread won a prefilled key.
6. **The "lost to another key" path was exercised** (keep the reservation, walk on): 16,112 times over 1,000 trials at 128. Keys ended off their home slot 7,510 times, and the longest probe distance was 29 (56 at max 64).

## Numbers (absolute values)

Rule runs: one process each, 10 launches x 1,000,000 threads (grid 3907 x 256, threads with tid >= 10^6 return). 64 keys, 15,624-15,626 attempts per key per launch.

| run | claimed / max | left claiming | delivered | EEXIST | E2BIG | unstable | longest wait, elapsed / requested sleep | violations |
|---|---:|---:|---:|---:|---:|---:|---|---|
| r1 protocol, 128 | 64 / 128 | 0 | 9,986,331 | 5,749 | 7,920 | 0 | 54.7 us / 6,080 ns | none |
| r2 protocol, 48 | 48 / 48 (16 keys never in) | 0 | 7,487,078 | 4,100 | 2,508,822 | 0 | 56.7 us / 1,984 ns | none |
| r3 control 1, 128 | 54 / 128 (bound 128, next_row 128) | 0 | 8,427,090 (rows hold 8,427,545) | 4,074 | 1,568,381 | 0 | 39.6 us / 0 ns | sums 40 keys, winners 40 keys, orphan adds, bookkeeping, 125 EEXIST-then-NULL |
| r4 control 2, 48 | **64** / 48 | 0 | 9,987,901 | 12,099 | 0 | 0 | 50.9 us / 6,080 ns | claimed > max |
| r5 control 3, 128 | 64 / 128 | 0 | 9,985,246 | 3,964 | 10,790 | 0 | 58.6 us / 10,176 ns | none |

- Delivered = attempts - EEXIST - E2BIG, per key and in total, in r1, r2 and r5.
- r2: launch 1 had 258,813 E2BIG; launches 2-10 had 250,001 each, all on the 16 keys never in.
- The 64-line per-key tables are in the appendix.

Further runs, reported separately (not named by the rules):

| run | trials | trials with a violation | EEXIST per trial | E2BIG per trial | unstable | longest wait, elapsed / requested |
|---|---:|---:|---|---|---:|---|
| t1 protocol, 128 | 1,000 | 0 | 4,138-7,575 | 7,061-11,695 | 0 | 73.9 us / 12,224 ns |
| t2 protocol, 48 | 1,000 | 0 | 1,908-6,807 | 2,505,634-2,515,062 | 0 | 77.3 us / 16,320 ns |
| t3 protocol, 64 | 1,000 | 0 | 2,800-7,922 | 8,530-20,256 | 0 | 84.3 us / 14,272 ns |
| t4 protocol, 128, 16 keys prefilled | 300 | 0 | 1,443-6,331 | 4,470-9,140 | 0 | 65.5 us / 14,272 ns |
| t5 protocol, 128, with reader | 300 | 0 | 2,465-6,655 | 7,389-10,166 | 0 | 64.6 us / 16,320 ns |
| t14 protocol, 128, wait bound 0 | 300 | 0 | 3,193-6,740 | 7,511-13,009 | 145,044 in all (28,827 then NULL) | 22.7 us / 0 ns |
| t6 control 3, 128, with reader | 300 | 0 | 700-5,652 | 6,501-11,588 | 0 | 72.1 us / 12,224 ns |
| t7 control 3, 128 | 1,000 | 0 | 2,095-6,463 | 7,752-14,893 | 0 | 77.1 us / 18,368 ns |
| t8 control 1, 128 | 200 | 200 (no key in two slots) | | | | |
| t9 control 2, 48 | 200 | 200 (claimed > max) | | | | |
| t10 control 1, 64 | 1,000 | 1,000 (no key in two slots) | | | | |
| t11 control 1, 48 | 1,000 | 1,000 (no key in two slots) | | | | |
| t12 control 1+2, 128 | 200 | 200 (key in two slots: 2,071) | | | | |
| t13 control 1+2, 48 | 200 | 200 (key in two slots: 3,797) | | | | |

- In every protocol and control 3 trial, EEXIST and E2BIG on keys that ended in a slot came only in launch 1.
- Single runs r6-r9 (reader at 128, control 3 with reader, prefill 16, max 64): no violations. They are in the appendix.

Concurrent reader (t5 / t6):
- It overlapped all 300 / 300 trials (it started before the first inserter block in each; 4.6 us before, in trial 0).
- It saw 28,112 / 25,114 claiming headers and 19,200 / 19,200 transitions to live (64 per trial).
- It summed 3,127,214 / 3,050,258 live rows, with 0 zero sums and 0 sums that went down.
- The smallest sum at a first sight of live was 29 / 12.

Run time: about 6.5 ms of GPU time for the 10 launches at 128 (9.4 ms at 48). The kernel time is not the subject of C4.

## What was built

### Directory, rows, control words
- Directory: S = next_pow2(2 x max_entries) u64 headers (256 at 128; 128 at 64 and 48), cudaMemset 0 before launch 1, not re-zeroed between launches.
- Header layout: key in bits 63..32, row in bits 31..3, state in bits 2..0 (0 empty, 1 claiming, 2 live).
- Home slot: `((u64)K ^ seed) * 0x9E3779B97F4A7C15 >> (64 - log2 S)`, with linear probing. Trial 0's seed is 0x0123456789abcdef; trial t uses splitmix64(seed + t).
- Rows: S rows x 256 shards x u64, zeroed. Only max_entries rows are used by the protocol; the slack is for the controls, with an in-kernel guard counted as "row overflow" (0 in all protocol runs).
- Shard: `(smid & 7) * 32 + laneid`.
- Control words: `bound` (ctl[0]) and `next_row` (ctl[1]), plus branch counters. Per-key EEXIST, E2BIG and unstable counters are indexed by the key's index j.
- Each thread also stores a one-byte outcome per launch (hit, won, EEXIST live/waited/unstable, E2BIG). The host tallies these by key and by launch, and checks them against the atomic counters.

### Workload
- `j = (tid * 2654435761u >> 26) & 63`, and `K = 1000 + 7919 * j`.
- Each thread: `v = lookup(K)`; on a hit, `red.global.add.u64 [v], 1`; otherwise `insert(K, 1, NOEXIST)`.
- After an EEXIST, the thread calls `lookup(K)` once more and counts NULL ("EEXIST then NULL"). It adds nothing.

### insert(K)
- Walk from home:
  - A live K: EEXIST.
  - A claiming K: bounded wait, then EEXIST (unstable if still claiming).
- At an empty slot:
  - If no reservation is held: `ld.relaxed(bound) >= max` gives E2BIG; else `atom.add(bound, 1)`, and old >= max gives `atom.add(bound, -1)` and E2BIG.
  - Then `atom.cas` empty -> {K, claiming}:
    - Won: `row = atom.add(next_row, 1)`, red.add 1 into the caller's shard of the row, `st.release` {K, row, live}.
    - Lost to K: give back, bounded wait, EEXIST.
    - Lost to another key: keep the reservation, walk on.

### Bounded wait
- Re-read the header with `ld.relaxed`.
- `nanosleep` backs off 64, 128, ... up to 2,048 ns, until 100,000 ns of requested sleep (`-DWAIT_BOUND_NS`, 0 in the `proto_nowait` build).
- Both the elapsed time (globaltimer) and the requested sleep are recorded.

### Reader
- Thread t watches slot t, with the design's acquire pattern: `ld.relaxed.gpu.global.b64` of the header, then `fence.acq_rel.gpu`.
- On live, it sums the row's 256 shards with `ld.relaxed`.
- A sum of 0, or a sum that goes down, is a violation.
- It stops when all inserter blocks have finished, or after 20 s (never reached).

### Readings and choices of mine (not in the rules)
- **Give-backs:**
  - A thread that holds a reservation gives it back on every exit that did not win. That covers the stated "lost to K" case, and also meeting live or claiming K later in the walk after "lost to another key".
  - A thread never reserves twice.
  - The check `bound == claimed slots` depends on this.
- **Extra checks:**
  - exactly one winner per inserted key;
  - outcome tallies equal the atomic counters;
  - no empty slot on a key's probe chain;
  - no two live headers with the same row;
  - no adds in rows that no live header points to;
  - no unknown key or state in a header;
  - a waited-on slot never changes key;
  - "EEXIST then NULL" = 0 (a NULL after an *unstable* EEXIST is reported, not a violation).
- **The controls are judged by the same checking code as the protocol, with one exception.** The check `bound == claimed slots` is skipped in builds without the reservation (control 2, control 1+2), where `bound` is never touched. `next_row == live headers` is still checked there.
- **Warm-up launch:**
  - Without it, the reader and the inserters did not overlap. The first inserter launch started only after the reader's 20 s timeout.
  - So one inserter launch with no work threads runs before any trial, and with it they overlap.
  - Inferred, not verified: the cause is the inserter's 88-byte local frame, which the driver must size before the first launch.
- **Timings are this test's, not the real helper's.**
  - `insert` and `wait_live` are `__noinline__` and take the context by reference, so it lives in local memory (88-byte frame).
  - The claim window, and so the wait lengths, may be longer than in a real helper or inline path.
- **Additions of mine:** the fourth control (plain store and no reservation together, only to show the two-slot check can fire) and the wait-bound-0 build.

## The PTX used

Helpers in `src/c4.cu`; every directory, control word, row and counter access goes through them on a `.global` address. The kernel converts each cudaMalloc pointer once:
```
cvta.to.global.u64 %0, %1;
```
Device operations (exact asm strings):
```
ld.relaxed.gpu.global.b64 %0, [%1];              // header walk, bound pre-check, wait re-read, reader loads
atom.relaxed.gpu.global.add.u64 %0, [%1], %2;    // bound +1 / -1 (give back: %2 = ~0), next_row +1
atom.relaxed.gpu.global.cas.b64 %0, [%1], %2, %3; // claim: empty (0) -> {K, claiming}
red.global.add.u64 [%0], %1;                     // row shard adds (hit and winner), counters
st.release.gpu.global.b64 [%0], %1;              // publish {K, row, live}
nanosleep.u32 %0;                                // bounded wait
mov.u32 %0, %%smid;   mov.u32 %0, %%laneid;      // shard
fence.acq_rel.gpu;                               // reader, after its ld.relaxed of a header
red.relaxed.gpu.global.max.u64 / .min.u64 [%0], %1;  // timestamps and wait maxima (bookkeeping only)
mov.u64 %0, %%globaltimer;
```
Controls:
```
st.global.b64 [%0], %1;               // control 1 and 1+2: claim by plain store, then "won"
st.relaxed.gpu.global.b64 [%0], %1;   // control 3: publish
(control 2: the bound pre-check and reservation are compiled out)
```

### The emitted PTX for insert's claim path
From `build/c4_proto.ptx` (debug `.loc` lines removed; Ctx fields come from local memory):
```
$L__BB1_2:
	ld.relaxed.gpu.global.b64 %rd18, [%rd19];        // header at slot i
	and.b64  	%rd6, %rd18, 7;
	setp.eq.s64 	%p2, %rd6, 0;
	@%p2 bra 	$L__BB1_8;                           // empty
	...
$L__BB1_8:
	...
	ld.relaxed.gpu.global.b64 %rd32, [%rd33];        // bound pre-check
	...
	atom.relaxed.gpu.global.add.u64 %rd37, [%rd7], %rd39;   // reserve (+1)
	...
$L__BB1_13:
	mov.u64 	%rd49, 0;
	atom.relaxed.gpu.global.cas.b64 %rd47, [%rd19], %rd49, %rd3;   // %rd3 = K<<32 | 1
	setp.eq.s64 	%p10, %rd47, 0;
	@%p10 bra 	$L__BB1_18;                          // won
	...
$L__BB1_18:
	atom.relaxed.gpu.global.add.u64 %rd62, [%rd63], %rd64;  // row = next_row++ (ctl + 8)
	...
	red.global.add.u64 [%rd70], %rd64;                // +1 into rows[row*256 + shard]
	...
$L__BB1_21:
	shl.b64 	%rd79, %rd62, 3;
	and.b64  	%rd80, %rd79, 4294967288;
	or.b64  	%rd81, %rd2, %rd80;
	or.b64  	%rd78, %rd81, 2;
	st.release.gpu.global.b64 [%rd19], %rd78;         // publish {K, row, live}
```

### Checks on the six final builds
- `diff build/c4_proto.ptx build/c4_relaxpub.ptx`, ignoring `.loc`: exactly one instruction differs, `st.release.gpu.global.b64` against `st.relaxed.gpu.global.b64`, plus the helper's mangled name.
- PTX: in all six builds, the only global access outside an inline-asm block is `st.global.u8` (the thread-private outcome byte).
- SASS, protocol build (`build/c4_proto.sass`):
  - `ATOMG.E.CAS.64.STRONG.GPU` x1 and `ATOMG.E.ADD.64.STRONG.GPU` x2 (reserve, next_row). Atomics whose result is unused became `RED.E.ADD.64.STRONG.GPU`.
  - `LDG.E.64.STRONG.GPU` x72 (71 in the builds without reservation). Most come from the reader's unrolled row sum.
  - The publish is `MEMBAR.ALL.GPU; ERRBAR; STG.E.64.STRONG.GPU`. In control 3 it is `STG.E.64.STRONG.GPU` alone.
  - Control 1's claim is `STG.E.64` (weak).
  - The reader's fence is `MEMBAR.ALL.GPU; ERRBAR; CCTL.IVALL`.
  - No generic `LD`/`ST`/`ATOM` on these cells.
- ptxas: `k_ops` uses 30 registers and an 88-byte stack frame, with no spills; `k_reader` uses 46 registers.

## Files

- `src/c4.cu`: the test.
  - `-DFAULT=0..4` selects protocol / control 1 / control 2 / control 3 / control 1+2.
  - `-DWAIT_BOUND_NS=0u` makes the `proto_nowait` build.
- `build.sh`: nvcc `-arch=sm_87 -O3 -lineinfo -std=c++17 -Xptxas -v --keep`. It writes `build/c4_<name>`, `.ptx`, `.sass` and `ptxas-<name>.txt`.
- `run_rules.sh`: r1-r5. `run_extra.sh`: r6-r9 and t1-t14. Each run is a fresh process under `timeout 600`, and exits 1 on a violation.
- `logs/`: one log per run; `logs/env.txt` has versions and sha256 of the source, binaries and PTX the logs came from.
- `raw_section.sh`: prints the appendix from `logs/`.

## Not covered

- Only this board (8 SMs) and one thread shape: 64 keys, 256-thread blocks.
- Claims happen once per key per trial, in launch 1's first wave.
  - The protocol build's 3,900 trials hold about 230,000 GPU claims, each raced by roughly 200 threads of the same key.
  - Control 3's 1,300 trials hold about 83,000 more.
- With the real bound (100,000 ns), the unstable path never ran. It was exercised only by the wait-bound-0 build.
- The reader cannot resolve a publish/add reordering shorter than the time a dozen adds take to land. A relaxed publish looks the same as a release here.
- Faults outside the three controls were not tried: generic instead of `.global` addresses, mixed widths, a CAS from llvmbpf's CMPXCHG.

## Appendix: raw outputs

Verbatim from `logs/`. Rule runs in full; other runs: the header, trial 0's summary lines and the final summary (each full log is in `logs/<name>.log`).

### r1-proto-128 (full)
```
### r1-proto-128: ./build/c4_proto --max 128
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=1 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)

=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9986331 EEXIST=5749 E2BIG=7920 unstable=0 | all rows=9986331 rows behind live headers=9986331
branches: lost_same_key=185 lost_other_key=4 walk_saw_claiming=3252 waits=3437 wait_sleeps=730 wait_max_ns=54656 givebacks=188 e2big_precheck=4222 e2big_after_add=3698 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.534 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 5749/7920/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=6080
per key: j K slots slot home dist row attempts delivered EEXIST(live/waited/unstable) E2BIG hits won ok
   0   1000 1  225  225   0   26  156260  156052     88(27/61/0)     120  156051 1 ok
   1   8919 1   81   81   0   32  156250  156044     94(32/62/0)     112  156043 1 ok
   2  16838 1  151  151   0    8  156260  156069     79(37/42/0)     112  156068 1 ok
   3  24757 1    8    7   1   63  156240  155956    157(30/127/0)     127  155955 1 ok
   4  32676 1  215  215   0   21  156250  156058     78(39/39/0)     114  156057 1 ok
   5  40595 1  137  137   0   39  156250  156040    102(29/73/0)     108  156039 1 ok
   6  48514 1  141  141   0    0  156260  156065     77(42/35/0)     118  156064 1 ok
   7  56433 1   63   63   0   44  156250  156026     90(34/56/0)     134  156025 1 ok
   8  64352 1  254  254   0   13  156250  156057     81(44/37/0)     112  156056 1 ok
   9  72271 1  234  234   0   29  156250  156042     94(26/68/0)     114  156041 1 ok
  10  80190 1  123  123   0   25  156240  156047     80(39/41/0)     113  156046 1 ok
  11  88109 1  160  160   0    5  156250  156058     83(40/43/0)     109  156057 1 ok
  12  96028 1  237  237   0   56  156250  155993     72(25/47/0)     185  155992 1 ok
  13 103947 1   18   18   0   47  156260  156035     91(34/57/0)     134  156034 1 ok
  14 111866 1  163  163   0   18  156240  156048     83(43/40/0)     109  156047 1 ok
  15 119785 1  166  166   0   27  156260  156051     86(27/59/0)     123  156050 1 ok
  16 127704 1   21   21   0   36  156240  156034     95(34/61/0)     111  156033 1 ok
  17 135623 1   92   92   0   10  156250  156058     78(38/40/0)     114  156057 1 ok
  18 143542 1  203  203   0   60  156250  155996     66(24/42/0)     188  155995 1 ok
  19 151461 1  222  222   0   23  156260  156068     81(44/37/0)     111  156067 1 ok
  20 159380 1   78   78   0   40  156240  156030     98(29/69/0)     112  156029 1 ok
  21 167299 1  148  148   0    2  156250  156057     80(39/41/0)     113  156056 1 ok
  22 175218 1    5    4   1   58  156250  155999    124(89/35/0)     127  155998 1 ok
  23 183137 1    6    6   0   15  156260  156067     80(40/40/0)     113  156066 1 ok
  24 191056 1  117  117   0   33  156240  156031     96(26/70/0)     113  156030 1 ok
  25 198975 1  131  131   0   53  156250  155994     72(21/51/0)     184  155993 1 ok
  26 206894 1  100  100   0   30  156250  156043     93(30/63/0)     114  156042 1 ok
  27 214813 1  245  245   0    7  156250  156057     81(43/38/0)     112  156056 1 ok
  28 222732 1  214  214   0   49  156250  156027     89(32/57/0)     134  156026 1 ok
  29 230651 1  171  171   0   20  156250  156058     84(38/46/0)     108  156057 1 ok
  30 238570 1  106  106   0   28  156250  156041     89(27/62/0)     120  156040 1 ok
  31 246489 1   28   28   0   43  156250  156026     91(33/58/0)     133  156025 1 ok
  32 254408 1   32   32   0   12  156250  156057     78(41/37/0)     115  156056 1 ok
  33 262327 1  161  160   1   59  156250  156001    126(91/35/0)     123  156000 1 ok
  34 270246 1  113  112   1   61  156250  155975    133(55/78/0)     142  155974 1 ok
  35 278165 1  224  224   0   41  156250  156041    100(26/74/0)     109  156040 1 ok
  36 286084 1   38   38   0    4  156260  156068     80(43/37/0)     112  156067 1 ok
  37 294003 1  150  150   0   46  156250  156026     94(33/61/0)     130  156025 1 ok
  38 301922 1  152  152   0   17  156240  156049     81(38/43/0)     110  156048 1 ok
  39 309841 1    7    7   0   35  156260  156050     95(26/69/0)     115  156049 1 ok
  40 317760 1   79   78   1   62  156240  155957    162(32/130/0)     121  155956 1 ok
  41 325679 1  247  247   0   34  156250  156043     95(32/63/0)     112  156042 1 ok
  42 333598 1  135  135   0    9  156260  156067     79(40/39/0)     114  156066 1 ok
  43 341517 1  104  104   0   50  156250  156025     92(35/57/0)     133  156024 1 ok
  44 349436 1   61   61   0   22  156250  156058     83(42/41/0)     109  156057 1 ok
  45 357355 1  252  252   0    1  156240  156049     82(39/43/0)     109  156048 1 ok
  46 365274 1  175  175   0   57  156250  155991     73(25/48/0)     186  155990 1 ok
  47 373193 1  178  178   0   14  156260  156067     82(43/39/0)     111  156066 1 ok
  48 381112 1  101  101   0   31  156240  156031     94(28/66/0)     115  156030 1 ok
  49 389031 1   53   53   0   52  156260  156035     93(34/59/0)     132  156034 1 ok
  50 396950 1  231  231   0   42  156240  156032     98(27/71/0)     110  156031 1 ok
  51 404869 1  235  235   0    6  156250  156058     79(39/40/0)     113  156057 1 ok
  52 412788 1  157  157   0   48  156250  156025     96(35/61/0)     129  156024 1 ok
  53 420707 1   93   93   0   19  156250  156058     81(38/43/0)     111  156057 1 ok
  54 428626 1   15   15   0   37  156250  156039     99(27/72/0)     112  156038 1 ok
  55 436545 1   19   19   0   54  156260  156003     73(25/48/0)     184  156002 1 ok
  56 444464 1  197  197   0   38  156240  156032     94(36/58/0)     114  156031 1 ok
  57 452383 1   75   75   0   11  156250  156059     78(36/42/0)     113  156058 1 ok
  58 460302 1  112  112   0   51  156250  156026     93(33/60/0)     131  156025 1 ok
  59 468221 1    1    1   0   24  156250  156058     80(40/40/0)     112  156057 1 ok
  60 476140 1    4    4   0    3  156250  156059     81(38/43/0)     110  156058 1 ok
  61 484059 1  115  115   0   55  156240  155982     73(24/49/0)     185  155981 1 ok
  62 491978 1  186  186   0   45  156260  156036     89(33/56/0)     135  156035 1 ok
  63 499897 1   41   41   0   16  156240  156047     81(43/38/0)     112  156046 1 ok
violations: none

=== summary build=protocol max_entries=128 trials=1 ===
trials_with_violation=0/1
violations by kind: none
counted: EEXIST=5749 E2BIG=7920 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=5749 max=5749, E2BIG min=7920 max=7920
branches: lost_same_key=185 lost_other_key=4 walk_saw_claiming=3252 waits=3437 wait_sleeps=730 wait_max_ns=54656 givebacks=188 e2big_precheck=4222 e2big_after_add=3698
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=54656 requested_ns=6080 (bound 100000)
dir: keys off home (sum over trials)=5 max probe distance=1 max claimed=64 trials with E2BIG while claimed < max_entries=1
RESULT CLEAN
### exit=0
```

### r2-proto-48 (full)
```
### r2-proto-48: ./build/c4_proto --max 48
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=48 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=1 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)

=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=48 live=48 claiming=0 (max_entries 48, S 128) bound=48 next_row=48
dir shape: keys sharing a home slot=25 keys off home=9 max probe distance=3
totals: attempts=10000000 prefill=0 delivered=7487078 EEXIST=4100 E2BIG=2508822 unstable=0 | all rows=7487078 rows behind live headers=7487078
branches: lost_same_key=14 lost_other_key=6 walk_saw_claiming=1888 waits=1902 wait_sleeps=512 wait_max_ns=56704 givebacks=15 e2big_precheck=2503395 e2big_after_add=5427 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 9.375 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 4100/258813/48/0 L2 0/250001/0/0 L3 0/250001/0/0 L4 0/250001/0/0 L5 0/250001/0/0 L6 0/250001/0/0 L7 0/250001/0/0 L8 0/250001/0/0 L9 0/250001/0/0 L10 0/250001/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=2250009; longest wait requested_ns=1984
per key: j K slots slot home dist row attempts delivered EEXIST(live/waited/unstable) E2BIG hits won ok
   0   1000 1  113  112   1   32  156260  155952    135(113/22/0)     173  155951 1 ok
   1   8919 1   40   40   0   29  156250  155988    107(63/44/0)     155  155987 1 ok
   2  16838 1   75   75   0   20  156260  156002    103(57/46/0)     155  156001 1 ok
   3  24757 0   -1    3  -1   -1  156240       0      0(0/0/0)  156240       0 0 ok
   4  32676 1  107  107   0   41  156250  155923     56(8/48/0)     271  155922 1 ok
   5  40595 1   68   68   0    4  156250  156056     48(26/22/0)     146  156055 1 ok
   6  48514 1   70   70   0   12  156260  156011     96(52/44/0)     153  156010 1 ok
   7  56433 0   -1   31  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
   8  64352 1  127  127   0   36  156250  155923     51(6/45/0)     276  155922 1 ok
   9  72271 1  119  117   2   44  156250  155925    145(123/22/0)     180  155924 1 ok
  10  80190 1   61   61   0   26  156240  155981    104(60/44/0)     155  155980 1 ok
  11  88109 1   80   80   0   17  156250  155993    108(57/51/0)     149  155992 1 ok
  12  96028 1  118  118   0   16  156250  156003     95(51/44/0)     152  156002 1 ok
  13 103947 0   -1    9  -1   -1  156260       0      0(0/0/0)  156260       0 0 ok
  14 111866 1   82   81   1   46  156240  155897     37(27/10/0)     306  155896 1 ok
  15 119785 1   83   83   0    1  156260  156064     45(24/21/0)     151  156063 1 ok
  16 127704 1   10   10   0   30  156240  155977    111(65/46/0)     152  155976 1 ok
  17 135623 1   46   46   0   22  156250  155992    106(58/48/0)     152  155991 1 ok
  18 143542 0   -1  101  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  19 151461 0   -1  111  -1   -1  156260       0      0(0/0/0)  156260       0 0 ok
  20 159380 1   39   39   0    6  156240  156045     46(25/21/0)     149  156044 1 ok
  21 167299 1   74   74   0   14  156250  156001     96(52/44/0)     153  156000 1 ok
  22 175218 1    5    2   3   47  156250  155864    111(77/34/0)     275  155863 1 ok
  23 183137 1    4    3   1   42  156260  155928    115(33/82/0)     217  155927 1 ok
  24 191056 1   58   58   0   10  156240  156014     75(36/39/0)     151  156013 1 ok
  25 198975 1   65   65   0   28  156250  155988    111(63/48/0)     151  155987 1 ok
  26 206894 0   -1   50  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  27 214813 1  122  122   0   19  156250  155997    102(55/47/0)     151  155996 1 ok
  28 222732 0   -1  107  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  29 230651 1   85   85   0   40  156250  155924     51(8/43/0)     275  155923 1 ok
  30 238570 1   53   53   0    3  156250  156055     47(25/22/0)     148  156054 1 ok
  31 246489 1   14   14   0   11  156250  156001     97(52/45/0)     152  156000 1 ok
  32 254408 1   16   16   0   35  156250  155923     57(6/51/0)     270  155922 1 ok
  33 262327 1   81   80   1   31  156250  155943    136(111/25/0)     171  155942 1 ok
  34 270246 0   -1   56  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  35 278165 1  112  112   0   25  156250  155992    104(59/45/0)     154  155991 1 ok
  36 286084 1   19   19   0    8  156260  156065     46(25/21/0)     149  156064 1 ok
  37 294003 1   77   75   2   45  156250  155920     91(56/35/0)     239  155919 1 ok
  38 301922 1   76   76   0   39  156240  155914     52(6/46/0)     274  155913 1 ok
  39 309841 1    3    3   0    0  156260  156064     48(26/22/0)     148  156063 1 ok
  40 317760 1   41   39   2   33  156240  155931    148(137/11/0)     161  155930 1 ok
  41 325679 0   -1  123  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  42 333598 1   67   67   0   21  156260  156006    100(54/46/0)     154  156005 1 ok
  43 341517 0   -1   52  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  44 349436 1   30   30   0   23  156250  155988    109(63/46/0)     153  155987 1 ok
  45 357355 1  126  126   0    5  156240  156045     48(24/24/0)     147  156044 1 ok
  46 365274 1   87   87   0   13  156250  156004     90(49/41/0)     156  156003 1 ok
  47 373193 1   89   89   0   37  156260  155932     57(4/53/0)     271  155931 1 ok
  48 381112 1   50   50   0    9  156240  156009     77(41/36/0)     154  156008 1 ok
  49 389031 0   -1   26  -1   -1  156260       0      0(0/0/0)  156260       0 0 ok
  50 396950 1  115  115   0   27  156240  155981    108(59/49/0)     151  155980 1 ok
  51 404869 1  117  117   0   18  156250  155991    102(59/43/0)     157  155990 1 ok
  52 412788 0   -1   78  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  53 420707 1   47   46   1   43  156250  155919    125(34/91/0)     206  155918 1 ok
  54 428626 1    7    7   0    2  156250  156055     42(21/21/0)     153  156054 1 ok
  55 436545 0   -1    9  -1   -1  156260       0      0(0/0/0)  156260       0 0 ok
  56 444464 0   -1   98  -1   -1  156240       0      0(0/0/0)  156240       0 0 ok
  57 452383 1   37   37   0   34  156250  155924     57(7/50/0)     269  155923 1 ok
  58 460302 0   -1   56  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  59 468221 1    0    0   0   24  156250  155986    110(65/45/0)     154  155985 1 ok
  60 476140 1    2    2   0    7  156250  156055     45(22/23/0)     150  156054 1 ok
  61 484059 1   57   57   0   15  156240  155995     95(48/47/0)     150  155994 1 ok
  62 491978 1   93   93   0   38  156260  155932     55(6/49/0)     273  155931 1 ok
  63 499897 0   -1   20  -1   -1  156240       0      0(0/0/0)  156240       0 0 ok
violations: none

=== summary build=protocol max_entries=48 trials=1 ===
trials_with_violation=0/1
violations by kind: none
counted: EEXIST=4100 E2BIG=2508822 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=4100 max=4100, E2BIG min=2508822 max=2508822
branches: lost_same_key=14 lost_other_key=6 walk_saw_claiming=1888 waits=1902 wait_sleeps=512 wait_max_ns=56704 givebacks=15 e2big_precheck=2503395 e2big_after_add=5427
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=2250009
longest wait: elapsed_ns=56704 requested_ns=1984 (bound 100000)
dir: keys off home (sum over trials)=9 max probe distance=3 max claimed=48 trials with E2BIG while claimed < max_entries=0
RESULT CLEAN
### exit=0
```

### r3-ctl1-plainclaim (full)
```
### r3-ctl1-plainclaim: ./build/c4_plainclaim --max 128
C4 build=ctl1-plain-store-claim FAULT=1 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=1 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)

=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=54 live=54 claiming=0 (max_entries 128, S 256) bound=128 next_row=128
dir shape: keys sharing a home slot=10 keys off home=0 max probe distance=0
totals: attempts=10000000 prefill=0 delivered=8427090 EEXIST=4074 E2BIG=1568381 unstable=0 | all rows=8427545 rows behind live headers=8427090
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=2144 waits=2144 wait_sleeps=0 wait_max_ns=39584 givebacks=0 e2big_precheck=1565341 e2big_after_add=3040 eexist_then_null=125 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=65
kernel span: first block start -> last block end = 8.296 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 4074/162104/128/0 L2 0/156253/0/0 L3 0/156253/0/0 L4 0/156253/0/0 L5 0/156253/0/0 L6 0/156253/0/0 L7 0/156253/0/0 L8 0/156253/0/0 L9 0/156253/0/0 L10 0/156253/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=1406277; longest wait requested_ns=0
per key: j K slots slot home dist row attempts delivered EEXIST(live/waited/unstable) E2BIG hits won ok
   0   1000 1  225  225   0   46  156260  156070     73(41/32/0)     104  156080 3 MISMATCH
   1   8919 1   81   81   0  122  156250  156055     74(28/46/0)     111  156063 2 MISMATCH
   2  16838 0   -1  151  -1   -1  156260       0      0(0/0/0)  156260       0 0 ok
   3  24757 1    7    7   0  107  156240  156044     37(35/2/0)     150  156049 4 MISMATCH
   4  32676 1  215  215   0   16  156250  156061     74(32/42/0)     111  156063 2 MISMATCH
   5  40595 1  137  137   0  114  156250  156056     72(36/36/0)     106  156070 2 MISMATCH
   6  48514 1  141  141   0   99  156260  156064     75(43/32/0)     106  156075 4 MISMATCH
   7  56433 1   63   63   0  127  156250  156056     78(24/54/0)     116  156055 1 ok
   8  64352 1  254  254   0   84  156250  156066     76(44/32/0)     108  156065 1 ok
   9  72271 1  234  234   0   60  156250  156056     74(30/44/0)     109  156065 2 MISMATCH
  10  80190 1  123  123   0   26  156240  156056     75(34/41/0)     109  156055 1 ok
  11  88109 1  160  160   0  119  156250  156056     53(26/27/0)     141  156055 1 ok
  12  96028 1  237  237   0  104  156250  156055     68(39/29/0)     109  156069 4 MISMATCH
  13 103947 0   -1   18  -1   -1  156260       0      0(0/0/0)  156260       0 0 ok
  14 111866 1  163  163   0  111  156240  156046     74(42/32/0)     108  156055 3 MISMATCH
  15 119785 1  166  166   0   96  156260  156064     76(36/40/0)     108  156073 3 MISMATCH
  16 127704 1   21   21   0   33  156240  156055     71(39/32/0)     108  156059 2 MISMATCH
  17 135623 1   92   92   0  124  156250  156055     84(22/62/0)     111  156054 1 ok
  18 143542 1  203  203   0   57  156250  156055     70(37/33/0)     109  156069 2 MISMATCH
  19 151461 1  222  222   0   20  156260  156072     76(33/43/0)     108  156074 2 MISMATCH
  20 159380 0   -1   78  -1   -1  156240       0     38(11/27/0)  156197       3 2 MISMATCH
  21 167299 1  148  148   0  101  156250  156054     78(44/34/0)     104  156065 3 MISMATCH
  22 175218 0   -1    4  -1   -1  156250       0     15(10/5/0)  156225       9 1 MISMATCH
  23 183137 1    6    6   0    4  156260  156071     72(40/32/0)     111  156075 2 MISMATCH
  24 191056 1  117  117   0  108  156240  156045     84(28/56/0)     106  156047 3 MISMATCH
  25 198975 1  131  131   0   30  156250  156063     73(43/30/0)     107  156067 3 MISMATCH
  26 206894 1  100  100   0  121  156250  156056     80(22/58/0)     114  156055 1 ok
  27 214813 1  245  245   0  106  156250  156054     71(42/29/0)     107  156068 4 MISMATCH
  28 222732 1  214  214   0   40  156250  156065     77(29/48/0)     108  156064 1 ok
  29 230651 1  171  171   0  113  156250  156055     73(40/33/0)     110  156064 3 MISMATCH
  30 238570 0   -1  106  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  31 246489 1   28   28   0   98  156250  156055     65(38/27/0)     112  156069 4 MISMATCH
  32 254408 1   32   32   0  126  156250  156056     79(21/58/0)     115  156055 1 ok
  33 262327 0   -1  160  -1   -1  156250       0     31(24/7/0)  156203      14 2 MISMATCH
  34 270246 0   -1  112  -1   -1  156250       0      0(0/0/0)  156247       1 2 MISMATCH
  35 278165 1  224  224   0  118  156250  156056     78(25/53/0)     116  156055 1 ok
  36 286084 1   38   38   0  103  156260  156065     74(42/32/0)     105  156078 3 MISMATCH
  37 294003 0   -1  150  -1   -1  156250       0      0(0/0/0)  156250       0 0 ok
  38 301922 1  152  152   0    8  156240  156052     79(34/45/0)     109  156051 1 ok
  39 309841 0   -1    7  -1   -1  156260       0     41(15/26/0)  156206       9 4 MISMATCH
  40 317760 1   78   78   0   95  156240  156046     33(31/2/0)     149  156054 4 MISMATCH
  41 325679 1  247  247   0  123  156250  156055     83(22/61/0)     112  156054 1 ok
  42 333598 1  135  135   0   56  156260  156066     74(41/33/0)     106  156077 3 MISMATCH
  43 341517 1  104  104   0   43  156250  156064     76(30/46/0)     110  156063 1 ok
  44 349436 1   61   61   0   18  156250  156064     74(40/34/0)     108  156066 2 MISMATCH
  45 357355 1  252  252   0  115  156240  156045     84(26/58/0)     111  156044 1 ok
  46 365274 1  175  175   0  100  156250  156055     69(38/31/0)     108  156069 4 MISMATCH
  47 373193 0   -1  178  -1   -1  156260       0      0(0/0/0)  156260       0 0 ok
  48 381112 1  101  101   0   61  156240  156046     73(39/34/0)     106  156058 3 MISMATCH
  49 389031 1   53   53   0    0  156260  156074     80(39/41/0)     106  156073 1 ok
  50 396950 1  231  231   0  120  156240  156046     78(32/46/0)     109  156051 2 MISMATCH
  51 404869 1  235  235   0  105  156250  156054     76(38/38/0)     109  156063 2 MISMATCH
  52 412788 1  157  157   0   53  156250  156057     69(39/30/0)     110  156069 2 MISMATCH
  53 420707 1   93   93   0   12  156250  156060     79(38/41/0)     108  156061 2 MISMATCH
  54 428626 1   15   15   0  112  156250  156055     69(39/30/0)     113  156066 2 MISMATCH
  55 436545 1   19   19   0   97  156260  156065     72(43/29/0)     107  156077 4 MISMATCH
  56 444464 1  197  197   0  125  156240  156046     79(23/56/0)     115  156045 1 ok
  57 452383 1   75   75   0   82  156250  156067     73(42/31/0)     110  156066 1 ok
  58 460302 1  112  112   0   58  156250  156055     74(33/41/0)     110  156064 2 MISMATCH
  59 468221 1    1    1   0   22  156250  156064     74(32/42/0)     112  156063 1 ok
  60 476140 1    4    4   0  117  156250  156055     72(24/48/0)     123  156054 1 ok
  61 484059 1  115  115   0  102  156240  156045     74(42/32/0)     105  156058 3 MISMATCH
  62 491978 1  186  186   0    6  156260  156071     79(28/51/0)     110  156070 1 ok
  63 499897 1   41   41   0  109  156240  156046     72(42/30/0)     110  156055 3 MISMATCH
violations: key_sum_mismatch=40 total_sum_mismatch=1 bookkeeping=2 winners_ne_1=40 tally_ne_counter=40 eexist_then_null=1 orphan_adds=1 key_changed=1
  note: orphan adds: all rows 8427545, rows behind live headers 8427090
  note: bound 128 != claimed slots 54
  note: next_row 128 != live headers 54
  note: EEXIST (not unstable), then own lookup NULL: 125
  note: waited-on slot changed key 65 times
  note: key j=0 K=1000: delivered 156070 != 156083 (attempts 156260 - EEXIST 73 - E2BIG 104 + prefill 0)
  note: key j=0: hits 156080 + won 3 + prefill 0 != delivered 156070
  note: key j=0: 3 winners (slots 1, prefilled 0)
  note: key j=1 K=8919: delivered 156055 != 156065 (attempts 156250 - EEXIST 74 - E2BIG 111 + prefill 0)
  note: key j=1: hits 156063 + won 2 + prefill 0 != delivered 156055
  note: key j=1: 2 winners (slots 1, prefilled 0)
  note: key j=3 K=24757: delivered 156044 != 156053 (attempts 156240 - EEXIST 37 - E2BIG 150 + prefill 0)
  note: key j=3: hits 156049 + won 4 + prefill 0 != delivered 156044
  note: key j=3: 4 winners (slots 1, prefilled 0)
  note: key j=4 K=32676: delivered 156061 != 156065 (attempts 156250 - EEXIST 74 - E2BIG 111 + prefill 0)
  note: key j=4: hits 156063 + won 2 + prefill 0 != delivered 156061
  note: key j=4: 2 winners (slots 1, prefilled 0)
  note: key j=5 K=40595: delivered 156056 != 156072 (attempts 156250 - EEXIST 72 - E2BIG 106 + prefill 0)
  note: key j=5: hits 156070 + won 2 + prefill 0 != delivered 156056
  note: key j=5: 2 winners (slots 1, prefilled 0)
  note: key j=6 K=48514: delivered 156064 != 156079 (attempts 156260 - EEXIST 75 - E2BIG 106 + prefill 0)
  note: key j=6: hits 156075 + won 4 + prefill 0 != delivered 156064
  note: key j=6: 4 winners (slots 1, prefilled 0)
  note: key j=9 K=72271: delivered 156056 != 156067 (attempts 156250 - EEXIST 74 - E2BIG 109 + prefill 0)

=== summary build=ctl1-plain-store-claim max_entries=128 trials=1 ===
trials_with_violation=1/1
violations by kind: key_sum_mismatch=40 total_sum_mismatch=1 bookkeeping=2 winners_ne_1=40 tally_ne_counter=40 eexist_then_null=1 orphan_adds=1 key_changed=1
counted: EEXIST=4074 E2BIG=1568381 unstable=0 eexist_then_null=125 unstable_then_null=0
per trial: EEXIST min=4074 max=4074, E2BIG min=1568381 max=1568381
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=2144 waits=2144 wait_sleeps=0 wait_max_ns=39584 givebacks=0 e2big_precheck=1565341 e2big_after_add=3040
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=1406277
longest wait: elapsed_ns=39584 requested_ns=0 (bound 100000)
dir: keys off home (sum over trials)=0 max probe distance=0 max claimed=54 trials with E2BIG while claimed < max_entries=1
RESULT VIOLATIONS
### exit=1
```

### r4-ctl2-nobound-48 (full)
```
### r4-ctl2-nobound-48: ./build/c4_nobound --max 48
C4 build=ctl2-no-bound-reservation FAULT=2 device="Orin" sm_87 SMs=8
config max_entries=48 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=1 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)

=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 48, S 128) bound=0 next_row=64
dir shape: keys sharing a home slot=25 keys off home=13 max probe distance=4
totals: attempts=10000000 prefill=0 delivered=9987901 EEXIST=12099 E2BIG=0 unstable=0 | all rows=9987901 rows behind live headers=9987901
branches: lost_same_key=2482 lost_other_key=613 walk_saw_claiming=5874 waits=8356 wait_sleeps=2719 wait_max_ns=50912 givebacks=0 e2big_precheck=0 e2big_after_add=0 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.939 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 12099/0/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=6080
per key: j K slots slot home dist row attempts delivered EEXIST(live/waited/unstable) E2BIG hits won ok
   0   1000 1  112  112   0    2  156260  156080    180(64/116/0)       0  156079 1 ok
   1   8919 1   40   40   0   46  156250  156060    190(37/153/0)       0  156059 1 ok
   2  16838 1   75   75   0   32  156260  156069    191(35/156/0)       0  156068 1 ok
   3  24757 1    4    3   1   51  156240  156048    192(65/127/0)       0  156047 1 ok
   4  32676 1  107  107   0   39  156250  156059    191(35/156/0)       0  156058 1 ok
   5  40595 1   68   68   0   19  156250  156060    190(51/139/0)       0  156059 1 ok
   6  48514 1   70   70   0   49  156260  156067    193(36/157/0)       0  156066 1 ok
   7  56433 1   31   31   0   27  156250  156060    190(53/137/0)       0  156059 1 ok
   8  64352 1  127  127   0   36  156250  156058    192(37/155/0)       0  156057 1 ok
   9  72271 1  119  117   2   59  156250  156060    190(107/83/0)       0  156059 1 ok
  10  80190 1   61   61   0   44  156240  156049    191(36/155/0)       0  156048 1 ok
  11  88109 1   80   80   0   23  156250  156058    192(55/137/0)       0  156057 1 ok
  12  96028 1  118  118   0   10  156250  156066    184(67/117/0)       0  156065 1 ok
  13 103947 1    9    9   0   29  156260  156069    191(54/137/0)       0  156068 1 ok
  14 111866 1   81   81   0   16  156240  156049    191(52/139/0)       0  156048 1 ok
  15 119785 1   83   83   0    3  156260  156078    182(65/117/0)       0  156077 1 ok
  16 127704 1   10   10   0   47  156240  156050    190(38/152/0)       0  156049 1 ok
  17 135623 1   46   46   0   34  156250  156059    191(37/154/0)       0  156058 1 ok
  18 143542 1  101  101   0   12  156250  156066    184(67/117/0)       0  156065 1 ok
  19 151461 1  111  111   0   41  156260  156069    191(37/154/0)       0  156068 1 ok
  20 159380 1   39   39   0   21  156240  156049    191(52/139/0)       0  156048 1 ok
  21 167299 1   74   74   0    8  156250  156064    186(67/119/0)       0  156063 1 ok
  22 175218 1    6    2   4   63  156250  156072    178(170/8/0)       0  156071 1 ok
  23 183137 1    5    3   2   56  156260  156068    192(130/62/0)       0  156067 1 ok
  24 191056 1   58   58   0   13  156240  156050    190(55/135/0)       0  156049 1 ok
  25 198975 1   65   65   0   45  156250  156059    191(40/151/0)       0  156058 1 ok
  26 206894 1   50   50   0   31  156250  156059    191(33/158/0)       0  156058 1 ok
  27 214813 1  122  122   0   11  156250  156065    185(69/116/0)       0  156064 1 ok
  28 222732 1  108  107   1   54  156250  156059    191(84/107/0)       0  156058 1 ok
  29 230651 1   85   85   0   18  156250  156059    191(50/141/0)       0  156058 1 ok
  30 238570 1   53   53   0    4  156250  156072    178(64/114/0)       0  156071 1 ok
  31 246489 1   14   14   0   48  156250  156059    191(38/153/0)       0  156058 1 ok
  32 254408 1   16   16   0   26  156250  156059    191(53/138/0)       0  156058 1 ok
  33 262327 1   82   80   2   60  156250  156059    191(103/88/0)       0  156058 1 ok
  34 270246 1   56   56   0   43  156250  156059    191(36/155/0)       0  156058 1 ok
  35 278165 1  113  112   1   55  156250  156059    191(1/190/0)       0  156058 1 ok
  36 286084 1   19   19   0    6  156260  156077    183(65/118/0)       0  156076 1 ok
  37 294003 1   77   75   2   61  156250  156059    191(103/88/0)       0  156058 1 ok
  38 301922 1   76   76   0   38  156240  156050    190(34/156/0)       0  156049 1 ok
  39 309841 1    3    3   0   15  156260  156067    193(55/138/0)       0  156066 1 ok
  40 317760 1   41   39   2   57  156240  156051    189(133/56/0)       0  156050 1 ok
  41 325679 1  123  123   0   25  156250  156059    191(54/137/0)       0  156058 1 ok
  42 333598 1   67   67   0   33  156260  156068    192(35/157/0)       0  156067 1 ok
  43 341517 1   52   52   0    1  156250  156068    182(64/118/0)       0  156067 1 ok
  44 349436 1   30   30   0   40  156250  156058    192(36/156/0)       0  156057 1 ok
  45 357355 1  126  126   0   20  156240  156049    191(55/136/0)       0  156048 1 ok
  46 365274 1   87   87   0   50  156250  156060    190(36/154/0)       0  156059 1 ok
  47 373193 1   89   89   0   28  156260  156068    192(54/138/0)       0  156067 1 ok
  48 381112 1   51   50   1   52  156240  156050    190(9/181/0)       0  156049 1 ok
  49 389031 1   26   26   0    7  156260  156073    187(67/120/0)       0  156072 1 ok
  50 396950 1  115  115   0   24  156240  156049    191(53/138/0)       0  156048 1 ok
  51 404869 1  117  117   0   30  156250  156058    192(36/156/0)       0  156057 1 ok
  52 412788 1   78   78   0    5  156250  156074    176(63/113/0)       0  156073 1 ok
  53 420707 1   47   46   1   53  156250  156058    192(87/105/0)       0  156057 1 ok
  54 428626 1    7    7   0   17  156250  156059    191(50/141/0)       0  156058 1 ok
  55 436545 1   11    9   2   58  156260  156069    191(135/56/0)       0  156068 1 ok
  56 444464 1   98   98   0    0  156240  156061    179(66/113/0)       0  156060 1 ok
  57 452383 1   37   37   0   35  156250  156060    190(30/160/0)       0  156059 1 ok
  58 460302 1   59   56   3   62  156250  156059    191(106/85/0)       0  156058 1 ok
  59 468221 1    0    0   0   42  156250  156059    191(36/155/0)       0  156058 1 ok
  60 476140 1    2    2   0   22  156250  156059    191(52/139/0)       0  156058 1 ok
  61 484059 1   57   57   0    9  156240  156056    184(67/117/0)       0  156055 1 ok
  62 491978 1   93   93   0   37  156260  156069    191(37/154/0)       0  156068 1 ok
  63 499897 1   20   20   0   14  156240  156049    191(52/139/0)       0  156048 1 ok
violations: claimed_gt_max=1
  note: claimed slots 64 > max_entries 48

=== summary build=ctl2-no-bound-reservation max_entries=48 trials=1 ===
trials_with_violation=1/1
violations by kind: claimed_gt_max=1
counted: EEXIST=12099 E2BIG=0 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=12099 max=12099, E2BIG min=0 max=0
branches: lost_same_key=2482 lost_other_key=613 walk_saw_claiming=5874 waits=8356 wait_sleeps=2719 wait_max_ns=50912 givebacks=0 e2big_precheck=0 e2big_after_add=0
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=50912 requested_ns=6080 (bound 100000)
dir: keys off home (sum over trials)=13 max probe distance=4 max claimed=64 trials with E2BIG while claimed < max_entries=0
RESULT VIOLATIONS
### exit=1
```

### r5-ctl3-relaxpub (full)
```
### r5-ctl3-relaxpub: ./build/c4_relaxpub --max 128
C4 build=ctl3-relaxed-publish FAULT=3 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=1 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)

=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9985246 EEXIST=3964 E2BIG=10790 unstable=0 | all rows=9985246 rows behind live headers=9985246
branches: lost_same_key=137 lost_other_key=10 walk_saw_claiming=2224 waits=2361 wait_sleeps=881 wait_max_ns=58592 givebacks=142 e2big_precheck=4927 e2big_after_add=5863 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.522 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 3964/10790/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=10176
per key: j K slots slot home dist row attempts delivered EEXIST(live/waited/unstable) E2BIG hits won ok
   0   1000 1  225  225   0    8  156260  156068     56(30/26/0)     136  156067 1 ok
   1   8919 1   81   81   0   36  156250  156023     76(37/39/0)     151  156022 1 ok
   2  16838 1  151  151   0   54  156260  155949     27(13/14/0)     284  155948 1 ok
   3  24757 1    7    7   0   47  156240  156015     78(33/45/0)     147  156014 1 ok
   4  32676 1  215  215   0   59  156250  155939     27(13/14/0)     284  155938 1 ok
   5  40595 1  137  137   0   13  156250  156051     47(10/37/0)     152  156050 1 ok
   6  48514 1  141  141   0   17  156260  156059     62(10/52/0)     139  156058 1 ok
   7  56433 1   63   63   0   39  156250  156023     77(37/40/0)     150  156022 1 ok
   8  64352 1  254  254   0   30  156250  156031     69(27/42/0)     150  156030 1 ok
   9  72271 1  234  234   0   14  156250  156051     64(10/54/0)     135  156050 1 ok
  10  80190 1  123  123   0    5  156240  156049     52(29/23/0)     139  156048 1 ok
  11  88109 1  161  160   1   51  156250  156001     93(71/22/0)     156  156000 1 ok
  12  96028 1  237  237   0   23  156250  156032     65(28/37/0)     153  156031 1 ok
  13 103947 1   18   18   0   44  156260  156033     76(36/40/0)     151  156032 1 ok
  14 111866 1  163  163   0   34  156240  155930    155(0/155/0)     155  155929 1 ok
  15 119785 1  166  166   0   10  156260  156068     53(23/30/0)     139  156067 1 ok
  16 127704 1   21   21   0    2  156240  156050     51(17/34/0)     139  156049 1 ok
  17 135623 1   92   92   0   55  156250  155939     26(12/14/0)     285  155938 1 ok
  18 143542 1  203  203   0   27  156250  156031     68(28/40/0)     151  156030 1 ok
  19 151461 1  222  222   0    0  156260  156069     48(16/32/0)     143  156068 1 ok
  20 159380 1   79   78   1   63  156240  155929     46(3/43/0)     265  155928 1 ok
  21 167299 1  148  148   0   19  156250  156052     60(8/52/0)     138  156051 1 ok
  22 175218 1    4    4   0   41  156250  156024     82(36/46/0)     144  156023 1 ok
  23 183137 1    6    6   0   32  156260  156044     59(24/35/0)     157  156043 1 ok
  24 191056 1  117  117   0   49  156240  156014     82(36/46/0)     144  156013 1 ok
  25 198975 1  131  131   0    7  156250  156059     54(27/27/0)     137  156058 1 ok
  26 206894 1  100  100   0   35  156250  156023     73(36/37/0)     154  156022 1 ok
  27 214813 1  245  245   0   25  156250  156030     69(29/40/0)     151  156029 1 ok
  28 222732 1  214  214   0   46  156250  156026     78(33/45/0)     146  156025 1 ok
  29 230651 1  171  171   0   61  156250  155942     20(13/7/0)     288  155941 1 ok
  30 238570 1  106  106   0   16  156250  156053     59(7/52/0)     138  156052 1 ok
  31 246489 1   28   28   0    3  156250  156059     53(17/36/0)     138  156058 1 ok
  32 254408 1   32   32   0   56  156250  155940     26(12/14/0)     284  155939 1 ok
  33 262327 1  160  160   0   29  156250  156034     67(25/42/0)     149  156033 1 ok
  34 270246 1  112  112   0    4  156250  156059     54(29/25/0)     137  156058 1 ok
  35 278165 1  224  224   0   62  156250  155942     21(12/9/0)     287  155941 1 ok
  36 286084 1   38   38   0   21  156260  156061     59(8/51/0)     140  156060 1 ok
  37 294003 1  150  150   0   43  156250  156022     79(37/42/0)     149  156021 1 ok
  38 301922 1  152  152   0   57  156240  155931     25(11/14/0)     284  155930 1 ok
  39 309841 1    8    7   1   52  156260  155979    102(57/45/0)     179  155978 1 ok
  40 317760 1   78   78   0    9  156240  156051     52(25/27/0)     137  156050 1 ok
  41 325679 1  247  247   0   37  156250  156020     79(39/40/0)     151  156019 1 ok
  42 333598 1  135  135   0   26  156260  156043     68(25/43/0)     149  156042 1 ok
  43 341517 1  104  104   0   48  156250  156026     75(34/41/0)     149  156025 1 ok
  44 349436 1   61   61   0   60  156250  155941     24(12/12/0)     285  155940 1 ok
  45 357355 1  252  252   0   18  156240  156042     60(8/52/0)     138  156041 1 ok
  46 365274 1  175  175   0   22  156250  156031     68(29/39/0)     151  156030 1 ok
  47 373193 1  178  178   0   40  156260  156035     72(33/39/0)     153  156034 1 ok
  48 381112 1  101  101   0   31  156240  156022     68(28/40/0)     150  156021 1 ok
  49 389031 1   53   53   0    6  156260  156069     55(29/26/0)     136  156068 1 ok
  50 396950 1  231  231   0   11  156240  156049     42(20/22/0)     149  156048 1 ok
  51 404869 1  235  235   0   24  156250  156035     61(23/38/0)     154  156034 1 ok
  52 412788 1  157  157   0   45  156250  156022     80(38/42/0)     148  156021 1 ok
  53 420707 1   93   93   0   58  156250  155940     25(12/13/0)     285  155939 1 ok
  54 428626 1   15   15   0   12  156250  156050     46(10/36/0)     154  156049 1 ok
  55 436545 1   19   19   0   15  156260  156061     61(10/51/0)     138  156060 1 ok
  56 444464 1  197  197   0   38  156240  156015     76(34/42/0)     149  156014 1 ok
  57 452383 1   75   75   0   28  156250  156034     64(26/38/0)     152  156033 1 ok
  58 460302 1  113  112   1   50  156250  156009     98(77/21/0)     143  156008 1 ok
  59 468221 1    1    1   0    1  156250  156059     47(15/32/0)     144  156058 1 ok
  60 476140 1    5    4   1   53  156250  155966     98(60/38/0)     186  155965 1 ok
  61 484059 1  115  115   0   20  156240  156039     62(11/51/0)     139  156038 1 ok
  62 491978 1  186  186   0   42  156260  156034     74(35/39/0)     152  156033 1 ok
  63 499897 1   41   41   0   33  156240  156019     71(30/41/0)     150  156018 1 ok
violations: none

=== summary build=ctl3-relaxed-publish max_entries=128 trials=1 ===
trials_with_violation=0/1
violations by kind: none
counted: EEXIST=3964 E2BIG=10790 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=3964 max=3964, E2BIG min=10790 max=10790
branches: lost_same_key=137 lost_other_key=10 walk_saw_claiming=2224 waits=2361 wait_sleeps=881 wait_max_ns=58592 givebacks=142 e2big_precheck=4927 e2big_after_add=5863
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=58592 requested_ns=10176 (bound 100000)
dir: keys off home (sum over trials)=5 max probe distance=1 max claimed=64 trials with E2BIG while claimed < max_entries=1
RESULT CLEAN
### exit=0
```

### r6-proto-128-reader
```
### r6-proto-128-reader: ./build/c4_proto --max 128 --reader 1
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=1 reader=1 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9987108 EEXIST=4297 E2BIG=8595 unstable=0 | all rows=9987108 rows behind live headers=9987108
branches: lost_same_key=96 lost_other_key=7 walk_saw_claiming=2387 waits=2483 wait_sleeps=1660 wait_max_ns=42304 givebacks=103 e2big_precheck=4684 e2big_after_add=3911 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.752 ms (10 launches)
reader: start_before_inserters=1 (reader start - first inserter block = -4736 ns) polls=138005 seen_claiming=80 transitions_seen=64 min_sum_at_transition=356 live_checks=10404 zero_sum=0 decrease=0 sum_changes=10340 timed_out=0 end_after_last_block_ns=12864
per launch (EEXIST/E2BIG/won/unstable): L1 4297/8595/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=4032
violations: none
branches: lost_same_key=96 lost_other_key=7 walk_saw_claiming=2387 waits=2483 wait_sleeps=1660 wait_max_ns=42304 givebacks=103 e2big_precheck=4684 e2big_after_add=3911
=== summary build=protocol max_entries=128 trials=1 ===
trials_with_violation=0/1
violations by kind: none
counted: EEXIST=4297 E2BIG=8595 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=4297 max=4297, E2BIG min=8595 max=8595
branches: lost_same_key=96 lost_other_key=7 walk_saw_claiming=2387 waits=2483 wait_sleeps=1660 wait_max_ns=42304 givebacks=103 e2big_precheck=4684 e2big_after_add=3911
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=42304 requested_ns=4032 (bound 100000)
dir: keys off home (sum over trials)=5 max probe distance=1 max claimed=64 trials with E2BIG while claimed < max_entries=1
reader: trials_with_overlap=1 polls=138005 seen_claiming=80 transitions_seen=64 live_checks=10404 zero_sum=0 decrease=0 sum_changes=10340 timed_out=0 min_sum_at_transition_over_trials=356
RESULT CLEAN
### exit=0 wall_s=.140539741
```

### r7-ctl3-relaxpub-reader
```
### r7-ctl3-relaxpub-reader: ./build/c4_relaxpub --max 128 --reader 1
C4 build=ctl3-relaxed-publish FAULT=3 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=1 reader=1 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9989147 EEXIST=3762 E2BIG=7091 unstable=0 | all rows=9989147 rows behind live headers=9989147
branches: lost_same_key=95 lost_other_key=11 walk_saw_claiming=2892 waits=2987 wait_sleeps=80 wait_max_ns=34624 givebacks=103 e2big_precheck=4271 e2big_after_add=2820 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.746 ms (10 launches)
reader: start_before_inserters=1 (reader start - first inserter block = -4896 ns) polls=107110 seen_claiming=77 transitions_seen=64 min_sum_at_transition=383 live_checks=9115 zero_sum=0 decrease=0 sum_changes=9051 timed_out=0 end_after_last_block_ns=12832
per launch (EEXIST/E2BIG/won/unstable): L1 3762/7091/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=192
violations: none
branches: lost_same_key=95 lost_other_key=11 walk_saw_claiming=2892 waits=2987 wait_sleeps=80 wait_max_ns=34624 givebacks=103 e2big_precheck=4271 e2big_after_add=2820
=== summary build=ctl3-relaxed-publish max_entries=128 trials=1 ===
trials_with_violation=0/1
violations by kind: none
counted: EEXIST=3762 E2BIG=7091 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=3762 max=3762, E2BIG min=7091 max=7091
branches: lost_same_key=95 lost_other_key=11 walk_saw_claiming=2892 waits=2987 wait_sleeps=80 wait_max_ns=34624 givebacks=103 e2big_precheck=4271 e2big_after_add=2820
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=34624 requested_ns=192 (bound 100000)
dir: keys off home (sum over trials)=5 max probe distance=1 max claimed=64 trials with E2BIG while claimed < max_entries=1
reader: trials_with_overlap=1 polls=107110 seen_claiming=77 transitions_seen=64 live_checks=9115 zero_sum=0 decrease=0 sum_changes=9051 timed_out=0 min_sum_at_transition_over_trials=383
RESULT CLEAN
### exit=0 wall_s=.139967914
```

### r8-proto-128-prefill16
```
### r8-proto-128-prefill16: ./build/c4_proto --max 128 --prefill 16
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=1 reader=0 prefill=16 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=80 delivered=9990230 EEXIST=2796 E2BIG=7054 unstable=0 | all rows=9990230 rows behind live headers=9990230
branches: lost_same_key=104 lost_other_key=13 walk_saw_claiming=1973 waits=2077 wait_sleeps=1054 wait_max_ns=41664 givebacks=114 e2big_precheck=3065 e2big_after_add=3989 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.545 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 2796/7054/48/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=8128
violations: none
branches: lost_same_key=104 lost_other_key=13 walk_saw_claiming=1973 waits=2077 wait_sleeps=1054 wait_max_ns=41664 givebacks=114 e2big_precheck=3065 e2big_after_add=3989
dir: keys off home (sum over trials)=5 max probe distance=1 max claimed=64 trials with E2BIG while claimed < max_entries=1
=== summary build=protocol max_entries=128 trials=1 ===
trials_with_violation=0/1
violations by kind: none
counted: EEXIST=2796 E2BIG=7054 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=2796 max=2796, E2BIG min=7054 max=7054
branches: lost_same_key=104 lost_other_key=13 walk_saw_claiming=1973 waits=2077 wait_sleeps=1054 wait_max_ns=41664 givebacks=114 e2big_precheck=3065 e2big_after_add=3989
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=41664 requested_ns=8128 (bound 100000)
dir: keys off home (sum over trials)=5 max probe distance=1 max claimed=64 trials with E2BIG while claimed < max_entries=1
RESULT CLEAN
### exit=0 wall_s=.142097106
```

### r9-proto-64
```
### r9-proto-64: ./build/c4_proto --max 64
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=64 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=1 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 64, S 128) bound=64 next_row=64
dir shape: keys sharing a home slot=25 keys off home=15 max probe distance=4
totals: attempts=10000000 prefill=0 delivered=9982310 EEXIST=5928 E2BIG=11762 unstable=0 | all rows=9982310 rows behind live headers=9982310
branches: lost_same_key=38 lost_other_key=12 walk_saw_claiming=2930 waits=2968 wait_sleeps=522 wait_max_ns=50208 givebacks=44 e2big_precheck=7475 e2big_after_add=4287 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.887 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 5928/11762/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=4032
violations: none
branches: lost_same_key=38 lost_other_key=12 walk_saw_claiming=2930 waits=2968 wait_sleeps=522 wait_max_ns=50208 givebacks=44 e2big_precheck=7475 e2big_after_add=4287
dir: keys off home (sum over trials)=15 max probe distance=4 max claimed=64 trials with E2BIG while claimed < max_entries=0
=== summary build=protocol max_entries=64 trials=1 ===
trials_with_violation=0/1
violations by kind: none
counted: EEXIST=5928 E2BIG=11762 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=5928 max=5928, E2BIG min=11762 max=11762
branches: lost_same_key=38 lost_other_key=12 walk_saw_claiming=2930 waits=2968 wait_sleeps=522 wait_max_ns=50208 givebacks=44 e2big_precheck=7475 e2big_after_add=4287
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=50208 requested_ns=4032 (bound 100000)
dir: keys off home (sum over trials)=15 max probe distance=4 max claimed=64 trials with E2BIG while claimed < max_entries=0
RESULT CLEAN
### exit=0 wall_s=.141405978
```

### t1-proto-128-x1000
```
### t1-proto-128-x1000: ./build/c4_proto --max 128 --trials 1000
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=1000 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9986356 EEXIST=5703 E2BIG=7941 unstable=0 | all rows=9986356 rows behind live headers=9986356
branches: lost_same_key=126 lost_other_key=10 walk_saw_claiming=3630 waits=3756 wait_sleeps=295 wait_max_ns=57568 givebacks=135 e2big_precheck=4724 e2big_after_add=3217 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.488 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 5703/7941/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=448
violations: none
branches: lost_same_key=153667 lost_other_key=16112 walk_saw_claiming=3326292 waits=3479959 wait_sleeps=458207 wait_max_ns=73920 givebacks=164622 e2big_precheck=5139887 e2big_after_add=3536035
dir: keys off home (sum over trials)=7510 max probe distance=29 max claimed=64 trials with E2BIG while claimed < max_entries=1000
=== summary build=protocol max_entries=128 trials=1000 ===
trials_with_violation=0/1000
violations by kind: none
counted: EEXIST=5739064 E2BIG=8675922 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=4138 max=7575, E2BIG min=7061 max=11695
branches: lost_same_key=153667 lost_other_key=16112 walk_saw_claiming=3326292 waits=3479959 wait_sleeps=458207 wait_max_ns=73920 givebacks=164622 e2big_precheck=5139887 e2big_after_add=3536035
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=73920 requested_ns=12224 (bound 100000)
dir: keys off home (sum over trials)=7510 max probe distance=29 max claimed=64 trials with E2BIG while claimed < max_entries=1000
RESULT CLEAN
### exit=0 wall_s=36.275109895
```

### t2-proto-48-x1000
```
### t2-proto-48-x1000: ./build/c4_proto --max 48 --trials 1000
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=48 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=1000 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=48 live=48 claiming=0 (max_entries 48, S 128) bound=48 next_row=48
dir shape: keys sharing a home slot=25 keys off home=7 max probe distance=3
totals: attempts=10000000 prefill=0 delivered=7486968 EEXIST=4265 E2BIG=2508767 unstable=0 | all rows=7486968 rows behind live headers=7486968
branches: lost_same_key=25 lost_other_key=7 walk_saw_claiming=2723 waits=2748 wait_sleeps=522 wait_max_ns=46592 givebacks=27 e2big_precheck=2504269 e2big_after_add=4498 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 9.459 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 4265/258785/48/0 L2 0/249998/0/0 L3 0/249998/0/0 L4 0/249998/0/0 L5 0/249998/0/0 L6 0/249998/0/0 L7 0/249998/0/0 L8 0/249998/0/0 L9 0/249998/0/0 L10 0/249998/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=2249982; longest wait requested_ns=1984
violations: none
branches: lost_same_key=14751 lost_other_key=7575 walk_saw_claiming=2247236 waits=2261987 wait_sleeps=768689 wait_max_ns=77280 givebacks=16202 e2big_precheck=2503798630 e2big_after_add=5080906
dir: keys off home (sum over trials)=9617 max probe distance=38 max claimed=48 trials with E2BIG while claimed < max_entries=0
=== summary build=protocol max_entries=48 trials=1000 ===
trials_with_violation=0/1000
violations by kind: none
counted: EEXIST=3969298 E2BIG=2508879536 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=1908 max=6807, E2BIG min=2505634 max=2515062
branches: lost_same_key=14751 lost_other_key=7575 walk_saw_claiming=2247236 waits=2261987 wait_sleeps=768689 wait_max_ns=77280 givebacks=16202 e2big_precheck=2503798630 e2big_after_add=5080906
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=2250003195
longest wait: elapsed_ns=77280 requested_ns=16320 (bound 100000)
dir: keys off home (sum over trials)=9617 max probe distance=38 max claimed=48 trials with E2BIG while claimed < max_entries=0
RESULT CLEAN
### exit=0 wall_s=50.970664794
```

### t3-proto-64-x1000
```
### t3-proto-64-x1000: ./build/c4_proto --max 64 --trials 1000
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=64 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=1000 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 64, S 128) bound=64 next_row=64
dir shape: keys sharing a home slot=25 keys off home=13 max probe distance=4
totals: attempts=10000000 prefill=0 delivered=9984719 EEXIST=5178 E2BIG=10103 unstable=0 | all rows=9984719 rows behind live headers=9984719
branches: lost_same_key=27 lost_other_key=12 walk_saw_claiming=2674 waits=2701 wait_sleeps=453 wait_max_ns=52320 givebacks=29 e2big_precheck=6207 e2big_after_add=3896 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.993 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 5178/10103/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=4032
violations: none
branches: lost_same_key=31342 lost_other_key=12640 walk_saw_claiming=2728519 waits=2759861 wait_sleeps=1205040 wait_max_ns=84256 givebacks=34878 e2big_precheck=7565353 e2big_after_add=5468466
dir: keys off home (sum over trials)=15223 max probe distance=56 max claimed=64 trials with E2BIG while claimed < max_entries=0
=== summary build=protocol max_entries=64 trials=1000 ===
trials_with_violation=0/1000
violations by kind: none
counted: EEXIST=5094288 E2BIG=13033819 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=2800 max=7922, E2BIG min=8530 max=20256
branches: lost_same_key=31342 lost_other_key=12640 walk_saw_claiming=2728519 waits=2759861 wait_sleeps=1205040 wait_max_ns=84256 givebacks=34878 e2big_precheck=7565353 e2big_after_add=5468466
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=84256 requested_ns=14272 (bound 100000)
dir: keys off home (sum over trials)=15223 max probe distance=56 max claimed=64 trials with E2BIG while claimed < max_entries=0
RESULT CLEAN
### exit=0 wall_s=36.966620073
```

### t4-proto-128-prefill16-x300
```
### t4-proto-128-prefill16-x300: ./build/c4_proto --max 128 --prefill 16 --trials 300
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=300 reader=0 prefill=16 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=80 delivered=9990501 EEXIST=2666 E2BIG=6913 unstable=0 | all rows=9990501 rows behind live headers=9990501
branches: lost_same_key=185 lost_other_key=9 walk_saw_claiming=2055 waits=2240 wait_sleeps=709 wait_max_ns=44000 givebacks=191 e2big_precheck=2933 e2big_after_add=3980 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.475 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 2666/6913/48/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=4032
violations: none
branches: lost_same_key=41606 lost_other_key=3809 walk_saw_claiming=707534 waits=749140 wait_sleeps=326402 wait_max_ns=65536 givebacks=44020 e2big_precheck=984811 e2big_after_add=878978
dir: keys off home (sum over trials)=2199 max probe distance=10 max claimed=64 trials with E2BIG while claimed < max_entries=300
=== summary build=protocol max_entries=128 trials=300 ===
trials_with_violation=0/300
violations by kind: none
counted: EEXIST=1030795 E2BIG=1863789 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=1443 max=6331, E2BIG min=4470 max=9140
branches: lost_same_key=41606 lost_other_key=3809 walk_saw_claiming=707534 waits=749140 wait_sleeps=326402 wait_max_ns=65536 givebacks=44020 e2big_precheck=984811 e2big_after_add=878978
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=65536 requested_ns=14272 (bound 100000)
dir: keys off home (sum over trials)=2199 max probe distance=10 max claimed=64 trials with E2BIG while claimed < max_entries=300
RESULT CLEAN
### exit=0 wall_s=11.010217524
```

### t5-proto-128-reader-x300
```
### t5-proto-128-reader-x300: ./build/c4_proto --max 128 --reader 1 --trials 300
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=300 reader=1 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9987434 EEXIST=4979 E2BIG=7587 unstable=0 | all rows=9987434 rows behind live headers=9987434
branches: lost_same_key=129 lost_other_key=11 walk_saw_claiming=3045 waits=3174 wait_sleeps=281 wait_max_ns=42432 givebacks=136 e2big_precheck=4869 e2big_after_add=2718 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.717 ms (10 launches)
reader: start_before_inserters=1 (reader start - first inserter block = -4544 ns) polls=106687 seen_claiming=98 transitions_seen=64 min_sum_at_transition=373 live_checks=8936 zero_sum=0 decrease=0 sum_changes=8872 timed_out=0 end_after_last_block_ns=11360
per launch (EEXIST/E2BIG/won/unstable): L1 4979/7587/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=192
violations: none
branches: lost_same_key=41076 lost_other_key=4681 walk_saw_claiming=679913 waits=720989 wait_sleeps=209434 wait_max_ns=64608 givebacks=44207 e2big_precheck=1578213 e2big_after_add=999215
=== summary build=protocol max_entries=128 trials=300 ===
trials_with_violation=0/300
violations by kind: none
counted: EEXIST=1377955 E2BIG=2577428 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=2465 max=6655, E2BIG min=7389 max=10166
branches: lost_same_key=41076 lost_other_key=4681 walk_saw_claiming=679913 waits=720989 wait_sleeps=209434 wait_max_ns=64608 givebacks=44207 e2big_precheck=1578213 e2big_after_add=999215
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=64608 requested_ns=16320 (bound 100000)
dir: keys off home (sum over trials)=2185 max probe distance=10 max claimed=64 trials with E2BIG while claimed < max_entries=300
reader: trials_with_overlap=300 polls=42307864 seen_claiming=28112 transitions_seen=19200 live_checks=3127214 zero_sum=0 decrease=0 sum_changes=3108014 timed_out=0 min_sum_at_transition_over_trials=29
RESULT CLEAN
### exit=0 wall_s=11.037461090
```

### t6-relaxpub-128-reader-x300
```
### t6-relaxpub-128-reader-x300: ./build/c4_relaxpub --max 128 --reader 1 --trials 300
C4 build=ctl3-relaxed-publish FAULT=3 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=300 reader=1 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9989174 EEXIST=4325 E2BIG=6501 unstable=0 | all rows=9989174 rows behind live headers=9989174
branches: lost_same_key=136 lost_other_key=9 walk_saw_claiming=3417 waits=3553 wait_sleeps=240 wait_max_ns=45792 givebacks=144 e2big_precheck=3973 e2big_after_add=2528 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.797 ms (10 launches)
reader: start_before_inserters=1 (reader start - first inserter block = -4608 ns) polls=112470 seen_claiming=66 transitions_seen=64 min_sum_at_transition=404 live_checks=9194 zero_sum=0 decrease=0 sum_changes=9130 timed_out=0 end_after_last_block_ns=12608
per launch (EEXIST/E2BIG/won/unstable): L1 4325/6501/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=448
violations: none
branches: lost_same_key=48604 lost_other_key=4361 walk_saw_claiming=344794 waits=393398 wait_sleeps=149592 wait_max_ns=72096 givebacks=50734 e2big_precheck=481601 e2big_after_add=2621973
=== summary build=ctl3-relaxed-publish max_entries=128 trials=300 ===
trials_with_violation=0/300
violations by kind: none
counted: EEXIST=849491 E2BIG=3103574 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=700 max=5652, E2BIG min=6501 max=11588
branches: lost_same_key=48604 lost_other_key=4361 walk_saw_claiming=344794 waits=393398 wait_sleeps=149592 wait_max_ns=72096 givebacks=50734 e2big_precheck=481601 e2big_after_add=2621973
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=72096 requested_ns=12224 (bound 100000)
dir: keys off home (sum over trials)=2185 max probe distance=10 max claimed=64 trials with E2BIG while claimed < max_entries=300
reader: trials_with_overlap=300 polls=40862961 seen_claiming=25114 transitions_seen=19200 live_checks=3050258 zero_sum=0 decrease=0 sum_changes=3031058 timed_out=0 min_sum_at_transition_over_trials=12
RESULT CLEAN
### exit=0 wall_s=11.050208515
```

### t7-relaxpub-128-x1000
```
### t7-relaxpub-128-x1000: ./build/c4_relaxpub --max 128 --trials 1000
C4 build=ctl3-relaxed-publish FAULT=3 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=1000 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9986491 EEXIST=4770 E2BIG=8739 unstable=0 | all rows=9986491 rows behind live headers=9986491
branches: lost_same_key=175 lost_other_key=9 walk_saw_claiming=2157 waits=2332 wait_sleeps=216 wait_max_ns=38048 givebacks=180 e2big_precheck=4380 e2big_after_add=4359 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.487 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 4770/8739/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=960
violations: none
branches: lost_same_key=149509 lost_other_key=15799 walk_saw_claiming=2324159 waits=2473668 wait_sleeps=592662 wait_max_ns=77056 givebacks=158809 e2big_precheck=4868469 e2big_after_add=4982291
dir: keys off home (sum over trials)=7556 max probe distance=23 max claimed=64 trials with E2BIG while claimed < max_entries=1000
=== summary build=ctl3-relaxed-publish max_entries=128 trials=1000 ===
trials_with_violation=0/1000
violations by kind: none
counted: EEXIST=4322103 E2BIG=9850760 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=2095 max=6463, E2BIG min=7752 max=14893
branches: lost_same_key=149509 lost_other_key=15799 walk_saw_claiming=2324159 waits=2473668 wait_sleeps=592662 wait_max_ns=77056 givebacks=158809 e2big_precheck=4868469 e2big_after_add=4982291
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=77056 requested_ns=18368 (bound 100000)
dir: keys off home (sum over trials)=7556 max probe distance=23 max claimed=64 trials with E2BIG while claimed < max_entries=1000
RESULT CLEAN
### exit=0 wall_s=36.149613310
```

### t8-plainclaim-128-x200
```
### t8-plainclaim-128-x200: ./build/c4_plainclaim --max 128 --trials 200
C4 build=ctl1-plain-store-claim FAULT=1 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=200 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=56 live=56 claiming=0 (max_entries 128, S 256) bound=128 next_row=128
dir shape: keys sharing a home slot=10 keys off home=0 max probe distance=0
totals: attempts=10000000 prefill=0 delivered=8739089 EEXIST=3745 E2BIG=1256828 unstable=0 | all rows=8739427 rows behind live headers=8739089
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=1661 waits=1661 wait_sleeps=1 wait_max_ns=47616 givebacks=0 e2big_precheck=1252540 e2big_after_add=4288 eexist_then_null=110 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=41
kernel span: first block start -> last block end = 7.275 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 3745/131837/128/0 L2 0/124999/0/0 L3 0/124999/0/0 L4 0/124999/0/0 L5 0/124999/0/0 L6 0/124999/0/0 L7 0/124999/0/0 L8 0/124999/0/0 L9 0/124999/0/0 L10 0/124999/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=1124991; longest wait requested_ns=64
violations: key_sum_mismatch=45 total_sum_mismatch=1 bookkeeping=2 winners_ne_1=45 tally_ne_counter=45 eexist_then_null=1 orphan_adds=1 key_changed=1
dir: claimed=50 live=50 claiming=0 (max_entries 128, S 256) bound=128 next_row=128
dir shape: keys sharing a home slot=13 keys off home=0 max probe distance=0
  note: orphan adds: all rows 8739427, rows behind live headers 8739089
  note: bound 128 != claimed slots 56
  note: next_row 128 != live headers 56
  note: EEXIST (not unstable), then own lookup NULL: 110
=== summary build=ctl1-plain-store-claim max_entries=128 trials=200 ===
trials_with_violation=200/200
violations by kind: key_sum_mismatch=8481 total_sum_mismatch=200 bookkeeping=400 winners_ne_1=8481 tally_ne_counter=8481 eexist_then_null=196 orphan_adds=200 key_changed=194
counted: EEXIST=639249 E2BIG=392608256 unstable=0 eexist_then_null=22508 unstable_then_null=0
per trial: EEXIST min=850 max=4537, E2BIG min=476033 max=4535434
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=315273 waits=315273 wait_sleeps=80331 wait_max_ns=76512 givebacks=0 e2big_precheck=391660768 e2big_after_add=947488
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=352125549
longest wait: elapsed_ns=76512 requested_ns=16320 (bound 100000)
dir: keys off home (sum over trials)=0 max probe distance=0 max claimed=61 trials with E2BIG while claimed < max_entries=200
RESULT VIOLATIONS
### exit=1 wall_s=9.753286209
```

### t9-nobound-48-x200
```
### t9-nobound-48-x200: ./build/c4_nobound --max 48 --trials 200
C4 build=ctl2-no-bound-reservation FAULT=2 device="Orin" sm_87 SMs=8
config max_entries=48 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=200 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 48, S 128) bound=0 next_row=64
dir shape: keys sharing a home slot=25 keys off home=13 max probe distance=4
totals: attempts=10000000 prefill=0 delivered=9987876 EEXIST=12124 E2BIG=0 unstable=0 | all rows=9987876 rows behind live headers=9987876
branches: lost_same_key=2735 lost_other_key=661 walk_saw_claiming=5749 waits=8484 wait_sleeps=3168 wait_max_ns=46432 givebacks=0 e2big_precheck=0 e2big_after_add=0 eexist_then_null=0 unstable_then_null=0 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.880 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 12124/0/64/0 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=4032
violations: claimed_gt_max=1
dir: claimed=64 live=64 claiming=0 (max_entries 48, S 128) bound=0 next_row=64
dir shape: keys sharing a home slot=24 keys off home=13 max probe distance=5
  note: claimed slots 64 > max_entries 48
  note: claimed slots 64 > max_entries 48
  note: claimed slots 64 > max_entries 48
=== summary build=ctl2-no-bound-reservation max_entries=48 trials=200 ===
trials_with_violation=200/200
violations by kind: claimed_gt_max=200
counted: EEXIST=2406584 E2BIG=0 unstable=0 eexist_then_null=0 unstable_then_null=0
per trial: EEXIST min=11330 max=12224, E2BIG min=0 max=0
branches: lost_same_key=559578 lost_other_key=143339 walk_saw_claiming=1093148 waits=1652726 wait_sleeps=365392 wait_max_ns=57664 givebacks=0 e2big_precheck=0 e2big_after_add=0
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=57664 requested_ns=10176 (bound 100000)
dir: keys off home (sum over trials)=2700 max probe distance=51 max claimed=64 trials with E2BIG while claimed < max_entries=0
RESULT VIOLATIONS
### exit=1 wall_s=7.738453203
```

### t10-plainclaim-64-x1000
```
### t10-plainclaim-64-x1000: ./build/c4_plainclaim --max 64 --trials 1000 --detail 0
C4 build=ctl1-plain-store-claim FAULT=1 device="Orin" sm_87 SMs=8
config max_entries=64 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=1000 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=1371448 waits=1371448 wait_sleeps=220942 wait_max_ns=71264 givebacks=0 e2big_precheck=4314063747 e2big_after_add=3941600
dir: keys off home (sum over trials)=0 max probe distance=0 max claimed=51 trials with E2BIG while claimed < max_entries=1000
=== summary build=ctl1-plain-store-claim max_entries=64 trials=1000 ===
trials_with_violation=1000/1000
violations by kind: key_sum_mismatch=21501 total_sum_mismatch=1000 bookkeeping=2000 winners_ne_1=21501 tally_ne_counter=21501 eexist_then_null=987 orphan_adds=1000 key_changed=979
counted: EEXIST=2701900 E2BIG=4318005347 unstable=0 eexist_then_null=120125 unstable_then_null=0
per trial: EEXIST min=1286 max=4581, E2BIG min=2036535 max=6721282
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=1371448 waits=1371448 wait_sleeps=220942 wait_max_ns=71264 givebacks=0 e2big_precheck=4314063747 e2big_after_add=3941600
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=3882091302
longest wait: elapsed_ns=71264 requested_ns=10176 (bound 100000)
dir: keys off home (sum over trials)=0 max probe distance=0 max claimed=51 trials with E2BIG while claimed < max_entries=1000
RESULT VIOLATIONS
### exit=1 wall_s=50.239351643
```

### t11-plainclaim-48-x1000
```
### t11-plainclaim-48-x1000: ./build/c4_plainclaim --max 48 --trials 1000 --detail 0
C4 build=ctl1-plain-store-claim FAULT=1 device="Orin" sm_87 SMs=8
config max_entries=48 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=1000 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=1058701 waits=1058701 wait_sleeps=5402 wait_max_ns=58880 givebacks=0 e2big_precheck=5100448889 e2big_after_add=3718240
dir: keys off home (sum over trials)=0 max probe distance=0 max claimed=42 trials with E2BIG while claimed < max_entries=1000
=== summary build=ctl1-plain-store-claim max_entries=48 trials=1000 ===
trials_with_violation=1000/1000
violations by kind: key_sum_mismatch=14200 total_sum_mismatch=1000 bookkeeping=2000 winners_ne_1=14200 tally_ne_counter=14200 eexist_then_null=959 orphan_adds=1000 key_changed=899
counted: EEXIST=2205909 E2BIG=5104167129 unstable=0 eexist_then_null=83587 unstable_then_null=0
per trial: EEXIST min=1147 max=3558, E2BIG min=3442746 max=7189802
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=1058701 waits=1058701 wait_sleeps=5402 wait_max_ns=58880 givebacks=0 e2big_precheck=5100448889 e2big_after_add=3718240
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=4590280251
longest wait: elapsed_ns=58880 requested_ns=192 (bound 100000)
dir: keys off home (sum over trials)=0 max probe distance=0 max claimed=42 trials with E2BIG while claimed < max_entries=1000
RESULT VIOLATIONS
### exit=1 wall_s=53.667140403
```

### t12-plainnobound-128-x200
```
### t12-plainnobound-128-x200: ./build/c4_plainnobound --max 128 --trials 200 --detail 0
C4 build=ctl4-plain-store-and-no-reservation FAULT=4 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=200 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=693319 waits=693319 wait_sleeps=9881 wait_max_ns=44960 givebacks=0 e2big_precheck=0 e2big_after_add=0
dir: keys off home (sum over trials)=1411 max probe distance=56 max claimed=113 trials with E2BIG while claimed < max_entries=0
=== summary build=ctl4-plain-store-and-no-reservation max_entries=128 trials=200 ===
trials_with_violation=200/200
violations by kind: key_in_two_slots=2071 key_sum_mismatch=12800 total_sum_mismatch=200 garbage_header=15934 bookkeeping=200 winners_ne_1=12250 tally_ne_counter=10588 eexist_then_null=200 orphan_adds=200 anomaly=200 key_changed=198
counted: EEXIST=1991420720 E2BIG=0 unstable=0 eexist_then_null=1991410566 unstable_then_null=0
per trial: EEXIST min=8748740 max=9997840, E2BIG min=0 max=0
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=693319 waits=693319 wait_sleeps=9881 wait_max_ns=44960 givebacks=0 e2big_precheck=0 e2big_after_add=0
after launch 1: EEXIST=1792828179 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=44960 requested_ns=6080 (bound 100000)
dir: keys off home (sum over trials)=1411 max probe distance=56 max claimed=113 trials with E2BIG while claimed < max_entries=0
RESULT VIOLATIONS
### exit=1 wall_s=10.459571979
```

### t13-plainnobound-48-x200
```
### t13-plainnobound-48-x200: ./build/c4_plainnobound --max 48 --trials 200 --detail 0
C4 build=ctl4-plain-store-and-no-reservation FAULT=4 device="Orin" sm_87 SMs=8
config max_entries=48 S=128 logS=7 rows_cap=128 threads=1000000 grid=3907x256 launches=10 trials=200 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=100000
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=662259 waits=662259 wait_sleeps=16112 wait_max_ns=61856 givebacks=0 e2big_precheck=0 e2big_after_add=0
dir: keys off home (sum over trials)=2921 max probe distance=126 max claimed=128 trials with E2BIG while claimed < max_entries=0
=== summary build=ctl4-plain-store-and-no-reservation max_entries=48 trials=200 ===
trials_with_violation=200/200
violations by kind: key_in_two_slots=3797 claimed_gt_max=200 key_sum_mismatch=12800 total_sum_mismatch=200 garbage_header=18496 bookkeeping=200 winners_ne_1=9622 tally_ne_counter=8448 eexist_then_null=200 orphan_adds=200 anomaly=200 key_changed=200
counted: EEXIST=1998601966 E2BIG=0 unstable=0 eexist_then_null=1998600552 unstable_then_null=0
per trial: EEXIST min=9684848 max=9997554, E2BIG min=0 max=0
branches: lost_same_key=0 lost_other_key=0 walk_saw_claiming=662259 waits=662259 wait_sleeps=16112 wait_max_ns=61856 givebacks=0 e2big_precheck=0 e2big_after_add=0
after launch 1: EEXIST=1799296866 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=61856 requested_ns=6080 (bound 100000)
dir: keys off home (sum over trials)=2921 max probe distance=126 max claimed=128 trials with E2BIG while claimed < max_entries=0
RESULT VIOLATIONS
### exit=1 wall_s=14.522620084
```

### t14-proto-nowait-128-x300
```
### t14-proto-nowait-128-x300: ./build/c4_proto_nowait --max 128 --trials 300
C4 build=protocol FAULT=0 device="Orin" sm_87 SMs=8
config max_entries=128 S=256 logS=8 rows_cap=256 threads=1000000 grid=3907x256 launches=10 trials=300 reader=0 prefill=0 seed0=0x0123456789abcdef wait_bound_ns=0
attempts per key per launch: min=15624 max=15626 (64 keys, K = 1000 + 7919*j)
=== trial 0 seed=0x0123456789abcdef ===
dir: claimed=64 live=64 claiming=0 (max_entries 128, S 256) bound=64 next_row=64
dir shape: keys sharing a home slot=10 keys off home=5 max probe distance=1
totals: attempts=10000000 prefill=0 delivered=9986045 EEXIST=5176 E2BIG=8779 unstable=492 | all rows=9986045 rows behind live headers=9986045
branches: lost_same_key=152 lost_other_key=6 walk_saw_claiming=3056 waits=3208 wait_sleeps=0 wait_max_ns=11552 givebacks=155 e2big_precheck=4470 e2big_after_add=4309 eexist_then_null=0 unstable_then_null=51 walk_overflow=0 row_overflow=0 key_changed=0
kernel span: first block start -> last block end = 6.518 ms (10 launches)
per launch (EEXIST/E2BIG/won/unstable): L1 5176/8779/64/492 L2 0/0/0/0 L3 0/0/0/0 L4 0/0/0/0 L5 0/0/0/0 L6 0/0/0/0 L7 0/0/0/0 L8 0/0/0/0 L9 0/0/0/0 L10 0/0/0/0
E2BIG after launch 1: keys that ended in a slot=0 keys never in=0; longest wait requested_ns=0
violations: none
branches: lost_same_key=45422 lost_other_key=5061 walk_saw_claiming=855006 waits=900428 wait_sleeps=0 wait_max_ns=22656 givebacks=48704 e2big_precheck=1490737 e2big_after_add=1302613
dir: keys off home (sum over trials)=2190 max probe distance=10 max claimed=64 trials with E2BIG while claimed < max_entries=300
=== summary build=protocol max_entries=128 trials=300 ===
trials_with_violation=0/300
violations by kind: none
counted: EEXIST=1531522 E2BIG=2793350 unstable=145044 eexist_then_null=0 unstable_then_null=28827
per trial: EEXIST min=3193 max=6740, E2BIG min=7511 max=13009
branches: lost_same_key=45422 lost_other_key=5061 walk_saw_claiming=855006 waits=900428 wait_sleeps=0 wait_max_ns=22656 givebacks=48704 e2big_precheck=1490737 e2big_after_add=1302613
after launch 1: EEXIST=0 won=0 unstable=0 E2BIG on keys that ended in a slot=0 E2BIG on keys never in=0
longest wait: elapsed_ns=22656 requested_ns=0 (bound 0)
dir: keys off home (sum over trials)=2190 max probe distance=10 max claimed=64 trials with E2BIG while claimed < max_entries=300
RESULT CLEAN
### exit=0 wall_s=11.378917734
```

