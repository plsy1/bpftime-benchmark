# BPFtime latest on Jetson: ssl-nginx bridge-path analysis

## Executive conclusion

The latest `bpftime:official-no-btf` image was tested in Docker's default bridge network, with both `wrk` and nginx communicating over the container's own loopback interface. This removes the host network namespace and Tailscale/netfilter path from the experiment.

Earlier July 12-14 latest-version summaries appeared to show a 16 B BPFtime advantage. A raw-window reconciliation now shows that those BPFtime measurements were strongly bimodal, mixing a reproducible 9.2-10.6k RPS state with an anomalous 12.2-15.1k RPS state. Their positive aggregate means are not safe evidence of a stable small-payload advantage.

The SSL probes execute in the nginx worker, not in the `sslsniff` reader. Consequently, profiling only the reader omits the BPF VM/JIT, helper, and event-production work charged to nginx.

For the large-payload regression, the main differentiator is the payload event-production/export path rather than `perf_buffer__poll` itself or reader-side formatting:

- Removing reader formatting and writes barely changes throughput.
- Emitting metadata-only events recovers a substantial part of BPFtime throughput and narrows the kernel/BPFtime gap.
- Retaining probe/map/payload-copy work but removing `bpf_perf_event_output` makes kernel and BPFtime approximately equal at 256 KB.
- Direct timing inside the nginx worker shows that per-event CPU-affinity save/pin/restore consumes about 68-70% of `bpf_perf_event_output` time.
- A diagnostic A/B that removes only those affinity operations improves BPFtime throughput by about 21-22%; caching the map type and handler lookup alone has no repeatable benefit.
- Producer-side accounting directly confirms that the BPFtime software perf buffer drops most attempted records under this workload without reporting them through the reader's lost-event callback.

The strongest current explanation is therefore:

> Large SSL responses raise BPFtime event-output calls from about 2.0 to 17.9 per request and attempted event bytes by about 315× per request. Each helper call costs roughly 8-9 microseconds on this Jetson, with per-event CPU-affinity operations accounting for about 68-70% of that time. The fixed affinity cost is therefore multiplied by record count, while successful large-record appends add a smaller payload-copy cost. Reader formatting is visible CPU work, but it is not the dominant cause of the throughput gap.

## Test identity

| Item | Value |
|---|---|
| Image | `bpftime:official-no-btf` |
| Image ID | `sha256:c278b16ccbdafc62539ff8c9154279dd329b2e60eb542e9d1b1bc100ae0c1b06` |
| Image source HEAD | `6bd5319d3afb683716c7f9273508598079a8dc7e` |
| Network | Docker default bridge; workload uses container loopback |
| PID namespace | Host, only so host `perf` can attach to container tasks |
| Tailscale | Active on the host, but outside the workload network namespace |
| Benchmark source SHA-256 | `e271ac3a46cc833b2eb9f182bd1109c9583824b60eec0004756a661acc424dc4` |
| Toolchain for diagnostic variants | Clang 16.0.6 / LLVM 16 |
| BTF policy | `.BTF` retained; `.BTF.ext` removed |

The production `benchmark/ssl-nginx/benchmark.py` was not edited. Diagnostic variants were built temporarily from the same image source and toolchain.

## Probe execution ownership

The event's PID/TID is produced by `bpf_get_current_pid_tgid()` in `sslsniff.bpf.c`. A separate low-load capture excluded from performance statistics showed:

| Mode | Parsed events | Event COMM | Event PID | Observed nginx worker PID | Match |
|---|---:|---|---:|---:|---|
| kernel | 43,910 | `nginx` | 1,556,635 | 1,556,635 | yes |
| BPFtime | 8,476 | `nginx` | 1,556,986 | 1,556,986 | yes |

This proves the probe program and event-production helpers run on the nginx worker's execution path. The `sslsniff` process only consumes the resulting records.

## Uninstrumented throughput

Five interleaved rounds per payload, 10-second `wrk` windows, with no `perf` attached:

| Payload | Baseline RPS | Kernel RPS | BPFtime RPS | BPFtime vs kernel | Kernel CV | BPFtime CV |
|---|---:|---:|---:|---:|---:|---:|
| 16 B | 16,659.46 | 11,165.64 | 10,219.80 | -8.47% | 1.81% | 1.64% |
| 256 KB | 1,697.22 | 1,170.48 | 1,033.30 | -11.72% | 1.81% | 5.36% |

The large payload widens the observed gap by 3.25 percentage points. This specific run did not reproduce a small-payload BPFtime advantage, so it should not be used to claim one.

### Why earlier latest 16 B summaries were positive

The earlier host and Docker reports averaged ten internal `wrk` windows into each displayed result. Inspecting the underlying windows instead of only the displayed means reveals two distinct BPFtime populations. The descriptive split below uses `<11k` and `>=12k` because the old Docker data has no observations between 10.57k and 13.00k; the host data has only one transition observation at 11.58k.

| Dataset | Raw BPFtime windows | Kernel mean | BPFtime aggregate mean | Low cluster | High cluster | Raw BPFtime SD |
|---|---:|---:|---:|---:|---:|---:|
| latest host, July 12 | 50 | 11,152.89 | 11,848.66 (+6.24%) | 22 at 9,701.92 | 27 at 13,607.74 | 2,029.73 |
| latest Docker, July 13-14 | 20 | 11,086.57 | 12,768.19 (+15.17%) | 6 at 10,230.20 | 14 at 13,855.91 | 1,737.10 |
| latest Docker bridge, July 21 | 5 | 11,165.64 | 10,219.80 (-8.47%) | 5 at 10,219.80 | 0 | 168.00 |

The old Docker low cluster and the later bridge BPFtime mean differ by only 0.10%; their kernel means differ by 0.71%. The later five-round result therefore reproduces the old low state almost exactly. The apparent conflict is caused by averaging the old low and high states together, not by an 8.47-point movement of one stable distribution.

The old high cluster is also close to the 13,224.97 RPS observed in the later 16 B `no-event-output` diagnostic variant. This suggests that some old high windows executed substantially less tracing/event-output work. It does not prove the exact historical failure mode: the old benchmark discarded tracer stdout/stderr and recorded neither producer event counts nor attachment validation, so an attach failure, partial attachment, or another reduction in event work cannot be distinguished retrospectively.

Consequently, the earlier `+6.24%` host and `+15.17%` Docker means should not be cited as stable latest-version 16 B performance. The active-probe bridge result is the better-controlled measurement, although its severe hidden event loss still limits a production-fidelity claim.

## Process accounting

Three rounds per payload with an 8-second `perf stat` window. Values are hardware instructions divided by `Requests/sec × 8 s`.

| Payload | Mode | nginx extra insn/request vs baseline | reader insn/request | attributed total insn/request |
|---|---|---:|---:|---:|
| 16 B | kernel | 25,455 | 56,468 | 81,923 |
| 16 B | BPFtime | 45,797 | 491,029 | 536,826 |
| 256 KB | kernel | 221,636 | 977,175 | 1,198,810 |
| 256 KB | BPFtime | 391,208 | 2,763,326 | 3,154,534 |

These are process-level attribution estimates, not exact call-path decompositions. Concurrent perf counters perturb throughput, so the table is used for cost location rather than headline RPS.

Kernel BPF runtime statistics during the separate 1 KB profile showed approximately 1.19 million program invocations and 2.01 seconds of BPF runtime over the 8-second observation window, equivalent to about 13.9 program invocations and 23.7 microseconds of BPF runtime per request. Thus kernel BPF execution is measurable even though it does not appear in the reader's instruction count.

## Call-path evidence

The BPFtime nginx profile contains:

```text
bpf_perf_event_output
bpftime_perf_event_output
bpftime::software_perf_event_data::output_data
bpftime::software_perf_event_data::get_current_thread_shard
```

The normal reader profile contains:

```text
perf_buffer__poll
perf_buffer__process_records
handle_event
print_event
__memset_zva64
__GI___libc_write
```

The profiles establish where work is executed. The controlled variants below determine which work is throughput-critical.

## Controlled 16 B and 256 KB variants

Each diagnostic variant used three interleaved kernel/BPFtime rounds with no `perf` attached.

| Payload | Variant | Kernel RPS | BPFtime RPS | BPFtime vs kernel |
|---|---|---:|---:|---:|
| 16 B | normal | 11,165.64 | 10,219.80 | -8.47% |
| 16 B | reader-discard | 11,295.32 | 10,275.63 | -9.03% |
| 16 B | metadata-only | 11,307.85 | 10,606.42 | -6.20% |
| 16 B | no-event-output | 12,648.86 | 13,224.97 | +4.55% |
| 256 KB | normal | 1,170.48 | 1,033.30 | -11.72% |
| 256 KB | reader-discard | 1,166.36 | 1,048.49 | -10.11% |
| 256 KB | metadata-only | 1,215.83 | 1,170.57 | -3.72% |
| 256 KB | no-event-output | 1,312.68 | 1,319.29 | +0.50% |

Variant definitions:

- `reader-discard`: the full payload event is still generated and consumed, but `handle_event()` does not enter `print_event()` or write output.
- `metadata-only`: the exit probe emits one fixed-size metadata record but skips `bpf_probe_read_user()` and payload bytes.
- `no-event-output`: the probe retains map lookup/delete and payload-copy work but omits `bpf_perf_event_output`; the reader remains attached and polls an idle buffer.

At 256 KB, reader-discard improves BPFtime by only 1.47% relative to normal. Metadata-only improves it by 13.29%, and no-event-output improves it by 27.68% and removes the kernel/BPFtime gap within run variance. This is causal evidence that payload record creation/export is the dominant differential path.

## Direct nginx producer-path decomposition

A temporary diagnostic agent was rebuilt inside `bpftime:official-no-btf` from image source HEAD `6bd5319d3afb683716c7f9273508598079a8dc7e` with Clang 16. The benchmark source and normal tracer were unchanged. The agent sampled one in every 256 helper calls and reported cumulative counters every 4,096 calls. All 24 A/B cases passed these checks:

- the helper, software-perf data layer, and producer buffer reported identical call counts;
- producer-buffer calls equaled successful appends plus dropped appends;
- the requested diagnostic flags matched the flags observed inside nginx;
- no helper-level failure or HTTP error occurred.

### A/B throughput

Three rounds per configuration, 10-second `wrk` windows, Docker bridge networking:

| Payload | Configuration | BPFtime RPS | SD | Change vs diagnostic base |
|---|---|---:|---:|---:|
| 16 B | current path | 10,129.11 | 260.46 | — |
| 16 B | cache map type and handler fd | 10,251.91 | 138.33 | +1.21% |
| 16 B | skip affinity save/pin/restore | 12,208.05 | 357.40 | +20.52% |
| 16 B | cache + skip affinity | 12,244.21 | 614.35 | +20.88% |
| 256 KB | current path | 1,040.97 | 50.39 | — |
| 256 KB | cache map type and handler fd | 1,014.44 | 30.68 | -2.55% |
| 256 KB | skip affinity save/pin/restore | 1,272.67 | 42.48 | +22.26% |
| 256 KB | cache + skip affinity | 1,274.66 | 99.26 | +22.45% |

The cache-only result is indistinguishable from noise and is negative at 256 KB. The affinity A/B is large and consistent at both payloads. This identifies the repeated `sched_getaffinity()` plus two `sched_setaffinity()` calls as the main avoidable cost inside the current helper implementation.

The skip-affinity variant is diagnostic, not a production-ready patch. The current pinning prevents migration between selecting a CPU-indexed perf handler and writing the event. A real fix must preserve that correctness property through a migration-safe retry, a stable per-thread/per-CPU design, or another mechanism.

### Base-path timing and payload amplification

An additional three-round base run split successful and failed ring appends:

Its mean RPS differs from the earlier uninstrumented base by only +0.73% at 16 B and +1.17% at 256 KB, both smaller than the corresponding three-round standard deviation. The low-frequency counters therefore did not materially depress the measured throughput.

| Metric | 16 B | 256 KB | Large/small |
|---|---:|---:|---:|
| Event-output calls/request | 1.977 | 17.918 | 9.06× |
| Bytes passed to output/call | 211.5 | 7,354.2 | 34.77× |
| Attempted output bytes/request | 418 | 131,772 | 315.11× |
| Whole helper time/call | 8,297 ns | 8,991 ns | 1.08× |
| Affinity save/pin/restore/call | 5,784 ns | 6,119 ns | 1.06× |
| Affinity share of helper time | 69.7% | 68.1% | — |
| Map type + handler lookup/call | 327 ns | 382 ns | 1.17× |
| `bpftime_perf_event_output`/call | 2,042 ns | 2,328 ns | 1.14× |
| `get_current_thread_shard`/call | 1,276 ns | 1,430 ns | 1.12× |
| Successful ring append | 414 ns | 747 ns | 1.80× |
| Dropped ring append | 193 ns | 210 ns | 1.09× |

The large response does not make a single helper call dramatically slower. Instead, it generates about nine times as many calls per request. The per-event affinity cost is paid on every one of those calls. Successful large-record copies are about 1.8× slower than small-record copies, but this is secondary to the repeated affinity cost in the observed runs.

## Event loss caveat

At 256 KB, the normal kernel runs reported 17,138 lost events across five rounds; BPFtime reported no lost-event callbacks. The reader-discard kernel runs reported 4,157 lost events across three rounds, while metadata-only reported none.

Producer-side instrumentation now confirms that BPFtime also loses records internally. In the three-round base timing run, `software_perf_event_buffer::append_sample()` rejected 95.93% of attempted 16 B records and 91.36% of attempted 256 KB records because the producer buffer lacked space. These are producer-shard append failures; a successful append still does not guarantee final reader delivery. The rates vary with producer/consumer scheduling, but all measured base and A/B runs showed severe internal loss.

The current `software_perf_event_buffer::output_data()` ignores the Boolean result of `append_sample()` and returns zero, so `bpf_perf_event_output` observes success and the reader's lost-event callback is not notified. This explains why a missing BPFtime lost-event callback did not mean zero loss.

### Hidden-loss root cause

Additional short, single-window diagnostics located the mechanism rather than estimating another throughput mean:

1. Producer, producer-shard, consumer-buffer, and reader counters were recorded separately. At 16 B the producer shard reached 65,498 of 65,536 bytes and rejected writes, while the consumer buffer peaked below half capacity and `drain_producer_shards()` never stopped because the consumer was full.
2. Timestamped per-CPU counters showed that drain activity ended about 0.3-0.6 seconds after pressure began, while nginx continued producing for the remaining 5+ seconds. The affected producer shard then remained full.
3. Removing `print_event()` did not restore drain progress. A 3-second profile of this minimal callback attributed 76.12% of cycles directly to `handle_event()` and the remaining 23.88% to `perf_event_read_simple()`/`perf_buffer__process_records()`, even though the callback only incremented counters.
4. Callback sampling proved that the same address was processed repeatedly. In one four-second run the callback was invoked more than 306 million times for the same apparent record. Its outer type was `PERF_RECORD_SAMPLE`, but `perf_event_header.size` was zero, so libbpf's `data_tail += ehdr_size` never advanced.
5. The first zero-size record in the final address check occurred on CPU 3 at callback 9,091. The consumer ring data base was `0xffffb06aea70`; the apparent record header was at `0xffffb06bea6f`, exactly offset 65,535 in a 65,536-byte data ring. Only one byte remained before wrap.

The source behavior explains that boundary exactly:

- BPFtime's `append_record_parts()` writes `first_size + second_size` bytes and advances `data_head` by that exact, potentially unaligned size. `output_data()` likewise stores `sizeof(perf_sample_raw) + payload_size` in the header without 8-byte alignment.
- libbpf's `perf_event_read_simple()` masks `data_tail` into the ring and immediately evaluates `ehdr->size`. It only performs a wrap copy after reading that size. When a header starts at offset 65,535, the size field itself crosses the ring boundary, so the direct read observes zero/out-of-ring bytes and the loop cannot advance.
- A pre-handoff scanner that reconstructed wrapped headers correctly found zero invalid snapshots in two runs (477/477 and 452/452 valid). This rules out an invalid record produced by the normal copy path and distinguishes the bug from payload corruption: the incompatibility is the unaligned BPFtime record layout combined with libbpf's assumption that the header can be read contiguously.

The resulting failure chain is:

```text
unaligned software-perf records
  -> header begins at the final ring byte
  -> libbpf reads size=0 before wrap handling
  -> perf_event_read_simple repeats the same callback indefinitely
  -> reader never returns to epoll/drain_producer_shards
  -> producer shards fill
  -> append_sample rejects subsequent records
  -> output_data hides those failures and reports success
```

This also changes the interpretation of the earlier high reader instruction count: much of the observed `print_event()`/`handle_event()` work was repeated processing of a malformed zero-size record, not legitimate delivered SSL events. Normal formatting remains expensive, but it is not sufficient to explain the hidden loss.

### Alignment-fix validation

A minimal producer-side fix was applied to the latest no-BTF source for validation. It rounds the complete software-perf record to eight bytes, stores the aligned length in `perf_event_header.size`, advances `data_head` by the same length, zero-fills padding, rejects sizes that cannot fit in the 16-bit perf header, and rejects unaligned records in the internal shard-copy path.

The new 64-byte-ring regression test deliberately walks an eight-byte `perf_event_header` through the final valid header slot. The three software-perf unit tests passed all 8,358 assertions, including record-length, padding, payload, concurrency, resize, and boundary checks.

The rebuilt agent and syscall server were then mounted into the unchanged latest Docker image and tested with the unchanged benchmark in bridge mode at 16 B:

- Before the fix, three eight-second reader windows consumed 7.78-7.84 task-seconds and 36.4-36.7 billion user instructions. This is about 0.98 CPU-seconds and 4.57 billion instructions per wall-second.
- After the fix, a five-second reader window consumed 0.210 task-seconds and 0.809 billion user instructions, or about 0.042 CPU-seconds and 0.162 billion instructions per wall-second. Normalized reader CPU time fell about 95.7%, and user instructions fell about 96.4%.
- A normal-output diagnostic reader continued from 3,938 to 157,764 callbacks across successive reports, with polling continuing and no reported lost callback. A separate minimal callback reached 170,121 events. Neither run observed the previous zero-size record or repeated-callback spin; sampled record sizes were 120 and 336 bytes, both eight-byte aligned.
- The two fixed runs measured 10,583.67 and 10,507.93 requests/second. These single runs validate correctness and elimination of the spin, not a new throughput mean.

This confirms that the boundary bug caused the abnormal reader instruction count. It does not yet solve the separate observability issue: `output_data()` still hides a failed producer append, so explicit submitted/copied/consumed/lost counters remain necessary before claiming complete event conservation.

### Fixed BPFtime reader versus kernel reader

For the reader process alone, the fixed BPFtime path is now materially cheaper than the kernel reader. The kernel values below are the mean of three latest Docker-bridge 16 B runs with eight-second perf windows. The fixed BPFtime value is one unchanged-benchmark run with a five-second perf window; both sides are normalized by wall time and Requests/sec.

| Reader metric | Kernel mean | Fixed BPFtime | BPFtime reduction |
|---|---:|---:|---:|
| CPU-seconds per wall-second | 0.312 | 0.0417 | 86.6% |
| User instructions per wall-second | 0.329 B | 0.162 B | 50.6% |
| Kernel instructions per wall-second | 0.298 B | 0.0251 B | 91.6% |
| Total instructions per wall-second | 0.627 B | 0.187 B | 70.1% |
| Reader CPU time per request | 28.1 us | 3.94 us | 86.0% |
| Reader total instructions per request | 56.5 k | 17.7 k | 68.6% |

This is specifically the `sslsniff` process cost. It is not the complete tracing cost: kernel eBPF execution occurs in the nginx request context and is not charged to the kernel `sslsniff` process, while BPFtime probe/agent work is likewise charged to nginx. A total backend comparison therefore requires same-round baseline, traced-nginx, and reader accounting after the alignment fix. The one fixed BPFtime sample is sufficient to show that the former reader spin is gone, but not to establish a stable cross-backend performance mean.

### Same-round total-cost comparison

A new fixed-build run measured baseline, kernel, and BPFtime sequentially under the same latest Docker-bridge 16 B configuration. The raw attributed overhead is `(traced nginx - baseline nginx) + reader`:

| Metric | Kernel | Fixed BPFtime | Raw difference |
|---|---:|---:|---:|
| nginx tracing delta | 25.1 k insn/req | 46.0 k insn/req | BPFtime 83.2% higher |
| reader cost | 56.4 k insn/req | 18.1 k insn/req | BPFtime 67.9% lower |
| attributed total | 81.5 k insn/req | 64.1 k insn/req | BPFtime 21.4% lower |
| attributed CPU overhead | 56.4 us/req | 41.8 us/req | BPFtime 25.9% lower |
| Requests/sec | 11,292.24 | 10,270.96 | BPFtime 9.0% lower |

The lower raw total does not establish an equivalent-scope win. The kernel reader observed 3.96 events/request, while fixed BPFtime observed 1.92. Repeating with 256 rather than 16 perf-buffer pages produced nearly identical attributed totals and event rates, so the two-to-one event gap was not caused by the software perf ring capacity.

A temporary uprobe then counted 156,305 calls to `bpftime_perf_event_output()` during an eight-second window at 9,557.30 Requests/sec, or 2.04 output calls/request. The BPFtime reader's received-event rate was consistent with that producer call rate. Kernel mode consistently delivered about 3.7-4.0 events/request with no reported loss. Separate counters showed balanced read/write events on both backends.

Follow-up PID/COMM validation resolved the apparent gap. Kernel sslsniff attaches with the default PID `-1`, so it traces both TLS endpoints: the nginx server and the OpenSSL-based `wrk` client. The BPFtime agent is preloaded into nginx only. In direct one-second captures, 12,293 requests produced exactly 12,293 READ plus 12,293 WRITE events from `wrk`; a separate 12,579-request capture produced exactly 12,579 READ plus 12,579 WRITE events from nginx. Kernel's approximately four events/request are therefore two nginx events plus two `wrk` events, while BPFtime's approximately two are the expected nginx-only events. This is not post-fix reader loss or missed Frida probe execution.

The full same-round analysis is stored in `benchmark-results/latest/diagnostics/alignment-fix-total-cost-16b-20260722_120904/total-cost-comparison.md`.

The benchmark therefore does not compare equivalent tracing scope between backends. Kernel mode also imposes uprobe/BPF cost on the load generator, while BPFtime mode instruments only the server. The raw kernel reader cost includes both endpoints, and the raw attributed total omits the separately induced `wrk` cost. Faster diagnostic producer variants can also fill the buffer sooner and show a higher drop percentage. Their throughput improvement locates CPU cost, but it must not be treated as a production optimization result until event scope and delivery semantics are held constant.

This caveat strengthens the recommendation to optimize and instrument the event-export path, but it limits claims about an exact production overhead ratio.

## Result locations

- `benchmark-results/latest/diagnostics/bridge-probe-accounting-1kb-5x-valid-20260721_214243/`
- `benchmark-results/latest/diagnostics/bridge-probe-profiles-1kb-20260721_214751/`
- `benchmark-results/latest/diagnostics/bridge-throughput-16b-5x-20260721_215021/`
- `benchmark-results/latest/diagnostics/bridge-throughput-256kb-5x-20260721_215021/`
- `benchmark-results/latest/diagnostics/bridge-accounting-16b-3x-20260721_215915/`
- `benchmark-results/latest/diagnostics/bridge-accounting-256kb-3x-20260721_215915/`
- `benchmark-results/latest/diagnostics/bridge-{16b,256kb}-{reader,metadata,noevent}-3x-*/`
- `benchmark-results/latest/diagnostics/sslsniff-variants-20260721_220942/`
- `benchmark-results/latest/diagnostics/nginx-output-ab-20260722_092644/producer-summary.{md,json}`
- `benchmark-results/latest/diagnostics/nginx-output-ab-20260722_092644/producer-cases.tsv`
- `benchmark-results/latest/diagnostics/nginx-output-append-split-20260722_094108/producer-summary.{md,json}`
- `benchmark-results/latest/diagnostics/nginx-output-agent-v6-20260722_092234/`
- `benchmark-results/latest/diagnostics/nginx-output-agent-v7-20260722_093948/`
- `benchmark-results/latest/diagnostics/loss-path-16b-percpu-time-20260722_105556/`
- `benchmark-results/latest/diagnostics/loss-path-16b-consumer-scan-20260722_110304/`
- `benchmark-results/latest/diagnostics/loss-path-16b-reader-stack-20260722_110456/`
- `benchmark-results/latest/diagnostics/loss-path-16b-discard-stack-20260722_110747/`
- `benchmark-results/latest/diagnostics/loss-path-16b-reader-spin-20260722_110939/`
- `benchmark-results/latest/diagnostics/loss-path-16b-mmap-check-20260722_111449/`
- `benchmark-results/latest/diagnostics/loss-path-binaries-20260722_102825/`
- `benchmark-results/latest/diagnostics/alignment-fix-bridge-16b-20260722_115025/`
- `benchmark-results/latest/diagnostics/alignment-fix-callback-check-16b-20260722_115230/`
- `benchmark-results/latest/diagnostics/alignment-fix-normal-reader-16b-20260722_115443/`
- `benchmark-results/latest/diagnostics/alignment-fix-total-cost-16b-20260722_120904/`
- `benchmark-results/latest/diagnostics/alignment-fix-total-cost-pages256-16b-20260722_121239/`
- `benchmark-results/latest/diagnostics/alignment-fix-output-call-count-16b-20260722_122120/`
- `benchmark-results/latest/diagnostics/ssl-call-count-manual-16b-20260722/`
- `benchmark-results/latest/full/host-5x-20260712/`
- `benchmark-results/latest/full/docker-2x-20260714/`

## Recommended next engineering target

The first target is correctness of the software perf record ABI. Records must preserve the alignment/wrap invariant expected by libbpf, or the reader must reconstruct a wrapped `perf_event_header` before reading its size. A defensive zero/invalid-size check is also required to prevent an infinite consume loop. This should be followed by a producer/copied/consumed/lost conservation test across ring wrap boundaries.

The second correctness target is observability: propagate or count failed producer appends and expose submitted/copied/consumed/lost totals. Only after loss semantics are comparable should throughput be used as a production performance claim.

For the next fair backend comparison, restrict kernel sslsniff to the nginx worker PID so that both backends trace only the server. Then repeat the same-round baseline, nginx, reader, and throughput accounting. After scope parity is established, the first BPFtime performance target remains the per-event CPU-affinity sequence in `bpf_perf_event_output`; the diagnostic skip identified roughly 20-22% throughput impact but is not a safe production patch. `software_perf_event_data::get_current_thread_shard()` is the next measurable nginx-side candidate at roughly 1.3-1.4 microseconds per event.
