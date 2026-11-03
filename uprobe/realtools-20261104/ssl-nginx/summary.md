
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 17514.5 | 17563.3 | 17552.3 | 0.7% | +45.4% | +0.0% | 0.0 |
| kernel | 12063.5 | 12073.3 | 12073.3 | 1.1% | +0.0% | -31.2% | 25.9 |
| dev-a4ac0d31 | 14309.5 | 14221.0 | 14269.8 | 2.3% | +18.2% | -18.7% | 13.1 |
| dev-7ad5a152 | 14242.9 | 14223.6 | 14223.8 | 4.0% | +17.8% | -19.0% | 13.3 |

7ad5a152 vs kernel, paired per rep: median +18.50%, 19/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 1743.3 | 1744.3 | 1743.5 | 0.9% | +38.9% | +0.0% | 0.0 |
| kernel | 1253.5 | 1257.9 | 1255.6 | 1.3% | +0.0% | -28.0% | 222.9 |
| dev-a4ac0d31 | 1373.9 | 1395.7 | 1394.6 | 3.8% | +11.1% | -20.0% | 143.5 |
| dev-7ad5a152 | 1386.5 | 1387.0 | 1387.0 | 2.9% | +10.5% | -20.4% | 147.4 |

7ad5a152 vs kernel, paired per rep: median +9.94%, 20/20 faster
