# Array update xlarge: experiment tables (2026-11-10/11)

Per-run mean of the 1000 timed launches; per set the median over runs [range], the median of the per-run fastest launch, and Mann-Whitney p against D and O (scripts/summarize_build.py, scripts/summarize_place.py). Sets: README.md.

## Experiment 1: segment size (6 rounds)

O official (50 MB), P official with BPFTIME_SHM_MEMORY_MB=128, D dev (128 MB), E dev with 50 MB.

```
Baseline (large)
  O:     31.7 [31.1-32.0] fastest launch    24.0 n6  p(vs D) 0.699  p(vs O) 1.000
  P:     30.9 [30.3-33.2] fastest launch    20.3 n6  p(vs D) 0.310  p(vs O) 0.132
  D:     31.2 [31.0-32.0] fastest launch    25.7 n6  p(vs D) 1.000  p(vs O) 0.699
  E:     31.3 [30.4-32.3] fastest launch    22.1 n6  p(vs D) 1.000  p(vs O) 0.575
Baseline (xlarge)
  O:    160.4 [159.7-162.1] fastest launch   157.0 n6  p(vs D) 0.937  p(vs O) 1.000
  P:    159.6 [159.1-160.0] fastest launch   155.9 n6  p(vs D) 0.149  p(vs O) 0.054
  D:    160.1 [159.4-163.0] fastest launch   156.4 n6  p(vs D) 1.000  p(vs O) 0.937
  E:    160.2 [160.1-162.5] fastest launch   156.2 n6  p(vs D) 0.699  p(vs O) 0.818
GPU Array map lookup (xlarge)
  O:    458.6 [453.3-469.0] fastest launch   447.4 n6  p(vs D) 0.015  p(vs O) 1.000
  P:    457.4 [455.2-459.8] fastest launch   445.2 n6  p(vs D) 0.041  p(vs O) 0.818
  D:    446.2 [443.6-459.7] fastest launch   434.5 n6  p(vs D) 1.000  p(vs O) 0.015
  E:    449.4 [443.7-454.0] fastest launch   438.7 n6  p(vs D) 0.310  p(vs O) 0.004
GPU Array map update (large)
  O:    219.5 [214.9-226.8] fastest launch   158.4 n6  p(vs D) 0.002  p(vs O) 1.000
  P:    220.4 [215.7-233.1] fastest launch   162.1 n6  p(vs D) 0.004  p(vs O) 1.000
  D:    247.5 [231.0-251.5] fastest launch   160.0 n6  p(vs D) 1.000  p(vs O) 0.002
  E:    241.9 [236.9-252.3] fastest launch   160.6 n6  p(vs D) 0.589  p(vs O) 0.002
GPU Array map update (xlarge)
  O:   1682.7 [1634.7-1711.8] fastest launch  1348.6 n6  p(vs D) 0.002  p(vs O) 1.000
  P:   1699.6 [1656.9-1769.0] fastest launch  1385.9 n6  p(vs D) 0.002  p(vs O) 0.180
  D:   1519.4 [1465.5-1540.1] fastest launch   989.8 n6  p(vs D) 1.000  p(vs O) 0.002
  E:   1480.9 [1445.3-1512.6] fastest launch   995.1 n6  p(vs D) 0.093  p(vs O) 0.002
```

## Experiment 2: counter position (2 reps x 12 thread counts)

BPFTIME_MAP_GPU_THREAD_COUNT 1024-1035 moves the counter by one ringbuf entry per thread; off is its offset in the segment, page_off inside its 4 KB page.

```
## large
O t1024 r1 off=0x36af80 page_off=0xf80 mean=222.8 min=144.7
O t1024 r2 off=0x36af80 page_off=0xf80 mean=223.8 min=160.2
O t1025 r1 off=0x36b110 page_off=0x110 mean=219.1 min=163.2
O t1025 r2 off=0x36b110 page_off=0x110 mean=227.7 min=167.7
O t1026 r1 off=0x36b2b0 page_off=0x2b0 mean=228.1 min=169.0
O t1026 r2 off=0x36b2b0 page_off=0x2b0 mean=220.2 min=156.7
O t1027 r1 off=0x36b440 page_off=0x440 mean=219.5 min=162.0
O t1027 r2 off=0x36b440 page_off=0x440 mean=222.4 min=164.4
O t1028 r1 off=0x36b5e0 page_off=0x5e0 mean=222.5 min=153.0
O t1028 r2 off=0x36b5e0 page_off=0x5e0 mean=225.9 min=172.3
O t1029 r1 off=0x36b770 page_off=0x770 mean=223.1 min=160.5
O t1029 r2 off=0x36b770 page_off=0x770 mean=215.2 min=152.2
O t1030 r1 off=0x36b910 page_off=0x910 mean=219.3 min=158.6
O t1030 r2 off=0x36b910 page_off=0x910 mean=220.7 min=147.9
O t1031 r1 off=0x36baa0 page_off=0xaa0 mean=223.2 min=158.5
O t1031 r2 off=0x36baa0 page_off=0xaa0 mean=217.8 min=156.9
O t1032 r1 off=0x36bc40 page_off=0xc40 mean=224.9 min=169.9
O t1032 r2 off=0x36bc40 page_off=0xc40 mean=218.8 min=149.5
O t1033 r1 off=0x36bdd0 page_off=0xdd0 mean=221.3 min=161.3
O t1033 r2 off=0x36bdd0 page_off=0xdd0 mean=216.5 min=152.8
O t1034 r1 off=0x36bf70 page_off=0xf70 mean=218.4 min=164.4
O t1034 r2 off=0x36bf70 page_off=0xf70 mean=224.3 min=166.4
O t1035 r1 off=0x36c100 page_off=0x100 mean=226.6 min=160.3
O t1035 r2 off=0x36c100 page_off=0x100 mean=227.0 min=163.5
O median 222.465 range 215.16 228.13 n 24
D t1024 r1 off=0x36f800 page_off=0x800 mean=246.6 min=164.3
D t1024 r2 off=0x36f800 page_off=0x800 mean=242.5 min=158.9
D t1025 r1 off=0x36f990 page_off=0x990 mean=238.3 min=159.1
D t1025 r2 off=0x36f990 page_off=0x990 mean=252.7 min=162.0
D t1026 r1 off=0x36fb40 page_off=0xb40 mean=248.8 min=170.8
D t1026 r2 off=0x36fb40 page_off=0xb40 mean=249.3 min=166.2
D t1027 r1 off=0x36fcd0 page_off=0xcd0 mean=242.4 min=163.5
D t1027 r2 off=0x36fcd0 page_off=0xcd0 mean=241.6 min=165.6
D t1028 r1 off=0x36fe80 page_off=0xe80 mean=242.7 min=174.9
D t1028 r2 off=0x36fe80 page_off=0xe80 mean=245.8 min=163.0
D t1029 r1 off=0x370010 page_off=0x010 mean=238.5 min=164.7
D t1029 r2 off=0x370010 page_off=0x010 mean=243.7 min=158.5
D t1030 r1 off=0x3701c0 page_off=0x1c0 mean=249.4 min=174.3
D t1030 r2 off=0x3701c0 page_off=0x1c0 mean=244.7 min=166.7
D t1031 r1 off=0x370350 page_off=0x350 mean=246.8 min=162.5
D t1031 r2 off=0x370350 page_off=0x350 mean=247.4 min=174.9
D t1032 r1 off=0x370500 page_off=0x500 mean=245.7 min=175.6
D t1032 r2 off=0x370500 page_off=0x500 mean=236.0 min=151.3
D t1033 r1 off=0x370690 page_off=0x690 mean=239.4 min=165.5
D t1033 r2 off=0x370690 page_off=0x690 mean=246.9 min=151.3
D t1034 r1 off=0x370840 page_off=0x840 mean=244.3 min=145.1
D t1034 r2 off=0x370840 page_off=0x840 mean=257.6 min=169.0
D t1035 r1 off=0x3709d0 page_off=0x9d0 mean=258.3 min=174.9
D t1035 r2 off=0x3709d0 page_off=0x9d0 mean=239.0 min=153.1
D median 245.195 range 236.03 258.3 n 24
## xlarge
O t1024 r1 off=0x36af80 page_off=0xf80 mean=1627.0 min=1349.5
O t1024 r2 off=0x36af80 page_off=0xf80 mean=1635.3 min=1324.7
O t1025 r1 off=0x36b110 page_off=0x110 mean=1647.4 min=1370.2
O t1025 r2 off=0x36b110 page_off=0x110 mean=1639.6 min=1370.7
O t1026 r1 off=0x36b2b0 page_off=0x2b0 mean=1701.2 min=1441.3
O t1026 r2 off=0x36b2b0 page_off=0x2b0 mean=1686.5 min=1409.2
O t1027 r1 off=0x36b440 page_off=0x440 mean=1721.0 min=1443.0
O t1027 r2 off=0x36b440 page_off=0x440 mean=1738.5 min=1411.9
O t1028 r1 off=0x36b5e0 page_off=0x5e0 mean=1780.9 min=1403.5
O t1028 r2 off=0x36b5e0 page_off=0x5e0 mean=1700.3 min=1355.8
O t1029 r1 off=0x36b770 page_off=0x770 mean=1802.7 min=1433.9
O t1029 r2 off=0x36b770 page_off=0x770 mean=1635.1 min=1342.8
O t1030 r1 off=0x36b910 page_off=0x910 mean=1631.9 min=1327.0
O t1030 r2 off=0x36b910 page_off=0x910 mean=1651.7 min=1366.1
O t1031 r1 off=0x36baa0 page_off=0xaa0 mean=1620.8 min=1312.5
O t1031 r2 off=0x36baa0 page_off=0xaa0 mean=1670.2 min=1286.3
O t1032 r1 off=0x36bc40 page_off=0xc40 mean=1696.0 min=1380.4
O t1032 r2 off=0x36bc40 page_off=0xc40 mean=1679.7 min=1345.7
O t1033 r1 off=0x36bdd0 page_off=0xdd0 mean=1660.5 min=1376.9
O t1033 r2 off=0x36bdd0 page_off=0xdd0 mean=1729.5 min=1410.4
O t1034 r1 off=0x36bf70 page_off=0xf70 mean=1655.1 min=1300.5
O t1034 r2 off=0x36bf70 page_off=0xf70 mean=1710.7 min=1391.5
O t1035 r1 off=0x36c100 page_off=0x100 mean=1671.8 min=1366.8
O t1035 r2 off=0x36c100 page_off=0x100 mean=1696.8 min=1348.6
O median 1675.7350000000001 range 1620.84 1802.67 n 24
D t1024 r1 off=0x36f800 page_off=0x800 mean=1472.4 min=999.5
D t1024 r2 off=0x36f800 page_off=0x800 mean=1506.8 min=954.8
D t1025 r1 off=0x36f990 page_off=0x990 mean=1518.5 min=1012.3
D t1025 r2 off=0x36f990 page_off=0x990 mean=1338.2 min=977.4
D t1026 r1 off=0x36fb40 page_off=0xb40 mean=1329.4 min=974.6
D t1026 r2 off=0x36fb40 page_off=0xb40 mean=1471.3 min=942.1
D t1027 r1 off=0x36fcd0 page_off=0xcd0 mean=1463.3 min=937.8
D t1027 r2 off=0x36fcd0 page_off=0xcd0 mean=1416.8 min=957.1
D t1028 r1 off=0x36fe80 page_off=0xe80 mean=1365.5 min=921.1
D t1028 r2 off=0x36fe80 page_off=0xe80 mean=1540.3 min=1021.8
D t1029 r1 off=0x370010 page_off=0x010 mean=1488.5 min=994.6
D t1029 r2 off=0x370010 page_off=0x010 mean=1332.0 min=950.7
D t1030 r1 off=0x3701c0 page_off=0x1c0 mean=1509.9 min=1016.5
D t1030 r2 off=0x3701c0 page_off=0x1c0 mean=1510.2 min=1023.9
D t1031 r1 off=0x370350 page_off=0x350 mean=1398.1 min=964.9
D t1031 r2 off=0x370350 page_off=0x350 mean=1497.7 min=1035.1
D t1032 r1 off=0x370500 page_off=0x500 mean=1372.4 min=941.1
D t1032 r2 off=0x370500 page_off=0x500 mean=1506.0 min=1020.0
D t1033 r1 off=0x370690 page_off=0x690 mean=1475.0 min=925.1
D t1033 r2 off=0x370690 page_off=0x690 mean=1493.7 min=988.3
D t1034 r1 off=0x370840 page_off=0x840 mean=1502.9 min=958.5
D t1034 r2 off=0x370840 page_off=0x840 mean=1484.8 min=944.7
D t1035 r1 off=0x3709d0 page_off=0x9d0 mean=1367.5 min=974.0
D t1035 r2 off=0x3709d0 page_off=0x9d0 mean=1459.6 min=986.5
D median 1473.685 range 1329.36 1540.3 n 24
```

## Experiment 3: registration and device code (8 rounds)

O official, D dev, R official with dev's CUDA registration (oreg.diff), C dev with official's device code (frame + gate + tramp patches).

```
Baseline (large)
  O:     30.9 [30.5-31.7] fastest launch    25.1 n8  p(vs D) 0.563  p(vs O) 1.000
  D:     31.3 [30.5-31.6] fastest launch    28.1 n8  p(vs D) 1.000  p(vs O) 0.563
  R:     31.2 [30.5-33.0] fastest launch    24.6 n8  p(vs D) 1.000  p(vs O) 0.372
  C:     31.2 [30.5-32.6] fastest launch    25.7 n8  p(vs D) 0.958  p(vs O) 0.400
Baseline (xlarge)
  O:    159.5 [158.8-160.1] fastest launch   156.4 n8  p(vs D) 0.645  p(vs O) 1.000
  D:    159.8 [158.3-160.9] fastest launch   155.8 n8  p(vs D) 1.000  p(vs O) 0.645
  R:    159.3 [158.9-160.0] fastest launch   156.1 n8  p(vs D) 0.442  p(vs O) 0.462
  C:    159.6 [158.9-160.7] fastest launch   156.5 n8  p(vs D) 0.959  p(vs O) 0.798
Baseline (xlarge_n32)
  O:     49.4 [48.7-50.4] fastest launch    47.1 n8  p(vs D) 0.442  p(vs O) 1.000
  D:     49.2 [48.8-49.7] fastest launch    46.6 n8  p(vs D) 1.000  p(vs O) 0.442
  R:     49.3 [49.0-50.1] fastest launch    47.0 n8  p(vs D) 0.372  p(vs O) 0.958
  C:     49.5 [48.5-50.1] fastest launch    46.6 n8  p(vs D) 0.563  p(vs O) 0.793
GPU Array map update (large)
  O:    223.0 [217.3-226.9] fastest launch   159.9 n8  p(vs D) 0.000  p(vs O) 1.000
  D:    246.2 [241.6-250.3] fastest launch   166.3 n8  p(vs D) 1.000  p(vs O) 0.000
  R:    220.6 [216.1-228.3] fastest launch   160.7 n8  p(vs D) 0.000  p(vs O) 0.645
  C:    238.3 [234.4-246.9] fastest launch   172.7 n8  p(vs D) 0.005  p(vs O) 0.001
GPU Array map update (xlarge)
  O:   1687.1 [1621.8-1820.1] fastest launch  1366.4 n8  p(vs D) 0.000  p(vs O) 1.000
  D:   1473.9 [1345.4-1514.2] fastest launch   982.7 n8  p(vs D) 1.000  p(vs O) 0.000
  R:   1680.6 [1639.6-1729.7] fastest launch  1342.0 n8  p(vs D) 0.000  p(vs O) 0.645
  C:   1591.3 [1574.7-1609.5] fastest launch  1323.5 n8  p(vs D) 0.000  p(vs O) 0.000
GPU Array map update (xlarge_n32)
  O:   1879.9 [1831.9-1897.2] fastest launch  1637.0 n8  p(vs D) 0.000  p(vs O) 1.000
  D:   2130.7 [2003.2-2203.4] fastest launch  1781.9 n8  p(vs D) 1.000  p(vs O) 0.000
  R:   1873.9 [1829.1-1902.8] fastest launch  1608.7 n8  p(vs D) 0.000  p(vs O) 0.721
  C:   1927.3 [1903.6-1950.3] fastest launch  1670.5 n8  p(vs D) 0.000  p(vs O) 0.000
```

## Experiment 4: launch path with the device code (6 rounds)

O, D, L dev with BPFTIME_CUDA_LOAD_SUBSTITUTION=0, C as above, K C with BPFTIME_CUDA_LOAD_SUBSTITUTION=0.

```
Baseline (xlarge)
  O:    160.2 [159.1-160.9] fastest launch   156.3 n6  p(vs D) 1.000  p(vs O) 1.000
  D:    159.8 [158.7-162.8] fastest launch   156.7 n6  p(vs D) 1.000  p(vs O) 1.000
  L:    159.4 [158.2-160.0] fastest launch   155.9 n6  p(vs D) 0.336  p(vs O) 0.132
  C:    160.3 [159.5-161.3] fastest launch   157.0 n6  p(vs D) 0.748  p(vs O) 0.699
  K:    159.6 [158.8-160.3] fastest launch   156.6 n6  p(vs D) 0.630  p(vs O) 0.310
GPU Array map update (large)
  O:    222.8 [219.7-231.5] fastest launch   161.5 n6  p(vs D) 0.002  p(vs O) 1.000
  D:    244.9 [237.4-253.7] fastest launch   163.5 n6  p(vs D) 1.000  p(vs O) 0.002
  L:    239.0 [232.2-256.5] fastest launch   166.3 n6  p(vs D) 0.310  p(vs O) 0.002
  C:    239.9 [239.4-240.3] fastest launch   177.0 n6  p(vs D) 0.394  p(vs O) 0.002
  K:    244.2 [239.8-247.9] fastest launch   183.9 n6  p(vs D) 1.000  p(vs O) 0.002
GPU Array map update (xlarge)
  O:   1666.5 [1623.3-1708.3] fastest launch  1376.6 n6  p(vs D) 0.002  p(vs O) 1.000
  D:   1490.1 [1416.8-1499.5] fastest launch   995.8 n6  p(vs D) 1.000  p(vs O) 0.002
  L:   1487.2 [1475.3-1536.8] fastest launch   998.0 n6  p(vs D) 0.818  p(vs O) 0.002
  C:   1583.5 [1557.0-1669.0] fastest launch  1344.5 n6  p(vs D) 0.002  p(vs O) 0.026
  K:   1603.3 [1573.5-1619.0] fastest launch  1336.7 n6  p(vs D) 0.002  p(vs O) 0.002
GPU Array map update (xlarge_n32)
  O:   1869.9 [1853.1-1897.2] fastest launch  1642.7 n6  p(vs D) 0.002  p(vs O) 1.000
  D:   2136.1 [2012.9-2176.3] fastest launch  1832.9 n6  p(vs D) 1.000  p(vs O) 0.002
  L:   2145.6 [1979.3-2179.8] fastest launch  1817.4 n6  p(vs D) 0.589  p(vs O) 0.002
  C:   1922.9 [1891.7-1953.1] fastest launch  1663.0 n6  p(vs D) 0.002  p(vs O) 0.009
  K:   1942.3 [1895.4-1972.6] fastest launch  1676.7 n6  p(vs D) 0.002  p(vs O) 0.004
```
