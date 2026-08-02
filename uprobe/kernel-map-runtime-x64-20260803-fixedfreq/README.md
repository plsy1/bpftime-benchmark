# Fixed-frequency x64 kernel array-update investigation

## Runtime A/B closure

The existing unmodified `kernel_map_runtime` diagnostic was rerun with the
same parameters as before, but under the CPU controls used by the official
fixed-frequency x64 uprobe run: CPU5 pinned at 2.2 GHz, CPU11 offline,
performance governor, turbo disabled, and Intel pstate min/max at 100/100.

| Operation | old x64, turbo on | fixed 2.2 GHz x64 | ARM64 fixed | x64/ARM64 fixed |
|---|---:|---:|---:|---:|
| array lookup | 0.515 ns/helper | 0.929 ns/helper | 1.384 ns/helper | 0.671x |
| array update-existing | 18.791 ns/helper | 32.877 ns/helper | 10.952 ns/helper | 3.002x |
| hash lookup-hit | 25.920 ns/helper | 48.228 ns/helper | 26.769 ns/helper | 1.802x |
| hash update-existing | 75.992 ns/helper | 127.492 ns/helper | 90.823 ns/helper | 1.404x |

The fixed-frequency array-update mean is 32.877 ns/helper with a five-round
sample standard deviation of 0.004 ns and CV of 0.012%. It is only 1.255
ns/helper below the official fixed-frequency top-level value of 34.132
ns/helper. This closes the apparent 18.8-versus-34.1 ns discrepancy: the old
x64 kernel-runtime run used turbo and did not isolate the SMT sibling.

The cross-architecture anomaly remains real after controlling frequency.
Fixed x64 kernel array update is 3.002x the Jetson kernel-runtime value, while
the fixed top-level values are 34.132 ns/helper on x64 and 11.858 ns/helper on
ARM64.

## Program counters

`bpftool prog profile` attached directly to the array-update control and real
BPF programs. Each metric was collected in a separate full diagnostic run to
avoid hardware-event multiplexing. Every pair captured exactly 80000 control
and 80000 real program executions, and `enabled == running` for all events.

| Metric | control/program | real/program | real-control/helper |
|---|---:|---:|---:|
| cycles | 3590.307 | 72809.925 | 69.220 cycles |
| instructions | 9734.696 | 119892.088 | 110.157 instructions |
| L1D loads | 191.894 | 25233.328 | 25.041 loads |
| LLC misses | 0.000338 | 0.006388 | 0.000006 misses |

The cycle result is consistent with elapsed time: 32.877 ns/helper at 2.2 GHz
corresponds to 72.329 nominal core cycles/helper. LLC misses are effectively
zero, so the x64 cost is not explained by cold data or last-level cache
misses.

## Perf and machine code

The x64 JIT emits a direct call from the real BPF program to
`array_map_update_elem`; it does not execute a generic helper or map-fd
dispatcher on each iteration. The JIT loop prepares the 4-byte key and 8-byte
value, calls `array_map_update_elem`, increments the key, and repeats.

`perf record -e cycles:k` captured 102279 samples with zero losses. Across the
complete four-operation diagnostic, `array_map_update_elem` accounts for
6.14% of all kernel-cycle samples and the array-update JIT body for another
3.36%. Call-chain samples also attribute 1.50% globally to `memcpy_orig`
under `array_map_update_elem`.

Live-kernel disassembly shows the ordinary-array/BPF_ANY fast path:

1. validate flags and key bounds;
2. calculate `index * elem_size + value_base`;
3. call `bpf_obj_memcpy(record, element, value, value_size, false)`;
4. call `bpf_obj_free_fields(record, element)`;
5. return success.

For this plain `u64` value, the map record is empty. `bpf_obj_memcpy` therefore
takes its no-record path but still calls the kernel's generic `memcpy` for the
8-byte value; `bpf_obj_free_fields` performs its null/empty-record checks.
Instruction-pointer samples inside `array_map_update_elem` concentrate around
the return sites of these two calls. This makes the generic object-copy/free
wrapper plus out-of-line 8-byte copy the strongest current implementation-level
hypothesis for the x64 cost. It is not yet a cross-architecture proof.

## Next cross-architecture check

Run the same array-update control/real `bpftool prog profile` on Jetson and
save cycles, instructions, L1D loads, JIT dump, and kernel disassembly or perf
samples. The decisive questions are:

- Does ARM64 execute materially fewer than x64's 110.157 net instructions/helper?
- Does Jetson inline or specialize the 8-byte copy instead of calling generic memcpy?
- Does its kernel use the same `bpf_obj_memcpy` and `bpf_obj_free_fields` wrappers?
- If instruction counts are similar, which wrapper has the larger cycles/instruction gap?

No runtime, kernel, BPF program, loop count, warm-up, or map semantics were
modified in this experiment.

## Files

- `raw.csv`, `summary.csv`: fixed-frequency kernel runtime A/B and statistics.
- `comparison.csv`: old x64, fixed x64, ARM64, and top-level closure.
- `profile-summary.csv`: validated non-multiplexed program counters.
- `raw-profile-*/`: raw profile JSON, frequency evidence, JIT/xlated dumps, and A/B output.
- `raw-perf-record/perf.data`: complete kernel cycles recording.
- `raw-perf-record/perf-report-root.txt`: symbol and call-chain report.
- `raw-perf-record/array-map-update-ip-counts.txt`: instruction-address sample counts.
- `raw-perf-record/gdb-*-disassembly.txt`: live-kernel machine code from `/proc/kcore`.
- `environment.txt`: source, binaries, CPU controls, tools, and restoration checks.
- `run-fixedfreq.sh`: fixed-frequency runtime A/B orchestration.
- `profile-array-update-fixedfreq.sh`: single-metric BPF program profile orchestration.
- `perf-record-fixedfreq.sh`: fixed-frequency kernel cycles recording.
