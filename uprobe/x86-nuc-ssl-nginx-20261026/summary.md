
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 96003.1 | 95574.6 | 95882.9 | 1.3% | +78.1% | +0.0% | 0.0 |
| kernel | 54012.6 | 53814.2 | 53827.1 | 3.7% | +0.0% | -43.9% | 8.1 |
| official | 62657.5 | 62501.4 | 62557.6 | 2.5% | +16.2% | -34.8% | 5.6 |
| a7d54d7e | 79641.7 | 79641.8 | 79641.7 | 3.7% | +48.0% | -16.9% | 2.1 |

a7d54d7e vs kernel, paired per rep: median +47.94%, 20/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 6827.1 | 6849.7 | 6845.0 | 0.8% | +48.4% | +0.0% | 0.0 |
| kernel | 4596.7 | 4620.2 | 4613.1 | 0.6% | +0.0% | -32.6% | 70.7 |
| official | 5189.1 | 5217.1 | 5212.0 | 0.6% | +13.0% | -23.9% | 45.8 |
| a7d54d7e | 5895.1 | 5910.0 | 5901.0 | 0.4% | +27.9% | -13.8% | 23.4 |

a7d54d7e vs kernel, paired per rep: median +28.00%, 20/20 faster
