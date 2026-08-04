# ARM64 matched helper/map lookup ladder

This experiment decomposes the ordinary `BPF_MAP_TYPE_HASH` top-level uprobe
gap on the Jetson Orin Nano. It uses the same victim ABI, CPU, loader setup,
iteration count, and runtime build for every case. The official benchmark and
the bpftime runtime implementation are not modified by the measurement.

Cases, each with 1000 loop/helper units per victim invocation:

1. `empty`: probe trigger and dispatch only.
2. `loop_control`: loop plus key/stack preparation, no helper.
3. `simple_helper`: `bpf_get_current_pid_tgid()`.
4. `array_lookup`: ordinary array lookup hit.
5. `hash_hit`: ordinary hash lookup hit.
6. `hash_miss`: ordinary hash lookup miss.

For each round, the incremental cost is calculated before cross-runtime
comparison:

```text
increment_X(mode) = (X - loop_control) / 1000
delta_X = increment_X(bpftime) - increment_X(kernel)
```

The raw values are paired by round. `summarize.py` writes the complete raw,
mean, median, sample standard deviation, minimum, maximum, PMU, and closure
tables.

The default runtime tree uses `fix_size_hash_map_impl`, backed by the
contiguous `bpftime_hash_map`; this is not the per-CPU Boost unordered-map
path.

Run after building the diagnostic target:

```bash
./run-wall.sh
./run-pmu.sh
./summarize.py
```

## Wall-time result (6 paired rounds)

All numbers below are incremental `ns/helper` values. `gap` means
`BPFtime - kernel` after subtracting the same round's `loop_control` value.

| component | kernel mean | BPFtime mean | gap mean | gap SD |
|---|---:|---:|---:|---:|
| loop minus empty | 1.199 | 0.803 | -0.396 | 0.008 |
| simple helper | 0.731 | 3.326 | +2.595 | 0.017 |
| array lookup | 1.372 | 12.157 | +10.785 | 0.365 |
| hash hit | 26.397 | 53.912 | +27.515 | 0.181 |
| hash miss | 25.198 | 182.947 | +157.749 | 0.643 |

The full hash-hit gap is `27.120 ns/helper`; the closure is exact:

```text
-0.396 + 27.515 = 27.120 ns/helper
```

This places the ordinary top-level gap in the hash-helper-containing path, not
in the loop/key preparation path.

## PMU result

Every PMU event reported `100%` running; there was no event multiplexing.
The BPFtime-minus-kernel instruction deltas are:

| component | cycles/helper | instructions/helper |
|---|---:|---:|
| simple helper | +4.417 | +24.986 |
| array lookup | +18.337 | +95.018 |
| hash hit | +48.438 | +240.449 |
| hash miss | +273.190 | +988.063 |

The instruction and cycle explosion for `hash_miss` is consistent with the
fixed-size open-addressing table being populated with 1000 entries out of a
1024-entry map (1031 adjusted prime buckets). An unsuccessful lookup can scan
long linear-probing runs. This is a separate stress case; the top-level
`hash_hit` result still shows a stable approximately `27.5 ns/helper` gap.

## Clean user-space profile

The profiles in `profiles/bpftime-clean-fp-*.txt` were collected with
`WARMUP=0`, so only one map setup occurred before the timed case. They are
symbol-level evidence, not replacement timing measurements:

- `simple_helper`: `bpftime_get_current_pid_tgid` accounts for 62.76% of
  sampled cycles.
- `array_lookup`: `bpf_map_handler::map_lookup_elem` accounts for 49.86%,
  `bpftime_shm::try_get_map_handler` 30.65%, and the helper wrapper 5.32%.
- `hash_hit`: `fix_size_hash_map_impl::elem_lookup` 46.85%, the AArch64
  spin-lock atomic 16.17%, `map_lookup_elem` 11.14%, `memcmp` 7.70%, and
  `try_get_map_handler` 5.45%.
- `hash_miss`: `fix_size_hash_map_impl::elem_lookup` 64.96% and `memcmp`
  16.12%, followed by the spin-lock atomic and map dispatch.

Thus the currently verified path is:

```text
helper wrapper -> shared-memory map lookup -> map handler/lock
  -> fix_size_hash_map_impl -> bpftime_hash_map hash/probing/memcmp
```

The profile does not by itself assign the entire 27.5 ns to one source line;
the wall/PMU decomposition and the profile should be read together.
