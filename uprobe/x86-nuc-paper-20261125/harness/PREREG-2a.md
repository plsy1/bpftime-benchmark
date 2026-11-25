# PREREG-2a — clarification of PREREG-2 §3 (2026-11-25, written during C1 before any C1 analysis)

PREREG-2 §3 says "x86 ±1.5%, folded with E18 Dn median 89,614 rps into ±0.084 us/event". The base of that fold
is Dn's whole time per event, not its overhead x over B: 1e6 / 89,614 / 2 = 5.580 us/event, times 1.5% = 0.0837.
(eval-plan §1.3 called that 5.58 "x"; it is the per-event service time, a naming slip.) This base makes the
us/event margin the same as the ±1.5% ratio margin at Dn's throughput, which is the intent. The margin value
(±0.084 us/event) and every rule in PREREG-2 are unchanged. Orin uses the same base: Dn-stock 15,776 rps (E18)
-> 31.69 us/event -> ±2% = ±0.634 us/event (the Jetson session's option (a)).
