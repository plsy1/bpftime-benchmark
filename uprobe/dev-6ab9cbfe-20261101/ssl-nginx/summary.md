
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 17544.9 | 17654.5 | 17599.7 | 0.8% | +44.5% | +0.0% | 0.0 |
| kernel | 12065.7 | 12209.8 | 12182.7 | 1.1% | +0.0% | -30.8% | 25.3 |
| official | 10773.7 | 10799.8 | 10775.8 | 0.6% | -11.5% | -38.8% | 36.0 |
| a7d54d7e | 14244.5 | 14261.0 | 14254.9 | 1.2% | +17.0% | -19.0% | 13.3 |
| dev-6ab9cbfe | 14008.2 | 14040.2 | 14018.9 | 1.3% | +15.1% | -20.3% | 14.5 |

dev-6ab9cbfe vs kernel, paired per rep: median +15.38%, 20/20 faster

dev-6ab9cbfe vs a7d54d7e, paired per rep: median -1.55%, 4/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 1755.2 | 1738.9 | 1743.8 | 0.7% | +38.5% | +0.0% | 0.0 |
| kernel | 1257.7 | 1260.4 | 1258.9 | 1.3% | +0.0% | -27.8% | 220.9 |
| official | 1138.3 | 1140.1 | 1139.3 | 2.9% | -9.5% | -34.7% | 304.3 |
| a7d54d7e | 1391.2 | 1399.6 | 1397.3 | 1.3% | +11.0% | -19.9% | 142.2 |
| dev-6ab9cbfe | 1381.6 | 1391.7 | 1387.9 | 2.4% | +10.3% | -20.4% | 147.0 |

dev-6ab9cbfe vs kernel, paired per rep: median +9.96%, 20/20 faster

dev-6ab9cbfe vs a7d54d7e, paired per rep: median -0.79%, 6/20 faster
