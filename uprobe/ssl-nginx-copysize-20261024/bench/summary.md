256 KB, 8 reps, 18 events/request; baseline median 1742.6 req/s

us per event over no tracing (mean +/- 95% CI)

| Part | cap 16 B | cap 1024 B | cap 4096 B | cap 8192 B | slope us/KB (95% CI) |
|---|---:|---:|---:|---:|---:|
| kernel stage 3 (read, no output) | 8.47 +/- 1.44 | 8.32 +/- 1.33 | 7.76 +/- 0.46 | 8.88 +/- 1.19 | +0.045 +/- 0.226 |
| kernel stage 4 (full) | 12.00 +/- 0.76 | 12.44 +/- 1.17 | 13.09 +/- 1.03 | 14.73 +/- 0.71 | +0.331 +/- 0.054 |
| kernel output only (4 - 3) | 3.53 +/- 1.54 | 4.12 +/- 0.59 | 5.33 +/- 0.92 | 5.85 +/- 1.53 | +0.286 +/- 0.239 |
| bca2ebce stage 3 (read, no output) | 5.96 +/- 1.96 | 5.56 +/- 1.27 | 6.19 +/- 1.64 | 6.41 +/- 1.44 | +0.083 +/- 0.248 |
| bca2ebce stage 4 (full) | 7.39 +/- 3.39 | 6.85 +/- 0.80 | 9.03 +/- 2.97 | 8.65 +/- 0.26 | +0.217 +/- 0.296 |
| bca2ebce output only (4 - 3) | 1.43 +/- 4.19 | 1.29 +/- 1.52 | 2.84 +/- 3.74 | 2.24 +/- 1.41 | +0.134 +/- 0.481 |

Median req/s: baseline 1742.6, kernel@s3c16 1404.0, kernel@s3c1024 1396.8, kernel@s3c4096 1401.8, kernel@s3c8192 1379.3, kernel@s4c16 1264.1, kernel@s4c1024 1271.9, kernel@s4c4096 1241.0, kernel@s4c8192 1200.8, bca2ebce@s3c16 1512.4, bca2ebce@s3c1024 1502.5, bca2ebce@s3c4096 1496.0, bca2ebce@s3c8192 1449.3, bca2ebce@s4c16 1458.1, bca2ebce@s4c1024 1443.2, bca2ebce@s4c4096 1387.8, bca2ebce@s4c8192 1377.5
