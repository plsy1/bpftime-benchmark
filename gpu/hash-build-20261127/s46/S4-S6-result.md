# S4 (hash snapshot) and S6 (routing pass for 1533 calls) (2026-11-28)

Commit 19f6b777b (local). Plan: `PLAN-stage1.md` S4, S6.

## S4

`bpftime_model_hash_snapshot`, a PTX module of its own in `nv_gpu_model_maps.cpp` (sm_70 forms;
ptxas sm_87: 26 registers, no stack). Per directory slot: the raw header with `ld.acquire.gpu.global`
(SASS `LDG.E.64.STRONG.GPU` + `CCTL.IVALL`), and for a live header the row's value (the sum of its
shards below r, its single word above; relaxed strong loads); then the summed loss counters and the
count, in the layout header's staging layout. Test entries `gpu_model::hash_snapshot_ptx_for_test`
and `hash_snapshot_for_test`. The delivery uses it from S5.

Tests (`[gpu-devhash-device]`): a hand-built block (max_entries 600: every state, rows below and
above r, decoys in the rows of non-live slots, counters, count): 2,048 slots, 0 wrong headers, 0
wrong values, 0 wrong counters, count 77. Text: one acquire load, four relaxed loads, no weak load.

## S6

1533 maps with a device copy in the routing pass: a constant-fd lookup becomes the inline hit path
(home slot, one `ld.relaxed.gpu.global` of the header; live K: the thread's shard of the row or the
row's single word; empty: NULL; otherwise a call of `__bpftime_devhash_lookup`; one `mov` at the
join; adds through it global), an update the call of `__bpftime_devhash_insert`, a delete -22; the
devhash text once after `.address_size`, only when a lookup or update was rewritten; base 0 or a
non-constant fd keep the trampoline's call; 1533 in the module gate.

Tests: `[hash-map-routing]` (new, 11 cases) and a `[tl-byvalue]` case (lat's probes stay by value
beside a 1533 lookup); all attach tests pass but `[ptx-failure]`, which fails on plain dev too. GPU:
a routed module (a probe as llvmbpf emits one, wrapped with the trampoline) over a full, seeded
directory: for 10,000 keys each thread's pointer equals the walk's (600 found), and the routed update
returns -17, -7 and -22 as 64-bit values (flags 1 and 0).

## Mutations (`mutate_s4_s6.py`, `mutations.txt`: 15 of 15 caught)

| mutation | caught by |
|---|---|
| S4.1 non-live rows summed | snapshot over the hand-built block |
| S4.1 claiming staged as empty | the same |
| S4.1 a row at or above r read as sharded | the same |
| S4.2 the header load relaxed | snapshot text |
| S6 no state check | routing text; GPU routed module |
| S6 a row at or above r addressed as sharded | routing text (single rows) |
| S6 row bits shifted by one | GPU routed module only |
| S6 the update result zero-extended | GPU routed module only |
| S6 the fallback writes the program register | routing text (one definition) |
| S6 labels without the sequence number | two lookups' labels; ptxas |
| S6 the text twice | the text once; ptxas |
| S6 the text when no 1533 call was rewritten | base 0 keeps the calls; deletes alone |
| S6 the text appended after the probes | the text before every function; ptxas |
| S6 1533 not in the gate | the gate test |
| S6 the inline header load weak | routing text |

Two mutations are caught by the GPU test only (row shift, zero-extended result): the text tests do
not decode the emitted arithmetic.

## Added afterwards (11-28)

- **The plan's mirrors of `test_sum_map_routing.cpp`** (c42a843cb): an address register written
  elsewhere keeps the guarded atomic; joined lookups keep the global atomic (and lose it when one
  comes from a map without a copy); a lookup of a map that is not a device hash map keeps the call;
  only 64-bit adds take the global path; a call block with more than the call is refused, a
  commented marker ignored. `[hash-map-routing]`: 17 cases pass.
- **No regression of today's 1531 tool on this build** (S3 changed the shared trampoline, S6 the
  module routing gate): `lat_mmvq` on tg128 with the worktree's agent and server (9bb80ba16): 56.11
  tokens/s against 68.01 without the agent, +0.1273 ns per thread, the 1531 sum = WANT and bucket
  194 = 0 (`regression-lat_mmvq.log`, `.analysis.txt`). The by-value build's 1531 form measured
  0.1276-0.1277 ns per thread in S0.2.
