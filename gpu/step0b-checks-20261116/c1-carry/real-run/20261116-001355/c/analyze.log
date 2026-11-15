# C1 part (c): cost per thread execution (entry write + exit read)

rounds: 10; batches dropped for a capped gate: 0; validation failures: 0
e2 comparability (anchor, c_ref / 0.433 ns): 0.98 -> comparable

## anchor (262144 threads)

| storage | ns per thread vs none, median [min-max] | vs empty probes | [projection] pp512 | resolved | regs, local B, blocks/SM (anchor) |
|---|---|---|---:|---|---|
| empty probes (both, return 0) | 0.026 [0.026-0.027] | 0.000 | 2.5% | yes | (6, 0, 3) |
| entry calls the timer helper, no storage (shared by 1-3) | 0.044 [0.043-0.044] | 0.017 | 4.1% | yes | (8, 0, 3) |
| (1) carry in .local, direct form | 0.104 [0.102-0.104] | 0.077 | 9.8% | yes | (34, 656, 3) |
| (1') carry in .local, via the lookup wrapper | 0.162 [0.160-0.162] | 0.135 | 15.2% | yes | (38, 656, 3) |
| (2) (SM, warp slot, lane) slot + owner tag, device memory | 0.285 [0.281-0.285] | 0.259 | 26.8% | yes | (38, 632, 3) |
| (3) today's 1512 lookup, registered memory (entry write + exit read) | 0.782 [0.780-0.787] | 0.756 | 73.6% | yes | (36, 632, 3) |
| e2's reference: threadhist exit-only lookup-and-increment (1512) | 0.426 [0.422-0.427] | 0.400 | 40.1% | yes | (32, 632, 3) |

c_none launch: 7.61 us (median)

## large (4194304 threads)

| storage | ns per thread vs none, median [min-max] | vs empty probes | [projection] pp512 | resolved | regs, local B, blocks/SM (anchor) |
|---|---|---|---:|---|---|
| empty probes (both, return 0) | 0.027 [0.027-0.027] | 0.000 | 2.5% | yes | (6, 0, 3) |
| entry calls the timer helper, no storage (shared by 1-3) | 0.031 [0.031-0.031] | 0.004 | 2.9% | yes | (8, 0, 3) |
| (1) carry in .local, direct form | 0.094 [0.093-0.094] | 0.067 | 8.8% | yes | (34, 656, 3) |
| (1') carry in .local, via the lookup wrapper | 0.152 [0.152-0.153] | 0.125 | 14.4% | yes | (38, 656, 3) |
| (2) (SM, warp slot, lane) slot + owner tag, device memory | 0.270 [0.267-0.270] | 0.243 | 25.4% | yes | (38, 632, 3) |
| (3) today's 1512 lookup, registered memory (entry write + exit read) | 0.746 [0.739-0.754] | 0.719 | 70.2% | yes | (36, 632, 3) |
| e2's reference: threadhist exit-only lookup-and-increment (1512) | 0.414 [0.413-0.415] | 0.387 | 39.0% | yes | (32, 632, 3) |

c_none launch: 87.30 us (median)

## full (12288 threads)

| storage | ns per thread vs none, median [min-max] | vs empty probes | [projection] pp512 | resolved | regs, local B, blocks/SM (anchor) |
|---|---|---|---:|---|---|
| empty probes (both, return 0) | 0.007 [0.006-0.007] | 0.000 | 0.6% | yes | (6, 0, 3) |
| entry calls the timer helper, no storage (shared by 1-3) | 0.176 [0.176-0.177] | 0.169 | 16.6% | yes | (8, 0, 3) |
| (1) carry in .local, direct form | 0.260 [0.253-0.262] | 0.254 | 24.5% | yes | (34, 656, 3) |
| (1') carry in .local, via the lookup wrapper | 0.343 [0.342-0.345] | 0.336 | 32.3% | yes | (38, 656, 3) |
| (2) (SM, warp slot, lane) slot + owner tag, device memory | 0.504 [0.502-0.506] | 0.497 | 47.4% | yes | (38, 632, 3) |
| (3) today's 1512 lookup, registered memory (entry write + exit read) | 0.961 [0.957-0.963] | 0.954 | 90.4% | yes | (36, 632, 3) |
| e2's reference: threadhist exit-only lookup-and-increment (1512) | 0.606 [0.604-0.611] | 0.600 | 57.1% | yes | (32, 632, 3) |

c_none launch: 2.72 us (median)

## Validation-only kernels

valid mark (exit without entry finds 0 in every thread): True; the mark-1 twin read its prologue's 1 as valid in every thread (so the check can fail): True; the no-mark control read stale bytes from the previous kernel as valid (reported): True
carry kernels whose validation failed: none; all failed validations: none

- anchor|c_local_bad|local: ok null=0/0 mismatch=262144/262144 
- anchor|c_local_d_bad|local: ok null=0/0 mismatch=262144/262144 
- anchor|c_slottag_bad|dev: ok null=0/0 mismatch=262144/262144 
- anchor|c_1512_bad|reg: ok null=0/0 mismatch=262144/262144 written=0/0 
- anchor|c_local_noentry|local: ok null=0/0 mismatch=262144/262144 
- anchor|c_local_d_noentry|local: ok null=0/0 mismatch=262144/262144 
- anchor|c_local_d_noentry_noinit|local: report null=0 mismatch=0/262144 (stale bytes read as valid: 262144) 
- anchor|c_local_d_noentry_mark1|local: ok null=0/0 mismatch=0/0 
- large|c_local_bad|local: ok null=0/0 mismatch=4194304/4194304 
- large|c_local_d_bad|local: ok null=0/0 mismatch=4194304/4194304 
- large|c_slottag_bad|dev: ok null=0/0 mismatch=4194304/4194304 
- large|c_1512_bad|reg: ok null=0/0 mismatch=4194304/4194304 written=0/0 
- large|c_local_noentry|local: ok null=0/0 mismatch=4194304/4194304 
- large|c_local_d_noentry|local: ok null=0/0 mismatch=4194304/4194304 
- large|c_local_d_noentry_noinit|local: report null=0 mismatch=0/4194304 (stale bytes read as valid: 4194304) 
- large|c_local_d_noentry_mark1|local: ok null=0/0 mismatch=0/0 
- full|c_local_bad|local: ok null=0/0 mismatch=12288/12288 
- full|c_local_d_bad|local: ok null=0/0 mismatch=12288/12288 
- full|c_slottag_bad|dev: ok null=0/0 mismatch=12288/12288 
- full|c_1512_bad|reg: ok null=0/0 mismatch=12288/12288 written=0/0 
- full|c_local_noentry|local: ok null=0/0 mismatch=12288/12288 
- full|c_local_d_noentry|local: ok null=0/0 mismatch=12288/12288 
- full|c_local_d_noentry_noinit|local: report null=0 mismatch=0/12288 (stale bytes read as valid: 12288) 
- full|c_local_d_noentry_mark1|local: ok null=0/0 mismatch=0/0 

