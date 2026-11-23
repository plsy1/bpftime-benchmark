# E0 (Jetson) — read-only checks and functional gate, 2026-11-23

All items pass. Not timing data (single reps).

| # | item | result |
|---|---|---|
| 0 | probe-site first instructions (G12) | SSL_read/SSL_write `paciasp`, `__bench_uprobe` `ldrb` — no `nop`, G12 not triggered (`probe-sites.md`) |
| 0 | kernel provenance | /proc/version + Image-btf mtime + source tarball sha OK; build tree gone, no byte compare (`probe-sites.md`) |
| 1 | image | `bpftime-ssl-nginx:20261023` = sha256:15365008c54c…, recorded in `../image-id.txt` |
| 2 | D unit tests | x86 only (plan) |
| 3 | activation audit | all 6 arms: 17/17 cases; D and Dpc log 2 `[array-jit-inline] inlined` sites. With the dev AOT cache the inline lines are in the loader log, not the victim log (audit script checks both) — `activation/` |
| 4 | smoke 16 B | every S4 tracing leg ev/req = 2.000; byte conservation exact for O, Odiag, L3port, D, Opc, Dpc; stage legs (S1, S2h, S2hm, S3, NOOP_OUTPUT) delivered 0, lost 0; response = 261 B per request (245 header + 16 body) |
| 4 | read_ev / req_nginx | 1 for every bpftime leg. K is always exactly 1 request short (1 read + 261 bytes): the harness's readiness `curl` is served before the kernel sslsniff attaches to the worker, while the bpftime agent is preloaded and sees it. Within τ; worth subtracting in the audit |
| 4 | hits/request (K+BPFSTATS) | rw_enter 3.001, read_exit 2.001, write_exit 1.000 → 6 hits/request (same as x86); run_time/hit 588 / 1765 / 4050 ns |
| 5 | final consume | cnt JSON present in every leg, no crash, tail_ev 0 |
| 6 | field consistency (`-x`) | K, O, Odiag+ALL3, L3port, D: every event comm=nginx, uid=65534, pid=tid=worker pid |
| 7 | Odiag all off | report counters all 0 (flags=0000); ALL3: worker no_pin≈2/req, uid/gid and pid/tid caches filled once; NOOP_OUTPUT counter = 2/req |
| 8 | S4 vs stock BPF | llvm-objdump identical |

256 KB single reps (not a result, for orientation): O delivered 36.9 % of bytes with 0 reported lost (silent); D 99.8 % with lost 443 = missing 443; K 99.86 %, missing 282 vs lost 264 (difference 18, inside τ = max(1e-4 × exp_ev, 2 × conns) = 200; τ is calibrated in E1).

Raw rows: `E0-smoke-rows.jsonl`; full raw run on the Jetson in `~/src/paper-runs-20261124/raw/E0-smoke-20261123/`.
