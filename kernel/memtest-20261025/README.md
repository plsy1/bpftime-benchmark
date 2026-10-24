# Memory test after the 2026-10-24 kernel panic (2026-10-25)

Question: is the Jetson's RAM bad? (see `../crash-20261024`)

- `memtester 4.6.0`, 4500 MB locked (of 7.4 GB; ~5.6 GB was available), stock kernel, idle machine.
- Loop 1: 00:00-00:20 JST, exit 0, all 16 tests ok (`memtester-4500M-loop1.log`).
- Loops 2-9 (`memtester 4500M 8`): 00:20-02:56 JST, exit 0, 0 FAILURE (`memtester-4500M-8loops.log`).
- Total 9 passes over 4.5 GB in ~3 h, no errors. No memory-controller (`mc-err`) or SError
  messages in the current log or the crash log.
- Temperatures: spot checks 51-53 C (all zones) during loop 1, NVMe 37-38 C. The per-minute log
  of the 8-loop run stopped after one line (`cv0-thermal` returns "No data available"; script bug),
  so there is no temperature record for loops 2-9.

Limits: memtester only tests memory it can lock; the ~2-3 GB used by the kernel and other
processes, where the crash happened, is not covered. A full test needs an offline tool
(MemTest86 for ARM64 UEFI from a USB stick, run at the board).

Reading: no evidence of bad RAM. Together with the crash signature (a pointer value of 0xa5, far
from what single bit flips of a kernel pointer produce, inside memcg slab accounting) a kernel
bug is more likely than a hardware fault.
