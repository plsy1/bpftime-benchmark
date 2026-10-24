# Invalid comparison

Do not use this directory for performance conclusions.

The candidate was accidentally configured with
`ENABLE_PROBE_READ_CHECK=ON`, `ENABLE_PROBE_WRITE_CHECK=ON`, and compile-time
`SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE`. The established baseline uses both
checks off and `SPDLOG_LEVEL_INFO`. This mismatch changed unrelated control
paths and invalidated the A/B comparison.

The data are retained only as an audit trail. The valid matched comparison is
in `../percpu-hash-copy-optimization-matched-arm64-20261012/`.
