| run | prompt | gen | tokens/s mean | sd | samples | victim wall ms | threadhist last line |
|---|---:|---:|---:|---:|---|---:|---|
| llama-base-pp32 | 32 | 0 | 1288.7 | 18.6 | 1268.2, 1293.7, 1304.3 | 645 | lookup=0 slots=262144 nonzero=0 last_nonzero=0 sum=0 max=0 slot0=0 |
| llama-base-pp32-b | 32 | 0 | 1286.2 | 17.7 | 1265.9, 1295.2, 1297.6 | 646 | lookup=0 slots=262144 nonzero=0 last_nonzero=0 sum=0 max=0 slot0=0 |
| llama-base-pp512 | 512 | 0 | 2475.1 | 1.4 | 2475.0, 2476.6, 2473.7 | 1397 | lookup=0 slots=262144 nonzero=0 last_nonzero=0 sum=0 max=0 slot0=0 |
| llama-base-pp512-b | 512 | 0 | 2478.4 | 1.1 | 2477.6, 2477.9, 2479.6 | 1399 | lookup=0 slots=262144 nonzero=0 last_nonzero=0 sum=0 max=0 slot0=0 |
| llama-empty-pp32-b | 32 | 0 | 1284.8 | 17.5 | 1264.6, 1295.7, 1294.0 | 15334 | lookup=0 slots=262144 nonzero=0 last_nonzero=0 sum=0 max=0 slot0=0 |
| llama-empty-pp512-b | 512 | 0 | 2476.7 | 0.5 | 2476.2, 2476.9, 2477.1 | 15832 | lookup=0 slots=262144 nonzero=0 last_nonzero=0 sum=0 max=0 slot0=0 |
| llama-hostcall-pp32 | 32 | 0 | 4.2 | 0.0 | 4.2 | 30296 | lookup=0 count=1411072 |
| llama-th-default-pp512 | 512 | 0 | 35.1 | 0.0 | 35.1 | 15404 | lookup=0 slots=1024 nonzero=1024 last_nonzero=1023 sum=45056 max=45 slot0=45 |
| llama-th-mmq-pp512 | 512 | 0 | 16.6 | 0.0 | 16.6 | 31642 | lookup=0 slots=90112 nonzero=90112 last_nonzero=90111 sum=1892352 max=21 slot0=21 |
| llama-th-mmvq-tg128 | 0 | 128 | 3.3 | 0.0 | 3.3 | 39131 | lookup=0 slots=720896 nonzero=720896 last_nonzero=720895 sum=3137339392 max=7040 slot0=7040 |
| llama-th-pp32 | 32 | 0 | 2.2 | 0.0 | 2.2 | 15687 | lookup=0 slots=262144 nonzero=16384 last_nonzero=16383 sum=705536 max=45 slot0=45 |
| llama-th-pp32-b | 32 | 0 | 1255.2 | 15.1 | 1237.8, 1263.3, 1264.6 | 15440 | lookup=0 slots=262144 nonzero=16384 last_nonzero=16383 sum=2822144 max=180 slot0=180 |
| llama-th-pp512 | 512 | 0 | 2394.6 | 1.1 | 2394.2, 2395.9, 2393.8 | 15973 | lookup=0 slots=262144 nonzero=262144 last_nonzero=262143 sum=45092864 max=180 slot0=180 |
| llama-th-pp512-b | 512 | 0 | 2397.6 | 1.1 | 2396.5, 2397.6, 2398.7 | 16054 | lookup=0 slots=262144 nonzero=262144 last_nonzero=262143 sum=45092864 max=180 slot0=180 |
| llama-th-ropek-pp512 | 512 | 0 | 35.0 | 0.0 | 35.0 | 15403 | lookup=0 slots=524288 nonzero=524288 last_nonzero=524287 sum=11534336 max=22 slot0=22 |
