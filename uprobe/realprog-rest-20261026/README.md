# Remaining bpftime issues from the real-program audit (2026-10-26)

Dev branch, on top of f9d2d9fc. Each commit has a test that fails without it.

| Commit | Change |
|---|---|
| 597d89f0 | `bpf_trace_printk` no longer writes the host's stdout by default (upstream contract #634): a bpftime log record, or `BPFTIME_TRACE_PRINTK_OUTPUT` = stdout / stderr / none / file (one `write`, no host stdio buffer); returns the length |
| 4341a60b | `bpf()`/`perf_event_open()` failing in userspace return `ENOMEM`/`EINVAL` instead of falling back to the kernel (which created a kernel map memleak's programs could not resolve); default shm 128 MB (Boost commits the segment, so it is resident) |
| da50a34c | per-event runtime errors rate-limited (1st, 10th, 100th, ...) |
| 71ce81b7 | a fork child of a started server closing an inherited fd no longer destroys the object for every process |
| b00be207 | `bpf_get_current_cgroup_id` (helper 80) |
| f2596ce8 | stack frames stripped of PAC with XPACI (`gum_strip_code_address` does not strip on Linux) |
| 4d9b8a61 | agent attach, thread creation, setenv and background threads hidden from probes (memleak 1,026 outstanding, kernel 1,027; was ~1,275) |
| a7d54d7e | rate-limited logging out of line: inlined, it made `bpf_probe_read` pay a stack-protector canary (+31% probe read, +27% probe write in `full10-4d9b8a61`) |

gethostlatency's 0.5-0.9% shortfall was the harness: expected calls were estimated as median rate
x seconds. With exact totals (`total_ops`) the tool's count plus its "lost N events" lines equals
the calls exactly, on the kernel too (perf buffer overflow when the tool prints a line per event).

## Validation (a7d54d7e)

- Daily filter plus `[trace_printk]`, `[ratelimit]`: 110 cases pass (`tests.log`, head 4d9b8a61;
  a7d54d7e changes only the logging header, `[ratelimit]` re-run). Frida suite 18 cases.
- Audit (`audit-a7d54d7e.jsonl`): all 7 tools match: funclatency (libbpf-tools) 6,000,000 /
  6,000,000; gethostlatency 342,833 + 866 lost = 343,700; memleak 1,026 (kernel 1,027);
  usdt_minimal 8/8 (was 7/8: printk no longer lost in the host's stdio buffer at kill).
- Full ten (`full10-a7d54d7e` vs f9d2d9fc): within +/-2%; probe read -0.2%, probe write +0.4%.
- Throughput (`measure-run-2`, 5 rounds, vs kernel): gethostlatency 1.45x, funclatency example
  3.60x, funclatency (libbpf-tools) 3.12x, memleak 1.28x, malloc example 0.99x.
  Paired vs f9d2d9fc -1.3% to -2.5%; `perf stat` (`st-*.txt` run): instructions per call
  2,367 vs 2,373, cycles per call 932 vs 932 in the second pair: no added work, layout/run noise.
- malloc example -74% vs f9d2d9fc is the printk default (its program prints on every malloc): the
  bpftime log costs 453k calls/s, `stdout` 650k, `none` 3.0M, kernel 534k. A shared trace ring
  would be the fast kernel-like default.
