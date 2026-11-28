# S8: measurement and correctness against today (2026-11-28)

Result: `S8-result.md`. Rules: `S8-RULES.md` (with its two addenda, each written before its runs).

- `run_s8_victim.sh`, `run_s8_llama.sh`: the drivers (pt_victim; llama.cpp B2/B3); B1 ran with
  `proto/meas/run_meas3.sh` (passes `s8b1`, `s8b2`).
- `analyze_s8_*.py`, `analysis-*.txt`: the analyses and their output (`analysis-AC.txt`: A and C;
  `analysis-nochk.txt`: addendum 1; `analysis-B1.txt`; `analysis-B23.txt`: B2/B3;
  `analysis-B23-add2.txt`: addendum 2).
- `out/<pass>/`: every run (meta, agent and server logs, map dumps). Left out: the host-path
  runs' `h.bin` (the whole 262,144-entry HASH map read key by key, 2 MB each; its sum is in
  `maps.json`).
- `recheck/`: the re-checks on the final build.
- `dump-lat-db21d44f.ptx.gz`: lat_mmvq's patched PTX on this build. S0.2's forms were
  regenerated from it with `../s0/make_s02.py` (`s8_dh_route`, `s8_dh_check`; `s8_hd_t0` is the
  dump itself).
- `tools/`: the tool sources as run (`pt_hash.bpf.c` with `HS_DEV`, `HS_NOCHK`, `HS_KEY_KMOD`;
  `pt_lat.bpf.c` with `PT_DEVHASH`; `pt_loader.c`; `build_hs.sh`).
