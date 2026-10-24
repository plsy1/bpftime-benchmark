This attempt was interrupted deliberately before correctness tests: invoking
four CMake targets separately kept re-running the libbpf install and rebuilding
the same runtime headers. Its EXIT-only Bash trap wrote status 0 on SIGINT;
that is NOT a successful build/test result. No default-tests.log existed.
The incremental runner uses explicit signal exit codes and generated per-target
rules, then requires all correctness/host logs before any performance tests.
