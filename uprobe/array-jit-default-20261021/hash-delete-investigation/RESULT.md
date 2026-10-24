# Root cause localized: bound-array erase changes GCC/LTO inlining

Investigation only; primary worktree remains the frozen default-on candidate.
No production fix, commit or push. The diagnostic change exists only in the
independent warm source/build and its archived agent artifact.

## Causal chain

The default-on generalization added `array_map_binding::erase`, which called
`handler->map_delete_elem(key, false)` to retain array identity for mixed
lookup/delete programs. This introduced another native caller of the generic
all-map delete dispatcher, in addition to the normal map-delete helper path.

With GCC 13 / LTO ON, the previous dispatcher was inlined into
`bpftime_map_delete_elem_helper`. In the new binary it was outlined into a
separate `bpf_map_handler::map_delete_elem(...).constprop.0`. The standard helper
and bound-array erase both tail-branch to it. Ordinary hash delete does not
itself use array lookup inlining, yet its native helper hot path changed.

The isolated diagnostic intervention changes ONLY the bound-array erase body
to call its already-known ordinary/per-CPU array implementation directly. It
retains ownership, FD identity, error semantics, default-on behavior and mixed
map support. The generic dispatcher loses the new caller and is again inlined
into the standard helper. No inline/noinline compiler attributes were added.

| Whole native function (NOT executed hot-path instruction counts) | Previous | Regression | Diagnostic |
|---|---:|---:|---:|
| Standard delete helper bytes | 9460 | 196 | 9460 |
| Standard delete helper instructions | 2365 | 49 | 2365 |
| Separate generic delete dispatcher bytes | absent | 9356 | absent |

The regression helper tail-jumps to the separate dispatcher; the old and
diagnostic helper include it. These function sizes are evidence of outlining,
not a claim that all map-type branches execute on every hash delete. Extra
function boundaries/frame handling and changed generated code are established;
the cycle contribution of each individual instruction/cache effect is NOT
separately measured. Address/layout differences may explain remaining small
differences between the previous and diagnostic binaries.

## Controlled timing

12 balanced ABC blocks (all six orders twice), 36 fresh processes/maps,
10,000 timed and 1,000 untimed warmup invocations per arm. Same single-case
victim from hash-delete-recheck, original full-suite eBPF object and loader.
Each invocation repopulates 1,000 keys outside the timer, then deletes them.
Current and diagnostic arms use the SAME frozen current syscall-server; only
the agent differs. Previous uses its corresponding frozen server/agent pair.
CPU 5 fixed 1,728 MHz; frequency checks and artifact hashes passed.

| Ordinary hash delete, ns/op | Previous | Regression | Diagnostic |
|---|---:|---:|---:|
| Mean | 37.0997 | 43.7989 | 36.2556 |
| Min | 36.8949 | 43.3587 | 36.1156 |
| Max | 37.3543 | 44.0773 | 36.3716 |

Diagnostic mean vs regression: -17.22%, faster in 12/12 blocks.
Diagnostic mean vs previous: -2.28%, faster in 12/12 blocks. The conclusion is
removal of the approximately 18% regression; do not advertise the small extra
difference as a broadly validated optimization.

Correctness on diagnostic build: 42 cases / 110422 assertions; host contract
18 cases / 201 assertions. Existing mixed per-CPU mutation/FD reuse test passed.
No kernel rerun, other-operation performance screen or full ten-round rerun
performed for this diagnostic artifact. Do not claim broad acceptance yet.

Next: ask before promoting the change to primary; if authorized, apply the
small erase change and recheck non-target operations and consuming array
workloads before accepting the default-on feature. Keep prior full results
and the 12-pair regression reproduction unchanged.
