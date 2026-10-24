16 B, 10 reps; baseline median 16415.3 req/s = 60.9 us/request

| Part | kernel us/req | bpftime us/req | kernel pp | bpftime pp | bpftime - kernel us/req |
|---|---:|---:|---:|---:|---:|
| **stage 2 total** (2 - 1) | +7.32 +/- 0.91 | +9.92 +/- 1.30 | +7.78 +/- 0.93 | +11.50 +/- 1.53 | +2.60 +/- 1.04 |
| bpf_get_current_uid_gid | +1.13 +/- 1.53 | +4.96 +/- 1.56 | +1.13 +/- 1.50 | +5.39 +/- 1.72 | +3.83 +/- 1.88 |
| pid_tgid + ktime_get_ns + get_current_comm | +1.57 +/- 1.08 | +1.91 +/- 1.18 | +1.57 +/- 1.07 | +1.96 +/- 1.22 | +0.33 +/- 1.70 |
| hash map update/lookup/delete (bufs, start_ns) | +4.95 +/- 1.26 | +2.20 +/- 1.95 | +5.10 +/- 1.32 | +2.35 +/- 2.02 | -2.74 +/- 2.14 |
| per-CPU array lookup (ssl_data) | +1.11 +/- 1.21 | -1.22 +/- 3.87 | +1.08 +/- 1.19 | -1.00 +/- 3.44 | -2.32 +/- 4.36 |

remainder (kernel): -1.43 +/- 2.81 us/request

remainder (db3d475f): +2.07 +/- 5.87 us/request

Median req/s: baseline 16415.3, kernel@1 13867.3, kernel@2 12599.1, kernel@2-nouid 12799.0, kernel@2-nometa 12793.5, kernel@2-nohash 13386.8, kernel@2-nopcpu 12704.2, db3d475f@1 14776.8, db3d475f@2 12863.1, db3d475f@2-nouid 13882.7, db3d475f@2-nometa 13163.2, db3d475f@2-nohash 13454.0, db3d475f@2-nopcpu 12940.3
