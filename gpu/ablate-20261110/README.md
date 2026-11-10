# Ablations: which dev change causes each official-vs-dev GPU benchmark difference (2026-11-10)

Follow-up to [`../compare-20261110`](../compare-20261110/) (official 4708f409 vs dev c115b33d on the
upstream GPU benchmark). Its [attribution.md](../compare-20261110/attribution.md) left several
differences open, with four candidate dev changes. Each candidate is undone here on its own,
on top of dev, and measured against official and dev in 10 interleaved rounds.

Tables: [summary.md](summary.md) (per case and set: median over rounds of the round means, and of
the round medians, with range; each ablation's shift against dev, the share of the official-dev
gap it closes, and a Mann-Whitney p against dev). Checks: [ablate-verify.csv](ablate-verify.csv).
The conclusions are in attribution.md, section "Ablations".

## Sets

| set | build | what is undone |
|---|---|---|
| O | official 4708f409 (`build-cuda` of the compare round, same hashes) | - |
| D | dev c115b33d (`build-cuda` of the compare round, same hashes) | - |
| L | D, run with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0` | 811b841f's launch path: patched copies loaded beside the originals, each launch redirected by kernel name, as official does |
| F | dev + [patches/frame.diff](patches/frame.diff), rebuilt | 636501ff: GPU programs without local calls keep 32 stack frames (16,464 B a thread instead of 592) |
| G | dev + [patches/gate.diff](patches/gate.diff), rebuilt | 5c5312d7: no `__bpftime_probes_enabled` load at the start of each probe |
| T | dev + [patches/tramp.diff](patches/tramp.diff), trampoline PTX regenerated, rebuilt | 518f05b5's device code for 1512/1513 lookup and update (official's address arithmetic, no bounds check or E2BIG, `membar.sys`) and for the GPU ringbuf a full check before dev's busy-word CAS (official returns there). IPC map branches and the host side (watcher, `drain_data`) are dev's |

F, G and T are separate builds from separate source copies (no `.git`) with the same flags as
D (RelWithDebInfo, gcc-13, LLVM 15, LTO off, `SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG`, CUDA 13.2;
[build/identity.txt](build/identity.txt), build logs in `build/`). The trampoline PTX regenerated
from unmodified dev source matched the committed `trampoline_ptx.h` byte for byte, so T's PTX
differs only by the patch. An independent reviewer read the three patches before the runs; T
was redone after its first version also changed the IPC branches and used volatile loads.

## Cases

The 18 open micro cases plus the five baselines, and the examples Baseline (minimal) and Kernel
Retsnoop (minimal); the micro probe asks for 1512/1513 and the victim uses the legacy launch,
as sets A/B of the compare round (`scripts/gen_ablate.py`, configs in `configs/`). Info logging.

## Runs

10 rounds, 15:15-18:57 JST. Each round runs O, D, L, F, G, T (micro, then examples), the order
rotated by one set per round (`scripts/rounds.sh`, `scripts/run_batch.sh`: jetson_clocks checked
before and after every batch, cpu5 1728000 and GPU 1020000000 throughout; a fresh shared memory
name per batch, removed after it; no leftover processes). All 120 batches ended with rc 0.

## Checks

- Smoke (`runs/S-*`, debug level, minimal geometry, `smoke-verify.csv`): every set valid; L logs
  each launch redirected (4 per case) and loads no patched image; F's kernels have 16,464 /
  16,504-byte frames (D: 592 / 632); G's probe PTX has no `ld.global.u32` of the gate (D: 2);
  Retsnoop 128 events on every dev set, 32 on official.
- Rounds (`ablate-verify.csv`, 1,500 case sections): every case valid and finished, no failure
  string; D, F, G, T log the patched image for every non-memtrace case, L and O only the
  official-style `Recorded pass`; Retsnoop 128 events on D, L, F, G, T and 32 on O in all 10
  rounds.

## Files

- `summary.md`, `ablate-verify.csv`, `smoke-verify.csv`.
- `runs/<set><round>/`: result json/md and runner logs; `runs/S-<set>/`: the smoke (logs over
  1 MB are `.xz`).
- `patches/`: the three ablation patches against dev c115b33d.
- `configs/`: generated runner configs. `scripts/`: build, config, rounds, smoke, batch, check and
  summary scripts.
- `build/`: identity (cache entries, library hashes) and build logs of F, G, T.
