
### 16 payload (10 reps)

baseline median 16343.6 req/s = 61.2 us/request

| Stage | kernel pp | bpftime pp | bpftime - kernel pp | kernel us/req | bpftime us/req |
|---|---:|---:|---:|---:|---:|
| 1 probe trigger/dispatch | +15.33 +/- 1.01 | +9.25 +/- 1.14 | -6.08 +/- 1.59 | +11.07 +/- 0.81 | +6.24 +/- 0.84 |
| 2 helpers/maps/metadata | +6.26 +/- 1.02 | +12.85 +/- 1.49 | +6.58 +/- 2.25 | +5.76 +/- 0.94 | +11.12 +/- 1.38 |
| 3 bpf_probe_read_user | +1.45 +/- 1.24 | +1.53 +/- 3.12 | +0.08 +/- 3.35 | +1.49 +/- 1.27 | +1.71 +/- 3.55 |
| 4 perf output | +7.61 +/- 1.17 | +3.45 +/- 2.71 | -4.16 +/- 2.62 | +8.71 +/- 1.35 | +3.64 +/- 3.15 |
| **total (stage 4 vs baseline)** | +30.65 | +27.08 | -3.58 | +27.04 | +22.72 |

Median req/s: baseline 16343.6, kernel@1 13878.6, kernel@2 12853.1, kernel@3 12549.4, kernel@4 11436.9, db3d475f@1 14832.2, db3d475f@2 12834.0, db3d475f@3 12764.2, db3d475f@4 11941.5

### 256k payload (10 reps)

baseline median 1706.1 req/s = 586.1 us/request

| Stage | kernel pp | bpftime pp | bpftime - kernel pp | kernel us/req | bpftime us/req |
|---|---:|---:|---:|---:|---:|
| 1 probe trigger/dispatch | +15.18 +/- 1.97 | +6.42 +/- 1.66 | -8.76 +/- 2.31 | +105.73 +/- 15.74 | +40.58 +/- 10.98 |
| 2 helpers/maps/metadata | +5.74 +/- 1.96 | +10.41 +/- 2.86 | +4.67 +/- 3.00 | +51.12 +/- 18.57 | +78.96 +/- 22.94 |
| 3 bpf_probe_read_user | -0.13 +/- 3.78 | +2.06 +/- 2.31 | +2.20 +/- 4.95 | -1.96 +/- 35.97 | +17.68 +/- 21.03 |
| 4 perf output | +11.09 +/- 2.05 | +7.27 +/- 1.08 | -3.82 +/- 1.84 | +120.58 +/- 22.11 | +71.40 +/- 10.59 |
| **total (stage 4 vs baseline)** | +31.87 | +26.17 | -5.71 | +275.47 | +208.63 |

Median req/s: baseline 1706.1, kernel@1 1446.0, kernel@2 1347.4, kernel@3 1365.3, kernel@4 1163.5, db3d475f@1 1588.7, db3d475f@2 1426.6, db3d475f@3 1384.3, db3d475f@4 1256.7
