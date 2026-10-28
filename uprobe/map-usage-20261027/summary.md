| Tool | Map | Type | Key | Value | Max | Prog ops/call | Lookups (miss) | Inserts | Updates | Deletes (miss) | Threads | bpf() ops |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| bashreadline-example | events | perf_event_array | 4 | 4 | 6 | 1.00 | 201 (0%) | 0 | 0 | 0 | 1 | 6 |
| bashreadline | events | perf_event_array | 4 | 4 | 6 | 1.00 | 201 (0%) | 0 | 0 | 0 | 1 | 12 |
| bpftrace-bashreadline | ringbuf_loss_co | array | 4 | 8 | 1 | - | 0 | 0 | 0 | 0 | - | 256 |
| bpftrace-gethostlatency | AT_name | hash | 8 | 8 | 4096 | 3.00 | 252000 (0%) | 252000 | 0 | 252000 (0%) | 1 | 1 |
| bpftrace-gethostlatency | AT_start | hash | 8 | 8 | 4096 | 4.00 | 504000 (0%) | 252000 | 0 | 252000 (0%) | 1 | 1 |
| bpftrace-gethostlatency | ringbuf_loss_co | array | 4 | 8 | 1 | - | 0 | 0 | 0 | 0 | - | 3598 |
| bpftrace-uprobe-count | AT_c | percpu_array | 4 | 8 | 1 | 1.00 | 5436000 (0%) | 0 | 0 | 0 | 1 | 3 |
| bpftrace-uprobe-count | ringbuf_loss_co | array | 4 | 8 | 1 | - | 0 | 0 | 0 | 0 | - | 88 |
| bpftrace-uretprobe-hist | AT_lat | percpu_hash | 8 | 8 | 4096 | 1.00 | 1181000 (0%) | 6 | 0 | 0 | 1 | 7 |
| bpftrace-uretprobe-hist | AT_s | hash | 8 | 8 | 4096 | 4.00 | 2362000 (0%) | 1181000 | 0 | 1181000 (0%) | 1 | 1 |
| bpftrace-uretprobe-hist | ringbuf_loss_co | array | 4 | 8 | 1 | - | 0 | 0 | 0 | 0 | - | 90 |
| bpftrace-usdt-jvm-gc | AT_c | percpu_array | 4 | 8 | 1 | 1.00 | 50 (0%) | 0 | 0 | 0 | 1 | 3 |
| bpftrace-usdt-jvm-gc | ringbuf_loss_co | array | 4 | 8 | 1 | - | 0 | 0 | 0 | 0 | - | 53 |
| bpftrace-ustack | AT_ | percpu_hash | 16 | 8 | 4096 | 1.00 | 3072000 (0%) | 1 | 0 | 0 | 1 | 2 |
| bpftrace-ustack | stack_bpftrace_ | stack_trace | 4 | 32 | 131072 | - | 0 | 0 | 0 | 0 | - | 1 |
| bpftrace-ustack | ringbuf_loss_co | array | 4 | 8 | 1 | - | 0 | 0 | 0 | 0 | - | 90 |
| funclatency-bpftime-example | starts | hash | 4 | 8 | 102400 | 2.00 | 2468000 (0%) | 1 | 2467999 | 0 | 1 | 0 |
| funclatency-bpftime-example | funclate.bss | array | 4 | 100 | 1 | 0.00 | 1 (0%) | 0 | 0 | 0 | 1 | 1 |
| funclatency-bpftime-example | funclate.rodata | array | 4 | 8 | 1 | 0.00 | 2 (0%) | 0 | 0 | 0 | 1 | 1 |
| funclatency-libbpf | starts | hash | 4 | 8 | 102400 | 3.00 | 1845000 (0%) | 1845000 | 0 | 1845000 (0%) | 1 | 0 |
| funclatency-libbpf | funclate.bss | array | 4 | 100 | 1 | 0.00 | 1 (0%) | 0 | 0 | 0 | 1 | 1 |
| funclatency-libbpf | funclate.rodata | array | 4 | 9 | 1 | 0.00 | 4 (0%) | 0 | 0 | 0 | 1 | 1 |
| get_stack_id-example | stack_traces | stack_trace | 4 | 160 | 1000 | - | 0 | 0 | 0 | 0 | - | 189 |
| gethostlatency | events | perf_event_array | 4 | 4 | 6 | 1.00 | 279800 (0%) | 0 | 0 | 0 | 1 | 12 |
| gethostlatency | starts | hash | 4 | 112 | 10240 | 3.00 | 279800 (0%) | 279800 | 0 | 279800 (0%) | 1 | 0 |
| gethostlatency | gethostl.rodata | array | 4 | 4 | 1 | 0.00 | 1 (0%) | 0 | 0 | 0 | 1 | 1 |
| malloc-example | libc_malloc_cal | hash | 4 | 8 | 1024 | 2.00 | 1402031 (0%) | 5 | 1402026 | 0 | 1 | 10 |
| malloc-example | .rodata.str1.1 | array | 4 | 27 | 1 | - | 0 | 0 | 0 | 0 | - | 1 |
| memleak | sizes | hash | 4 | 8 | 10240 | ? (2130081 ops) | 710027 (0%) | 710027 | 0 | 710027 (0%) | 1 | 0 |
| memleak | allocs | hash | 8 | 24 | 1000000 | ? (2128031 ops) | 709003 (0%) | 710027 | 0 | 709001 (0%) | 1 | 7451 |
| memleak | memleak_.bss | array | 4 | 8 | 1 | ? (10 ops) | 10 (0%) | 0 | 0 | 0 | 1 | 1 |
| memleak | memleak_.rodata | array | 4 | 192 | 1 | ? (106 ops) | 106 (0%) | 0 | 0 | 0 | 1 | 1 |
| memleak | stack_traces | stack_trace | 4 | 1016 | 10240 | - | 0 | 0 | 0 | 0 | - | 11 |
| sslsniff-example | perf_SSL_events | perf_event_array | 4 | 4 | 6 | 1.00 | 71000 (0%) | 0 | 0 | 0 | 1 | 12 |
| sslsniff-example | ssl_data | percpu_array | 4 | 8256 | 1 | 1.00 | 71000 (0%) | 0 | 0 | 0 | 1 | 0 |
| sslsniff-example | start_ns | hash | 4 | 8 | 10240 | 3.00 | 71000 (0%) | 71000 | 0 | 71000 (0%) | 1 | 0 |
| sslsniff-example | bufs | hash | 4 | 8 | 10240 | 3.00 | 71000 (0%) | 71000 | 0 | 71000 (0%) | 1 | 0 |
| usdt_minimal | usdt_mi.kconfig | array | 4 | 1 | 1 | 0.12 | 1 (0%) | 0 | 0 | 0 | 1 | 1 |
| usdt_minimal | usdt_min.rodata | array | 4 | 19 | 1 | 0.12 | 1 (0%) | 0 | 0 | 0 | 1 | 1 |
| usdt_minimal | __bpf_usdt_spec | array | 4 | 304 | 256 | - | 0 | 0 | 0 | 0 | - | 2 |
