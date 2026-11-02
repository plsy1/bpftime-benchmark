
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 17534.6 | 17544.2 | 17544.2 | 0.6% | +44.8% | +0.0% | 0.0 |
| kernel | 12198.8 | 12102.0 | 12114.0 | 0.9% | +0.0% | -31.0% | 25.6 |
| official | 10794.4 | 10781.6 | 10781.6 | 1.4% | -11.0% | -38.5% | 35.8 |
| dev-68f19c1f | 14152.8 | 14173.2 | 14152.8 | 1.7% | +16.8% | -19.3% | 13.7 |
| dev-a4ac0d31 | 14250.6 | 14189.5 | 14216.2 | 2.2% | +17.4% | -19.0% | 13.3 |

dev-a4ac0d31 vs kernel, paired per rep: median +17.32%, 20/20 faster

dev-a4ac0d31 vs dev-68f19c1f, paired per rep: median +0.12%, 12/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 1748.5 | 1752.9 | 1751.8 | 1.1% | +38.8% | +0.0% | 0.0 |
| kernel | 1263.0 | 1257.5 | 1261.9 | 1.5% | +0.0% | -28.0% | 221.6 |
| official | 1133.6 | 1139.2 | 1136.1 | 2.8% | -10.0% | -35.1% | 309.3 |
| dev-68f19c1f | 1387.1 | 1383.3 | 1387.1 | 3.8% | +9.9% | -20.8% | 150.1 |
| dev-a4ac0d31 | 1401.2 | 1399.4 | 1399.4 | 2.2% | +10.9% | -20.1% | 143.8 |

dev-a4ac0d31 vs kernel, paired per rep: median +11.53%, 20/20 faster

dev-a4ac0d31 vs dev-68f19c1f, paired per rep: median +1.58%, 14/20 faster
