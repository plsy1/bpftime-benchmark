
### 16 payload (10 reps)

baseline median 17242.5 req/s = 58.0 us/request

| Stage | kernel pp | bpftime pp | bpftime - kernel pp | kernel us/req | bpftime us/req |
|---|---:|---:|---:|---:|---:|
| 1 probe trigger/dispatch | +15.43 +/- 1.67 | +8.85 +/- 0.76 | -6.57 +/- 1.97 | +10.62 +/- 1.33 | +5.64 +/- 0.52 |
| 2 helpers/maps/metadata | +5.42 +/- 1.70 | +7.84 +/- 3.31 | +2.42 +/- 4.22 | +4.67 +/- 1.44 | +6.16 +/- 3.00 |
| 3 bpf_probe_read_user | +0.99 +/- 1.22 | -0.67 +/- 3.87 | -1.66 +/- 4.43 | +0.92 +/- 1.15 | -0.70 +/- 3.51 |
| 4 perf output | +9.65 +/- 0.83 | +5.20 +/- 1.54 | -4.45 +/- 1.76 | +10.45 +/- 0.90 | +4.56 +/- 1.37 |
| **total (stage 4 vs baseline)** | +31.48 | +21.22 | -10.26 | +26.65 | +15.67 |

Median req/s: baseline 17242.5, kernel@1u 15636.6, kernel@1r 15264.7, kernel@1 14637.4, kernel@2 13705.0, kernel@3 13514.1, kernel@4 11839.4, 7ed339ad@1u 16092.1, 7ed339ad@1r 15878.2, 7ed339ad@1 15736.4, 7ed339ad@2 14571.3, 7ed339ad@3 14557.7, 7ed339ad@4 13665.5

### 256k payload (10 reps)

baseline median 1745.5 req/s = 572.9 us/request

| Stage | kernel pp | bpftime pp | bpftime - kernel pp | kernel us/req | bpftime us/req |
|---|---:|---:|---:|---:|---:|
| 1 probe trigger/dispatch | +19.31 +/- 2.46 | +7.67 +/- 0.85 | -11.64 +/- 2.50 | +137.99 +/- 21.17 | +47.53 +/- 5.37 |
| 2 helpers/maps/metadata | +1.29 +/- 4.07 | +6.43 +/- 1.58 | +5.14 +/- 3.99 | +11.30 +/- 36.33 | +46.62 +/- 12.18 |
| 3 bpf_probe_read_user | +3.08 +/- 3.37 | +3.07 +/- 3.49 | -0.01 +/- 2.57 | +29.91 +/- 33.11 | +26.78 +/- 32.45 |
| 4 perf output | +8.73 +/- 3.56 | +9.97 +/- 5.69 | +1.23 +/- 4.16 | +96.23 +/- 39.54 | +94.97 +/- 57.72 |
| **total (stage 4 vs baseline)** | +32.41 | +27.14 | -5.27 | +275.43 | +215.89 |

Median req/s: baseline 1745.5, kernel@1 1401.4, kernel@2 1395.7, kernel@3 1367.9, kernel@4 1203.1, 7ed339ad@1 1615.5, 7ed339ad@2 1502.5, 7ed339ad@3 1475.1, 7ed339ad@4 1286.8
