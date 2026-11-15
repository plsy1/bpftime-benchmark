# C3 summary (10 rounds; medians [min-max] over rounds)

Smoke or loaded-board numbers are not measurements; only a run on a quiet board counts.

**Run status: valid**

[projection] % of a pp512: 192.56M thread executions or 6.02M warp records over 204.6 ms of GPU time.

## Gate: comparable with step 0 (anchor, R = 1)

- F reg fk1: 0.428 ns (step 0 0.433, ratio 0.988, tolerance 10%) ok
- F dev fk1: 0.366 ns (step 0 0.369, ratio 0.991, tolerance 10%) ok
- st_lane32 dev fk0: 0.034 ns (step 0 0.032, ratio 1.058, tolerance 25%) ok
- st_lane32 dev fk1: 0.255 ns (step 0 0.259, ratio 0.983, tolerance 10%) ok
- loop / exit form F reg: 1.01
- loop / exit form F dev: 1.01
- loop / exit form N reg: 1.01 (sanity line, not in the gate)
- loop / exit form N dev: 1.04 (sanity line, not in the gate)
- **comparable with step 0**

## (a) Cost ladder, ns per thread execution at R = 1 [projection %] (slope R1->Rbig)

### anchor dev

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.3657 [0.3640-0.3674] | 34.4 | 0.2849 | 26.8 |
| N no fence | 0.1353 [0.1346-0.1367] | 12.7 | 0.1297 | 12.2 |
| IF inline+fence | 0.3316 [0.3309-0.3333] | 31.2 | 0.2997 | 28.2 |
| IC inline, helper fallback | 0.0906 [0.0904-0.0916] | 8.5 | 0.1007 | 9.5 |
| I inline | 0.0911 [0.0905-0.0922] | 8.6 | 0.0889 | 8.4 |
| K inline, no probe call | 0.0842 [0.0840-0.0852] | 7.9 | 0.0792 | 7.4 |
| M memop alone | 0.0555 [0.0554-0.0563] | 5.2 | 0.0429 | 4.0 |
| M64 memop u64 | 0.0469 [0.0467-0.0488] | 4.4 | 0.0332 | 3.1 |
| E empty probe | 0.0100 [0.0098-0.0103] | 0.9 | 0.0152 | 1.4 |
| 8-B store | 0.0339 [0.0333-0.0351] | 3.2 | 0.0852 | 8.0 |
| 8-B store + membar.sys | 0.2546 [0.2448-0.2558] | 24.0 | 0.2423 | 22.8 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.2304 [0.2293-0.2308] | 21.7 | True |
| fence on the inlined path (IF - I) | 0.2404 [0.2398-0.2422] | 22.6 | True |
| helper call + dispatch (N - I) | 0.0442 [0.0441-0.0446] | 4.2 | True |
| cold helper fallback present (IC - I) | -0.0006 [-0.0007-0.0001] | -0.1 | False |
| probe call (I - K) | 0.0069 [0.0057-0.0071] | 0.7 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0288 [0.0284-0.0294] | 2.7 | True |
| memory op (M) | 0.0555 [0.0554-0.0563] | 5.2 |  |
| u32 split (M - M64) | 0.0086 [0.0070-0.0095] | 0.8 | True |
| RMW over a plain store (M - 8-B store) | 0.0218 [0.0204-0.0224] | 2.1 | True |

### anchor reg

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.4278 [0.4263-0.4301] | 40.3 | 0.4038 | 38.0 |
| N no fence | 0.1919 [0.1917-0.1928] | 18.1 | 0.1296 | 12.2 |
| IF inline+fence | 0.4023 [0.3998-0.4061] | 37.9 | 0.3874 | 36.5 |
| IC inline, helper fallback | 0.1725 [0.1723-0.1731] | 16.2 | 0.1148 | 10.8 |
| I inline | 0.1735 [0.1732-0.1741] | 16.3 | 0.1142 | 10.7 |
| K inline, no probe call | 0.1704 [0.1702-0.1707] | 16.0 | 0.1072 | 10.1 |
| M memop alone | 0.1615 [0.1613-0.1619] | 15.2 | 0.0853 | 8.0 |
| M64 memop u64 | 0.1534 [0.1533-0.1547] | 14.4 | 0.0717 | 6.7 |
| E empty probe | 0.0100 [0.0098-0.0103] | 0.9 | 0.0152 | 1.4 |
| 8-B store | 0.0655 [0.0652-0.0679] | 6.2 | 0.0801 | 7.5 |
| 8-B store + membar.sys | 0.2568 [0.2539-0.2576] | 24.2 | 0.2405 | 22.6 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.2359 [0.2343-0.2377] | 22.2 | True |
| fence on the inlined path (IF - I) | 0.2290 [0.2265-0.2320] | 21.6 | True |
| helper call + dispatch (N - I) | 0.0185 [0.0180-0.0188] | 1.7 | True |
| cold helper fallback present (IC - I) | -0.0009 [-0.0014--0.0006] | -0.1 | True |
| probe call (I - K) | 0.0031 [0.0024-0.0035] | 0.3 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0089 [0.0085-0.0093] | 0.8 | True |
| memory op (M) | 0.1615 [0.1613-0.1619] | 15.2 |  |
| u32 split (M - M64) | 0.0081 [0.0067-0.0086] | 0.8 | True |
| RMW over a plain store (M - 8-B store) | 0.0960 [0.0934-0.0966] | 9.0 | True |

### large dev

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.5040 [0.5026-0.5066] | 47.4 | 0.3743 | 35.2 |
| N no fence | 0.1741 [0.1737-0.1758] | 16.4 | 0.1079 | 10.2 |
| IF inline+fence | 0.4849 [0.4816-0.4863] | 45.6 | 0.3346 | 31.5 |
| IC inline, helper fallback | 0.1557 [0.1557-0.1560] | 14.7 | 0.0621 | 5.8 |
| I inline | 0.1558 [0.1556-0.1561] | 14.7 | 0.0453 | 4.3 |
| K inline, no probe call | 0.1533 [0.1530-0.1534] | 14.4 | 0.0370 | 3.5 |
| M memop alone | 0.1460 [0.1458-0.1463] | 13.7 | 0.0044 | 0.4 |
| M64 memop u64 | 0.1460 [0.1458-0.1464] | 13.7 | 0.0018 | 0.2 |
| E empty probe | 0.0085 [0.0084-0.0085] | 0.8 | 0.0173 | 1.6 |
| 8-B store | 0.0605 [0.0603-0.0609] | 5.7 | 0.0800 | 7.5 |
| 8-B store + membar.sys | 0.2493 [0.2484-0.2505] | 23.5 | 0.2240 | 21.1 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.3298 [0.3286-0.3329] | 31.0 | True |
| fence on the inlined path (IF - I) | 0.3292 [0.3259-0.3302] | 31.0 | True |
| helper call + dispatch (N - I) | 0.0184 [0.0176-0.0199] | 1.7 | True |
| cold helper fallback present (IC - I) | -0.0000 [-0.0002-0.0001] | -0.0 | False |
| probe call (I - K) | 0.0025 [0.0024-0.0031] | 0.2 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0073 [0.0071-0.0076] | 0.7 | True |
| memory op (M) | 0.1460 [0.1458-0.1463] | 13.7 |  |
| u32 split (M - M64) | -0.0000 [-0.0004-0.0003] | -0.0 | False |
| RMW over a plain store (M - 8-B store) | 0.0855 [0.0851-0.0857] | 8.0 | True |

### large reg

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.4163 [0.4156-0.4185] | 39.2 | 0.3885 | 36.6 |
| N no fence | 0.1768 [0.1765-0.1773] | 16.6 | 0.1327 | 12.5 |
| IF inline+fence | 0.3935 [0.3928-0.3973] | 37.0 | 0.3684 | 34.7 |
| IC inline, helper fallback | 0.1611 [0.1608-0.1612] | 15.2 | 0.1088 | 10.2 |
| I inline | 0.1619 [0.1616-0.1621] | 15.2 | 0.1056 | 9.9 |
| K inline, no probe call | 0.1589 [0.1588-0.1594] | 15.0 | 0.1034 | 9.7 |
| M memop alone | 0.1535 [0.1532-0.1537] | 14.5 | 0.1007 | 9.5 |
| M64 memop u64 | 0.1452 [0.1450-0.1455] | 13.7 | 0.0847 | 8.0 |
| E empty probe | 0.0085 [0.0084-0.0085] | 0.8 | 0.0173 | 1.6 |
| 8-B store | 0.0603 [0.0601-0.0606] | 5.7 | 0.0802 | 7.5 |
| 8-B store + membar.sys | 0.2480 [0.2472-0.2496] | 23.3 | 0.2343 | 22.1 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.2395 [0.2389-0.2413] | 22.5 | True |
| fence on the inlined path (IF - I) | 0.2315 [0.2308-0.2354] | 21.8 | True |
| helper call + dispatch (N - I) | 0.0148 [0.0146-0.0155] | 1.4 | True |
| cold helper fallback present (IC - I) | -0.0009 [-0.0011--0.0004] | -0.1 | True |
| probe call (I - K) | 0.0029 [0.0027-0.0032] | 0.3 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0054 [0.0051-0.0059] | 0.5 | True |
| memory op (M) | 0.1535 [0.1532-0.1537] | 14.5 |  |
| u32 split (M - M64) | 0.0083 [0.0080-0.0086] | 0.8 | True |
| RMW over a plain store (M - 8-B store) | 0.0932 [0.0929-0.0934] | 8.8 | True |

### full dev

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.5366 [0.5361-0.5381] | 50.5 | 0.3006 | 28.3 |
| N no fence | 0.3144 [0.3137-0.3158] | 29.6 | 0.1385 | 13.0 |
| IF inline+fence | 0.4907 [0.4904-0.4920] | 46.2 | 0.3009 | 28.3 |
| IC inline, helper fallback | 0.2671 [0.2656-0.2681] | 25.1 | 0.1079 | 10.2 |
| I inline | 0.2676 [0.2670-0.2682] | 25.2 | 0.0956 | 9.0 |
| K inline, no probe call | 0.2611 [0.2534-0.2650] | 24.6 | 0.0854 | 8.0 |
| M memop alone | 0.1792 [0.1781-0.1808] | 16.9 | 0.0476 | 4.5 |
| M64 memop u64 | 0.1375 [0.1362-0.1382] | 12.9 | 0.0384 | 3.6 |
| E empty probe | 0.0071 [0.0065-0.0082] | 0.7 | 0.0172 | 1.6 |
| 8-B store | 0.1741 [0.1732-0.1752] | 16.4 | 0.0429 | 4.0 |
| 8-B store + membar.sys | 0.2729 [0.2646-0.2901] | 25.7 | 0.2234 | 21.0 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.2224 [0.2205-0.2244] | 20.9 | True |
| fence on the inlined path (IF - I) | 0.2234 [0.2225-0.2241] | 21.0 | True |
| helper call + dispatch (N - I) | 0.0469 [0.0461-0.0484] | 4.4 | True |
| cold helper fallback present (IC - I) | -0.0003 [-0.0018-0.0002] | -0.0 | False |
| probe call (I - K) | 0.0065 [0.0029-0.0139] | 0.6 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0825 [0.0746-0.0855] | 7.8 | True |
| memory op (M) | 0.1792 [0.1781-0.1808] | 16.9 |  |
| u32 split (M - M64) | 0.0418 [0.0404-0.0431] | 3.9 | True |
| RMW over a plain store (M - 8-B store) | 0.0052 [0.0041-0.0056] | 0.5 | True |

### full reg

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.6426 [0.6391-0.6472] | 60.5 | 0.4156 | 39.1 |
| N no fence | 0.4478 [0.4463-0.4494] | 42.1 | 0.1416 | 13.3 |
| IF inline+fence | 0.6101 [0.6059-0.6136] | 57.4 | 0.3972 | 37.4 |
| IC inline, helper fallback | 0.3912 [0.3906-0.3933] | 36.8 | 0.1290 | 12.1 |
| I inline | 0.3888 [0.3857-0.3917] | 36.6 | 0.1243 | 11.7 |
| K inline, no probe call | 0.3849 [0.3835-0.3863] | 36.2 | 0.1266 | 11.9 |
| M memop alone | 0.3028 [0.3011-0.3049] | 28.5 | 0.1409 | 13.3 |
| M64 memop u64 | 0.2999 [0.2984-0.3010] | 28.2 | 0.0764 | 7.2 |
| E empty probe | 0.0071 [0.0065-0.0082] | 0.7 | 0.0172 | 1.6 |
| 8-B store | 0.2147 [0.2034-0.2161] | 20.2 | 0.0784 | 7.4 |
| 8-B store + membar.sys | 0.3353 [0.3337-0.3374] | 31.6 | 0.2420 | 22.8 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.1956 [0.1897-0.1998] | 18.4 | True |
| fence on the inlined path (IF - I) | 0.2223 [0.2167-0.2247] | 20.9 | True |
| helper call + dispatch (N - I) | 0.0591 [0.0567-0.0619] | 5.6 | True |
| cold helper fallback present (IC - I) | 0.0024 [0.0012-0.0051] | 0.2 | True |
| probe call (I - K) | 0.0033 [0.0016-0.0061] | 0.3 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0821 [0.0806-0.0834] | 7.7 | True |
| memory op (M) | 0.3028 [0.3011-0.3049] | 28.5 |  |
| u32 split (M - M64) | 0.0032 [0.0014-0.0046] | 0.3 | True |
| RMW over a plain store (M - 8-B store) | 0.0880 [0.0862-0.0991] | 8.3 | True |

**Decision (a), anchor, device memory, R = 1:** raw-store band reachable only with the op reduced to its memory access: no probe call, no probe switch, eBPF-stack key, 3-D thread id or bounds checks (M 5.2%, K 7.9%)
- the fence-free helper path still projects >= 10% (12.7%)
- single-digit levels: IC, I, K, M
- secondary anchor reg: raw-store band not reachable even by the memory access alone (M 15.2%); the fence-free helper path still projects >= 10% (18.1%); the read-modify-write costs 9.0% more than the 8-B store
- secondary large dev: raw-store band not reachable even by the memory access alone (M 13.7%); the fence-free helper path still projects >= 10% (16.4%); the read-modify-write costs 8.0% more than the 8-B store
- secondary large reg: raw-store band not reachable even by the memory access alone (M 14.5%); the fence-free helper path still projects >= 10% (16.6%); the read-modify-write costs 8.8% more than the 8-B store
- secondary full dev: raw-store band not reachable even by the memory access alone (M 16.9%); the fence-free helper path still projects >= 10% (29.6%); the read-modify-write costs 0.5% more than the 8-B store
- secondary full reg: raw-store band not reachable even by the memory access alone (M 28.5%); the fence-free helper path still projects >= 10% (42.1%); the read-modify-write costs 8.3% more than the 8-B store

## (b) Slot addressing, device memory, ns per thread execution at R = 1

### anchor

| item | ns [min-max] | % | slope ns | regs |
|---|---:|---:|---:|---:|
| slot_e | 0.0598 [0.0594-0.0608] | 5.6 | 0.0728 | 19 |
| slot_ex | 0.1049 [0.1047-0.1071] | 9.9 | 0.0671 | 26 |
| slot_excg | 0.1111 [0.1110-0.1134] | 10.5 | 0.0948 | 26 |
| tid_e | 0.1484 [0.1481-0.1520] | 14.0 | 0.0593 | 20 |
| tid_ex | 0.1480 [0.1478-0.1484] | 13.9 | 0.0607 | 28 |
| tidw_e | 0.0586 [0.0584-0.0591] | 5.5 | 0.0719 | 14 |
| slot_cx | 0.4167 [0.4160-0.4202] | 39.2 | 0.4306 | 26 |
| slot_cxr | 0.3970 [0.3962-0.4000] | 37.4 | 0.4073 | 26 |

| term | ns [min-max] | % | resolved |
|---|---:|---:|---|
| special-register addressing (slot_e - tidw_e) | 0.0013 [0.0003-0.0021] | 0.1 | True |
| exit check (slot_ex - slot_e) | 0.0451 [0.0449-0.0477] | 4.2 | True |
| exit check from L2 (slot_excg - slot_e) | 0.0513 [0.0512-0.0540] | 4.8 | True |
| footprint: tid table vs folded (tid_e - tidw_e) | 0.0898 [0.0896-0.0934] | 8.4 | True |
| slot entry vs 8-B store (slot_e - st_lane32) | 0.0259 [0.0248-0.0267] | 2.4 | True |
| claim/release over tag store (slot_cx - slot_ex) | 0.3119 [0.3112-0.3135] | 29.4 | True |
| acquire/release ordering (slot_cx - slot_cxr) | 0.0199 [0.0176-0.0205] | 1.9 | True |

### large

| item | ns [min-max] | % | slope ns | regs |
|---|---:|---:|---:|---:|
| slot_e | 0.0537 [0.0531-0.0541] | 5.1 | 0.0738 | 19 |
| slot_ex | 0.0995 [0.0988-0.1002] | 9.4 | 0.0554 | 26 |
| slot_excg | 0.1039 [0.1032-0.1047] | 9.8 | 0.0791 | 26 |
| tid_e | 0.1405 [0.1404-0.1411] | 13.2 | 0.0002 | 20 |
| tid_ex | 0.1405 [0.1403-0.1411] | 13.2 | 0.0005 | 28 |
| tidw_e | 0.0529 [0.0527-0.0532] | 5.0 | 0.0710 | 14 |
| slot_cx | 0.4067 [0.4063-0.4096] | 38.3 | 0.4299 | 26 |
| slot_cxr | 0.3871 [0.3858-0.3902] | 36.4 | 0.4075 | 26 |

| term | ns [min-max] | % | resolved |
|---|---:|---:|---|
| special-register addressing (slot_e - tidw_e) | 0.0009 [0.0004-0.0012] | 0.1 | True |
| exit check (slot_ex - slot_e) | 0.0457 [0.0456-0.0460] | 4.3 | True |
| exit check from L2 (slot_excg - slot_e) | 0.0502 [0.0490-0.0506] | 4.7 | True |
| footprint: tid table vs folded (tid_e - tidw_e) | 0.0877 [0.0872-0.0882] | 8.3 | True |
| slot entry vs 8-B store (slot_e - st_lane32) | -0.0068 [-0.0074--0.0062] | -0.6 | True |
| claim/release over tag store (slot_cx - slot_ex) | 0.3074 [0.3069-0.3094] | 28.9 | True |
| acquire/release ordering (slot_cx - slot_cxr) | 0.0197 [0.0191-0.0206] | 1.9 | True |

### full

| item | ns [min-max] | % | slope ns | regs |
|---|---:|---:|---:|---:|
| slot_e | 0.2173 [0.2166-0.2191] | 20.4 | 0.0723 | 19 |
| slot_ex | 0.2622 [0.2614-0.2635] | 24.7 | 0.0740 | 26 |
| slot_excg | 0.2648 [0.2635-0.2660] | 24.9 | 0.0956 | 26 |
| tid_e | 0.2197 [0.2191-0.2215] | 20.7 | 0.0713 | 20 |
| tid_ex | 0.2199 [0.2190-0.2212] | 20.7 | 0.0742 | 28 |
| tidw_e | 0.1759 [0.1747-0.1772] | 16.6 | 0.0741 | 14 |
| slot_cx | 0.6161 [0.6129-0.6179] | 58.0 | 0.4342 | 26 |
| slot_cxr | 0.5894 [0.5813-0.6043] | 55.5 | 0.4110 | 26 |

| term | ns [min-max] | % | resolved |
|---|---:|---:|---|
| special-register addressing (slot_e - tidw_e) | 0.0415 [0.0408-0.0444] | 3.9 | True |
| exit check (slot_ex - slot_e) | 0.0446 [0.0430-0.0457] | 4.2 | True |
| exit check from L2 (slot_excg - slot_e) | 0.0473 [0.0448-0.0483] | 4.5 | True |
| footprint: tid table vs folded (tid_e - tidw_e) | 0.0439 [0.0430-0.0457] | 4.1 | True |
| slot entry vs 8-B store (slot_e - st_lane32) | 0.0436 [0.0424-0.0451] | 4.1 | True |
| claim/release over tag store (slot_cx - slot_ex) | 0.3540 [0.3515-0.3563] | 33.3 | True |
| acquire/release ordering (slot_cx - slot_cxr) | 0.0275 [0.0113-0.0329] | 2.6 | True |

**Decision (b):** {"control_badtag_detected_every_mismatch": true, "control_collide_counted_claim_failures": true, "special_register_addressing": "negligible (< 1%: 0.0013 ns, 0.12%)", "entry_plus_exit_protocol": "single digit (< 10%) (0.1049 ns, 9.9%)", "claim_protocol_cost": ">= 10% (0.4167 ns, 39.2%)", "timed_claim_failures": 0, "validation_misses": 0, "timed_misses": 0, "timed_out_of_range": 0, "finding_B5": "none counted"}

## (d) Fence kinds, ns per warp-op at R = 1

### anchor

| item | ns/warp [min-max] | fence part ns/warp | % per thread exec | % per warp record |
|---|---:|---:|---:|---:|
| fence_lane32 none membar.sys | 6.860 [6.843-6.920] |  | 20.2 | 20.2 |
| fence_lane32 none membar.gl | 1.001 [0.996-1.022] |  | 2.9 | 2.9 |
| fence_lane32 none fence.acq_rel.gpu | 1.002 [0.998-1.022] |  | 2.9 | 2.9 |
| fence_lane32 none fence.acq_rel.sys | 6.869 [6.852-6.930] |  | 20.2 | 20.2 |
| fence_lane1 none membar.sys | 6.750 [6.740-6.764] |  | 19.9 | 19.9 |
| fence_lane1 none membar.gl | 1.016 [1.013-1.041] |  | 3.0 | 3.0 |
| fence_lane1 none fence.acq_rel.gpu | 1.016 [1.011-1.032] |  | 3.0 | 3.0 |
| fence_lane1 none fence.acq_rel.sys | 6.744 [6.741-6.752] |  | 19.8 | 19.8 |
| st_lane32 dev none | 1.084 [1.067-1.124] |  | 3.2 | 3.2 |
| st_lane32 dev membar.sys | 8.146 [7.833-8.186] | 7.058 | 24.0 | 24.0 |
| st_lane32 dev membar.gl | 2.060 [2.037-2.093] | 0.981 | 6.1 | 6.1 |
| st_lane32 dev fence.acq_rel.gpu | 2.059 [2.050-2.098] | 0.978 | 6.1 | 6.1 |
| st_lane32 dev fence.acq_rel.sys | 8.142 [8.054-8.190] | 7.054 | 23.9 | 24.0 |
| st_lane32 reg none | 2.096 [2.087-2.173] |  | 6.2 | 6.2 |
| st_lane32 reg membar.sys | 8.216 [8.125-8.245] | 6.124 | 24.2 | 24.2 |
| st_lane32 reg membar.gl | 2.124 [2.118-2.142] | 0.032 | 6.2 | 6.3 |
| st_lane32 reg fence.acq_rel.gpu | 2.124 [2.118-2.133] | 0.030 | 6.2 | 6.2 |
| st_lane32 reg fence.acq_rel.sys | 8.223 [8.151-8.295] | 6.128 | 24.2 | 24.2 |
| st_lane1 dev none | 0.989 [0.979-1.003] |  | 2.9 | 2.9 |
| st_lane1 dev membar.sys | 7.198 [7.170-7.228] | 6.210 | 21.2 | 21.2 |
| st_lane1 dev membar.gl | 1.883 [1.877-1.912] | 0.896 | 5.5 | 5.5 |
| st_lane1 reg none | 0.992 [0.986-1.006] |  | 2.9 | 2.9 |
| st_lane1 reg membar.sys | 6.932 [6.909-6.958] | 5.934 | 20.4 | 20.4 |
| st_lane1 reg membar.gl | 1.884 [1.880-1.900] | 0.893 | 5.5 | 5.5 |

- after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.98, sys 7.06 ns per warp)
- after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.03, sys 6.12 ns per warp)
- lane 0 after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.90, sys 6.21 ns per warp)
- lane 0 after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.89, sys 5.93 ns per warp)
- alone, all lanes: membar.gl materially cheaper (>= 25%) (gl 1.00, sys 6.86 ns per warp)
- alone, lane 0: membar.gl materially cheaper (>= 25%) (gl 1.02, sys 6.75 ns per warp)

### large

| item | ns/warp [min-max] | fence part ns/warp | % per thread exec | % per warp record |
|---|---:|---:|---:|---:|
| fence_lane32 none membar.sys | 6.736 [6.719-6.774] |  | 19.8 | 19.8 |
| fence_lane32 none membar.gl | 0.951 [0.948-0.957] |  | 2.8 | 2.8 |
| fence_lane32 none fence.acq_rel.gpu | 0.951 [0.947-0.957] |  | 2.8 | 2.8 |
| fence_lane32 none fence.acq_rel.sys | 6.738 [6.729-6.785] |  | 19.8 | 19.8 |
| fence_lane1 none membar.sys | 6.503 [6.496-6.521] |  | 19.1 | 19.1 |
| fence_lane1 none membar.gl | 0.886 [0.880-0.894] |  | 2.6 | 2.6 |
| fence_lane1 none fence.acq_rel.gpu | 0.886 [0.882-0.892] |  | 2.6 | 2.6 |
| fence_lane1 none fence.acq_rel.sys | 6.502 [6.495-6.520] |  | 19.1 | 19.1 |
| st_lane32 dev none | 1.937 [1.930-1.948] |  | 5.7 | 5.7 |
| st_lane32 dev membar.sys | 7.978 [7.947-8.016] | 6.044 | 23.5 | 23.5 |
| st_lane32 dev membar.gl | 1.936 [1.930-1.961] | -0.000 | 5.7 | 5.7 |
| st_lane32 dev fence.acq_rel.gpu | 1.936 [1.932-1.941] | 0.000 | 5.7 | 5.7 |
| st_lane32 dev fence.acq_rel.sys | 7.992 [7.962-8.032] | 6.054 | 23.5 | 23.5 |
| st_lane32 reg none | 1.930 [1.924-1.939] |  | 5.7 | 5.7 |
| st_lane32 reg membar.sys | 7.935 [7.912-7.986] | 6.004 | 23.3 | 23.3 |
| st_lane32 reg membar.gl | 1.941 [1.936-1.957] | 0.012 | 5.7 | 5.7 |
| st_lane32 reg fence.acq_rel.gpu | 1.939 [1.934-1.941] | 0.010 | 5.7 | 5.7 |
| st_lane32 reg fence.acq_rel.sys | 7.924 [7.907-7.951] | 5.995 | 23.3 | 23.3 |
| st_lane1 dev none | 0.807 [0.802-0.814] |  | 2.4 | 2.4 |
| st_lane1 dev membar.sys | 6.668 [6.645-6.702] | 5.864 | 19.6 | 19.6 |
| st_lane1 dev membar.gl | 1.678 [1.662-1.690] | 0.870 | 4.9 | 4.9 |
| st_lane1 reg none | 0.807 [0.798-0.812] |  | 2.4 | 2.4 |
| st_lane1 reg membar.sys | 6.666 [6.643-6.690] | 5.863 | 19.6 | 19.6 |
| st_lane1 reg membar.gl | 1.677 [1.657-1.687] | 0.869 | 4.9 | 4.9 |

- after store, dev: membar.gl materially cheaper (>= 25%) (gl -0.00, sys 6.04 ns per warp)
- after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.01, sys 6.00 ns per warp)
- lane 0 after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.87, sys 5.86 ns per warp)
- lane 0 after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.87, sys 5.86 ns per warp)
- alone, all lanes: membar.gl materially cheaper (>= 25%) (gl 0.95, sys 6.74 ns per warp)
- alone, lane 0: membar.gl materially cheaper (>= 25%) (gl 0.89, sys 6.50 ns per warp)

### full

| item | ns/warp [min-max] | fence part ns/warp | % per thread exec | % per warp record |
|---|---:|---:|---:|---:|
| fence_lane32 none membar.sys | 8.465 [8.458-8.473] |  | 24.9 | 24.9 |
| fence_lane32 none membar.gl | 1.459 [1.428-1.472] |  | 4.3 | 4.3 |
| fence_lane32 none fence.acq_rel.gpu | 1.467 [1.422-1.492] |  | 4.3 | 4.3 |
| fence_lane32 none fence.acq_rel.sys | 8.473 [8.431-8.493] |  | 24.9 | 24.9 |
| fence_lane1 none membar.sys | 8.449 [8.431-8.487] |  | 24.9 | 24.9 |
| fence_lane1 none membar.gl | 1.518 [1.490-1.552] |  | 4.5 | 4.5 |
| fence_lane1 none fence.acq_rel.gpu | 1.514 [1.481-1.561] |  | 4.5 | 4.5 |
| fence_lane1 none fence.acq_rel.sys | 8.451 [8.437-8.453] |  | 24.9 | 24.9 |
| st_lane32 dev none | 5.573 [5.541-5.606] |  | 16.4 | 16.4 |
| st_lane32 dev membar.sys | 8.731 [8.466-9.283] | 3.147 | 25.7 | 25.7 |
| st_lane32 dev membar.gl | 5.613 [5.597-5.634] | 0.043 | 16.5 | 16.5 |
| st_lane32 dev fence.acq_rel.gpu | 5.623 [5.596-5.649] | 0.048 | 16.5 | 16.5 |
| st_lane32 dev fence.acq_rel.sys | 8.758 [8.532-8.958] | 3.179 | 25.8 | 25.8 |
| st_lane32 reg none | 6.869 [6.509-6.916] |  | 20.2 | 20.2 |
| st_lane32 reg membar.sys | 10.730 [10.678-10.796] | 3.846 | 31.6 | 31.6 |
| st_lane32 reg membar.gl | 6.357 [6.331-6.418] | -0.517 | 18.7 | 18.7 |
| st_lane32 reg fence.acq_rel.gpu | 6.344 [6.316-6.411] | -0.505 | 18.7 | 18.7 |
| st_lane32 reg fence.acq_rel.sys | 10.718 [10.548-10.756] | 3.845 | 31.5 | 31.5 |
| st_lane1 dev none | 5.532 [5.485-5.588] |  | 16.3 | 16.3 |
| st_lane1 dev membar.sys | 8.455 [8.442-8.524] | 2.932 | 24.9 | 24.9 |
| st_lane1 dev membar.gl | 5.610 [5.591-5.638] | 0.077 | 16.5 | 16.5 |
| st_lane1 reg none | 5.526 [5.440-5.615] |  | 16.3 | 16.3 |
| st_lane1 reg membar.sys | 9.162 [8.772-9.287] | 3.632 | 26.9 | 27.0 |
| st_lane1 reg membar.gl | 5.848 [5.823-5.903] | 0.313 | 17.2 | 17.2 |

- after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.04, sys 3.15 ns per warp)
- after store, reg: membar.gl materially cheaper (>= 25%) (gl -0.52, sys 3.85 ns per warp)
- lane 0 after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.08, sys 2.93 ns per warp)
- lane 0 after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.31, sys 3.63 ns per warp)
- alone, all lanes: membar.gl materially cheaper (>= 25%) (gl 1.46, sys 8.46 ns per warp)
- alone, lane 0: membar.gl materially cheaper (>= 25%) (gl 1.52, sys 8.45 ns per warp)

## Counters

- validation failures: 0; hold-capped rows excluded: 0
- timed launches: slot misses 0, slot out-of-range 0, claim failures 0, inline-path misses 0
