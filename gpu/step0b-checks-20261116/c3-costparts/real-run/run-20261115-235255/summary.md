# C3 summary (10 rounds; medians [min-max] over rounds)

Smoke or loaded-board numbers are not measurements; only a run on a quiet board counts.

**Run status: valid**

[projection] % of a pp512: 192.56M thread executions or 6.02M warp records over 204.6 ms of GPU time.

## Gate: comparable with step 0 (anchor, R = 1)

- F reg fk1: 0.432 ns (step 0 0.433, ratio 0.998, tolerance 10%) ok
- F dev fk1: 0.367 ns (step 0 0.369, ratio 0.995, tolerance 10%) ok
- st_lane32 dev fk0: 0.032 ns (step 0 0.032, ratio 0.989, tolerance 25%) ok
- st_lane32 dev fk1: 0.255 ns (step 0 0.259, ratio 0.985, tolerance 10%) ok
- loop / exit form F reg: 1.01
- loop / exit form F dev: 1.01
- loop / exit form N reg: 1.01 (sanity line, not in the gate)
- loop / exit form N dev: 1.04 (sanity line, not in the gate)
- **comparable with step 0**

## (a) Cost ladder, ns per thread execution at R = 1 [projection %] (slope R1->Rbig)

### anchor dev

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.3670 [0.3663-0.3681] | 34.5 | 0.2860 | 26.9 |
| N no fence | 0.1363 [0.1361-0.1365] | 12.8 | 0.1308 | 12.3 |
| IF inline+fence | 0.3330 [0.3325-0.3337] | 31.3 | 0.3003 | 28.3 |
| IC inline, helper fallback | 0.0916 [0.0914-0.0916] | 8.6 | 0.1014 | 9.5 |
| I inline | 0.0922 [0.0921-0.0924] | 8.7 | 0.0895 | 8.4 |
| K inline, no probe call | 0.0857 [0.0853-0.0858] | 8.1 | 0.0797 | 7.5 |
| M memop alone | 0.0556 [0.0553-0.0557] | 5.2 | 0.0433 | 4.1 |
| M64 memop u64 | 0.0479 [0.0477-0.0488] | 4.5 | 0.0334 | 3.1 |
| E empty probe | 0.0103 [0.0102-0.0103] | 1.0 | 0.0153 | 1.4 |
| 8-B store | 0.0316 [0.0311-0.0338] | 3.0 | 0.0856 | 8.1 |
| 8-B store + membar.sys | 0.2552 [0.2458-0.2560] | 24.0 | 0.2452 | 23.1 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.2306 [0.2300-0.2317] | 21.7 | True |
| fence on the inlined path (IF - I) | 0.2409 [0.2402-0.2414] | 22.7 | True |
| helper call + dispatch (N - I) | 0.0441 [0.0439-0.0443] | 4.2 | True |
| cold helper fallback present (IC - I) | -0.0007 [-0.0009--0.0005] | -0.1 | True |
| probe call (I - K) | 0.0066 [0.0064-0.0068] | 0.6 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0301 [0.0298-0.0303] | 2.8 | True |
| memory op (M) | 0.0556 [0.0553-0.0557] | 5.2 |  |
| u32 split (M - M64) | 0.0077 [0.0068-0.0080] | 0.7 | True |
| RMW over a plain store (M - 8-B store) | 0.0239 [0.0218-0.0243] | 2.2 | True |

### anchor reg

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.4320 [0.4308-0.4324] | 40.7 | 0.4081 | 38.4 |
| N no fence | 0.1942 [0.1940-0.1946] | 18.3 | 0.1306 | 12.3 |
| IF inline+fence | 0.4086 [0.4071-0.4102] | 38.5 | 0.3918 | 36.9 |
| IC inline, helper fallback | 0.1742 [0.1737-0.1748] | 16.4 | 0.1155 | 10.9 |
| I inline | 0.1750 [0.1747-0.1756] | 16.5 | 0.1149 | 10.8 |
| K inline, no probe call | 0.1719 [0.1715-0.1724] | 16.2 | 0.1081 | 10.2 |
| M memop alone | 0.1621 [0.1618-0.1784] | 15.3 | 0.0868 | 8.2 |
| M64 memop u64 | 0.1539 [0.1536-0.1546] | 14.5 | 0.0728 | 6.9 |
| E empty probe | 0.0103 [0.0102-0.0103] | 1.0 | 0.0153 | 1.4 |
| 8-B store | 0.0649 [0.0647-0.0655] | 6.1 | 0.0801 | 7.5 |
| 8-B store + membar.sys | 0.2588 [0.2576-0.2592] | 24.4 | 0.2437 | 22.9 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.2378 [0.2365-0.2382] | 22.4 | True |
| fence on the inlined path (IF - I) | 0.2335 [0.2322-0.2351] | 22.0 | True |
| helper call + dispatch (N - I) | 0.0191 [0.0184-0.0197] | 1.8 | True |
| cold helper fallback present (IC - I) | -0.0008 [-0.0014--0.0004] | -0.1 | True |
| probe call (I - K) | 0.0033 [0.0027-0.0041] | 0.3 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0096 [-0.0064-0.0101] | 0.9 | False |
| memory op (M) | 0.1621 [0.1618-0.1784] | 15.3 |  |
| u32 split (M - M64) | 0.0082 [0.0075-0.0246] | 0.8 | True |
| RMW over a plain store (M - 8-B store) | 0.0973 [0.0966-0.1136] | 9.2 | True |

### large dev

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.5063 [0.5054-0.5132] | 47.7 | 0.3769 | 35.5 |
| N no fence | 0.1753 [0.1750-0.1754] | 16.5 | 0.1091 | 10.3 |
| IF inline+fence | 0.4882 [0.4863-0.4931] | 45.9 | 0.3358 | 31.6 |
| IC inline, helper fallback | 0.1565 [0.1563-0.1570] | 14.7 | 0.0635 | 6.0 |
| I inline | 0.1574 [0.1574-0.1577] | 14.8 | 0.0451 | 4.2 |
| K inline, no probe call | 0.1549 [0.1548-0.1552] | 14.6 | 0.0369 | 3.5 |
| M memop alone | 0.1462 [0.1462-0.1545] | 13.8 | 0.0043 | 0.4 |
| M64 memop u64 | 0.1462 [0.1461-0.1470] | 13.8 | 0.0016 | 0.1 |
| E empty probe | 0.0085 [0.0085-0.0085] | 0.8 | 0.0175 | 1.6 |
| 8-B store | 0.0604 [0.0603-0.0659] | 5.7 | 0.0801 | 7.5 |
| 8-B store + membar.sys | 0.2509 [0.2494-0.2514] | 23.6 | 0.2245 | 21.1 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.3312 [0.3302-0.3378] | 31.2 | True |
| fence on the inlined path (IF - I) | 0.3308 [0.3286-0.3356] | 31.1 | True |
| helper call + dispatch (N - I) | 0.0178 [0.0176-0.0180] | 1.7 | True |
| cold helper fallback present (IC - I) | -0.0010 [-0.0011--0.0004] | -0.1 | True |
| probe call (I - K) | 0.0026 [0.0025-0.0027] | 0.2 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0086 [0.0004-0.0090] | 0.8 | True |
| memory op (M) | 0.1462 [0.1462-0.1545] | 13.8 |  |
| u32 split (M - M64) | 0.0001 [-0.0006-0.0083] | 0.0 | False |
| RMW over a plain store (M - 8-B store) | 0.0858 [0.0803-0.0942] | 8.1 | True |

### large reg

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.4169 [0.4162-0.4183] | 39.2 | 0.3898 | 36.7 |
| N no fence | 0.1775 [0.1772-0.1794] | 16.7 | 0.1338 | 12.6 |
| IF inline+fence | 0.3979 [0.3965-0.3988] | 37.4 | 0.3656 | 34.4 |
| IC inline, helper fallback | 0.1619 [0.1616-0.1644] | 15.2 | 0.1094 | 10.3 |
| I inline | 0.1624 [0.1623-0.1627] | 15.3 | 0.1061 | 10.0 |
| K inline, no probe call | 0.1597 [0.1594-0.1611] | 15.0 | 0.1035 | 9.7 |
| M memop alone | 0.1541 [0.1539-0.1789] | 14.5 | 0.1002 | 9.4 |
| M64 memop u64 | 0.1452 [0.1451-0.1458] | 13.7 | 0.0838 | 7.9 |
| E empty probe | 0.0085 [0.0085-0.0085] | 0.8 | 0.0175 | 1.6 |
| 8-B store | 0.0602 [0.0601-0.0634] | 5.7 | 0.0801 | 7.5 |
| 8-B store + membar.sys | 0.2467 [0.2461-0.2474] | 23.2 | 0.2302 | 21.7 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.2393 [0.2385-0.2408] | 22.5 | True |
| fence on the inlined path (IF - I) | 0.2354 [0.2338-0.2365] | 22.2 | True |
| helper call + dispatch (N - I) | 0.0151 [0.0148-0.0171] | 1.4 | True |
| cold helper fallback present (IC - I) | -0.0006 [-0.0011-0.0020] | -0.1 | False |
| probe call (I - K) | 0.0028 [0.0012-0.0029] | 0.3 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0056 [-0.0193-0.0070] | 0.5 | False |
| memory op (M) | 0.1541 [0.1539-0.1789] | 14.5 |  |
| u32 split (M - M64) | 0.0090 [0.0086-0.0332] | 0.8 | True |
| RMW over a plain store (M - 8-B store) | 0.0939 [0.0907-0.1188] | 8.8 | True |

### full dev

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.5385 [0.5370-0.5399] | 50.7 | 0.3010 | 28.3 |
| N no fence | 0.3142 [0.3136-0.3146] | 29.6 | 0.1397 | 13.1 |
| IF inline+fence | 0.4910 [0.4905-0.4924] | 46.2 | 0.3016 | 28.4 |
| IC inline, helper fallback | 0.2684 [0.2679-0.2689] | 25.3 | 0.1096 | 10.3 |
| I inline | 0.2688 [0.2672-0.2693] | 25.3 | 0.0973 | 9.2 |
| K inline, no probe call | 0.2662 [0.2654-0.2672] | 25.1 | 0.0861 | 8.1 |
| M memop alone | 0.1801 [0.1793-0.1809] | 16.9 | 0.0476 | 4.5 |
| M64 memop u64 | 0.1375 [0.1368-0.1421] | 12.9 | 0.0384 | 3.6 |
| E empty probe | 0.0080 [0.0068-0.0087] | 0.8 | 0.0172 | 1.6 |
| 8-B store | 0.1747 [0.1739-0.1756] | 16.4 | 0.0378 | 3.6 |
| 8-B store + membar.sys | 0.2667 [0.2652-0.2709] | 25.1 | 0.2192 | 20.6 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.2244 [0.2225-0.2263] | 21.1 | True |
| fence on the inlined path (IF - I) | 0.2225 [0.2217-0.2233] | 20.9 | True |
| helper call + dispatch (N - I) | 0.0454 [0.0446-0.0475] | 4.3 | True |
| cold helper fallback present (IC - I) | -0.0004 [-0.0008-0.0008] | -0.0 | False |
| probe call (I - K) | 0.0026 [0.0010-0.0037] | 0.2 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0861 [0.0850-0.0870] | 8.1 | True |
| memory op (M) | 0.1801 [0.1793-0.1809] | 16.9 |  |
| u32 split (M - M64) | 0.0426 [0.0382-0.0428] | 4.0 | True |
| RMW over a plain store (M - 8-B store) | 0.0051 [0.0040-0.0070] | 0.5 | True |

### full reg

| level | ns [min-max] | % | slope ns | slope % |
|---|---:|---:|---:|---:|
| F today | 0.6531 [0.6519-0.6562] | 61.5 | 0.4216 | 39.7 |
| N no fence | 0.4582 [0.4561-0.4601] | 43.1 | 0.1425 | 13.4 |
| IF inline+fence | 0.6104 [0.6075-0.6128] | 57.4 | 0.4125 | 38.8 |
| IC inline, helper fallback | 0.3958 [0.3942-0.3986] | 37.3 | 0.1292 | 12.2 |
| I inline | 0.3932 [0.3910-0.3960] | 37.0 | 0.1258 | 11.8 |
| K inline, no probe call | 0.3862 [0.3844-0.3878] | 36.4 | 0.1290 | 12.1 |
| M memop alone | 0.3005 [0.2983-0.3015] | 28.3 | 0.1408 | 13.2 |
| M64 memop u64 | 0.3014 [0.2997-0.3029] | 28.4 | 0.0781 | 7.4 |
| E empty probe | 0.0080 [0.0068-0.0087] | 0.8 | 0.0172 | 1.6 |
| 8-B store | 0.2186 [0.2114-0.2189] | 20.6 | 0.0781 | 7.4 |
| 8-B store + membar.sys | 0.3426 [0.3279-0.3442] | 32.2 | 0.2435 | 22.9 |

| term (paired per round) | ns [min-max] | % | resolved |
|---|---:|---:|---|
| fence (F - N) | 0.1959 [0.1926-0.1972] | 18.4 | True |
| fence on the inlined path (IF - I) | 0.2174 [0.2134-0.2208] | 20.5 | True |
| helper call + dispatch (N - I) | 0.0656 [0.0620-0.0686] | 6.2 | True |
| cold helper fallback present (IC - I) | 0.0029 [-0.0000-0.0065] | 0.3 | False |
| probe call (I - K) | 0.0069 [0.0033-0.0117] | 0.6 | True |
| lookup inline: switch, eBPF stack key, 3-D id, bounds, address (K - M) | 0.0853 [0.0837-0.0886] | 8.0 | True |
| memory op (M) | 0.3005 [0.2983-0.3015] | 28.3 |  |
| u32 split (M - M64) | -0.0012 [-0.0037-0.0012] | -0.1 | False |
| RMW over a plain store (M - 8-B store) | 0.0817 [0.0802-0.0901] | 7.7 | True |

**Decision (a), anchor, device memory, R = 1:** raw-store band reachable only with the op reduced to its memory access: no probe call, no probe switch, eBPF-stack key, 3-D thread id or bounds checks (M 5.2%, K 8.1%)
- the fence-free helper path still projects >= 10% (12.8%)
- single-digit levels: IC, I, K, M
- secondary anchor reg: raw-store band not reachable even by the memory access alone (M 15.3%); the fence-free helper path still projects >= 10% (18.3%); the read-modify-write costs 9.2% more than the 8-B store
- secondary large dev: raw-store band not reachable even by the memory access alone (M 13.8%); the fence-free helper path still projects >= 10% (16.5%); the read-modify-write costs 8.1% more than the 8-B store
- secondary large reg: raw-store band not reachable even by the memory access alone (M 14.5%); the fence-free helper path still projects >= 10% (16.7%); the read-modify-write costs 8.8% more than the 8-B store
- secondary full dev: raw-store band not reachable even by the memory access alone (M 16.9%); the fence-free helper path still projects >= 10% (29.6%); the read-modify-write costs 0.5% more than the 8-B store
- secondary full reg: raw-store band not reachable even by the memory access alone (M 28.3%); the fence-free helper path still projects >= 10% (43.1%); the read-modify-write costs 7.7% more than the 8-B store

## (b) Slot addressing, device memory, ns per thread execution at R = 1

### anchor

| item | ns [min-max] | % | slope ns | regs |
|---|---:|---:|---:|---:|
| slot_e | 0.0606 [0.0605-0.0607] | 5.7 | 0.0734 | 19 |
| slot_ex | 0.1067 [0.1066-0.1068] | 10.0 | 0.0676 | 26 |
| slot_excg | 0.1129 [0.1128-0.1130] | 10.6 | 0.0954 | 26 |
| tid_e | 0.1461 [0.1459-0.1472] | 13.7 | 0.0600 | 20 |
| tid_ex | 0.1460 [0.1457-0.1463] | 13.7 | 0.0614 | 28 |
| tidw_e | 0.0587 [0.0585-0.0587] | 5.5 | 0.0724 | 14 |
| slot_cx | 0.4198 [0.4194-0.4201] | 39.5 | 0.4333 | 26 |
| slot_cxr | 0.3998 [0.3993-0.4004] | 37.6 | 0.4104 | 26 |

| term | ns [min-max] | % | resolved |
|---|---:|---:|---|
| special-register addressing (slot_e - tidw_e) | 0.0020 [0.0019-0.0020] | 0.2 | True |
| exit check (slot_ex - slot_e) | 0.0460 [0.0460-0.0461] | 4.3 | True |
| exit check from L2 (slot_excg - slot_e) | 0.0523 [0.0522-0.0524] | 4.9 | True |
| footprint: tid table vs folded (tid_e - tidw_e) | 0.0874 [0.0871-0.0886] | 8.2 | True |
| slot entry vs 8-B store (slot_e - st_lane32) | 0.0290 [0.0267-0.0295] | 2.7 | True |
| claim/release over tag store (slot_cx - slot_ex) | 0.3131 [0.3128-0.3135] | 29.5 | True |
| acquire/release ordering (slot_cx - slot_cxr) | 0.0199 [0.0193-0.0204] | 1.9 | True |

### large

| item | ns [min-max] | % | slope ns | regs |
|---|---:|---:|---:|---:|
| slot_e | 0.0541 [0.0541-0.0542] | 5.1 | 0.0743 | 19 |
| slot_ex | 0.1002 [0.1001-0.1003] | 9.4 | 0.0557 | 26 |
| slot_excg | 0.1047 [0.1047-0.1048] | 9.9 | 0.0797 | 26 |
| tid_e | 0.1406 [0.1404-0.1423] | 13.2 | 0.0002 | 20 |
| tid_ex | 0.1405 [0.1404-0.1672] | 13.2 | 0.0003 | 28 |
| tidw_e | 0.0532 [0.0532-0.0533] | 5.0 | 0.0715 | 14 |
| slot_cx | 0.4095 [0.4092-0.4100] | 38.5 | 0.4328 | 26 |
| slot_cxr | 0.3899 [0.3896-0.3902] | 36.7 | 0.4100 | 26 |

| term | ns [min-max] | % | resolved |
|---|---:|---:|---|
| special-register addressing (slot_e - tidw_e) | 0.0009 [0.0008-0.0010] | 0.1 | True |
| exit check (slot_ex - slot_e) | 0.0460 [0.0459-0.0461] | 4.3 | True |
| exit check from L2 (slot_excg - slot_e) | 0.0506 [0.0505-0.0507] | 4.8 | True |
| footprint: tid table vs folded (tid_e - tidw_e) | 0.0873 [0.0872-0.0890] | 8.2 | True |
| slot entry vs 8-B store (slot_e - st_lane32) | -0.0063 [-0.0118--0.0061] | -0.6 | True |
| claim/release over tag store (slot_cx - slot_ex) | 0.3094 [0.3090-0.3098] | 29.1 | True |
| acquire/release ordering (slot_cx - slot_cxr) | 0.0195 [0.0193-0.0203] | 1.8 | True |

### full

| item | ns [min-max] | % | slope ns | regs |
|---|---:|---:|---:|---:|
| slot_e | 0.2182 [0.2165-0.2189] | 20.5 | 0.0737 | 19 |
| slot_ex | 0.2632 [0.2627-0.2635] | 24.8 | 0.0744 | 26 |
| slot_excg | 0.2737 [0.2683-0.2787] | 25.8 | 0.0964 | 26 |
| tid_e | 0.2198 [0.2188-0.2249] | 20.7 | 0.0716 | 20 |
| tid_ex | 0.2198 [0.2194-0.2229] | 20.7 | 0.0745 | 28 |
| tidw_e | 0.1764 [0.1755-0.1780] | 16.6 | 0.0741 | 14 |
| slot_cx | 0.6182 [0.6173-0.6206] | 58.2 | 0.4374 | 26 |
| slot_cxr | 0.5834 [0.5806-0.5912] | 54.9 | 0.4139 | 26 |

| term | ns [min-max] | % | resolved |
|---|---:|---:|---|
| special-register addressing (slot_e - tidw_e) | 0.0419 [0.0409-0.0422] | 3.9 | True |
| exit check (slot_ex - slot_e) | 0.0448 [0.0442-0.0467] | 4.2 | True |
| exit check from L2 (slot_excg - slot_e) | 0.0555 [0.0497-0.0610] | 5.2 | True |
| footprint: tid table vs folded (tid_e - tidw_e) | 0.0434 [0.0421-0.0483] | 4.1 | True |
| slot entry vs 8-B store (slot_e - st_lane32) | 0.0434 [0.0412-0.0450] | 4.1 | True |
| claim/release over tag store (slot_cx - slot_ex) | 0.3549 [0.3538-0.3571] | 33.4 | True |
| acquire/release ordering (slot_cx - slot_cxr) | 0.0341 [0.0260-0.0383] | 3.2 | True |

**Decision (b):** {"control_badtag_detected_every_mismatch": true, "control_collide_counted_claim_failures": true, "special_register_addressing": "negligible (< 1%: 0.0020 ns, 0.19%)", "entry_plus_exit_protocol": ">= 10% (0.1067 ns, 10.0%)", "claim_protocol_cost": ">= 10% (0.4198 ns, 39.5%)", "timed_claim_failures": 0, "validation_misses": 0, "timed_misses": 0, "timed_out_of_range": 0, "finding_B5": "none counted"}

## (d) Fence kinds, ns per warp-op at R = 1

### anchor

| item | ns/warp [min-max] | fence part ns/warp | % per thread exec | % per warp record |
|---|---:|---:|---:|---:|
| fence_lane32 none membar.sys | 6.911 [6.902-6.928] |  | 20.3 | 20.3 |
| fence_lane32 none membar.gl | 1.006 [1.004-1.010] |  | 3.0 | 3.0 |
| fence_lane32 none fence.acq_rel.gpu | 1.007 [1.004-1.010] |  | 3.0 | 3.0 |
| fence_lane32 none fence.acq_rel.sys | 6.910 [6.898-6.921] |  | 20.3 | 20.3 |
| fence_lane1 none membar.sys | 6.755 [6.749-6.764] |  | 19.9 | 19.9 |
| fence_lane1 none membar.gl | 1.022 [1.019-1.029] |  | 3.0 | 3.0 |
| fence_lane1 none fence.acq_rel.gpu | 1.024 [1.019-1.027] |  | 3.0 | 3.0 |
| fence_lane1 none fence.acq_rel.sys | 6.751 [6.749-6.759] |  | 19.9 | 19.9 |
| st_lane32 dev none | 1.013 [0.997-1.082] |  | 3.0 | 3.0 |
| st_lane32 dev membar.sys | 8.167 [7.865-8.193] | 7.143 | 24.0 | 24.0 |
| st_lane32 dev membar.gl | 2.091 [2.062-2.097] | 1.074 | 6.2 | 6.2 |
| st_lane32 dev fence.acq_rel.gpu | 2.091 [2.061-2.100] | 1.080 | 6.2 | 6.2 |
| st_lane32 dev fence.acq_rel.sys | 8.154 [8.037-8.169] | 7.120 | 24.0 | 24.0 |
| st_lane32 reg none | 2.077 [2.071-2.095] |  | 6.1 | 6.1 |
| st_lane32 reg membar.sys | 8.283 [8.244-8.296] | 6.207 | 24.4 | 24.4 |
| st_lane32 reg membar.gl | 2.129 [2.125-2.134] | 0.052 | 6.3 | 6.3 |
| st_lane32 reg fence.acq_rel.gpu | 2.130 [2.126-2.153] | 0.053 | 6.3 | 6.3 |
| st_lane32 reg fence.acq_rel.sys | 8.281 [8.251-8.313] | 6.203 | 24.4 | 24.4 |
| st_lane1 dev none | 1.007 [1.004-1.010] |  | 3.0 | 3.0 |
| st_lane1 dev membar.sys | 7.275 [7.259-7.335] | 6.269 | 21.4 | 21.4 |
| st_lane1 dev membar.gl | 1.887 [1.883-1.890] | 0.880 | 5.6 | 5.6 |
| st_lane1 reg none | 0.996 [0.991-1.000] |  | 2.9 | 2.9 |
| st_lane1 reg membar.sys | 6.929 [6.905-6.946] | 5.932 | 20.4 | 20.4 |
| st_lane1 reg membar.gl | 1.898 [1.895-1.906] | 0.904 | 5.6 | 5.6 |

- after store, dev: membar.gl materially cheaper (>= 25%) (gl 1.07, sys 7.14 ns per warp)
- after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.05, sys 6.21 ns per warp)
- lane 0 after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.88, sys 6.27 ns per warp)
- lane 0 after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.90, sys 5.93 ns per warp)
- alone, all lanes: membar.gl materially cheaper (>= 25%) (gl 1.01, sys 6.91 ns per warp)
- alone, lane 0: membar.gl materially cheaper (>= 25%) (gl 1.02, sys 6.75 ns per warp)

### large

| item | ns/warp [min-max] | fence part ns/warp | % per thread exec | % per warp record |
|---|---:|---:|---:|---:|
| fence_lane32 none membar.sys | 6.779 [6.774-6.792] |  | 19.9 | 19.9 |
| fence_lane32 none membar.gl | 0.957 [0.955-0.958] |  | 2.8 | 2.8 |
| fence_lane32 none fence.acq_rel.gpu | 0.958 [0.956-0.959] |  | 2.8 | 2.8 |
| fence_lane32 none fence.acq_rel.sys | 6.783 [6.772-6.796] |  | 20.0 | 20.0 |
| fence_lane1 none membar.sys | 6.520 [6.512-6.526] |  | 19.2 | 19.2 |
| fence_lane1 none membar.gl | 0.891 [0.885-0.899] |  | 2.6 | 2.6 |
| fence_lane1 none fence.acq_rel.gpu | 0.892 [0.888-0.897] |  | 2.6 | 2.6 |
| fence_lane1 none fence.acq_rel.sys | 6.520 [6.510-6.527] |  | 19.2 | 19.2 |
| st_lane32 dev none | 1.933 [1.931-2.110] |  | 5.7 | 5.7 |
| st_lane32 dev membar.sys | 8.028 [7.980-8.045] | 6.089 | 23.6 | 23.6 |
| st_lane32 dev membar.gl | 1.936 [1.931-1.951] | 0.003 | 5.7 | 5.7 |
| st_lane32 dev fence.acq_rel.gpu | 1.937 [1.931-2.005] | 0.002 | 5.7 | 5.7 |
| st_lane32 dev fence.acq_rel.sys | 8.013 [7.976-8.042] | 6.078 | 23.6 | 23.6 |
| st_lane32 reg none | 1.926 [1.924-2.028] |  | 5.7 | 5.7 |
| st_lane32 reg membar.sys | 7.895 [7.876-7.917] | 5.968 | 23.2 | 23.2 |
| st_lane32 reg membar.gl | 1.934 [1.933-1.958] | 0.009 | 5.7 | 5.7 |
| st_lane32 reg fence.acq_rel.gpu | 1.935 [1.932-1.949] | 0.008 | 5.7 | 5.7 |
| st_lane32 reg fence.acq_rel.sys | 7.900 [7.874-7.954] | 5.970 | 23.2 | 23.2 |
| st_lane1 dev none | 0.813 [0.812-0.814] |  | 2.4 | 2.4 |
| st_lane1 dev membar.sys | 6.695 [6.684-6.721] | 5.882 | 19.7 | 19.7 |
| st_lane1 dev membar.gl | 1.690 [1.688-1.692] | 0.876 | 5.0 | 5.0 |
| st_lane1 reg none | 0.813 [0.812-0.814] |  | 2.4 | 2.4 |
| st_lane1 reg membar.sys | 6.668 [6.651-6.699] | 5.854 | 19.6 | 19.6 |
| st_lane1 reg membar.gl | 1.688 [1.687-1.690] | 0.875 | 5.0 | 5.0 |

- after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.00, sys 6.09 ns per warp)
- after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.01, sys 5.97 ns per warp)
- lane 0 after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.88, sys 5.88 ns per warp)
- lane 0 after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.88, sys 5.85 ns per warp)
- alone, all lanes: membar.gl materially cheaper (>= 25%) (gl 0.96, sys 6.78 ns per warp)
- alone, lane 0: membar.gl materially cheaper (>= 25%) (gl 0.89, sys 6.52 ns per warp)

### full

| item | ns/warp [min-max] | fence part ns/warp | % per thread exec | % per warp record |
|---|---:|---:|---:|---:|
| fence_lane32 none membar.sys | 8.461 [8.102-8.484] |  | 24.9 | 24.9 |
| fence_lane32 none membar.gl | 1.469 [1.458-1.499] |  | 4.3 | 4.3 |
| fence_lane32 none fence.acq_rel.gpu | 1.477 [1.454-1.496] |  | 4.3 | 4.3 |
| fence_lane32 none fence.acq_rel.sys | 8.459 [8.443-8.496] |  | 24.9 | 24.9 |
| fence_lane1 none membar.sys | 8.436 [8.415-8.486] |  | 24.8 | 24.8 |
| fence_lane1 none membar.gl | 1.526 [1.513-1.540] |  | 4.5 | 4.5 |
| fence_lane1 none fence.acq_rel.gpu | 1.540 [1.503-1.551] |  | 4.5 | 4.5 |
| fence_lane1 none fence.acq_rel.sys | 8.431 [8.418-8.446] |  | 24.8 | 24.8 |
| st_lane32 dev none | 5.590 [5.565-5.620] |  | 16.4 | 16.4 |
| st_lane32 dev membar.sys | 8.534 [8.485-8.670] | 2.946 | 25.1 | 25.1 |
| st_lane32 dev membar.gl | 5.627 [5.606-5.655] | 0.030 | 16.5 | 16.6 |
| st_lane32 dev fence.acq_rel.gpu | 5.629 [5.613-5.648] | 0.033 | 16.6 | 16.6 |
| st_lane32 dev fence.acq_rel.sys | 8.571 [8.482-8.858] | 2.982 | 25.2 | 25.2 |
| st_lane32 reg none | 6.995 [6.765-7.006] |  | 20.6 | 20.6 |
| st_lane32 reg membar.sys | 10.964 [10.492-11.015] | 3.979 | 32.2 | 32.3 |
| st_lane32 reg membar.gl | 6.621 [6.598-6.644] | -0.370 | 19.5 | 19.5 |
| st_lane32 reg fence.acq_rel.gpu | 6.622 [6.464-6.674] | -0.371 | 19.5 | 19.5 |
| st_lane32 reg fence.acq_rel.sys | 10.985 [10.951-11.013] | 3.991 | 32.3 | 32.3 |
| st_lane1 dev none | 5.557 [5.532-5.587] |  | 16.3 | 16.3 |
| st_lane1 dev membar.sys | 8.448 [8.435-8.511] | 2.889 | 24.8 | 24.9 |
| st_lane1 dev membar.gl | 5.611 [5.586-5.674] | 0.051 | 16.5 | 16.5 |
| st_lane1 reg none | 5.552 [5.531-5.580] |  | 16.3 | 16.3 |
| st_lane1 reg membar.sys | 8.870 [8.839-8.899] | 3.312 | 26.1 | 26.1 |
| st_lane1 reg membar.gl | 5.715 [5.694-5.759] | 0.154 | 16.8 | 16.8 |

- after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.03, sys 2.95 ns per warp)
- after store, reg: membar.gl materially cheaper (>= 25%) (gl -0.37, sys 3.98 ns per warp)
- lane 0 after store, dev: membar.gl materially cheaper (>= 25%) (gl 0.05, sys 2.89 ns per warp)
- lane 0 after store, reg: membar.gl materially cheaper (>= 25%) (gl 0.15, sys 3.31 ns per warp)
- alone, all lanes: membar.gl materially cheaper (>= 25%) (gl 1.47, sys 8.46 ns per warp)
- alone, lane 0: membar.gl materially cheaper (>= 25%) (gl 1.53, sys 8.44 ns per warp)

## Counters

- validation failures: 0; hold-capped rows excluded: 0
- timed launches: slot misses 0, slot out-of-range 0, claim failures 0, inline-path misses 0
