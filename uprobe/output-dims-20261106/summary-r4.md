# Round 4 summary (summarize_r4.py)

## R4: count_event call duration (ns), bursts of 100 at 4 us, every 10 ms

52 runs, 0 not ok.

| obj | threads | backend | p50 | p90 | p99 | p99.9 | max |
|---|---:|---|---:|---:|---:|---:|---:|
| - | 1 | none | 64 | 64 | 64 | 80 | 6241 |
| - | 4 | none | 64 | 64 | 64 | 96 | 11072 |
| out-control.bpf.o | 1 | fix-final | 288 | 288 | 448 | 1856 | 107924 |
| out-control.bpf.o | 1 | kernel | 1280 | 1280 | 1728 | 3840 | 48402 |
| out-control.bpf.o | 4 | fix-final | 288 | 576 | 1024 | 2560 | 137877 |
| out-control.bpf.o | 4 | kernel | 1280 | 2688 | 3712 | 4608 | 47570 |
| out-perf-256.bpf.o | 1 | fix-final | 432 | 528 | 1056 | 3712 | 314539 |
| out-perf-256.bpf.o | 1 | kernel | 3584 | 4096 | 5120 | 9728 | 51266 |
| out-perf-256.bpf.o | 4 | fix-final | 576 | 1216 | 1536 | 4096 | 286362 |
| out-perf-256.bpf.o | 4 | kernel | 3072 | 4352 | 6144 | 10752 | 56786 |
| out-perf-4096.bpf.o | 1 | fix-final | 928 | 1216 | 4096 | 6400 | 334268 |
| out-perf-4096.bpf.o | 1 | kernel | 3584 | 4608 | 5120 | 11776 | 52386 |
| out-perf-4096.bpf.o | 4 | fix-final | 1152 | 1920 | 4352 | 9472 | 334940 |
| out-perf-4096.bpf.o | 4 | kernel | 3328 | 4608 | 6656 | 12288 | 78755 |
| out-rbres-256.bpf.o | 1 | fix-final | 1792 | 2816 | 3584 | 7936 | 179846 |
| out-rbres-256.bpf.o | 1 | kernel | 2432 | 3584 | 3968 | 7680 | 44658 |
| out-rbres-256.bpf.o | 4 | fix-final | 1152 | 2944 | 3968 | 15360 | 218280 |
| out-rbres-256.bpf.o | 4 | kernel | 2944 | 4608 | 6144 | 9216 | 105396 |
| out-rbres-4088.bpf.o | 1 | fix-final | 1856 | 2816 | 3584 | 8192 | 171494 |
| out-rbres-4088.bpf.o | 1 | kernel | 2432 | 3840 | 4096 | 8704 | 46818 |
| out-rbres-4088.bpf.o | 4 | fix-final | 1536 | 2816 | 3712 | 7936 | 186054 |
| out-rbres-4088.bpf.o | 4 | kernel | 2432 | 4352 | 5888 | 9216 | 1230411 |
| out-rbres-4096.bpf.o | 1 | fix-final | 544 | 2816 | 3328 | 6400 | 188919 |
| out-rbres-4096.bpf.o | 1 | kernel | 2432 | 3584 | 3968 | 7936 | 47826 |
| out-rbres-4096.bpf.o | 4 | fix-final | 512 | 1984 | 3456 | 5632 | 188534 |
| out-rbres-4096.bpf.o | 4 | kernel | 2560 | 3968 | 5120 | 8192 | 48289 |

## R5: variable sizes 72/520/1480/3992 B, matched arrival

48 runs, 0 not ok.

| obj | burst | procs | threads | delivered k / b | first p50 us k / b | first p90 | all p99 us k / b |
|---|---:|---:|---:|---|---|---|---|
| out-perf-var.bpf.o | 100 | 1 | 1 | 100.0% / 100.0% | 10.3 / 376.0 | 16.0 / 413.2 | 11.5 / 1054.9 |
| out-perf-var.bpf.o | 100 | 1 | 4 | 100.0% / 100.0% | 9.2 / 200.2 | 13.7 / 356.9 | 103.3 / 1054.1 |
| out-perf-var.bpf.o | 1000 | 1 | 1 | 100.0% / 100.0% | 8.9 / 351.3 | 15.3 / 400.6 | 8.5 / 1026.5 |
| out-perf-var.bpf.o | 1000 | 1 | 4 | 99.5% / 99.1% | 9.1 / 181.1 | 14.6 / 320.5 | 9.7 / 943.5 |
| out-rbout-var.bpf.o | 100 | 1 | 1 | 100.0% / 100.0% | 8.4 / 9.7 | 14.5 / 14.8 | 9.6 / 1043.9 |
| out-rbout-var.bpf.o | 100 | 1 | 4 | 100.0% / 96.5% | 8.3 / 9.5 | 13.9 / 15.2 | 8.6 / 1039.0 |
| out-rbout-var.bpf.o | 1000 | 1 | 1 | 100.0% / 85.7% | 7.9 / 9.5 | 13.9 / 272.9 | 6.4 / 1040.2 |
| out-rbout-var.bpf.o | 1000 | 1 | 4 | 99.0% / 81.1% | 8.8 / 10.4 | 14.8 / 22.2 | 8.3 / 1031.3 |
| out-rbres-var.bpf.o | 100 | 1 | 1 | 100.0% / 100.0% | 7.9 / 9.1 | 14.4 / 13.3 | 9.2 / 1044.2 |
| out-rbres-var.bpf.o | 100 | 1 | 4 | 99.9% / 95.1% | 10.4 / 9.4 | 14.3 / 15.4 | 161.3 / 1043.1 |
| out-rbres-var.bpf.o | 1000 | 1 | 1 | 100.0% / 85.4% | 7.7 / 9.4 | 14.0 / 15.5 | 6.2 / 1040.1 |
| out-rbres-var.bpf.o | 1000 | 1 | 4 | 98.6% / 70.8% | 8.5 / 9.9 | 15.6 / 124.6 | 8.3 / 1044.8 |

## R6: processes vs threads, bursts of 100 at 4 us

60 runs, 0 not ok.

| obj | burst | procs | threads | delivered k / b | first p50 us k / b | first p90 | all p99 us k / b |
|---|---:|---:|---:|---|---|---|---|
| out-perf-256.bpf.o | 100 | 1 | 1 | 100.0% / 100.0% | 8.4 / 535.8 | 14.8 / 962.5 | 75.5 / 1051.8 |
| out-perf-256.bpf.o | 100 | 1 | 2 | 100.0% / 100.0% | 9.4 / 412.3 | 15.1 / 799.3 | 181.9 / 1055.9 |
| out-perf-256.bpf.o | 100 | 1 | 4 | 100.0% / 100.0% | 10.7 / 313.0 | 13.9 / 714.1 | 9.5 / 1068.3 |
| out-perf-256.bpf.o | 100 | 2 | 1 | 100.0% / 100.0% | 8.6 / 545.0 | 14.5 / 959.4 | 9.6 / 1051.6 |
| out-perf-256.bpf.o | 100 | 4 | 1 | 100.0% / 100.0% | 7.7 / 541.2 | 15.0 / 958.1 | 36.8 / 1055.7 |
| out-rbres-256.bpf.o | 100 | 1 | 1 | 100.0% / 100.0% | 7.8 / 8.8 | 14.0 / 11.8 | 8.9 / 993.4 |
| out-rbres-256.bpf.o | 100 | 1 | 2 | 100.0% / 100.0% | 12.0 / 8.9 | 13.4 / 14.9 | 24.0 / 938.8 |
| out-rbres-256.bpf.o | 100 | 1 | 4 | 100.0% / 100.0% | 9.9 / 11.4 | 13.3 / 16.4 | 143.5 / 982.6 |
| out-rbres-256.bpf.o | 100 | 2 | 1 | 100.0% / 100.0% | 9.2 / 12.9 | 13.5 / 15.0 | 69.8 / 966.1 |
| out-rbres-256.bpf.o | 100 | 4 | 1 | 100.0% / 100.0% | 9.2 / 9.5 | 14.2 / 15.5 | 186.5 / 962.4 |
| out-rbres-4088.bpf.o | 100 | 1 | 1 | 100.0% / 100.0% | 7.5 / 8.7 | 13.5 / 12.2 | 8.7 / 8.9 |
| out-rbres-4088.bpf.o | 100 | 1 | 2 | 99.9% / 100.0% | 7.9 / 8.8 | 13.7 / 12.1 | 8.4 / 9.4 |
| out-rbres-4088.bpf.o | 100 | 1 | 4 | 100.0% / 99.8% | 8.0 / 9.4 | 12.9 / 12.9 | 7.9 / 12.7 |
| out-rbres-4088.bpf.o | 100 | 2 | 1 | 99.2% / 100.0% | 11.1 / 9.1 | 13.2 / 14.5 | 11.8 / 10.5 |
| out-rbres-4088.bpf.o | 100 | 4 | 1 | 99.7% / 99.6% | 8.0 / 9.3 | 13.9 / 17.2 | 7.9 / 13.3 |

## R7: 16 and 64 processes, bursts of 10 at 4 us every 10 ms

16 runs, 8 not ok.

| obj | burst | procs | threads | delivered k / b | first p50 us k / b | first p90 | all p99 us k / b |
|---|---:|---:|---:|---|---|---|---|
| out-perf-256.bpf.o | 10 | 16 | 1 | 100.0% / 100.0% | 11.5 / 406.1 | 15.5 / 878.1 | 24.5 / 1055.3 |
| out-perf-256.bpf.o | 10 | 64 | 1 | 100.0% / 100.0% | 12.4 / 276.3 | 47.4 / 765.5 | 68.7 / 1072.0 |
| out-rbres-256.bpf.o | 10 | 16 | 1 | 100.0% / 100.0% | 10.0 / 12.0 | 15.1 / 19.3 | 22.0 / 68.3 |
| out-rbres-256.bpf.o | 10 | 64 | 1 | 100.0% / 100.0% | 12.9 / 15.2 | 56.9 / 42.7 | 125.7 / 1058.4 |

## R6 back to back: ns per call, 4 producers as threads or processes

36 runs, 0 not ok.

| obj | layout | kernel ns/call | bpftime ns/call | delivered k / b |
|---|---|---:|---:|---|
| out-control.bpf.o | 1 proc x 4 thr | 3437.5 | 4192.3 | 0.0% / 0.0% |
| out-control.bpf.o | 4 proc x 1 thr | 2875.1 | 281.6 | 0.0% / 0.0% |
| out-perf-8.bpf.o | 1 proc x 4 thr | 5400.0 | 4044.1 | 100.0% / 100.0% |
| out-perf-8.bpf.o | 4 proc x 1 thr | 4203.6 | 845.8 | 100.0% / 100.0% |
| out-rbres-8.bpf.o | 1 proc x 4 thr | 4094.1 | 3947.5 | 100.0% / 100.0% |
| out-rbres-8.bpf.o | 4 proc x 1 thr | 3202.4 | 3864.7 | 100.0% / 100.0% |
