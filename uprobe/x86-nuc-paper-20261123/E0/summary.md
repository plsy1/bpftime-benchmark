# x86 E0 (functional gates and read-only checks)

- Probe sites: libssl 3.0.13 (container image) SSL_read and SSL_write start with endbr64; benchmark/test __bench_uprobe starts with movslq. (Jetson: paciasp / ldrb — G12 not fired.)
- Smoke (one rep per build, see README): every build traces; counts, field consistency and Odiag switch counters verified (Odiag 016a5151).
- dev 9ccda7e8 runtime unit tests (build-unittest, LTO off, one process per case, root): 280 cases; 277 pass, 3 skipped by design (map usage counters, compiled out with BPFTIME_ENABLE_MAP_USAGE_STATS=OFF), 0 failures. The first pass listed 13 failures that were harness errors: 10 case names contain commas, which Catch2 reads as OR; re-run with escaped commas, all pass.
- Activation audit with the official benchmark harness: not repeated on x86 (the Jetson ran it, 17/17 per arm); E3's 17 kernel + 17 userspace + embed cases all produced finite values for every arm.
