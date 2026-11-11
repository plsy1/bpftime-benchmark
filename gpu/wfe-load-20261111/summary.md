Runs: 160, valid 160; rounds 1,2,3,4,5; max temperature 54.5 C; device flags seen ['0x8']


### load on, spin 2000 us per launch, probe host call at exit

| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) | load (M iter/s) | load vs P (median, min-max) | host call mean (us) | host call median / p90 / p99 / max (us) | calls > 3 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 5 | 0.016 | 2,036.7 | 2,075.6 | 0.83 | 1.000 (1.000-1.000) | - | - | - |
| N | 5 | 0.080 | 3,073.3 | 3,145.7 | 0.82 | 0.988 (0.988-0.989) | 1,048.4 | 1,024.1 / 1,032.4 / 2,927.3 / 8,313.5 | 0.9% |
| W | 5 | 0.784 | 2,061.6 | 2,576.1 | 0.73 | 0.870 (0.868-0.872) | 475.5 | 14.8 / 3,510.3 / 3,896.7 / 10,157.9 | 11.0% |
| W0 | 5 | 0.185 | 2,222.6 | 2,469.1 | 0.81 | 0.970 (0.965-0.970) | 360.1 | 172.4 / 658.0 / 3,393.7 / 7,044.8 | 2.2% |

### load on, spin 2000 us per launch, probe no host call

| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) | load (M iter/s) | load vs P (median, min-max) |
|---|---:|---:|---:|---:|---:|---:|
| P | 5 | 0.016 | 2,036.8 | 2,077.2 | 0.83 | 1.000 (1.000-1.000) |
| N | 5 | 0.021 | 2,048.9 | 2,096.4 | 0.83 | 0.998 (0.993-0.999) |
| W | 5 | 0.806 | 2,048.4 | 2,103.1 | 0.72 | 0.867 (0.860-0.871) |
| W0 | 5 | 0.123 | 2,048.7 | 2,116.9 | 0.82 | 0.979 (0.978-0.982) |

### load on, spin 20000 us per launch, probe host call at exit

| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) | load (M iter/s) | load vs P (median, min-max) | host call mean (us) | host call median / p90 / p99 / max (us) | calls > 3 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 5 | 0.002 | 20,042.2 | 20,089.2 | 0.84 | 1.000 (1.000-1.000) | - | - | - |
| N | 5 | 0.014 | 20,990.8 | 20,995.3 | 0.83 | 0.996 (0.995-0.998) | 886.2 | 933.3 / 958.3 / 1,056.0 / 7,353.5 | 0.3% |
| W | 5 | 0.804 | 20,077.2 | 21,142.4 | 0.72 | 0.863 (0.857-0.869) | 985.0 | 15.2 / 3,941.0 / 3,989.7 / 7,972.9 | 23.4% |
| W0 | 5 | 0.025 | 20,143.3 | 20,342.2 | 0.83 | 0.995 (0.992-0.997) | 211.9 | 89.1 / 725.8 / 1,064.6 / 4,205.9 | 0.1% |

### load on, spin 20000 us per launch, probe no host call

| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) | load (M iter/s) | load vs P (median, min-max) |
|---|---:|---:|---:|---:|---:|---:|
| P | 5 | 0.002 | 20,043.4 | 20,083.2 | 0.84 | 1.000 (1.000-1.000) |
| N | 5 | 0.004 | 20,054.0 | 20,121.7 | 0.84 | 1.001 (0.998-1.002) |
| W | 5 | 0.818 | 20,054.6 | 20,177.8 | 0.72 | 0.865 (0.859-0.869) |
| W0 | 5 | 0.016 | 20,053.8 | 20,148.3 | 0.83 | 0.997 (0.996-1.007) |

### load off, spin 2000 us per launch, probe host call at exit

| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) | host call mean (us) | host call median / p90 / p99 / max (us) | calls > 3 ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 5 | 0.013 | 2,027.2 | 2,028.7 | - | - | - |
| N | 5 | 0.079 | 3,069.4 | 3,068.2 | 1,026.6 | 1,030.6 / 1,035.3 / 1,052.4 / 2,651.3 | 0.0% |
| W | 5 | 1.017 | 2,054.3 | 2,055.5 | 14.0 | 13.8 / 14.8 / 15.0 / 159.9 | 0.0% |
| W0 | 5 | 0.202 | 2,218.0 | 2,218.4 | 176.6 | 177.9 / 182.7 / 198.0 / 506.6 | 0.0% |

### load off, spin 2000 us per launch, probe no host call

| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) |
|---|---:|---:|---:|---:|
| P | 5 | 0.013 | 2,028.1 | 2,029.7 |
| N | 5 | 0.019 | 2,039.9 | 2,040.8 |
| W | 5 | 1.015 | 2,039.6 | 2,042.3 |
| W0 | 5 | 0.125 | 2,040.8 | 2,042.0 |

### load off, spin 20000 us per launch, probe host call at exit

| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) | host call mean (us) | host call median / p90 / p99 / max (us) | calls > 3 ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 5 | 0.002 | 20,041.5 | 20,041.8 | - | - | - |
| N | 5 | 0.013 | 20,979.0 | 20,982.0 | 928.6 | 928.2 / 956.6 / 989.7 / 1,063.1 | 0.0% |
| W | 5 | 1.001 | 20,067.1 | 20,068.6 | 14.3 | 14.2 / 15.0 / 16.2 / 145.5 | 0.0% |
| W0 | 5 | 0.025 | 20,129.6 | 20,138.5 | 85.7 | 77.3 / 110.9 / 179.6 / 1,067.7 | 0.0% |

### load off, spin 20000 us per launch, probe no host call

| set | runs | victim CPU s per wall s | per-launch median (us) | per-launch mean (us) |
|---|---:|---:|---:|---:|
| P | 5 | 0.002 | 20,041.2 | 20,041.8 |
| N | 5 | 0.004 | 20,052.0 | 20,053.0 |
| W | 5 | 1.001 | 20,052.3 | 20,053.9 |
| W0 | 5 | 0.015 | 20,052.8 | 20,054.4 |
