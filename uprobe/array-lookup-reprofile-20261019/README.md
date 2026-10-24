# Accepted array lookup reprofile

Completed: four valid production captures and six rounds of independent layer
diagnostics. Read [REPORT.md](REPORT.md) for conclusions, limitations, raw-data
links and the capture-script recovery notes. Production code is unchanged.

Production baseline: `fefe2f6edf276e1b2957711203b5022dd1be8d7c`.
No runtime/production source changes or new optimization are part of this task.
Prior address-dependency variants are rejected and baseline restore has
completed (17 cases, 106,562 assertions; restore exit 0).

## Actual production-path sampling

Use the immutable accepted libraries under
`/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors`. Confirm identical
current production library hashes and clean runtime source. The existing
helper-map-closure BPF object/loader/victim are unchanged and checksummed.
Each target invokes 1000 helpers per BPF invocation, with keys 0 through 999.

Capture ordinary and per-CPU array lookup twice each, alternate case order,
pin to CPU 5 at 1728000 kHz. Delay capture 5 seconds and sample a 5-second
window during a 2,000,000-invocation single-case run; `--kill=none --wait=primary`
allows the workload to finish normally. Use different cycle sampling periods
(1,728,000 and 1,572,863) to check sensitivity to periodic sampling. Save the
victim's actual mappings and PID for ASLR-aware instruction-PC analysis.

Sampling shares identify where execution is observed, not independent timing
components. ARM sampling skid, out-of-order execution and inlining prevent
treating an instruction's sample count as its isolated latency. Validate
target/main-thread samples and inspect non-helper shares before assuming the
capture window excludes all initialization. Cross-JIT unwinding may be partial.

## Independent layer diagnostics

Old harnesses use the generic helper and cannot alone represent today's
specialized production helper. Copies outside the source worktree will add
an explicit specialized `l4` entry and relabel the legacy `std_function`
synthetic case as `callable` (the current wrapper is a concrete template).
Standalone timings have different call boundaries, code layout and loop
overhead from actual JIT execution. They are supporting evidence and are not
an additive cost decomposition or substitutes for the full benchmark table.

All profile reports, exact libraries/hashes, process mappings, instruction
annotations and standalone diagnostic sources are retained locally. Large
binary/profile databases are excluded from git by this directory's ignore file.
