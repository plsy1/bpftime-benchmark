# Host-call watcher: WFE while probed kernels run (2026-11-11)

Direction 2, layer 1, item "watcher wake-up". Dev **46f5a7cb** on top of aa6eb973. Before:
`../hostwake-20261111` (a host call after a pause waited for the watcher's 50 us -> 1 ms
sleeps: 60-950 us at the median). Mechanism tested alone: `../wfe-20261111` (a GPU write wakes a
core in WFE in about 2 us, at the power of a sleeping core).

The change: launch hooks note every launch of probed code (per-stream event, generation, wake of a
sleeping watcher); while a noted launch may run the watcher waits on the request flag with WFE,
otherwise it sleeps as before but a noted launch ends the sleep. Details, rules and costs: the
commit message and CLAUDE.md, "CUDA Attach: Host Calls And GPU Ringbuf". Build:
`bpftime-gpu-work/wake/build-cuda` (`scripts/build.sh`, the compare round's flags); the earlier
builds (l1 = aa6eb973, the worktree's build-cuda = c115b33d) are untouched.

## Sparse caller (`sweep/`, harness of `../hostwake-20261111`, 3 rounds x 200 calls per gap)

`sweep/summary-final.md` (final build; `wake-*` is the build before the review fixes, same
numbers within noise): p50 13.5-16.6 us at every gap from 0 to 20 ms (before 60-950 us); p90 at
most 26.8 us; max 1,216 us (one call at 2 ms), otherwise at most 278 us.

## 24-case subset (`summary.md`, 10 interleaved rounds with the ready runner, 16:3x JST)

N dev aa6eb973 against W the change (`scripts/rounds_wake.sh`, prefix F; prefix R was the build
before the review fixes, `summary-prefix.md`, the same picture). All 20 batches rc 0, all 460 case
sections valid (`verify-final.csv`), loader numbers unchanged (calls=97, 128 events, 7 threads,
counter[0]=28).

- Faster (p < 0.01): Empty probe tiny 34.2 -> 28.0 us, CPU Array update 51.0 -> 41.1, lookup
  48.7 -> 45.0, GPU Array update large 245.3 -> 226.5. Likely the watcher keeping a core awake
  while kernels run (CPU-side launch latency); not attributed.
- Slower: GPU Ringbuf small 41.6 -> 46.0 (p 0.007), Memory Trace 57.5 -> 61.9 (p 0.005); by 1-5%
  Ringbuf xlarge (p 0.001), Empty xlarge, GPU Array update small, lookup large (p 0.023-0.031).
  Likely the note per probed launch (capture check, event record); not timed alone.
- The other 13 cases 0.91-1.08, p >= 0.054; CUDA Counter (back-to-back calls) 2,929 -> 2,897.

## Checks

- Unit tests: `[cuda-host-call]`, `[cuda-ringbuf]`, `[cuda-no-device]` (12 cases) and
  `bpftime_nv_attach_tests` (32) pass.
- Mutations (`mutations/`, final run `mut4.log`): no WFE; never back to sleeping; a noted launch
  not ending the sleep; note() not counting; noting a stream being captured: each fails a test.
  The first run (`mut.log`) let "no WFE" and "noted launch not ending the sleep" pass: the first
  exposed a real bug (without WFE the watcher spun forever after the first note), the second a
  test too loose; both fixed before the commit.
- Review (`review/findings.txt`: 20 findings, each with a refuting verifier): fixed the capture
  case on the driver path (+ test), the possible hang on event_mutex (try-lock), the watcher's
  queries during another thread's capture (relaxed capture mode), `_ptsz` stream 0 on the legacy
  stream, no opt-out (`BPFTIME_CUDA_WATCHER_WFE=0`). Left as documented costs: the core held
  while any probed kernel runs, every kernel of a patched library counting, caches never
  invalidated, CUDA 13 kernel handles not resolved (old latency).
