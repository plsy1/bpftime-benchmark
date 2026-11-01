
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 17558.4 | 17511.8 | 17541.2 | 0.6% | +45.0% | +0.0% | 0.0 |
| kernel | 12055.2 | 12116.1 | 12096.9 | 1.0% | +0.0% | -31.0% | 25.7 |
| official | 10826.9 | 10818.5 | 10826.4 | 0.9% | -10.5% | -38.3% | 35.4 |
| dev-6ab9cbfe | 14163.1 | 14106.1 | 14106.1 | 1.0% | +16.6% | -19.6% | 13.9 |
| dev-68f19c1f | 14185.5 | 14226.5 | 14185.5 | 2.3% | +17.3% | -19.1% | 13.5 |

dev-68f19c1f vs kernel, paired per rep: median +17.30%, 20/20 faster

dev-68f19c1f vs dev-6ab9cbfe, paired per rep: median +0.88%, 14/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 1748.2 | 1752.4 | 1749.0 | 1.0% | +39.0% | +0.0% | 0.0 |
| kernel | 1254.3 | 1261.0 | 1258.1 | 0.8% | +0.0% | -28.1% | 223.1 |
| official | 1135.9 | 1123.2 | 1128.2 | 2.6% | -10.3% | -35.5% | 314.6 |
| dev-6ab9cbfe | 1382.8 | 1386.3 | 1386.3 | 1.7% | +10.2% | -20.7% | 149.6 |
| dev-68f19c1f | 1387.2 | 1391.0 | 1391.0 | 1.5% | +10.6% | -20.5% | 147.1 |

dev-68f19c1f vs kernel, paired per rep: median +10.27%, 20/20 faster

dev-68f19c1f vs dev-6ab9cbfe, paired per rep: median +0.48%, 13/20 faster
