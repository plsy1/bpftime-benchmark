# More dimensions: what the earlier rounds did not measure (2026-11-06)

The dimensions left after the event output rounds (`../output-dims-20261106`): helpers' memory
reads, process start-up, the traced program's tail latency, many probes and many processes,
memory footprint, long runs. Kernel bpf against the frozen bpftime build `fix-final` (dev
d46ceb8f) unless a part says otherwise. One directory per part; `common.py` holds the shared
runner pieces (the loader is `output-dims-20261106/src/outattach`).

| part | directory | status |
|---|---|---|
| 1 probe reads: cost, bad pointers, containers | `n1-probe-read` | done |
