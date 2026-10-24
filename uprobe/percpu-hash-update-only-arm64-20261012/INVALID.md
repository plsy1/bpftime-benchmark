# Invalid comparison

Do not use this directory for performance conclusions.

This update-only diagnostic used the same mismatched candidate configuration
as the preceding combined attempt: probe read/write checks were on and the
compile-time log level was TRACE, while the baseline has checks off and INFO.
It is retained only to document how the mismatch was diagnosed.

The valid matched comparison is in
`../percpu-hash-copy-optimization-matched-arm64-20261012/`.
