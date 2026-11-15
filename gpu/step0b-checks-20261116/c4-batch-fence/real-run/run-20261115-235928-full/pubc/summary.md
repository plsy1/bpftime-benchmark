# C4 correctness summary (pubc)

Counts are chunk-rounds (one chunk-round = G warp records behind one system fence). Rates include the CPU readers (4 threads, one chunk per reader at a time), so they are rates of the whole loop, not GPU costs. A smoke is not a measurement.

## l8, G = 1 warps per system fence (384 chunks)

- validity: early (flag before records) failed 2719 of 40255488 -> **valid**
- power reference: no fence at all (none): 342 of 78460416 chunk-rounds violated (p = 4.36e-06)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 201956352 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.1M |
| ar_sc | 213305856 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.7M |
| none | 78460416 | 342 | 342 | 6352 / 0 | 0 / 6352 | ok | 9.81M |
| sysonly | 79461888 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 9.93M |
| nosys | 78231552 | 32 | 32 | 664 / 0 | 0 / 664 | ok | 9.78M |
| early | 40255488 | 2719 | 2719 | 71372 / 0 | 1669 / 69703 | ok | 10.1M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 201956352 chunk-rounds (201956352 warp records); 95% upper bound 3/N = 1.5e-08 per chunk-round; with power (the no-fence control would have given about 880 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 213305856 chunk-rounds (213305856 warp records); 95% upper bound 3/N = 1.4e-08 per chunk-round; with power (the no-fence control would have given about 930 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 79461888 without the contributors' gpu-scope release although no fence at all would have failed about 346 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: the completer's system-scope fence is needed in practice too (observed: 32 of 78231552 violated)
- control none: the check can fail at this G (power reference)

## l8, G = 8 warps per system fence (48 chunks)

- validity: early (flag before records) failed 1154372 of 5238720 -> **valid**
- power reference: no fence at all (none): 200977 of 11150400 chunk-rounds violated (p = 0.018)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 25742784 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.3M |
| ar_sc | 24037440 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 9.61M |
| none | 11150400 | 200977 | 159262 | 3747486 / 11689623 | 3656496 / 11780613 | ok | 11.2M |
| sysonly | 10200576 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.2M |
| nosys | 10288512 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.3M |
| early | 5238720 | 1154372 | 750030 | 31134977 / 43577395 | 25473546 / 49238826 | ok | 10.5M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 25742784 chunk-rounds (205942272 warp records); 95% upper bound 3/N = 1.2e-07 per chunk-round; with power (the no-fence control would have given about 4.64e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 24037440 chunk-rounds (192299520 warp records); 95% upper bound 3/N = 1.2e-07 per chunk-round; with power (the no-fence control would have given about 4.33e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 10200576 without the contributors' gpu-scope release although no fence at all would have failed about 1.84e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 10288512 without the completer's system-scope fence although no fence at all would have failed about 1.85e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## l8, G = 48 warps per system fence (8 chunks)

- validity: early (flag before records) failed 339250 of 468448 -> **valid**
- power reference: no fence at all (none): 314931 of 927808 chunk-rounds violated (p = 0.339)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 1976224 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.74M |
| rmw_ar (literal, GPU side) | 998624 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.79M |
| ar32_aa (literal, GPU side) | 972512 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.67M |
| ar_sc | 1983936 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.76M |
| tf_sc | 991040 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.76M |
| rel_sc | 994208 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.77M |
| ar_ar (literal, shared fence) | 1000992 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.8M |
| rel_ar (literal, shared fence) | 997344 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.79M |
| ar32_sc | 1007680 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.84M |
| ar32_ar (literal, shared fence) | 991104 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.76M |
| none | 927808 | 314931 | 16920 | 5770734 / 28880203 | 7034621 / 27616316 | ok | 5.57M |
| sysonly | 905376 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 5.43M |
| nosys | 802272 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.81M |
| early | 468448 | 339250 | 116149 | 7736440 / 50556281 | 19176118 / 39116603 | ok | 5.62M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 1976224 chunk-rounds (94858752 warp records); 95% upper bound 3/N = 1.5e-06 per chunk-round; with power (the no-fence control would have given about 6.71e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rmw_ar** (literal, GPU side): consistent with the model: no violation in 998624 chunk-rounds (47933952 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.39e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar32_aa** (literal, GPU side): consistent with the model: no violation in 972512 chunk-rounds (46680576 warp records); 95% upper bound 3/N = 3.1e-06 per chunk-round; with power (the no-fence control would have given about 3.3e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 1983936 chunk-rounds (95228928 warp records); 95% upper bound 3/N = 1.5e-06 per chunk-round; with power (the no-fence control would have given about 6.73e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **tf_sc**: consistent with the model: no violation in 991040 chunk-rounds (47569920 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.36e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rel_sc**: consistent with the model: no violation in 994208 chunk-rounds (47721984 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.37e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_ar** (literal, shared fence): consistent with the model: no violation in 1000992 chunk-rounds (48047616 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.4e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rel_ar** (literal, shared fence): consistent with the model: no violation in 997344 chunk-rounds (47872512 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.39e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar32_sc**: consistent with the model: no violation in 1007680 chunk-rounds (48368640 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.42e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar32_ar** (literal, shared fence): consistent with the model: no violation in 991104 chunk-rounds (47572992 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 3.36e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 905376 without the contributors' gpu-scope release although no fence at all would have failed about 3.07e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 802272 without the completer's system-scope fence although no fence at all would have failed about 2.72e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## l8, G = 384 warps per system fence (1 chunks)

- validity: early (flag before records) failed 6299 of 13148 -> **valid**
- power reference: no fence at all (none): 6772 of 27036 chunk-rounds violated (p = 0.25)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 51336 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 0.986M |
| ar_sc | 51776 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 0.994M |
| none | 27036 | 6772 | 0 | 63040 / 2540487 | 337738 / 2265789 | ok | 1.3M |
| sysonly | 26036 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.25M |
| nosys | 20352 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 0.977M |
| early | 13148 | 6299 | 2097 | 39431 / 1804190 | 217160 / 1626461 | ok | 1.26M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 51336 chunk-rounds (19713024 warp records); 95% upper bound 3/N = 5.8e-05 per chunk-round; with power (the no-fence control would have given about 1.29e+04 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 51776 chunk-rounds (19881984 warp records); 95% upper bound 3/N = 5.8e-05 per chunk-round; with power (the no-fence control would have given about 1.3e+04 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 26036 without the contributors' gpu-scope release although no fence at all would have failed about 6.52e+03 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 20352 without the completer's system-scope fence although no fence at all would have failed about 5.1e+03 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## w32, G = 1 warps per system fence (384 chunks)

- validity: early (flag before records) failed 168101 of 71428608 -> **valid**
- power reference: no fence at all (none): 19 of 159979008 chunk-rounds violated (p = 1.19e-07)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 257499648 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 12.9M |
| ar_sc | 254237184 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 12.7M |
| none | 159979008 | 19 | 0 | 76 / 0 | 0 / 76 | ok | 20M |
| sysonly | 118679040 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 14.8M |
| nosys | 157687296 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 19.7M |
| early | 71428608 | 168101 | 168101 | 666442 / 0 | 22729 / 643713 | ok | 17.9M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 257499648 chunk-rounds (257499648 warp records); 95% upper bound 3/N = 1.2e-08 per chunk-round; with power (the no-fence control would have given about 30.6 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 254237184 chunk-rounds (254237184 warp records); 95% upper bound 3/N = 1.2e-08 per chunk-round; with power (the no-fence control would have given about 30.2 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 118679040 without the contributors' gpu-scope release although no fence at all would have failed about 14.1 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 157687296 without the completer's system-scope fence although no fence at all would have failed about 18.7 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## w32, G = 8 warps per system fence (48 chunks)

- validity: early (flag before records) failed 3333510 of 5265600 -> **valid**
- power reference: no fence at all (none): 762375 of 10814784 chunk-rounds violated (p = 0.0705)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 24200256 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 9.68M |
| ar_sc | 22350720 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 8.94M |
| none | 10814784 | 762375 | 41877 | 3044410 / 3913409 | 1456610 / 5501209 | ok | 10.8M |
| sysonly | 10534848 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 10.5M |
| nosys | 11086464 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 11.1M |
| early | 5265600 | 3333510 | 1235397 | 13329544 / 5217824 | 9632621 / 8914747 | ok | 10.5M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 24200256 chunk-rounds (193602048 warp records); 95% upper bound 3/N = 1.2e-07 per chunk-round; with power (the no-fence control would have given about 1.71e+06 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 22350720 chunk-rounds (178805760 warp records); 95% upper bound 3/N = 1.3e-07 per chunk-round; with power (the no-fence control would have given about 1.58e+06 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 10534848 without the contributors' gpu-scope release although no fence at all would have failed about 7.43e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 11086464 without the completer's system-scope fence although no fence at all would have failed about 7.82e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## w32, G = 48 warps per system fence (8 chunks)

- validity: early (flag before records) failed 248189 of 397568 -> **valid**
- power reference: no fence at all (none): 345485 of 783488 chunk-rounds violated (p = 0.441)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 1991872 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.78M |
| rmw_ar (literal, GPU side) | 1004448 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.82M |
| ar_sc | 1989216 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.77M |
| tf_sc | 991136 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.76M |
| rel_sc | 991616 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.76M |
| ar_ar (literal, shared fence) | 1007104 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.83M |
| rel_ar (literal, shared fence) | 1002848 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.81M |
| none | 783488 | 345485 | 142543 | 474366 / 18388240 | 12171641 / 6690965 | ok | 4.7M |
| sysonly | 912160 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 5.47M |
| nosys | 799168 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 4.79M |
| early | 397568 | 248189 | 49817 | 789256 / 9929373 | 7556427 / 3162202 | ok | 4.77M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 1991872 chunk-rounds (95609856 warp records); 95% upper bound 3/N = 1.5e-06 per chunk-round; with power (the no-fence control would have given about 8.78e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rmw_ar** (literal, GPU side): consistent with the model: no violation in 1004448 chunk-rounds (48213504 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.43e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 1989216 chunk-rounds (95482368 warp records); 95% upper bound 3/N = 1.5e-06 per chunk-round; with power (the no-fence control would have given about 8.77e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **tf_sc**: consistent with the model: no violation in 991136 chunk-rounds (47574528 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.37e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rel_sc**: consistent with the model: no violation in 991616 chunk-rounds (47597568 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.37e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_ar** (literal, shared fence): consistent with the model: no violation in 1007104 chunk-rounds (48340992 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.44e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **rel_ar** (literal, shared fence): consistent with the model: no violation in 1002848 chunk-rounds (48136704 warp records); 95% upper bound 3/N = 3e-06 per chunk-round; with power (the no-fence control would have given about 4.42e+05 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 912160 without the contributors' gpu-scope release although no fence at all would have failed about 4.02e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 799168 without the completer's system-scope fence although no fence at all would have failed about 3.52e+05 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## w32, G = 384 warps per system fence (1 chunks)

- validity: early (flag before records) failed 6653 of 13552 -> **valid**
- power reference: no fence at all (none): 8509 of 26972 chunk-rounds violated (p = 0.315)

| variant | chunk-rounds | violating | in a launch's first round | bad words (completer's warp / others) | stale / poison | harness | records/s |
|---|---:|---:|---:|---|---|---|---:|
| ar_aa (literal, GPU side) | 55576 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.07M |
| ar_sc | 54772 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.05M |
| none | 26972 | 8509 | 1766 | 96 / 744571 | 623421 / 121246 | ok | 1.29M |
| sysonly | 26972 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.29M |
| nosys | 22172 | 0 | 0 | 0 / 0 | 0 / 0 | ok | 1.06M |
| early | 13552 | 6653 | 3252 | 13987 / 317236 | 232050 / 99173 | ok | 1.3M |

- **ar_aa** (literal, GPU side): consistent with the model: no violation in 55576 chunk-rounds (21341184 warp records); 95% upper bound 3/N = 5.4e-05 per chunk-round; with power (the no-fence control would have given about 1.75e+04 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- **ar_sc**: consistent with the model: no violation in 54772 chunk-rounds (21032448 warp records); 95% upper bound 3/N = 5.5e-05 per chunk-round; with power (the no-fence control would have given about 1.73e+04 failures in as many rounds); not attributable to the fences' ordering (a system fence lasts microseconds, see the fence durations)
- control sysonly: 0 of 26972 without the contributors' gpu-scope release although no fence at all would have failed about 8.51e+03 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control nosys: 0 of 22172 without the completer's system-scope fence although no fence at all would have failed about 6.99e+03 times: on this GPU the remaining fences (or their duration) hide the race. Its necessity is not shown by data and rests on the model; a design keeps it.
- control none: the check can fail at this G (power reference)

## Calls

- calls that wrote to stderr: none
- calls without a result file (their verdicts are missing above): none
- any protocol violation anywhere in this run: no

## Timed and contiguous runs: verdicts

- l8_ar_aa_G48_timed.json (l8 ar_aa G=48, timed): no violation in 494336 chunk-rounds
- l8_ar_sc_G48_timed.json (l8 ar_sc G=48, timed): no violation in 500992 chunk-rounds
- w32_ar_aa_G48_timed.json (w32 ar_aa G=48, timed): no violation in 504096 chunk-rounds
- w32_ar_aa_G8_contig.json (w32 ar_aa G=8, contiguous): no violation in 3646464 chunk-rounds
- w32_ar_sc_G48_timed.json (w32 ar_sc G=48, timed): no violation in 497728 chunk-rounds
- w32_none_G8_contig.json (w32 none G=8, contiguous): 820553 of 4361088 violating (control)

## System-fence durations (completer, --time-fence runs)

| layout | variant | G | first round of a launch p50 / p90 / p99 (ns) | other rounds p50 / p90 / p99 (ns) | mean | max | violating |
|---|---|---:|---|---|---:|---:|---:|
| l8 | ar_aa | 48 | 55680 / 71680 / 83328 | 1600 / 23680 / 56128 | 18719 | 115840 | 0 of 494336 |
| l8 | ar_sc | 48 | 55744 / 71552 / 82560 | 1600 / 28800 / 57472 | 19075 | 116224 | 0 of 500992 |
| w32 | ar_aa | 48 | 54208 / 73280 / 84160 | 1664 / 23488 / 56064 | 18000 | 114208 | 0 of 504096 |
| w32 | ar_sc | 48 | 52352 / 71488 / 83072 | 1664 / 24000 / 55872 | 17864 | 112960 | 0 of 497728 |

## Contiguous chunks (a chunk = consecutive warps)

- w32 ar_aa G=8: 0 of 3646464 violating
- w32 none G=8: 820553 of 4361088 violating

