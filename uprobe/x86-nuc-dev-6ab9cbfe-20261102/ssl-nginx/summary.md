
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 95607.4 | 95709.5 | 95709.5 | 0.8% | +78.6% | +0.0% | 0.0 |
| kernel | 53790.3 | 53402.7 | 53574.1 | 1.2% | +0.0% | -44.0% | 8.2 |
| official | 62612.4 | 62355.3 | 62551.5 | 2.0% | +16.8% | -34.6% | 5.5 |
| a7d54d7e | 79700.8 | 79754.8 | 79754.8 | 0.6% | +48.9% | -16.7% | 2.1 |
| dev-6ab9cbfe | 79476.7 | 79618.5 | 79550.5 | 0.5% | +48.5% | -16.9% | 2.1 |

dev-6ab9cbfe vs kernel, paired per rep: median +47.83%, 20/20 faster

dev-6ab9cbfe vs a7d54d7e, paired per rep: median -0.33%, 5/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 6861.7 | 6857.9 | 6857.9 | 0.8% | +47.4% | +0.0% | 0.0 |
| kernel | 4642.3 | 4658.6 | 4654.1 | 0.5% | +0.0% | -32.1% | 69.0 |
| official | 5219.4 | 5227.2 | 5222.5 | 0.4% | +12.2% | -23.8% | 45.7 |
| a7d54d7e | 5919.4 | 5920.2 | 5919.4 | 0.5% | +27.2% | -13.7% | 23.1 |
| dev-6ab9cbfe | 5897.0 | 5904.9 | 5900.6 | 0.5% | +26.8% | -14.0% | 23.7 |

dev-6ab9cbfe vs kernel, paired per rep: median +26.57%, 20/20 faster

dev-6ab9cbfe vs a7d54d7e, paired per rep: median -0.69%, 7/20 faster
