# x86 NUC uprobe sanity check, dev 42595149 (sanity-4.4ghz-turbo)

Lower is better. Each leg ran kernel, userspace and embedded groups in one benchmark.py --iter 2 session; ratios use the kernel group of the same leg. 6ab9cbfe is the frozen 11/02 build, measured again in this session. Array lookup rows do not consume the returned value. Two samples per value: a sanity check, not a performance result.

## Map operations (ns per operation)

| Operation | 42595149 ns/op | kernel (its round) | 42595149/kernel | 6ab9cbfe ns/op | kernel (its round) | 6ab9cbfe/kernel | 42595149 vs 6ab9cbfe | 6ab9cbfe 11/02 ten rounds |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 0.45 | 1.81 | 0.25x | 0.45 | 1.82 | 0.25x | +0.1% | 0.45 |
| Ordinary array update | 3.71 | 7.42 | 0.50x | 3.34 | 7.46 | 0.45x | +10.9% | 3.36 |
| Ordinary hash lookup | 5.28 | 10.29 | 0.51x | 5.40 | 10.60 | 0.51x | -2.2% | 5.28 |
| Ordinary hash update | 10.02 | 34.45 | 0.29x | 11.98 | 34.91 | 0.34x | -16.4% | 11.98 |
| Ordinary hash delete | 15.59 | 34.14 | 0.46x | 13.63 | 34.68 | 0.39x | +14.3% | 13.71 |
| Per-CPU array lookup | 1.28 | 1.81 | 0.70x | 1.28 | 1.82 | 0.70x | -0.0% | 1.28 |
| Per-CPU array update | 4.94 | 7.51 | 0.66x | 4.73 | 7.44 | 0.64x | +4.5% | 4.73 |
| Per-CPU hash lookup | 7.20 | 11.38 | 0.63x | 10.24 | 10.75 | 0.95x | -29.7% | 10.24 |
| Per-CPU hash update | 11.68 | 25.76 | 0.45x | 16.14 | 25.34 | 0.64x | -27.7% | 16.14 |
| Per-CPU hash delete | 25.17 | 112.01 | 0.22x | 31.69 | 111.55 | 0.28x | -20.6% | 31.87 |

## Probe and helper overhead (ns per call)

| Operation | 42595149 ns | kernel (its round) | 42595149/kernel | 6ab9cbfe ns | kernel (its round) | 6ab9cbfe/kernel | 42595149 vs 6ab9cbfe | 6ab9cbfe 11/02 ten rounds |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| uprobe | 125.98 | 1103.91 | 0.11x | 123.16 | 1105.81 | 0.11x | +2.3% | 122.81 |
| uretprobe | 186.34 | 1288.94 | 0.14x | 184.86 | 1298.17 | 0.14x | +0.8% | 183.80 |
| uprobe + uretprobe | 217.76 | 1335.82 | 0.16x | 212.98 | 1346.53 | 0.16x | +2.2% | 212.96 |
| probe read | 1507.34 | 19335.10 | 0.08x | 1506.66 | 18617.20 | 0.08x | +0.0% | 1512.48 |
| probe write | 1280.61 | 19095.67 | 0.07x | 1279.73 | 20969.19 | 0.06x | +0.1% | 1280.62 |

Embedded (dev-42595149): embed avg 62.83

Embedded (dev-6ab9cbfe): embed avg 63.19

Embedded (6ab9cbfe, 11/02 ten rounds): embed avg 63.82
