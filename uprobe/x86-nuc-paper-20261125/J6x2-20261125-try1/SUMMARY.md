# J6x2-20261125-try1 (x86, exploratory, Q6) — pinbench-ws v2, direct per-iteration timing
i5-1240P, sniff core cpu4 (P core): L2 1,280 KiB per core, L3 12 MiB shared. 5 runs x 100k iterations per cell; medians (ns).

| mask | ws ≤ 1024 KB | 2048 KB | 4096 KB | getpid ≤1024 / ≥2048 |
|---|---|---|---|---|
| single (already pinned) | 793–856 | 1,332 | 1,339 | 90–115 / 267–269 |
| pair (4,6) | 1,618–1,711 | 2,505 | 2,507 | 89–114 / 268 |
| all (0-15) | 1,573–1,643 | 2,351 | 2,403 | 90–108 / 255–261 |

- The pin sequence's price is flat while the touched working set fits in the 1.25 MiB L2 and rises by 45–70% once it does
  not (2 MiB and up); a plain getpid rises 2.4x there. Consistent with a cache-capacity effect on kernel entry paths.
- nginx's per-event working set on x86 evidently fits (in-situ pin premium 1.08x, E9). The Orin's L2 is 256 KB per core,
  so the same mechanism predicts a premium there (Orin J6 v1 runs tonight; v2 requested).
- v1 (J6x-20261125-try1) subtracted loop totals; above 1 MiB that was swamped by touch-loop variance (negative values). Use v2.
