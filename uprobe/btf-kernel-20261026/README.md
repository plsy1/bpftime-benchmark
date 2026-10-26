# BTF kernel: CO-RE real-tool audit and kernel baselines (2026-10-26)

Kernel `6.8.12-1021-tegra-btf` (BTF, syscall tracepoints, kprobes, function tracer), the
default boot entry since 10/26 11:03; `jetson_clocks`, cpu5 1728000 before and after every
run. Builds: `official` = upstream 177a1dc unpatched (per-event CPU pinning included),
`a7d54d7e` = dev branch HEAD; both frozen in `ssl-nginx-2x2-20261023/builds`.

## Tool builds (no BTF workarounds any more)

- libbpf-tools v0.37.0, unmodified sources (the no-BTF Makefile/btf_helpers diff reverted),
  clang-15, bpftime's libbpf 1.7.0, CO-RE relocations kept (`.BTF.ext` present). Tools:
  bashreadline, funclatency, gethostlatency, memleak, javagc, opensnoop, syscount.
  `INCLUDES` only adds the libbpf uapi and `/usr/include/aarch64-linux-gnu` paths.
- bpftime examples, unmodified, built in `-core` copies of the official tree: malloc,
  funclatency, usdt_minimal, bashreadline, sslsniff, get_stack_id_example, opensnoop, syscount.
- bpftrace 0.20.2 (Ubuntu), OpenJDK 17 (its libjvm.so carries the hotspot USDT probes).
- Logs: `realprog-audit/src/build-*.log`.

## Functional audit (`realprog-audit/final.jsonl`, raw outputs trimmed in `audit-raw/`)

18 tools; each prints/records what the workload did, compared with the workload's exact count.
New workloads: `sslloop` (OpenSSL client+server over a BIO pair), `openloop`, `GcLoop.java`.

| Tool | kernel | official | a7d54d7e |
|---|---|---|---|
| funclatency (libbpf-tools) | exact | traced process crashes | exact |
| funclatency (bpftime example) | exact | exact | exact |
| gethostlatency | exact | 434 events missing, no loss reported | exact |
| memleak | 1,027 leaks | 0 leaks, 1.05M stack errors | 1,026 leaks |
| malloc example | exact (+1,037 setup mallocs) | exact this run (over-counts at times) | exact |
| bashreadline (libbpf-tools) | 200/200 | 200/200 | 200/200 |
| usdt_minimal | 8/8 | 7/8 (printk lost from stdio buffer) | 8/8 |
| sslsniff (example) | exact | exact | exact |
| bashreadline (example) | 200/200 | 200/200 | 200/200 |
| get_stack_id_example | stacks fail: `-EEXIST` (the example traces every process; map full) | 864 events incl. agent's own mallocs, 2-frame stacks | 31 events, 6-frame stacks |
| bpftrace uprobe count | exact | exact | exact |
| bpftrace uprobe+uretprobe hist | exact | exact | exact |
| bpftrace `ustack` count | exact | exact, workload ~11x slower (gum backtracer) | exact |
| bpftrace USDT `hotspot:gc__begin` in a JVM | 50/50 | 50/50 | 50/50 |
| bpftrace bashreadline.bt | 200/200 | 200/200 | 200/200 |
| bpftrace gethostlatency.bt | exact | exact | exact |
| javagc (libbpf-tools) | 50/50 | not run | not run |
| opensnoop (libbpf-tools, syscall tracepoints) | 38,797/38,800 | 0, silently | 0, silently |

- ustack symbols (live process): kernel `target_func, __libc_start_call_main,
  __libc_start_main_impl, _start`; a7d54d7e the same plus the agent's own
  `bpftime_hooked_main` frame (it wraps main). The immediate caller is missing at entry in both.
- javagc needs the JVM's pid before it loads (`-p`), i.e. injecting the agent into a running
  process (`bpftime attach`, the frida CLI), which these builds do not include.
- opensnoop/syscount on bpftime: load and "Attach successfully", then record nothing: the aarch64
  syscall trampoline is missing. They should fail at attach instead.
- Not bpftime: opensnoop `-n NAME` exits with "error polling ring/perf buffer: Operation not
  permitted" on the kernel too (v0.37 `handle_event` returns -1 for filtered events, which
  `ring_buffer__poll` propagates); the audit filters the output itself. The bpftime examples
  `opensnoop` (attaches `sys_enter_open`, absent on arm64) is not arm64-ready.

## Real-tool throughput (`realprog-measure/`, 5 rounds x 10 s, medians, traced program ops/s)

| Tool | not traced | kernel | official | a7d54d7e | a7d54d7e / kernel | official / kernel |
|---|---:|---:|---:|---:|---:|---:|
| gethostlatency | 112,673 | 62,086 | 56,258 | 88,226 | 1.42x | 0.91x |
| malloc example (printk per call) | 42.5M | 537,594 | 1,940,265 | 519,392 | 0.97x | 3.61x |
| funclatency (bpftime example) | 89.8M | 505,050 | 1,681,368 | 1,847,435 | 3.66x | 3.33x |
| funclatency (libbpf-tools) | 90.0M | 484,264 | crash | 1,473,208 | 3.04x | - |
| memleak | 42.5M | 331,007 | not working | 419,432 | 1.27x | - |
| bpftrace uprobe count | 89.7M | 795,350 | 3,946,528 | 4,290,175 | 5.39x | 4.96x |
| bpftrace uprobe+uretprobe hist | 90.2M | 468,611 | 1,162,976 | 1,233,130 | 2.63x | 2.48x |
| bpftrace gethostlatency.bt | 113,484 | 61,412 | 87,401 | 85,162 | 1.39x | 1.42x |

Event counts match the workload in every run except reported perf-buffer losses
(gethostlatency: missing = "lost N events", kernel and a7d54d7e; official misses 327-663 per
run without reporting them) and malloc example's 1,000-odd setup mallocs (all backends).
Stock-kernel figures (10/26 01:00, `realprog-rest-20261026/measure-run-2`): kernel within
-1% to +4% of these.

## ssl-nginx (`ssl-nginx/`, 2 rounds x 10 reps, wrk -c 100 -d 10, same CO-RE sslsniff for every leg)

| Leg | 16 B req/s | vs kernel | 256 KB req/s | vs kernel |
|---|---:|---:|---:|---:|
| baseline (no tracing) | 17,580 | +44.5% | 1,749.9 | +39.1% |
| kernel | 12,163 | 0 | 1,258.1 | 0 |
| official 177a1dc | 10,845 | -10.8% | 1,139.6 | -9.4% |
| a7d54d7e | 14,293 | +17.5% | 1,389.4 | +10.4% |

Paired per rep a7d54d7e vs kernel: +17.6% (16 B), +10.7% (256 KB), 20/20 faster each.
Audit: 2.000 events/request for all legs at 16 B; at 256 KB kernel 18.04, a7d54d7e 17.85,
official 9.46 (its perf output drops half, fixed by db3d475f). Stock kernel, 10/24: kernel
11,967-12,027 (16 B) and 1,204-1,216 (256 KB).

## Complete ten-round uprobe suite with the kernel arm (`full10-a7d54d7e-kernel/`)

`full10/comparison.md`: kernel arm on the BTF kernel vs the stock kernel (10/21 run, same
benchmark binaries) moved -3.1% to +2.2% per operation (uprobe +1.0%, uretprobe +2.0%,
uprobe+uretprobe +1.5%); a7d54d7e's userspace arm is within ±4% of its 10/26 01:52 run, and
its ratios to the kernel are unchanged within ±0.2 (e.g. uprobe 0.16x, array lookup 0.31x,
hash lookup 0.82x, array delete 5.66x).

Conclusion: the BTF/tracing kernel config does not change kernel uprobe or map costs beyond
±3%, so the earlier kernel comparisons stand.
