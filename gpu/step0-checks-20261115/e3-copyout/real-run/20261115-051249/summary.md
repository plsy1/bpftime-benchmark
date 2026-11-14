# Check 3 summary: /home/jetson/src/bpftime-gpu-work/step0/e3-copyout/out/20261115-051249

Decision rules and thresholds: README.md, written before any real run.

Preflight: date 2026-11-15 05:12:49 JST | uptime  05:12:49 up 19 days, 18:09,  5 users,  load average: 0.92, 0.95, 0.59 | cpu5_khz 1728000 (want 1728000) | gpu_hz 1020000000 (want 1020000000) | possible GPU users:

## Part A: ordering of a concurrent DtoH copy

| layout | writer | area | chunk B | copies | flagged | viol chunks | older words | mixed | copies w/ change | quiet ok | writer exits stop/cap/deadline | valid |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| after | normal | 1048576 | 2048 | 5384 | 2756608 | 0 | 0 | 291964 | 5384 | yes | 256/0/0 | yes |
| after | reversed | 1048576 | 2048 | 5253 | 2689536 | 747104 | 151900064 | 284966 | 5253 | yes | 256/0/0 | yes |
| before | normal | 1048576 | 2048 | 5366 | 2747392 | 0 | 0 | 290298 | 5366 | yes | 256/0/0 | yes |
| before | reversed | 1048576 | 2048 | 5308 | 2717696 | 735438 | 148682144 | 286482 | 5308 | yes | 256/0/0 | yes |
| farafter | normal | 1048576 | 2048 | 5449 | 2789888 | 1920584 | 476240912 | 286685 | 5448 | yes | 256/0/0 | yes |
| farafter | reversed | 1048576 | 2048 | 5443 | 2786816 | 2499631 | 629156320 | 278281 | 5443 | yes | 256/0/0 | yes |
| farbefore | normal | 1048576 | 2048 | 5538 | 2835411 | 0 | 0 | 295362 | 5538 | yes | 256/0/0 | yes |
| farbefore | reversed | 1048576 | 2048 | 5538 | 2835456 | 39045 | 6868352 | 283009 | 5538 | yes | 256/0/0 | yes |
| sepff | normal | 1048576 | 2048 | 5487 | 2809344 | 0 | 0 | 290489 | 5487 | yes | 256/0/0 | yes |
| sepff | reversed | 1048576 | 2048 | 5503 | 2817536 | 4941 | 510352 | 282099 | 5503 | yes | 256/0/0 | yes |
| seppf | normal | 1048576 | 2048 | 5406 | 2767872 | 2120715 | 527719264 | 286367 | 5406 | yes | 256/0/0 | yes |
| seppf | reversed | 1048576 | 2048 | 5428 | 2779136 | 2618368 | 662421072 | 278150 | 5428 | yes | 256/0/0 | yes |
| after | normal | 1048576 | 16384 | 2613 | 167202 | 0 | 0 | 79058 | 2613 | yes | 256/0/0 | yes |
| after | reversed | 1048576 | 16384 | 2548 | 163072 | 123875 | 166917328 | 80347 | 2548 | yes | 256/0/0 | yes |
| before | normal | 1048576 | 16384 | 2612 | 167133 | 0 | 0 | 78188 | 2612 | yes | 256/0/0 | yes |
| before | reversed | 1048576 | 16384 | 2552 | 163328 | 121918 | 162680720 | 80453 | 2552 | yes | 256/0/0 | yes |
| farafter | normal | 1048576 | 16384 | 2624 | 167936 | 15204 | 9526032 | 77805 | 2624 | yes | 256/0/0 | yes |
| farafter | reversed | 1048576 | 16384 | 2588 | 165632 | 159070 | 282939296 | 80915 | 2588 | yes | 256/0/0 | yes |
| farbefore | normal | 1048576 | 16384 | 2634 | 168512 | 0 | 0 | 79261 | 2633 | yes | 256/0/0 | yes |
| farbefore | reversed | 1048576 | 16384 | 2598 | 166272 | 59375 | 57554032 | 81741 | 2598 | yes | 256/0/0 | yes |
| sepff | normal | 1048576 | 16384 | 2573 | 164608 | 0 | 0 | 77148 | 2572 | yes | 256/0/0 | yes |
| sepff | reversed | 1048576 | 16384 | 2547 | 163008 | 53151 | 50279616 | 79475 | 2547 | yes | 256/0/0 | yes |
| seppf | normal | 1048576 | 16384 | 2572 | 164608 | 19947 | 14335280 | 76725 | 2572 | yes | 256/0/0 | yes |
| seppf | reversed | 1048576 | 16384 | 2542 | 162688 | 158619 | 289303264 | 79183 | 2542 | yes | 256/0/0 | yes |
| after | normal | 16777216 | 2048 | 276 | 2255360 | 0 | 0 | 16500 | 276 | yes | 256/0/0 | yes |
| after | reversed | 16777216 | 2048 | 276 | 2255107 | 41331 | 8388608 | 16654 | 276 | yes | 256/0/0 | yes |
| before | normal | 16777216 | 2048 | 276 | 2255616 | 0 | 0 | 16648 | 276 | yes | 256/0/0 | yes |
| before | reversed | 16777216 | 2048 | 276 | 2255969 | 38814 | 7773936 | 16412 | 276 | yes | 256/0/0 | yes |
| farafter | normal | 16777216 | 2048 | 301 | 2465792 | 2314063 | 592130336 | 16815 | 301 | yes | 256/0/0 | yes |
| farafter | reversed | 16777216 | 2048 | 300 | 2457600 | 2317450 | 593015376 | 16855 | 300 | yes | 256/0/0 | yes |
| farbefore | normal | 16777216 | 2048 | 304 | 2482176 | 0 | 0 | 16932 | 303 | yes | 256/0/0 | yes |
| farbefore | reversed | 16777216 | 2048 | 307 | 2506752 | 6 | 432 | 17285 | 306 | yes | 256/0/0 | yes |
| sepff | normal | 16777216 | 2048 | 306 | 2498560 | 0 | 0 | 16963 | 305 | yes | 256/0/0 | yes |
| sepff | reversed | 16777216 | 2048 | 304 | 2482176 | 0 | 0 | 17074 | 303 | yes | 256/0/0 | yes |
| seppf | normal | 16777216 | 2048 | 298 | 2441216 | 2300909 | 588783040 | 16492 | 298 | yes | 256/0/0 | yes |
| seppf | reversed | 16777216 | 2048 | 300 | 2457600 | 2335238 | 597584320 | 16822 | 300 | yes | 256/0/0 | yes |
| after | normal | 16777216 | 16384 | 307 | 313576 | 0 | 0 | 58783 | 307 | yes | 256/0/0 | yes |
| after | reversed | 16777216 | 16384 | 305 | 311563 | 68271 | 79222032 | 59237 | 305 | yes | 256/0/0 | yes |
| before | normal | 16777216 | 16384 | 307 | 313579 | 0 | 0 | 59031 | 307 | yes | 256/0/0 | yes |
| before | reversed | 16777216 | 16384 | 305 | 311532 | 65977 | 74514672 | 59033 | 305 | yes | 256/0/0 | yes |
| farafter | normal | 16777216 | 16384 | 308 | 315296 | 280301 | 561449680 | 58600 | 308 | yes | 256/0/0 | yes |
| farafter | reversed | 16777216 | 16384 | 309 | 316365 | 294963 | 591630256 | 59637 | 309 | yes | 256/0/0 | yes |
| farbefore | normal | 16777216 | 16384 | 317 | 323533 | 0 | 0 | 60809 | 316 | yes | 256/0/0 | yes |
| farbefore | reversed | 16777216 | 16384 | 316 | 322524 | 778 | 634384 | 60692 | 315 | yes | 256/0/0 | yes |
| sepff | normal | 16777216 | 16384 | 316 | 322504 | 0 | 0 | 60502 | 315 | yes | 256/0/0 | yes |
| sepff | reversed | 16777216 | 16384 | 316 | 322525 | 356 | 195648 | 61142 | 315 | yes | 256/0/0 | yes |
| seppf | normal | 16777216 | 16384 | 309 | 316350 | 282900 | 566917504 | 58960 | 309 | yes | 256/0/0 | yes |
| seppf | reversed | 16777216 | 16384 | 309 | 316367 | 295873 | 593989264 | 59797 | 309 | yes | 256/0/0 | yes |

Per layout (normal writer): after=consistent, before=consistent, farafter=violates, farbefore=consistent, sepff=consistent, seppf=violates
Control seppf (payload copied before flags) violates: True
**A answer: fixable** (yes: one copy suffices for every flag placement; fixable: two-step copy, flags first)
Single-copy layouts that passed: after, before, farbefore. They are not relied on: the copy engine's read order is not documented, and another placement of the same data did not pass.
Sub-verdict, one copy with flags placed before their payload (before + farbefore; rests on the observed, undocumented ascending copy order): consistent. after and farafter read each flag after its payload, a reader-side race that no writer protocol fixes.

Sequence-cap path: 324 relaunches, 82944 cap exits, 0 violations, 0 quiet flag mismatches

| writer | bytes | lat p50 us | lat p99 us | lat max us | event p50 us | issue p50 us | GB/s | writer ran throughout |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| none | 65536 | 28.1 | 38.3 | 51.1 | 7.9 | 7.5 | 20.64 | - |
| none | 1048576 | 64.6 | 69.3 | 92.8 | 45.1 | 7.6 | 25.96 | - |
| none | 16777216 | 616.3 | 629.0 | 636.3 | 596.3 | 7.5 | 28.33 | - |
| running | 65536 | 82.5 | 91.5 | 93.5 | 31.2 | 7.2 | 3.34 | yes |
| running | 1048576 | 233.5 | 254.0 | 258.1 | 177.0 | 7.3 | 6.31 | yes |
| running | 16777216 | 2514.8 | 2568.8 | 2586.6 | 2456.2 | 8.1 | 6.85 | yes |

16 MiB copies with the writer running: 6.85 GB/s against the offered 0.9-6.2 GB/s.

## Part B0: copier next to a capture loop (captest)

| case | mode | iters | capture open | end errors (first) | kernel launch errors | graph launches | shim copies | copies during a capture | shim copy/wait errors (first) | shim counts match |
|---|---|---:|---:|---|---:|---:|---:|---:|---|---|
| noshim | relaxed/nonblocking | 4341 | 0.30 | 0 (0) | 0 | 4341 | - | - | - | - |
| idle-relaxed | relaxed/nonblocking | 4285 | 0.31 | 0 (0) | 0 | 4285 | 0 | 0 | 0 (0) | yes |
| async-relaxed | relaxed/nonblocking | 4254 | 0.31 | 0 (0) | 0 | 4254 | 58057 | 18866 | 0 (0) | yes |
| async-global | global/nonblocking | 4991 | 0.46 | 1767 (901) | 253972 | 3224 | 80416 | 69036 | 66137 (900) | yes |
| async-tlocal | thread_local/nonblocking | 4284 | 0.30 | 0 (0) | 0 | 4284 | 58131 | 18611 | 0 (0) | yes |
| asyncevent-relaxed | relaxed/nonblocking | 4279 | 0.30 | 0 (0) | 0 | 4279 | 43215 | 15391 | 0 (0) | yes |
| asyncevent-global | global/nonblocking | 8573 | 1.00 | 8573 (901) | 1509935 | 0 | 82561 | 76555 | 76554 (900) | yes |
| legacysync-relaxed | relaxed/nonblocking | 3844 | 0.33 | 0 (0) | 0 | 3844 | 55240 | 15381 | 0 (0) | yes |
| legacysync-relaxed-blk | relaxed/blocking | 4520 | 0.46 | 1260 (901) | 171352 | 3260 | 10482 | 2519 | 100001 (906) | yes |
| legacysync-global | global/nonblocking | 3940 | 0.34 | 0 (0) | 0 | 3940 | 55364 | 15762 | 0 (0) | yes |
| memalloc-relaxed | relaxed/nonblocking | 4253 | 0.31 | 0 (0) | 0 | 4253 | 25182 | 17668 | 0 (0) | yes |
| memalloc-global | global/nonblocking | 4094 | 0.65 | 2378 (901) | 354547 | 1716 | 72268 | 64722 | 62616 (900) | yes |
| ctxsync-relaxed | - | crashed or no output (rc 139) | | | | | | | | |
| ctxsync-global | global/nonblocking | 4674 | 0.48 | 1604 (901) | 277021 | 3070 | 70806 | 62622 | 60117 (900) | yes |

Interposer counts equal captest's own: True. Relaxed-mode control (legacy-stream copy, blocking capture stream) invalidates captures: True. Global-mode controls (memalloc or ctxsync): True. In global mode the async copier is safe: False.
**B0 verdict (async copier safe next to relaxed-mode captures): pass**

## Part B: copier shim inside llama-bench, graphs on

| config | test | tokens/s per round | mean | loss vs noshim |
|---|---|---|---:|---:|
| noshim | pp512 | 2487.99, 2497.55, 2501.95 | 2495.83 | noise 0.6% |
| noshim | tg128 | 67.57, 67.88, 67.97 | 67.80 | noise 0.6% |
| idle | pp512 | 2489.99, 2493.36, 2492.75 | 2492.03 | 0.2% |
| idle | tg128 | 67.65, 67.77, 67.97 | 67.80 | 0.0% |
| c1m1ms | pp512 | 2484.68, 2488.04, 2494.47 | 2489.06 | 0.3% |
| c1m1ms | tg128 | 67.30, 67.64, 67.33 | 67.43 | 0.6% |
| c16m1ms | pp512 | 2416.33, 2398.17, 2408.70 | 2407.73 | 3.5% |
| c16m1ms | tg128 | 62.45, 62.45, 62.45 | 62.45 | 7.9% |
| cont | pp512 | 2361.49, 2364.20, 2354.86 | 2360.18 | 5.4% |
| cont | tg128 | 59.55, 59.59, 59.52 | 59.56 | 12.2% |

Graph launches in round 1, tg128 (shim count): idle=768, c1m1ms=768, c16m1ms=768, cont=768
Graphs-off control: launches=0 captures=0 rc=0 -> ok
nsys cross-check (cudaGraphLaunch rows equal the shim's count): True

| tag | copies | during a capture | copies/s | GB/s | missed periods | lat p50/p99/max us | ctx wait s |
|---|---:|---:|---:|---:|---:|---|---:|
| r1-c16m1ms-pp512 | 934 | 4 | 781 | 13.111 | 257 | 651/2072/192686 | 0.083 |
| r1-c16m1ms-tg128 | 6431 | 4 | 989 | 16.599 | 65 | 696/791/24562 | 0.086 |
| r1-c1m1ms-pp512 | 904 | 3 | 788 | 0.826 | 240 | 65/946/192568 | 0.083 |
| r1-c1m1ms-tg128 | 5973 | 2 | 991 | 1.040 | 49 | 70/89/22869 | 0.084 |
| r1-cont-pp512 | 1421 | 6 | 1148 | 19.257 | 0 | 651/1442/193194 | 0.084 |
| r1-cont-tg128 | 9659 | 6 | 1416 | 23.760 | 0 | 695/765/28650 | 0.083 |
| r2-c16m1ms-pp512 | 943 | 5 | 758 | 12.720 | 296 | 655/2398/192925 | 0.083 |
| r2-c16m1ms-tg128 | 6429 | 3 | 982 | 16.482 | 113 | 697/782/41533 | 0.087 |
| r2-c1m1ms-pp512 | 904 | 3 | 762 | 0.799 | 278 | 66/2072/192204 | 0.084 |
| r2-c1m1ms-tg128 | 5947 | 3 | 985 | 1.032 | 90 | 71/85/34582 | 0.087 |
| r2-cont-pp512 | 1427 | 6 | 1122 | 18.826 | 0 | 649/1473/192675 | 0.083 |
| r2-cont-tg128 | 9655 | 6 | 1407 | 23.606 | 0 | 691/762/49635 | 0.085 |
| r3-c16m1ms-pp512 | 938 | 4 | 740 | 12.419 | 325 | 655/2559/192605 | 0.083 |
| r3-c16m1ms-tg128 | 6428 | 4 | 980 | 16.448 | 125 | 693/781/50296 | 0.086 |
| r3-c1m1ms-pp512 | 905 | 3 | 725 | 0.760 | 340 | 66/2018/191809 | 0.084 |
| r3-c1m1ms-tg128 | 5972 | 3 | 982 | 1.029 | 110 | 72/90/47351 | 0.084 |
| r3-cont-pp512 | 1420 | 6 | 1091 | 18.305 | 0 | 651/1519/193386 | 0.083 |
| r3-cont-tg128 | 9666 | 5 | 1404 | 23.554 | 0 | 693/769/53017 | 0.087 |

**B: graphs keep working with the copier: yes; idle shim within noise: True; 1 MiB/1 ms loss bounded (<= 5.0%, both tests): yes.**

nsys llama-cont: kernels 54696, in graphs 54272 (99.2%); cudaGraphLaunch nsys 256 vs shim 256; shim copies seen 3863, copy time overlapping kernels 81.4%
nsys llama-idle: kernels 54696, in graphs 54272 (99.2%); cudaGraphLaunch nsys 256 vs shim 256; shim copies seen 0, copy time overlapping kernels -

## Part C: POSIX-FD VMM export to a second process

**fill-plain-rdma0**: cuInit=CUDA_SUCCESS, cuMemGetAllocationGranularity=CUDA_SUCCESS, cuMemCreate=CUDA_SUCCESS, cuMemAddressReserve=CUDA_SUCCESS, cuMemMap=CUDA_SUCCESS, cuMemSetAccess_device=CUDA_SUCCESS, cuMemExportToShareableHandle=CUDA_SUCCESS, cuMemUnmap=CUDA_SUCCESS, cuMemAddressFree=CUDA_SUCCESS, cuMemRelease=CUDA_SUCCESS, cuMemImportFromShareableHandle=CUDA_SUCCESS, cuMemGetAllocationPropertiesFromHandle=CUDA_SUCCESS, cuMemcpyDtoH_pageable=CUDA_SUCCESS, cuMemHostRegister_dst=CUDA_SUCCESS, cuMemAlloc_local=CUDA_SUCCESS, cuMemSetAccess_host=CUDA_ERROR_NOT_SUPPORTED, cuMemSetAccess_host_numa=CUDA_ERROR_NOT_SUPPORTED, cuPointerGetAttribute_HOST_POINTER=CUDA_ERROR_INVALID_VALUE

- fd: /dmabuf:; mmap before CUDA init: 1; updates by process 1: 1228; distinct generations seen: DtoH 1229, kernel 1229, CPU mapping 1229
- stale CPU-mapped words: gen 1 326336 (civac 326336); after the writer stopped 0/10/100/1000 ms: 113904/88512/88512/88512; after process 1 exited: 0; final DtoH stale 0
- DtoH bandwidth, imported 27.3711 GB/s vs local cuMemAlloc 27.4178 GB/s; fill us mean 387.433
- tier 1 (CPU mapping usable): False; tier 2 (import + reads in own context): True; both processes reported: True

**fill-wt-rdma0**: cuInit=CUDA_SUCCESS, cuMemGetAllocationGranularity=CUDA_SUCCESS, cuMemCreate=CUDA_SUCCESS, cuMemAddressReserve=CUDA_SUCCESS, cuMemMap=CUDA_SUCCESS, cuMemSetAccess_device=CUDA_SUCCESS, cuMemExportToShareableHandle=CUDA_SUCCESS, cuMemUnmap=CUDA_SUCCESS, cuMemAddressFree=CUDA_SUCCESS, cuMemRelease=CUDA_SUCCESS, cuMemImportFromShareableHandle=CUDA_SUCCESS, cuMemGetAllocationPropertiesFromHandle=CUDA_SUCCESS, cuMemcpyDtoH_pageable=CUDA_SUCCESS, cuMemHostRegister_dst=CUDA_SUCCESS, cuMemAlloc_local=CUDA_SUCCESS, cuMemSetAccess_host=CUDA_ERROR_NOT_SUPPORTED, cuMemSetAccess_host_numa=CUDA_ERROR_NOT_SUPPORTED, cuPointerGetAttribute_HOST_POINTER=CUDA_ERROR_INVALID_VALUE

- fd: /dmabuf:; mmap before CUDA init: 1; updates by process 1: 1252; distinct generations seen: DtoH 1253, kernel 1252, CPU mapping 1252
- stale CPU-mapped words: gen 1 370048 (civac 323680); after the writer stopped 0/10/100/1000 ms: 126176/107744/107744/107744; after process 1 exited: 0; final DtoH stale 0
- DtoH bandwidth, imported 27.3355 GB/s vs local cuMemAlloc 27.3951 GB/s; fill us mean 340.917
- tier 1 (CPU mapping usable): False; tier 2 (import + reads in own context): True; both processes reported: True

**fill-sysfence-rdma0**: cuInit=CUDA_SUCCESS, cuMemGetAllocationGranularity=CUDA_SUCCESS, cuMemCreate=CUDA_SUCCESS, cuMemAddressReserve=CUDA_SUCCESS, cuMemMap=CUDA_SUCCESS, cuMemSetAccess_device=CUDA_SUCCESS, cuMemExportToShareableHandle=CUDA_SUCCESS, cuMemUnmap=CUDA_SUCCESS, cuMemAddressFree=CUDA_SUCCESS, cuMemRelease=CUDA_SUCCESS, cuMemImportFromShareableHandle=CUDA_SUCCESS, cuMemGetAllocationPropertiesFromHandle=CUDA_SUCCESS, cuMemcpyDtoH_pageable=CUDA_SUCCESS, cuMemHostRegister_dst=CUDA_SUCCESS, cuMemAlloc_local=CUDA_SUCCESS, cuMemSetAccess_host=CUDA_ERROR_NOT_SUPPORTED, cuMemSetAccess_host_numa=CUDA_ERROR_NOT_SUPPORTED, cuPointerGetAttribute_HOST_POINTER=CUDA_ERROR_INVALID_VALUE

- fd: /dmabuf:; mmap before CUDA init: 1; updates by process 1: 1208; distinct generations seen: DtoH 1208, kernel 1207, CPU mapping 1207
- stale CPU-mapped words: gen 1 302992 (civac 302400); after the writer stopped 0/10/100/1000 ms: 124272/117120/117120/117120; after process 1 exited: 0; final DtoH stale 0
- DtoH bandwidth, imported 27.2843 GB/s vs local cuMemAlloc 27.4263 GB/s; fill us mean 429.405
- tier 1 (CPU mapping usable): False; tier 2 (import + reads in own context): True; both processes reported: True

**fill-plain-rdma1**: cuInit=CUDA_SUCCESS, cuMemGetAllocationGranularity=CUDA_SUCCESS, cuMemCreate=CUDA_ERROR_INVALID_DEVICE

- fd: None; mmap before CUDA init: None; updates by process 1: 0; distinct generations seen: DtoH None, kernel None, CPU mapping None
- stale CPU-mapped words: gen 1 None (civac None); after the writer stopped 0/10/100/1000 ms: None/None/None/None; after process 1 exited: None; final DtoH stale None
- DtoH bandwidth, imported None GB/s vs local cuMemAlloc None GB/s; fill us mean -1
- tier 1 (CPU mapping usable): False; tier 2 (import + reads in own context): False; both processes reported: True

**C: tier 1 (direct CPU mapping) works: False; tier 2 (import, kernel and DtoH reads) works: True; verdict: works.**

## Overall

Can storage the CPU reads live in device memory on the Orin at all? **yes**
Paths that passed: agent copy-out into the segment (two-step copy, flags first); reader imports the fd and copies in its own CUDA context (needs CUDA in the reader; a copy concurrent with the writer, so ordering is part A's answer, here fixable; cost on a real application not measured: process 1 is a toy writer)
Path 1 (agent copy-out: A fixable, B graphs yes, B bounded yes, B0 pass): pass. Part C: works.

