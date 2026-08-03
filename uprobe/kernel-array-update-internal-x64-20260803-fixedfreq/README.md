# x64 kernel array-update internal cycles sampling

## Conclusion

This experiment isolates the 8-byte ordinary kernel-BPF array
`update-existing` workload and increases kernel-cycle sampling density over the
earlier four-operation recording. Three independent fixed-frequency runs show
that the x64 fixed path spends substantial time in the non-inlined object-copy
chain:

| Observed symbol/segment | Mean share of net attributed samples | Estimated cycles/helper |
|---|---:|---:|
| `array_map_update_elem` symbol | 43.64% | 30.62 |
| `bpf_obj_memcpy` symbol | 19.10% | 13.40 |
| `memcpy_orig` / `__pi_memcpy` | 11.60% | 8.14 |
| `bpf_obj_free_fields` symbol | 4.16% | 2.92 |
| real-minus-control JIT symbol | 21.32% | 14.96 |
| other callees | 0.17% | 0.12 |

`bpf_obj_memcpy` plus its actual memcpy accounts for 30.70% of the net
attributed samples, or approximately 21.54 of the measured 70.168
cycles/helper. This confirms that the out-of-line copy chain is an important
fixed-cost component on this x64 kernel. The value-size sweep already showed
that payload-length growth is not the source of the original 8-byte gap.

It would be incorrect to assign exactly 13.40 cycles to the wrapper or to
claim that it explains the entire 54.647-cycle x64-minus-ARM64 gap. Precise-IP
level 1 cycle sampling still has skid. Of samples whose top symbol is
`array_map_update_elem`, 16.99% land immediately after the
`bpf_obj_memcpy` return and 34.71% land after the `bpf_obj_free_fields` return
or in the final epilogue. Some callee latency is therefore charged to the
caller return sites.

The evidence now supports this bounded conclusion:

- ARM64 inlines the `bpf_obj_memcpy` wrapper; x64 executes it out of line.
- That x64-only wrapper and copy call chain is a material part of the fixed
  short-value cost.
- The full cross-architecture cycle difference also includes caller/callee
  skid, different short-memcpy implementations, branch/dependency behavior,
  and different kernel compiler output. Sampling cannot separate those into
  exact per-function latency.

## Method

The workload is the exact 8B case from
`kernel-array-update-sizes-x64-20260803-fixedfreq`:

```text
20,000 invocations x 5 rounds + 1,000 warm-up
1,000 bpf_map_update_elem(..., BPF_ANY) calls per real invocation
odd rounds control-first; even rounds real-first
```

Each of three independent runs records CPU5 kernel cycles with:

```text
perf record -C 5 -e cycles:kp -c 100000 -g --call-graph fp
```

`cycles:kp` produced `precise_ip=1`. Samples are classified by top IP.
Unknown callees are accepted only when their call chain contains
`array_map_update_elem`. Top-IP samples in the four known target symbols are
accepted directly because epilogue samples can omit the intermediate caller
frame. This is safe for this isolated 8B-only workload and avoids the frame
loss observed at function return.

The JIT contribution is:

```text
samples in kaus_8_real JIT - samples in kaus_8_ctrl JIT
```

Estimated cycles/helper apply each mean sample share to the independently
measured, unprofiled value of 70.167633762 cycles/helper. They are attribution
estimates, not entry/exit timestamps.

## Repetition Stability

| Run | All samples | Helper-path samples | Real JIT | Control JIT | Net attributed |
|---:|---:|---:|---:|---:|---:|
| 1 | 99,360 | 58,493 | 18,261 | 4,488 | 72,266 |
| 2 | 98,609 | 58,935 | 19,822 | 2,754 | 76,003 |
| 3 | 97,855 | 58,780 | 19,879 | 2,864 | 75,795 |

The direct `bpf_obj_memcpy` share has only 0.13 percentage-point sample
standard deviation. The memcpy share varies more, with a 0.75-cycle estimated
standard deviation, but the combined copy chain remains present in every run.

Perf sampling raises observed wall time to 34.78-36.41 ns/helper, compared
with 31.974 ns/helper unprofiled. For that reason, sampled wall time is retained
as perturbation evidence and is not used as the absolute cost denominator.

## Validation

- All three runs use CPU5 with SMT sibling CPU11 offline.
- Governors are `performance`, turbo is disabled, and pstate min/max are
  100/100 during capture.
- All three turbostat files report CPU5 `Bzy_MHz=2200`.
- Perf captured 99,360, 98,609, and 97,855 samples; report stderr is empty and
  there is no lost-record warning.
- Each raw workload CSV has the expected five alternating rounds.
- Final CPU11 is online, turbo and governor settings are restored,
  `kernel.bpf_stats_enabled=0`, and BPF link count is zero.

## Files

- `raw-perf/rep*.perf.data`: three raw kernel-cycle recordings.
- `raw-perf/rep*-perf-report.txt`: symbol reports and perf metadata.
- `raw-perf/rep*-raw.csv`: sampled workload wall-time output.
- `raw-perf/rep*-turbostat.txt`: per-run fixed-frequency evidence.
- `internal-samples.csv`: per-run symbol counts and attribution estimates.
- `internal-summary.csv`: three-run mean and standard deviation.
- `array-map-update-ip.csv`: top-IP distribution within
  `array_map_update_elem`.
- `run-perf-x64.sh`, `summarize.py`: exact capture and analysis scripts.
- `environment.txt`: source, tools, CPU controls, hashes, and cleanup state.

The matching live-kernel disassembly is archived in
`../kernel-map-runtime-x64-20260803-fixedfreq/raw-perf-record/`.

## Next Discriminating Test

Further ordinary perf sampling will not remove call-return skid. The cleanest
next discriminator is a diagnostic kernel build from the same source and
compiler configuration with only the inlining decision controlled, followed
by the same untouched BPF harness. Comparing stock versus forced-inline x64
would directly measure the wrapper decision; matching ARM64/x64 kernel source
and compiler versions would then separate compiler output from ISA and
microarchitecture.
