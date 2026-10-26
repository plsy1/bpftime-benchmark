| Leg | 16b | 1kb | 2kb | 4kb | 16kb | 128kb | 256kb |
|---|---:|---:|---:|---:|---:|---:|---:|
| Baseline (no tracing) req/s | 96231 | 93362 | 90248 | 83432 | 53583 | 12822 | 6874 |
| Kernel sslsniff req/s | 53575 | 52710 | 51583 | 49188 | 32651 | 8529 | 4677 |
| bpftime official 177a1dc req/s | 62723 | 61067 | 59363 | 56747 | 36999 | 9615 | 5219 |
| bpftime a7d54d7e req/s | 80137 | 77443 | 74827 | 69820 | 44812 | 11025 | 5920 |
| Baseline (no tracing) vs kernel | +79.8% | +77.6% | +75.0% | +70.0% | +64.3% | +50.3% | +46.7% |
| bpftime official 177a1dc vs kernel | +16.4% | +15.8% | +15.3% | +15.7% | +13.6% | +13.2% | +11.6% |
| bpftime a7d54d7e vs kernel | +50.2% | +46.8% | +44.8% | +42.2% | +37.7% | +29.8% | +26.4% |

| Leg | 16b | 1kb | 2kb | 4kb | 16kb | 128kb | 256kb |
|---|---:|---:|---:|---:|---:|---:|---:|
| events/request | 2 | 2 | 2 | 2 | 3 | 10 | 18 |
| Kernel sslsniff loss vs no tracing | 44.4% | 43.7% | 42.8% | 41.2% | 39.1% | 33.5% | 31.8% |
| bpftime official 177a1dc loss vs no tracing | 34.7% | 34.4% | 34.3% | 31.9% | 30.8% | 24.8% | 24.0% |
| bpftime a7d54d7e loss vs no tracing | 16.7% | 16.8% | 16.8% | 16.3% | 16.4% | 14.0% | 14.1% |
| Kernel sslsniff us per event | 4.15 | 4.15 | 4.15 | 4.19 | 4.00 | 3.92 | 3.78 |
| bpftime official 177a1dc us per event | 2.77 | 2.81 | 2.91 | 2.82 | 2.77 | 2.58 | 2.56 |
| bpftime a7d54d7e us per event | 1.04 | 1.09 | 1.13 | 1.17 | 1.22 | 1.27 | 1.32 |
