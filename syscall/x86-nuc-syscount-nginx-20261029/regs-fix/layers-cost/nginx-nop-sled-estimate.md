Estimate of the zpoline nop sled cost per nginx system call (x86, 1.7 GHz).

- write loop, 59e5e154 rewrite leg (perf): nop sled 23.4% of 562 cycles per write = 131 cycles for 511 nops = 0.257 cycles per nop
- nginx call mix (audit-e299cd3f.jsonl, transformer leg, numbers from asm/unistd_64.h): 4122297 calls, 457244 requests, 9.016 calls per request

| system call | nr | nops (512 - nr) | calls per request |
|---|---:|---:|---:|
| setsockopt | 54 | 458 | 2.000 |
| close | 3 | 509 | 1.001 |
| recvfrom | 45 | 467 | 1.000 |
| openat | 257 | 255 | 1.000 |
| fstat | 5 | 507 | 1.000 |
| writev | 20 | 492 | 1.000 |
| sendfile | 40 | 472 | 1.000 |
| write | 1 | 511 | 1.000 |
| epoll_wait | 232 | 280 | 0.011 |
| epoll_ctl | 233 | 279 | 0.001 |
| accept4 | 288 | 224 | 0.001 |

(calls below 0.001 per request omitted from the table, counted in the totals)

- mean nops per call: 459; estimated sled cost: **118 cycles per call**, 1063 cycles per request
- assumes the same cycles per nop as in the write loop (not measured in nginx)
