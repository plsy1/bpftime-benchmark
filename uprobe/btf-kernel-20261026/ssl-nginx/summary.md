
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 17580.3 | 17577.5 | 17580.3 | 0.5% | +44.5% | +0.0% | 0.0 |
| kernel | 12163.3 | 12160.6 | 12163.3 | 1.0% | +0.0% | -30.8% | 25.3 |
| official | 10865.7 | 10844.9 | 10844.9 | 1.5% | -10.8% | -38.3% | 35.3 |
| a7d54d7e | 14182.1 | 14328.8 | 14292.9 | 1.0% | +17.5% | -18.7% | 13.1 |

a7d54d7e vs kernel, paired per rep: median +17.63%, 20/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 1752.4 | 1745.5 | 1749.9 | 0.8% | +39.1% | +0.0% | 0.0 |
| kernel | 1262.7 | 1256.0 | 1258.1 | 2.0% | +0.0% | -28.1% | 223.4 |
| official | 1139.5 | 1139.6 | 1139.6 | 4.3% | -9.4% | -34.9% | 306.1 |
| a7d54d7e | 1392.6 | 1384.6 | 1389.4 | 1.2% | +10.4% | -20.6% | 148.3 |

a7d54d7e vs kernel, paired per rep: median +10.67%, 20/20 faster
