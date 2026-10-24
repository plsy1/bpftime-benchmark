
### 16 payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 17161.8 | 17167.2 | 17167.2 | 0.7% | +45.2% | +0.0% | 0.0 |
| kernel | 11765.1 | 11905.9 | 11820.4 | 1.8% | +0.0% | -31.1% | 26.3 |
| 7ed339ad | 13569.8 | 13622.4 | 13569.8 | 1.3% | +14.8% | -21.0% | 15.4 |
| bca2ebce | 13812.5 | 13925.2 | 13878.3 | 1.8% | +17.4% | -19.2% | 13.8 |

bca2ebce vs 7ed339ad, paired per rep: median +2.01%, 15/20 faster

### 256k payload

| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | 1751.5 | 1741.3 | 1745.9 | 1.1% | +43.6% | +0.0% | 0.0 |
| kernel | 1219.1 | 1210.8 | 1215.6 | 1.6% | +0.0% | -30.4% | 249.9 |
| 7ed339ad | 1330.1 | 1323.7 | 1326.4 | 3.6% | +9.1% | -24.0% | 181.2 |
| bca2ebce | 1362.6 | 1355.5 | 1357.8 | 3.6% | +11.7% | -22.2% | 163.7 |

bca2ebce vs 7ed339ad, paired per rep: median +3.11%, 12/20 faster
