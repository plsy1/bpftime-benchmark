
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 17123.4 | 17203.4 | 17157.4 | 0.6% | +45.3% | +0.0% | 0.0 |
| kernel | 11872.8 | 11776.2 | 11808.0 | 2.0% | +0.0% | -31.2% | 26.4 |
| db3d475f | 12665.3 | 12906.6 | 12750.6 | 2.0% | +8.0% | -25.7% | 20.1 |
| 7ed339ad | 13738.3 | 13763.9 | 13751.0 | 1.5% | +16.5% | -19.9% | 14.4 |

7ed339ad vs db3d475f, paired per rep: median +7.80%, 20/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 1748.8 | 1752.1 | 1748.8 | 1.0% | +48.9% | +0.0% | 0.0 |
| kernel | 1167.9 | 1179.3 | 1174.4 | 2.6% | +0.0% | -32.8% | 279.7 |
| db3d475f | 1254.4 | 1266.5 | 1254.8 | 4.4% | +6.8% | -28.2% | 225.1 |
| 7ed339ad | 1337.5 | 1312.8 | 1326.4 | 6.6% | +12.9% | -24.2% | 182.1 |

7ed339ad vs db3d475f, paired per rep: median +6.02%, 13/20 faster
