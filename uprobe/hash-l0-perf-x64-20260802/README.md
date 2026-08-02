# x64 direct hash L0 perf record and annotate

## Technical summary

This is the first investigation step after the cross-architecture map-path
diagnosis. It profiles direct hash L0 lookup-hit and update-existing on the same
x64 host and unchanged BPFtime binary. No runtime or diagnostic source was
modified, and the full uprobe benchmark was not run.

The evidence strongly supports prime-bucket modulo as a major cost, but rejects
the stronger claim that integer division alone explains the full x64/ARM64 L0
gap. The workload performs an average of 2.972 probes per successful operation,
so it also executes an average of 2.972 dynamic `% 1031` operations and 2.972
four-byte `memcmp` calls. On x64, both operations are prominent hotspots.

## Key findings

| Operation | Samples | Core map symbol | `memcmp` + PLT | `memmove` | Divider-active cycles/op | L0-control cycles/op |
|---|---:|---:|---:|---:|---:|---:|
| lookup hit | 40,303 | 52.06% | 45.10% | 0% | 28.703 | 134.147 |
| update existing | 46,910 | 57.11% | 35.66% | 3.16% | 27.697 | 133.716 |

`arith.divider_active` is 21.40% of lookup L0-control cycles and 20.71% of
update L0-control cycles. Against the previous direct cross-architecture
map-implementation gaps (+55.155 cycles lookup, +67.291 cycles update), the
entire measured x64 divider-active budget is only 52% and 41% respectively.
ARM64 also pays a nonzero divide cost, so modulo cannot by itself account for
the complete gap.

## Instruction annotation

### Lookup

Within `fix_size_hash_map_impl::elem_lookup`, samples land immediately after:

| Source operation | Instruction | Local symbol samples |
|---|---|---:|
| `hash_func(...) % _num_buckets` | `divq %rbx` | 22.76% |
| `(index + 1) % _num_buckets` | `divq -0x40(%rbp)` | 28.45% |

Together these post-`divq` locations hold 51.21% of the core symbol's samples,
or about 26.66% of all process samples after multiplying by the symbol's
52.06% flat share. Sampling skid means these percentages are hotspot evidence,
not exact instruction latency.

### Update

Within the LTO clone of `bpftime_hash_map::elem_update`:

| Source operation | Instruction | Local symbol samples |
|---|---|---:|
| `hash_func(...) % _num_buckets` | `divq %rcx` | 24.47% |
| `(index + 1) % _num_buckets` | `divq -0x48(%rbp)` | 30.67% |

These locations contain 55.14% of the core symbol's samples, approximately
31.49% of process samples after applying the 57.11% flat symbol share.

## Why the probing modulo is frequent

The harness primes keys `0..999` into 1031 buckets. Replaying the exact
four-byte little-endian hash and insertion order gives:

- Mean probes per hit: 2.972.
- Mean probe-step modulos per hit: 1.972.
- Mean total modulos and `memcmp` calls per hit: 2.972.
- 345 of 1000 keys need more than one probe; maximum probe count is 18.

The second `divq` is therefore a frequent steady-state operation, not merely a
cold collision path. The full distribution is in `probe-distribution.txt`.

## The second major hotspot: four-byte memcmp

Flat profiles place `__memcmp_avx2_movbe` at 43.12% of lookup cycles samples and
33.88% of update samples, plus 1.98% and 1.78% in the PLT entry. The annotated
glibc path receives a dynamic length of four but normally enters a short-length
AVX2 path that loads and compares 32 bytes before masking to the requested
length. This is repeated once per probe.

This does not establish that ARM64's `memcmp` is cheaper; Jetson must run the
same H0-H5 harness or equivalent profiling before making that claim. It does
show that any explanation restricted to `divq` is incomplete.

## Bucket and memory work

Inside the core symbols, samples also appear on the occupancy load/test after
bucket offset multiplication: 8.61% local for lookup and 7.38% for update.
Address multiplication itself is much smaller. Existing-entry update adds
3.16% process samples in `__memmove_avx_unaligned_erms` for the eight-byte value
copy.

## Interpretation

The current ranking for the x64 map-body cost is:

1. Repeated four-byte `memcmp` calls caused by the probe distribution.
2. Repeated variable-divisor modulo, with directly measured divider occupancy
   around 28 cycles/op.
3. Bucket occupancy/key loads and dependent probing.
4. Eight-byte value copy for update.

The first two are coupled: the weak hash distribution causes both additional
probe modulo operations and additional key comparisons. H0-H5 is still needed
to separate their cross-architecture deltas without changing production code.

## Methodology and limitations

- Binary and commit match `map-path-x64-20260802`.
- CPU 5 was pinned; SMT sibling CPU 11 was offline.
- Performance governor was active and turbo disabled, then both restored.
- Record event: user-space cycles, period 100003, frame-pointer call graph.
- Exact direct parameters: 1M warm-up and 3 x 10M formal operations.
- No samples were lost.
- `perf record` overhead raised wall time by about 5% for lookup and 16% for
  update. Sampling percentages are used only for localization; the stage-three
  non-recording measurements remain the latency baseline.
- `arith.divider_active` measures cycles in which the divider is active; it can
  overlap other execution and is not equivalent to end-to-end stall cycles.
- Hardware sampling skid attributes much of each divide cost to its following
  instruction.

## Artifacts

- `lookup.perf.data`, `update.perf.data`: raw profiles.
- `*.report-flat.txt`, `*.report-self.txt`, `*.report-children.txt`: symbol
  reports.
- `lookup.annotate.txt`, `update.annotate.txt`: core-symbol instruction views.
- `*.memcmp.annotate.txt`: libc `memcmp` instruction views.
- `*.stat.csv`: cycles, instructions, and divider-active counters.
- `hotspots.csv`: normalized headline metrics.
- `probe-distribution.txt`: exact deterministic probe distribution.
- `environment.txt`, `*.perf-header.txt`, `*.buildids.txt`: reproduction
  metadata.

## Next step

Implement a diagnostic-only H0-H5 harness with unchanged key population and
loop semantics, then run it on both x64 and Jetson. H2 must isolate initial
`hash % 1031`; H4 must include the real collision distribution so that probing
modulo and repeated `memcmp` remain observable separately.
