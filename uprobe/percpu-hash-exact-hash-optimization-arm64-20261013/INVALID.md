# Invalid/incomplete run

This directory is not a benchmark result and must not be used for conclusions.

The run stopped during round 1 because the equality-only baseline build did
not yet contain `tools/bpftimetool/bpftimetool`, which the paired runner uses
to remove BPFtime shared memory after a userspace sample. Exit status was 1.
Only the first kernel sample completed; no five-round A/B summary exists.

The missing target was subsequently built. The retry uses a new output
directory so this incomplete attempt remains auditable.
