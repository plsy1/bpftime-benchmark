# C4 correctness summary (pubc)

Counts are chunk-rounds (one chunk-round = G warp records behind one system fence). Rates include the CPU readers (4 threads, one chunk per reader at a time), so they are rates of the whole loop, not GPU costs. A smoke is not a measurement.

## l8, G = 1 warps per system fence (384 chunks)

- validity: early (flag before records) failed 35460 of 37817856 -> **valid**
- power reference: no fence at all (none): 1220 of 78004224 chunk-rounds violated (p = 1.56e-05)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 201507840 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.1M |
| ar_sc | 201315840 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.1M |
| none | 78004224 | 1220 | 1220 | 24538 / 0 | 184 / 24354 | ok | 9.75M |
| sysonly | 81067008 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.1M |
| nosys | 77617152 | 76 | 74 | 1144 / 0 | 0 / 1144 | ok | 9.7M |
| early | 37817856 | 35460 | 35460 | 968031 / 0 | 36899 / 931132 | ok | 9.45M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 201507840 chunk-rounds (201507840 warp records); 95% upper bound 3/N = 1.5e-08 per chunk-round; with power (the no-fence control would have given about 3.15e+03 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 201315840 chunk-rounds (201315840 warp records); 95% upper bound 3/N = 1.5e-08 per chunk-round; with power (the no-fence control would have given about 3.15e+03 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 81067008 without the contributors' gpu-scope release although no fence at all would have failed about 1.27e+03 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: the completer's system-scope fence is needed in practice too (observed: 76 of 77617152 violated)
- control none: the check can fail at this G (power reference)

## l8, G = 8 warps per system fence (48 chunks)

- validity: early (flag before records) failed 872254 of 5611968 -> **valid**
- power reference: no fence at all (none): 210569 of 10732032 chunk-rounds violated (p = 0.0196)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 23862912 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 9.55M |
| ar_sc | 24185856 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 9.67M |
| none | 10732032 | 210569 | 161354 | 3483653 / 9749861 | 1527841 / 11705673 | ok | 10.7M |
| sysonly | 10324416 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.3M |
| nosys | 11016576 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 11M |
| early | 5611968 | 872254 | 636463 | 27123626 / 22492672 | 8017454 / 41598844 | ok | 11.2M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 23862912 chunk-rounds (190903296 warp records); 95% upper bound 3/N = 1.3e-07 per chunk-round; with power (the no-fence control would have given about 4.68e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 24185856 chunk-rounds (193486848 warp records); 95% upper bound 3/N = 1.2e-07 per chunk-round; with power (the no-fence control would have given about 4.75e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 10324416 without the contributors' gpu-scope release although no fence at all would have failed about 2.03e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 11016576 without the completer's system-scope fence although no fence at all would have failed about 2.16e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## l8, G = 48 warps per system fence (8 chunks)

- validity: early (flag before records) failed 341776 of 470368 -> **valid**
- power reference: no fence at all (none): 326004 of 925056 chunk-rounds violated (p = 0.352)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 1963456 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.71M |
| rmw_ar (literal, GPU side) | 1006976 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.83M |
| ar32_aa (literal, GPU side) | 1006208 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.83M |
| ar_sc | 1967936 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.72M |
| tf_sc | 990528 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.75M |
| rel_sc | 978272 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.7M |
| ar_ar (literal, shared fence) | 991104 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.76M |
| rel_ar (literal, shared fence) | 1009312 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.84M |
| ar32_sc | 977088 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.69M |
| ar32_ar (literal, shared fence) | 1011840 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.86M |
| none | 925056 | 326004 | 134469 | 4772696 / 27893334 | 4973435 / 27692595 | ok | 5.55M |
| sysonly | 874016 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 5.24M |
| nosys | 800928 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.81M |
| early | 470368 | 341776 | 117292 | 7364772 / 47857775 | 18597844 / 36624703 | ok | 5.64M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 1963456 chunk-rounds (94245888 warp records); 95% upper bound 3/N = 1.5e-06 per chunk-round; with power (the no-fence control would have given about 6.92e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rmw_ar** (literal, GPU side): consistent with the model: no violation in 1006976 chunk-rounds (48334848 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.55e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar32_aa** (literal, GPU side): consistent with the model: no violation in 1006208 chunk-rounds (48297984 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.55e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 1967936 chunk-rounds (94460928 warp records); 95% upper bound 3/N = 1.5e-06 per chunk-round; with power (the no-fence control would have given about 6.94e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **tf_sc**: consistent with the model: no violation in 990528 chunk-rounds (47545344 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.49e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rel_sc**: consistent with the model: no violation in 978272 chunk-rounds (46957056 warp records); 95% upper bound 3/N = 3.1e-06 per chunk-round; with power (the no-fence control would have given about 3.45e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_ar** (literal, shared fence): consistent with the model: no violation in 991104 chunk-rounds (47572992 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.49e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rel_ar** (literal, shared fence): consistent with the model: no violation in 1009312 chunk-rounds (48446976 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.56e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar32_sc**: consistent with the model: no violation in 977088 chunk-rounds (46900224 warp records); 95% upper bound 3/N = 3.1e-06 per chunk-round; with power (the no-fence control would have given about 3.44e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar32_ar** (literal, shared fence): consistent with the model: no violation in 1011840 chunk-rounds (48568320 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.57e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 874016 without the contributors' gpu-scope release although no fence at all would have failed about 3.08e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 800928 without the completer's system-scope fence although no fence at all would have failed about 2.82e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## l8, G = 384 warps per system fence (1 chunks)

- validity: early (flag before records) failed 7146 of 13004 -> **valid**
- power reference: no fence at all (none): 5103 of 20540 chunk-rounds violated (p = 0.248)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 51364 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 0.986M |
| ar_sc | 51764 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 0.994M |
| none | 20540 | 5103 | 0 | 21024 / 1083056 | 7632 / 1096448 | ok | 0.986M |
| sysonly | 25956 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.25M |
| nosys | 20616 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 0.989M |
| early | 13004 | 7146 | 3118 | 125108 / 2025148 | 677758 / 1472498 | ok | 1.25M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 51364 chunk-rounds (19723776 warp records); 95% upper bound 3/N = 5.8e-05 per chunk-round; with power (the no-fence control would have given about 1.28e+04 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 51764 chunk-rounds (19877376 warp records); 95% upper bound 3/N = 5.8e-05 per chunk-round; with power (the no-fence control would have given about 1.29e+04 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 25956 without the contributors' gpu-scope release although no fence at all would have failed about 6.45e+03 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 20616 without the completer's system-scope fence although no fence at all would have failed about 5.12e+03 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## w32, G = 1 warps per system fence (384 chunks)

- validity: early (flag before records) failed 183062 of 73602048 -> **valid**
- power reference: no fence at all (none): 5 of 161734656 chunk-rounds violated (p = 3.09e-08)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 250423296 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 12.5M |
| ar_sc | 257032704 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 12.9M |
| none | 161734656 | 5 | 0 | 20 / 0 | 0 / 20 | ok | 20.2M |
| sysonly | 118702080 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 14.8M |
| nosys | 156446208 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 19.6M |
| early | 73602048 | 183062 | 183060 | 727576 / 0 | 33836 / 693740 | ok | 18.4M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 250423296 chunk-rounds (250423296 warp records); 95% upper bound 3/N = 1.2e-08 per chunk-round; with power (the no-fence control would have given about 7.74 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 257032704 chunk-rounds (257032704 warp records); 95% upper bound 3/N = 1.2e-08 per chunk-round; with power (the no-fence control would have given about 7.95 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 118702080 without the contributors' gpu-scope release although no fence at all would have failed about 3.67 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 156446208 without the completer's system-scope fence although no fence at all would have failed about 4.84 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## w32, G = 8 warps per system fence (48 chunks)

- validity: early (flag before records) failed 3444625 of 5480832 -> **valid**
- power reference: no fence at all (none): 655311 of 10858560 chunk-rounds violated (p = 0.0603)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 23822208 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 9.53M |
| ar_sc | 26072640 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.4M |
| none | 10858560 | 655311 | 88395 | 2616613 / 5313338 | 555206 / 7374745 | ok | 10.9M |
| sysonly | 9762816 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 9.76M |
| nosys | 9053184 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 9.05M |
| early | 5480832 | 3444625 | 1264344 | 13774666 / 9678106 | 13284855 / 10167917 | ok | 11M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 23822208 chunk-rounds (190577664 warp records); 95% upper bound 3/N = 1.3e-07 per chunk-round; with power (the no-fence control would have given about 1.44e+06 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 26072640 chunk-rounds (208581120 warp records); 95% upper bound 3/N = 1.2e-07 per chunk-round; with power (the no-fence control would have given about 1.57e+06 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 9762816 without the contributors' gpu-scope release although no fence at all would have failed about 5.89e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 9053184 without the completer's system-scope fence although no fence at all would have failed about 5.46e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## w32, G = 48 warps per system fence (8 chunks)

- validity: early (flag before records) failed 328342 of 444704 -> **valid**
- power reference: no fence at all (none): 434661 of 936640 chunk-rounds violated (p = 0.464)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 1969152 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.73M |
| rmw_ar (literal, GPU side) | 985376 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.73M |
| ar_sc | 1994464 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.79M |
| tf_sc | 991616 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.76M |
| rel_sc | 988128 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.74M |
| ar_ar (literal, shared fence) | 1012800 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.86M |
| rel_ar (literal, shared fence) | 995296 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.78M |
| none | 936640 | 434661 | 116978 | 909087 / 15319666 | 12516830 / 3711923 | ok | 5.62M |
| sysonly | 914272 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 5.49M |
| nosys | 798176 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.79M |
| early | 444704 | 328342 | 110874 | 964799 / 12094221 | 8691962 / 4367058 | ok | 5.34M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 1969152 chunk-rounds (94519296 warp records); 95% upper bound 3/N = 1.5e-06 per chunk-round; with power (the no-fence control would have given about 9.14e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rmw_ar** (literal, GPU side): consistent with the model: no violation in 985376 chunk-rounds (47298048 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.57e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 1994464 chunk-rounds (95734272 warp records); 95% upper bound 3/N = 1.5e-06 per chunk-round; with power (the no-fence control would have given about 9.26e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **tf_sc**: consistent with the model: no violation in 991616 chunk-rounds (47597568 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.6e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rel_sc**: consistent with the model: no violation in 988128 chunk-rounds (47430144 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.59e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_ar** (literal, shared fence): consistent with the model: no violation in 1012800 chunk-rounds (48614400 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.7e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rel_ar** (literal, shared fence): consistent with the model: no violation in 995296 chunk-rounds (47774208 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.62e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 914272 without the contributors' gpu-scope release although no fence at all would have failed about 4.24e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 798176 without the completer's system-scope fence although no fence at all would have failed about 3.7e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## w32, G = 384 warps per system fence (1 chunks)

- validity: early (flag before records) failed 5205 of 13892 -> **valid**
- power reference: no fence at all (none): 13250 of 26968 chunk-rounds violated (p = 0.491)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 55268 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.06M |
| ar_sc | 51976 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 0.998M |
| none | 26968 | 13250 | 6256 | 0 / 411898 | 325052 / 86846 | ok | 1.29M |
| sysonly | 26616 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.28M |
| nosys | 23500 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.13M |
| early | 13892 | 5205 | 1670 | 2473 / 351989 | 272040 / 82422 | ok | 1.33M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 55268 chunk-rounds (21222912 warp records); 95% upper bound 3/N = 5.4e-05 per chunk-round; with power (the no-fence control would have given about 2.72e+04 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 51976 chunk-rounds (19958784 warp records); 95% upper bound 3/N = 5.8e-05 per chunk-round; with power (the no-fence control would have given about 2.55e+04 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 26616 without the contributors' gpu-scope release although no fence at all would have failed about 1.31e+04 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 23500 without the completer's system-scope fence although no fence at all would have failed about 1.15e+04 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## Calls

- calls that wrote to stderr: none
- calls without a result file (their verdicts are missing above): none
- any protocol violation anywhere in this run: no

## Timed and contiguous runs: verdicts

- l8_ar_aa_G48_timed.json (l8 ar_aa G=48, timed): no violation in 491040 chunk-rounds
- l8_ar_sc_G48_timed.json (l8 ar_sc G=48, timed): no violation in 499808 chunk-rounds
- w32_ar_aa_G48_timed.json (w32 ar_aa G=48, timed): no violation in 492992 chunk-rounds
- w32_ar_aa_G8_contig.json (w32 ar_aa G=8, contiguous): no violation in 4165056 chunk-rounds
- w32_ar_sc_G48_timed.json (w32 ar_sc G=48, timed): no violation in 499200 chunk-rounds
- w32_none_G8_contig.json (w32 none G=8, contiguous): 1078499 of 4736640 violating (control)

## System-fence durations (completer, --time-fence runs)

| layout | variant | G | first round of a launch p50 / p90 / p99 (ns) | other rounds p50 / p90 / p99 (ns) | mean | max | violating |
|---|---|---:|---|---|---:|---:|---:|
| l8 | ar_aa | 48 | 54016 / 69760 / 81152 | 1600 / 26560 / 54336 | 18677 | 107840 | 0 of 491040 |
| l8 | ar_sc | 48 | 55168 / 70976 / 82304 | 1664 / 25344 / 55360 | 18686 | 116864 | 0 of 499808 |
| w32 | ar_aa | 48 | 54528 / 73600 / 83776 | 1664 / 21952 / 56000 | 18030 | 118656 | 0 of 492992 |
| w32 | ar_sc | 48 | 51072 / 69824 / 82176 | 1664 / 20032 / 56960 | 17176 | 115008 | 0 of 499200 |

## Contiguous chunks (a chunk = consecutive warps)

- w32 ar_aa G=8: 0 of 4165056 violating
- w32 none G=8: 1078499 of 4736640 violating

