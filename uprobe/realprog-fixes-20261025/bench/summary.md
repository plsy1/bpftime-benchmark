
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 17191.0 | 17191.8 | 17191.8 | 0.6% | +45.6% | +0.0% | 0.0 |
| kernel | 11769.0 | 11813.6 | 11807.6 | 1.9% | +0.0% | -31.3% | 26.5 |
| bca2ebce | 13849.6 | 14008.0 | 13926.9 | 2.4% | +17.9% | -19.0% | 13.6 |
| 2448aeaf | 14104.3 | 13919.0 | 14065.0 | 2.4% | +19.1% | -18.2% | 12.9 |

2448aeaf vs bca2ebce, paired per rep: median +0.90%, 13/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 1758.9 | 1754.4 | 1756.2 | 0.9% | +45.6% | +0.0% | 0.0 |
| kernel | 1214.9 | 1205.5 | 1206.1 | 2.1% | +0.0% | -31.3% | 259.7 |
| bca2ebce | 1355.2 | 1352.5 | 1353.5 | 2.9% | +12.2% | -22.9% | 169.5 |
| 2448aeaf | 1377.6 | 1338.3 | 1356.0 | 3.7% | +12.4% | -22.8% | 168.0 |

2448aeaf vs bca2ebce, paired per rep: median +0.57%, 12/20 faster
