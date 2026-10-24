| Leg | 16b | 1kb | 2kb | 4kb | 16kb | 128kb | 256kb |
|---|---:|---:|---:|---:|---:|---:|---:|
| Baseline (no tracing) req/s | 17164 | 16578 | 15780 | 14789 | 10073 | 3125 | 1733 |
| Kernel sslsniff req/s | 12057 | 11530 | 10966 | 10454 | 7288 | 2188 | 1229 |
| bpftime official 177a1dc req/s | 10362 | 10355 | 9924 | 9596 | 6579 | 2018 | 1104 |
| bpftime bca2ebce req/s | 14076 | 13352 | 12526 | 11991 | 8326 | 2452 | 1365 |
| Baseline (no tracing) vs kernel | +42.6% | +42.0% | +42.8% | +41.3% | +38.1% | +43.4% | +41.0% |
| bpftime official 177a1dc vs kernel | -14.1% | -11.3% | -9.5% | -8.5% | -9.7% | -8.1% | -8.1% |
| bpftime bca2ebce vs kernel | +16.7% | +15.9% | +13.8% | +14.9% | +14.1% | +11.3% | +12.2% |

| Leg | 16b | 1kb | 2kb | 4kb | 16kb | 128kb | 256kb |
|---|---:|---:|---:|---:|---:|---:|---:|
| events/request | 2 | 2 | 2 | 2 | 3 | 10 | 18 |
| Kernel sslsniff loss vs no tracing | 29.9% | 29.6% | 30.0% | 29.2% | 27.6% | 30.3% | 29.1% |
| bpftime official 177a1dc loss vs no tracing | 40.0% | 37.5% | 37.3% | 35.7% | 34.7% | 35.4% | 36.7% |
| bpftime bca2ebce loss vs no tracing | 17.9% | 19.0% | 20.5% | 19.6% | 17.4% | 22.7% | 21.0% |
| Kernel sslsniff us per event | 12.43 | 12.82 | 13.67 | 13.98 | 12.62 | 13.86 | 13.11 |
| bpftime official 177a1dc us per event | 19.29 | 18.12 | 18.78 | 18.57 | 17.54 | 17.55 | 18.54 |
| bpftime bca2ebce us per event | 6.41 | 7.11 | 8.17 | 8.17 | 6.95 | 9.25 | 8.53 |
