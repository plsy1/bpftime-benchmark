# Go end to end with the agent and syscount attached (`../e2e.sh`, 22:35-22:42 JST)

| Program | hook stack (20 runs) | a4ac0d31 (3 runs) |
|---|---|---|
| go/smoke-dyn (64 goroutines: getpid, pipe, stat) | 20 ok | 3 crash (fatal error: fault, SIGSEGV) |
| go/deep (a call at every depth, canaries) | 20 ok, 0 corrupted words | 3 crash |
| go/grow (stacks grown first) | 20 ok | 3 ok |
| go/httpd (HTTP server + 64 clients, GC on, 19,200 checked requests) | 20 ok | 3 crash (fatal error, out of memory, SIGSEGV) |

Counts (`syscount-count*.out`, `../countcheck/`): the example syscount used here
(`example/tracing/syscount`, built 10-29) increments `val->count = val->count + 1`, not atomically
(bcc's libbpf-tools uses `__sync_fetch_and_add`), so threads making the same call lose counts:
a C program of 6 threads x 200,000 getpid shows 544,008 (1 thread: 200,000 exactly), the Go
smoke 920k of 1.28M with either transformer (a4ac0d31 on go/grow: same). With GOMAXPROCS=1
(`countcheck/gomaxprocs1.out`) the counts are exact: getpid 1,280,110, write 1,280,001, read
1,280,017, newfstatat 1,280,000 for 1,280,000 each made: every system call of Go's runtime is
traced. bcc's own syscount (realprog-20261026-btf) printed no counts under bpftime: not
investigated.
