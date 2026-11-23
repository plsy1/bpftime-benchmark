# x86 E8: payload sweep (policy M, 5 reps per payload, every leg audited, counting loader)

Session E8-sweep-20261123-try1, 140 rows, all valid. GMR vs K, paired by rep. del/exp: events/(2 x requests) at <= 1 KB, written bytes delivered / nginx bytes_sent above.

| payload (B) | K req/s | O/K | D/K | O delivered | O events/req | K events/req | D delivered | D reported lost/req |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 58,125 | 1.043 | 1.532 | 0.9999 | 2.00 | 2.00 | 1.0000 | 0.0000 |
| 1,024 | 56,789 | 1.043 | 1.513 | 0.776 | 1.55 | 2.00 | 0.9999 | 0.0001 |
| 2,048 | 55,182 | 1.048 | 1.488 | 0.452 | 1.01 | 2.00 | 0.9995 | 0.0009 |
| 4,096 | 53,066 | 1.048 | 1.458 | 0.276 | 0.80 | 2.00 | 0.9988 | 0.0019 |
| 16,384 | 34,018 | 1.044 | 1.384 | 0.226 | 1.19 | 3.00 | 0.9997 | 0.0007 |
| 131,072 | 8,626 | 1.028 | 1.279 | 0.130 | 3.03 | 9.99 | 0.9993 | 0.0059 |
| 262,144 | 4,619 | 1.047 | 1.267 | 0.116 | 3.84 | 17.97 | 0.9993 | 0.0119 |

- Official starts losing events silently at 1 KB under full load (22% of events lost) and delivers 12-13% of the bytes at 128-256 KB, never reporting a loss. Its +3-5% throughput at every payload is measured while doing less output work above 16 B; only 16 B is an equal-work comparison.
- dev delivers >= 99.9% at every payload and reports what it drops (missing = reported, see E4).
- dev's lead shrinks with payload (+53% at 16 B to +27% at 256 KB) as per-request nginx/TLS work grows.
- Throughput here uses the counting loader at every payload; at 256 KB the stock-loader figures in E4 are the headline (G0(a)).
