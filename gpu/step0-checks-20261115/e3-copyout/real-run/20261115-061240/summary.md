# Check 3 summary: /home/jetson/src/bpftime-gpu-work/step0/e3-copyout/out/20261115-061240

Decision rules and thresholds: README.md, written before any real run.

Preflight: date 2026-11-15 06:12:40 JST | uptime  06:12:40 up 19 days, 19:09,  5 users,  load average: 0.89, 0.94, 1.07 | cpu5_khz 1728000 (want 1728000) | gpu_hz 1020000000 (want 1020000000) | possible GPU users:

## Part A: ordering of a concurrent DtoH copy

| layout | writer | area | chunk B | copies | flagged | viol chunks | older words | mixed | copies w/ change | quiet ok | writer exits stop/cap/deadline | valid |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| after | normal | 1048576 | 2048 | 5301 | 2714112 | 0 | 0 | 286134 | 5301 | yes | 256/0/0 | yes |
| after | reversed | 1048576 | 2048 | 5222 | 2671104 | 743646 | 150469120 | 285422 | 5217 | yes | 256/0/0 | yes |
| before | normal | 1048576 | 2048 | 5286 | 2706432 | 0 | 0 | 285492 | 5286 | yes | 256/0/0 | yes |
| before | reversed | 1048576 | 2048 | 5194 | 2659328 | 724911 | 145850304 | 283759 | 5194 | yes | 256/0/0 | yes |
| farafter | normal | 1048576 | 2048 | 5397 | 2763264 | 1823835 | 450571648 | 282637 | 5397 | yes | 256/0/0 | yes |
| farafter | reversed | 1048576 | 2048 | 5388 | 2758656 | 2453217 | 615926800 | 282937 | 5388 | yes | 256/0/0 | yes |
| farbefore | normal | 1048576 | 2048 | 5491 | 2811300 | 0 | 0 | 289489 | 5491 | yes | 256/0/0 | yes |
| farbefore | reversed | 1048576 | 2048 | 5486 | 2808832 | 42162 | 7338640 | 285565 | 5486 | yes | 256/0/0 | yes |
| sepff | normal | 1048576 | 2048 | 5448 | 2789376 | 0 | 0 | 284515 | 5448 | yes | 256/0/0 | yes |
| sepff | reversed | 1048576 | 2048 | 5456 | 2793472 | 7099 | 817616 | 286430 | 5456 | yes | 256/0/0 | yes |
| seppf | normal | 1048576 | 2048 | 5358 | 2743296 | 2038417 | 505527008 | 280418 | 5358 | yes | 256/0/0 | yes |
| seppf | reversed | 1048576 | 2048 | 5385 | 2757120 | 2588463 | 653964768 | 283853 | 5385 | yes | 256/0/0 | yes |
| after | normal | 1048576 | 16384 | 2644 | 169168 | 0 | 0 | 78743 | 2644 | yes | 256/0/0 | yes |
| after | reversed | 1048576 | 16384 | 2587 | 165568 | 125543 | 170133024 | 80019 | 2587 | yes | 256/0/0 | yes |
| before | normal | 1048576 | 16384 | 2641 | 168984 | 0 | 0 | 78711 | 2641 | yes | 256/0/0 | yes |
| before | reversed | 1048576 | 16384 | 2588 | 165632 | 123720 | 166002256 | 80776 | 2588 | yes | 256/0/0 | yes |
| farafter | normal | 1048576 | 16384 | 2646 | 169344 | 14883 | 8885648 | 78596 | 2646 | yes | 256/0/0 | yes |
| farafter | reversed | 1048576 | 16384 | 2620 | 167680 | 161234 | 287319200 | 81605 | 2620 | yes | 256/0/0 | yes |
| farbefore | normal | 1048576 | 16384 | 2653 | 169728 | 0 | 0 | 79510 | 2652 | yes | 256/0/0 | yes |
| farbefore | reversed | 1048576 | 16384 | 2623 | 167872 | 60042 | 58523744 | 82195 | 2623 | yes | 256/0/0 | yes |
| sepff | normal | 1048576 | 16384 | 2604 | 166592 | 0 | 0 | 77500 | 2603 | yes | 256/0/0 | yes |
| sepff | reversed | 1048576 | 16384 | 2579 | 165056 | 54282 | 51083632 | 80596 | 2579 | yes | 256/0/0 | yes |
| seppf | normal | 1048576 | 16384 | 2596 | 166144 | 19923 | 13819824 | 77380 | 2596 | yes | 256/0/0 | yes |
| seppf | reversed | 1048576 | 16384 | 2569 | 164416 | 160609 | 293455440 | 79743 | 2569 | yes | 256/0/0 | yes |
| after | normal | 16777216 | 2048 | 276 | 2256105 | 0 | 0 | 16398 | 276 | yes | 256/0/0 | yes |
| after | reversed | 16777216 | 2048 | 276 | 2255900 | 41009 | 8322016 | 16481 | 276 | yes | 256/0/0 | yes |
| before | normal | 16777216 | 2048 | 276 | 2255878 | 0 | 0 | 16372 | 276 | yes | 256/0/0 | yes |
| before | reversed | 16777216 | 2048 | 276 | 2255937 | 38934 | 7787904 | 16523 | 276 | yes | 256/0/0 | yes |
| farafter | normal | 16777216 | 2048 | 299 | 2449408 | 2298051 | 588015680 | 16651 | 299 | yes | 256/0/0 | yes |
| farafter | reversed | 16777216 | 2048 | 300 | 2457600 | 2318384 | 593260480 | 16794 | 300 | yes | 256/0/0 | yes |
| farbefore | normal | 16777216 | 2048 | 307 | 2506752 | 0 | 0 | 17252 | 306 | yes | 256/0/0 | yes |
| farbefore | reversed | 16777216 | 2048 | 306 | 2498560 | 2 | 96 | 17296 | 305 | yes | 256/0/0 | yes |
| sepff | normal | 16777216 | 2048 | 306 | 2498560 | 0 | 0 | 17164 | 305 | yes | 256/0/0 | yes |
| sepff | reversed | 16777216 | 2048 | 308 | 2514944 | 0 | 0 | 17352 | 307 | yes | 256/0/0 | yes |
| seppf | normal | 16777216 | 2048 | 295 | 2416640 | 2283419 | 584298640 | 16553 | 295 | yes | 256/0/0 | yes |
| seppf | reversed | 16777216 | 2048 | 301 | 2465792 | 2342766 | 599508768 | 16879 | 301 | yes | 256/0/0 | yes |
| after | normal | 16777216 | 16384 | 307 | 313583 | 0 | 0 | 58564 | 307 | yes | 256/0/0 | yes |
| after | reversed | 16777216 | 16384 | 304 | 310538 | 68109 | 78794224 | 59062 | 304 | yes | 256/0/0 | yes |
| before | normal | 16777216 | 16384 | 307 | 313622 | 0 | 0 | 58666 | 307 | yes | 256/0/0 | yes |
| before | reversed | 16777216 | 16384 | 304 | 311094 | 66078 | 74244672 | 59361 | 304 | yes | 256/0/0 | yes |
| farafter | normal | 16777216 | 16384 | 309 | 316329 | 281013 | 562956160 | 58993 | 309 | yes | 256/0/0 | yes |
| farafter | reversed | 16777216 | 16384 | 310 | 317398 | 295928 | 593483616 | 59659 | 310 | yes | 256/0/0 | yes |
| farbefore | normal | 16777216 | 16384 | 317 | 323519 | 0 | 0 | 60810 | 316 | yes | 256/0/0 | yes |
| farbefore | reversed | 16777216 | 16384 | 316 | 322527 | 763 | 607056 | 61079 | 315 | yes | 256/0/0 | yes |
| sepff | normal | 16777216 | 16384 | 317 | 323477 | 0 | 0 | 60810 | 316 | yes | 256/0/0 | yes |
| sepff | reversed | 16777216 | 16384 | 317 | 323584 | 342 | 170880 | 61278 | 316 | yes | 256/0/0 | yes |
| seppf | normal | 16777216 | 16384 | 309 | 316416 | 282909 | 566908608 | 59095 | 309 | yes | 256/0/0 | yes |
| seppf | reversed | 16777216 | 16384 | 311 | 318347 | 297867 | 598122848 | 59830 | 311 | yes | 256/0/0 | yes |

Per layout (normal writer): after=consistent, before=consistent, farafter=violates, farbefore=consistent, sepff=consistent, seppf=violates
Control seppf (payload copied before flags) violates: True
**A answer: fixable** (yes: one copy suffices for every flag placement; fixable: two-step copy, flags first)
Single-copy layouts that passed: after, before, farbefore. They are not relied on: the copy engine's read order is not documented, and another placement of the same data did not pass.
Sub-verdict, one copy with flags placed before their payload (before + farbefore; rests on the observed, undocumented ascending copy order): consistent. after and farafter read each flag after its payload, a reader-side race that no writer protocol fixes.

Sequence-cap path: 389 relaunches, 99584 cap exits, 0 violations, 0 quiet flag mismatches

| writer | bytes | lat p50 us | lat p99 us | lat max us | event p50 us | issue p50 us | GB/s | writer ran throughout |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| none | 65536 | 26.3 | 36.4 | 42.6 | 7.6 | 6.7 | 20.55 | - |
| none | 1048576 | 63.8 | 69.1 | 82.6 | 44.8 | 6.9 | 25.93 | - |
| none | 16777216 | 616.4 | 629.6 | 633.7 | 596.4 | 7.2 | 28.31 | - |
| running | 65536 | 82.2 | 93.5 | 94.8 | 31.0 | 7.0 | 3.36 | yes |
| running | 1048576 | 232.0 | 248.1 | 251.8 | 175.4 | 7.2 | 6.33 | yes |
| running | 16777216 | 2525.1 | 2592.3 | 2597.9 | 2466.2 | 8.0 | 6.83 | yes |

16 MiB copies with the writer running: 6.83 GB/s against the offered 0.9-6.2 GB/s.

## Part B0: copier next to a capture loop (captest)

| case | mode | iters | capture open | end errors (first) | kernel launch errors | graph launches | shim copies | copies during a capture | shim copy/wait errors (first) | shim counts match |
|---|---|---:|---:|---|---:|---:|---:|---:|---|---|
| noshim | relaxed/nonblocking | 4341 | 0.30 | 0 (0) | 0 | 4341 | - | - | - | - |
| idle-relaxed | relaxed/nonblocking | 4297 | 0.30 | 0 (0) | 0 | 4297 | 0 | 0 | 0 (0) | yes |
| async-relaxed | relaxed/nonblocking | 4226 | 0.31 | 0 (0) | 0 | 4226 | 57499 | 19089 | 0 (0) | yes |
| async-global | global/nonblocking | 4889 | 0.48 | 1771 (901) | 252815 | 3118 | 79796 | 68990 | 66180 (900) | yes |
| async-tlocal | thread_local/nonblocking | 4263 | 0.30 | 0 (0) | 0 | 4263 | 58103 | 19027 | 0 (0) | yes |
| asyncevent-relaxed | relaxed/nonblocking | 4274 | 0.30 | 0 (0) | 0 | 4274 | 43292 | 15092 | 0 (0) | yes |
| asyncevent-global | global/nonblocking | 8569 | 1.00 | 8569 (901) | 1512088 | 0 | 82539 | 76501 | 76500 (900) | yes |
| legacysync-relaxed | relaxed/nonblocking | 3687 | 0.32 | 0 (0) | 0 | 3687 | 55738 | 15385 | 0 (0) | yes |
| legacysync-relaxed-blk | relaxed/blocking | 4578 | 0.45 | 1209 (901) | 167354 | 3369 | 10446 | 2417 | 100001 (906) | yes |
| legacysync-global | global/nonblocking | 3921 | 0.34 | 0 (0) | 0 | 3921 | 55198 | 15679 | 0 (0) | yes |
| memalloc-relaxed | relaxed/nonblocking | 4256 | 0.30 | 0 (0) | 0 | 4256 | 24483 | 17012 | 0 (0) | yes |
| memalloc-global | global/nonblocking | 4113 | 0.66 | 2374 (901) | 358320 | 1739 | 72177 | 64671 | 62556 (900) | yes |
| ctxsync-relaxed | - | crashed or no output (rc 139) | | | | | | | | |
| ctxsync-global | global/nonblocking | 4955 | 0.46 | 1678 (901) | 293201 | 3277 | 70967 | 62745 | 60092 (900) | yes |

Interposer counts equal captest's own: True. Relaxed-mode control (legacy-stream copy, blocking capture stream) invalidates captures: True. Global-mode controls (memalloc or ctxsync): True. In global mode the async copier is safe: False.
**B0 verdict (async copier safe next to relaxed-mode captures): pass**

## Part B: copier shim inside llama-bench, graphs on

| config | test | tokens/s per round | mean | loss vs noshim |
|---|---|---|---:|---:|
| noshim | pp512 | 2487.60, 2498.15, 2495.25 | 2493.67 | noise 0.4% |
| noshim | tg128 | 67.58, 67.91, 67.83 | 67.77 | noise 0.5% |
| idle | pp512 | 2489.21, 2504.40, 2500.36 | 2497.99 | -0.2% |
| idle | tg128 | 67.47, 67.78, 67.74 | 67.66 | 0.2% |
| c1m1ms | pp512 | 2480.03, 2493.63, 2489.35 | 2487.67 | 0.2% |
| c1m1ms | tg128 | 67.30, 67.59, 67.48 | 67.46 | 0.5% |
| c16m1ms | pp512 | 2401.67, 2404.85, 2405.30 | 2403.94 | 3.6% |
| c16m1ms | tg128 | 62.45, 61.98, 62.06 | 62.16 | 8.3% |
| cont | pp512 | 2367.55, 2362.16, 2364.35 | 2364.69 | 5.2% |
| cont | tg128 | 59.66, 59.53, 59.47 | 59.55 | 12.1% |

Graph launches in round 1, tg128 (shim count): idle=768, c1m1ms=768, c16m1ms=768, cont=768
Graphs-off control: launches=0 captures=0 rc=0 -> ok
nsys cross-check (cudaGraphLaunch rows equal the shim's count): True

| tag | copies | during a capture | copies/s | GB/s | missed periods | lat p50/p99/max us | ctx wait s |
|---|---:|---:|---:|---:|---:|---|---:|
| r1-c16m1ms-pp512 | 940 | 4 | 781 | 13.111 | 260 | 656/2133/193183 | 0.083 |
| r1-c16m1ms-tg128 | 6426 | 4 | 989 | 16.598 | 65 | 693/788/24990 | 0.085 |
| r1-c1m1ms-pp512 | 909 | 2 | 789 | 0.827 | 240 | 67/1180/191487 | 0.083 |
| r1-c1m1ms-tg128 | 5971 | 3 | 991 | 1.039 | 53 | 71/90/23938 | 0.083 |
| r1-cont-pp512 | 1418 | 6 | 1148 | 19.262 | 0 | 651/1364/192625 | 0.084 |
| r1-cont-tg128 | 9664 | 6 | 1419 | 23.800 | 0 | 691/769/29765 | 0.082 |
| r2-c16m1ms-pp512 | 938 | 2 | 758 | 12.709 | 296 | 657/2254/193131 | 0.082 |
| r2-c16m1ms-tg128 | 6478 | 3 | 982 | 16.480 | 111 | 687/782/42700 | 0.085 |
| r2-c1m1ms-pp512 | 905 | 3 | 763 | 0.800 | 278 | 65/1426/191315 | 0.082 |
| r2-c1m1ms-tg128 | 5950 | 3 | 986 | 1.034 | 81 | 70/87/35800 | 0.088 |
| r2-cont-pp512 | 1418 | 6 | 1119 | 18.766 | 0 | 650/1519/193129 | 0.083 |
| r2-cont-tg128 | 9661 | 6 | 1408 | 23.630 | 0 | 694/764/42413 | 0.084 |
| r3-c16m1ms-pp512 | 939 | 3 | 740 | 12.412 | 326 | 655/2721/193235 | 0.082 |
| r3-c16m1ms-tg128 | 6468 | 4 | 981 | 16.456 | 122 | 690/783/49574 | 0.086 |
| r3-c1m1ms-pp512 | 909 | 3 | 743 | 0.779 | 311 | 65/1895/192171 | 0.083 |
| r3-c1m1ms-tg128 | 5963 | 2 | 982 | 1.030 | 108 | 70/91/47597 | 0.084 |
| r3-cont-pp512 | 1424 | 6 | 1094 | 18.362 | 0 | 649/1621/192818 | 0.084 |
| r3-cont-tg128 | 9660 | 6 | 1397 | 23.438 | 0 | 693/767/77499 | 0.087 |

**B: graphs keep working with the copier: yes; idle shim within noise: True; 1 MiB/1 ms loss bounded (<= 5.0%, both tests): yes.**

nsys llama-cont: kernels 54696, in graphs 54272 (99.2%); cudaGraphLaunch nsys 256 vs shim 256; shim copies seen 3798, copy time overlapping kernels 82.5%
nsys llama-idle: kernels 54696, in graphs 54272 (99.2%); cudaGraphLaunch nsys 256 vs shim 256; shim copies seen 0, copy time overlapping kernels -

## Part C: POSIX-FD VMM export to a second process

**fill-plain-rdma0**: cuInit=CUDA_SUCCESS, cuMemGetAllocationGranularity=CUDA_SUCCESS, cuMemCreate=CUDA_SUCCESS, cuMemAddressReserve=CUDA_SUCCESS, cuMemMap=CUDA_SUCCESS, cuMemSetAccess_device=CUDA_SUCCESS, cuMemExportToShareableHandle=CUDA_SUCCESS, cuMemUnmap=CUDA_SUCCESS, cuMemAddressFree=CUDA_SUCCESS, cuMemRelease=CUDA_SUCCESS, cuMemImportFromShareableHandle=CUDA_SUCCESS, cuMemGetAllocationPropertiesFromHandle=CUDA_SUCCESS, cuMemcpyDtoH_pageable=CUDA_SUCCESS, cuMemHostRegister_dst=CUDA_SUCCESS, cuMemAlloc_local=CUDA_SUCCESS, cuMemSetAccess_host=CUDA_ERROR_NOT_SUPPORTED, cuMemSetAccess_host_numa=CUDA_ERROR_NOT_SUPPORTED, cuPointerGetAttribute_HOST_POINTER=CUDA_ERROR_INVALID_VALUE

- fd: /dmabuf:; mmap before CUDA init: 1; updates by process 1: 1251; distinct generations seen: DtoH 1251, kernel 1250, CPU mapping 1250
- stale CPU-mapped words: gen 1 318208 (civac 317552); after the writer stopped 0/10/100/1000 ms: 113232/96720/96720/96720; after process 1 exited: 0; final DtoH stale 0
- DtoH bandwidth, imported 27.3616 GB/s vs local cuMemAlloc 27.3852 GB/s; fill us mean 343.392
- tier 1 (CPU mapping usable): False; tier 2 (import + reads in own context): True; both processes reported: True

**fill-wt-rdma0**: cuInit=CUDA_SUCCESS, cuMemGetAllocationGranularity=CUDA_SUCCESS, cuMemCreate=CUDA_SUCCESS, cuMemAddressReserve=CUDA_SUCCESS, cuMemMap=CUDA_SUCCESS, cuMemSetAccess_device=CUDA_SUCCESS, cuMemExportToShareableHandle=CUDA_SUCCESS, cuMemUnmap=CUDA_SUCCESS, cuMemAddressFree=CUDA_SUCCESS, cuMemRelease=CUDA_SUCCESS, cuMemImportFromShareableHandle=CUDA_SUCCESS, cuMemGetAllocationPropertiesFromHandle=CUDA_SUCCESS, cuMemcpyDtoH_pageable=CUDA_SUCCESS, cuMemHostRegister_dst=CUDA_SUCCESS, cuMemAlloc_local=CUDA_SUCCESS, cuMemSetAccess_host=CUDA_ERROR_NOT_SUPPORTED, cuMemSetAccess_host_numa=CUDA_ERROR_NOT_SUPPORTED, cuPointerGetAttribute_HOST_POINTER=CUDA_ERROR_INVALID_VALUE

- fd: /dmabuf:; mmap before CUDA init: 1; updates by process 1: 1252; distinct generations seen: DtoH 1253, kernel 1252, CPU mapping 1252
- stale CPU-mapped words: gen 1 369328 (civac 322800); after the writer stopped 0/10/100/1000 ms: 127280/103776/103776/103776; after process 1 exited: 0; final DtoH stale 0
- DtoH bandwidth, imported 27.3449 GB/s vs local cuMemAlloc 27.4249 GB/s; fill us mean 341.343
- tier 1 (CPU mapping usable): False; tier 2 (import + reads in own context): True; both processes reported: True

**fill-sysfence-rdma0**: cuInit=CUDA_SUCCESS, cuMemGetAllocationGranularity=CUDA_SUCCESS, cuMemCreate=CUDA_SUCCESS, cuMemAddressReserve=CUDA_SUCCESS, cuMemMap=CUDA_SUCCESS, cuMemSetAccess_device=CUDA_SUCCESS, cuMemExportToShareableHandle=CUDA_SUCCESS, cuMemUnmap=CUDA_SUCCESS, cuMemAddressFree=CUDA_SUCCESS, cuMemRelease=CUDA_SUCCESS, cuMemImportFromShareableHandle=CUDA_SUCCESS, cuMemGetAllocationPropertiesFromHandle=CUDA_SUCCESS, cuMemcpyDtoH_pageable=CUDA_SUCCESS, cuMemHostRegister_dst=CUDA_SUCCESS, cuMemAlloc_local=CUDA_SUCCESS, cuMemSetAccess_host=CUDA_ERROR_NOT_SUPPORTED, cuMemSetAccess_host_numa=CUDA_ERROR_NOT_SUPPORTED, cuPointerGetAttribute_HOST_POINTER=CUDA_ERROR_INVALID_VALUE

- fd: /dmabuf:; mmap before CUDA init: 1; updates by process 1: 1238; distinct generations seen: DtoH 1239, kernel 1238, CPU mapping 1238
- stale CPU-mapped words: gen 1 354432 (civac 307584); after the writer stopped 0/10/100/1000 ms: 125840/111392/111392/111392; after process 1 exited: 0; final DtoH stale 0
- DtoH bandwidth, imported 27.3081 GB/s vs local cuMemAlloc 27.3849 GB/s; fill us mean 368.927
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

