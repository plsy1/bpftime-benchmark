official      177a1dc unpatched (consuming-official-vs-optimized-20261021/official-artifacts)
official-noaff 177a1dc + no-affinity.patch only (/home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023/build-jit)
opt-aff       72aebadd: patch stack incl. map optimizations, affinity pinning kept
opt-noaff     20870a03: 72aebadd + affinity pinning removed
sslsniff      built from /home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023/example/tracing/sslsniff (same source as July)
opt-noaff-fork a90fa447: 20870a03 + fork children execute inherited array JIT programs (built from candidate build-jit, LLVM 16)
