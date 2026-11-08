## 1. A process traced by memleak -c (10 runs; ms, p50 [min-max])

| cell | exec -> main | tool set-up until the command starts |
|---|---:|---:|
| kernel | 0.7 [0.7-0.8] | 153.8 [144.6-173.4] |
| old | 308.3 [305.9-311.4] | 431.5 [430.3-435.7] |
| new | 38.1 [37.9-38.7] | 743.1 [739.5-753.4] |
| new-noaot | 309.2 [305.9-312.1] | 431.1 [430.1-432.9] |

## 2. K processes starting at once, one program (ms; two passes a/b)

| backend | K | child p50 | child max | child CPU p50 | round p50 | ok |
|---|---:|---:|---:|---:|---:|---|
| kernel | 1 | 0.5/0.5 | 2.8/0.6 | 0.5/0.5 | 0.5/0.5 | True |
| kernel | 2 | 0.5/0.5 | 0.7/0.7 | 0.5/0.5 | 0.7/0.7 | True |
| kernel | 4 | 0.6/0.6 | 0.9/0.8 | 0.5/0.5 | 1.0/1.0 | True |
| kernel | 8 | 0.8/0.7 | 1.7/1.7 | 0.6/0.5 | 1.6/1.5 | True |
| kernel | 16 | 1.5/1.3 | 3.2/2.8 | 0.6/0.6 | 3.1/2.6 | True |
| dev-9c088441-probecheck | 1 | 22.5/22.2 | 23.6/23.3 | 22.4/22.2 | 22.5/22.2 | True |
| dev-9c088441-probecheck | 2 | 23.7/23.4 | 36.5/36.9 | 22.3/22.4 | 35.7/35.7 | True |
| dev-9c088441-probecheck | 4 | 36.8/37.1 | 63.4/63.2 | 22.7/22.9 | 62.3/62.5 | True |
| dev-9c088441-probecheck | 8 | 64.7/63.7 | 115.4/115.9 | 22.9/22.9 | 115.6/115.4 | True |
| dev-9c088441-probecheck | 16 | 118.9/116.2 | 220.0/208.3 | 22.8/22.9 | 220.6/220.7 | True |
| dev-9898ed19-probecheck | 1 | 13.6/13.5 | 14.0/14.0 | 13.6/13.5 | 13.6/13.5 | True |
| dev-9898ed19-probecheck | 2 | 14.3/14.1 | 19.5/18.9 | 13.6/13.7 | 18.4/18.5 | True |
| dev-9898ed19-probecheck | 4 | 19.3/19.4 | 29.2/28.6 | 13.9/14.0 | 28.1/28.3 | True |
| dev-9898ed19-probecheck | 8 | 29.5/29.6 | 46.5/47.9 | 14.1/14.0 | 47.1/47.1 | True |
| dev-9898ed19-probecheck | 16 | 49.0/46.8 | 80.3/78.1 | 13.8/13.8 | 86.8/86.9 | True |
| dev-9898ed19-probecheck:noaot | 1 | 22.5/22.5 | 22.9/23.0 | 22.4/22.4 | 22.5/22.5 | True |
| dev-9898ed19-probecheck:noaot | 2 | 23.7/23.2 | 36.9/36.8 | 22.7/22.6 | 36.3/36.0 | True |
| dev-9898ed19-probecheck:noaot | 4 | 36.6/37.0 | 63.2/63.9 | 22.9/22.9 | 62.8/62.6 | True |
| dev-9898ed19-probecheck:noaot | 8 | 64.6/63.9 | 114.3/115.3 | 23.0/22.9 | 116.0/115.9 | True |
| dev-9898ed19-probecheck:noaot | 16 | 117.3/118.3 | 207.7/212.1 | 22.9/22.8 | 221.9/222.5 | True |

## 3. Output cost per call (output-dims run.py, 3 x 6 s, ns per call per thread, median)

| program | threads | old | new | new/old |
|---|---:|---:|---:|---:|
| out-control.bpf.o | 1 | 224 | 225 | 1.004 |
| out-control.bpf.o | 6 | 4750 | 4671 | 0.983 |
| out-perf-8.bpf.o | 1 | 613 | 604 | 0.985 |
| out-perf-8.bpf.o | 6 | 4672 | 4713 | 1.009 |
| out-rbres-8.bpf.o | 1 | 391 | 383 | 0.982 |
| out-rbres-8.bpf.o | 6 | 4917 | 5050 | 1.027 |
