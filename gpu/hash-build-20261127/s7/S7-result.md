# S7: tools and documentation (2026-11-28)

Plan: `PLAN-stage1.md` S7. Commit dfbc4c603 (local) on proto/map-model-70f7bc1f.

## Tools (`proto/tools`, copies in `tools/`)

- `pt_common.h`: `MT_DEV_HASH 1533`.
- `pt_hash.bpf.c`: `-DHS_DEV` makes `h` the device hash map and declares its companion `h_st`
  (an ARRAY of 16 words: the hash's name cut to 12 bytes plus `_st`); only CNT, PRE and MISS
  build with it (1533 takes NOEXIST inserts and adds through lookups); `HS_MAX` sets
  `max_entries` (default 262144).
- `build_hs.sh`: the 1533 variants `bpf/hd_<op>_<rate>_<key>_<set>.bpf.o` (the victim matrix's
  CNT/PRE/MISS and llama.cpp's PRE-rms, CNT-block-rms, CNT-leader-llama4).
- `pt_loader.c`: a 1533 map is dumped by key (`get_next_key`) into `<map>.kv` ({u32 key, u32 0,
  u64 value}), its entries and sum in `maps.json`; the companion's 16 words are printed by name.

## Found by the first end-to-end run: the devhash text after its callers

The first run of a real tool and victim (`hd_cnt_thread_kc_test` under `pt_victim`, the agent and
server from the worktree build) loaded the patched module without probes: ptxas refused it
("Call to '__bpftime_devhash_lookup' requires call prototype"; `runs/s7-smoke-cnt-144214`,
`runs/s7-dump-144324`). The routing pass put the devhash text after the module's `.address_size`,
but the passes place the probe functions ahead of the module's own header; the trampoline's
header leads the assembled module and the repeated header is filtered out, so the text came
after the calls (the dumped PTX: calls at lines 2650-3103, the functions from 3191). S6's tests
and the S5 children routed modules whose probes followed the header, and S0.2's form was
hand-written, so none saw it. The text now goes before the module's first function. Tests
(`[hash-map-routing]`, 19 cases): the text precedes probe functions placed before the header;
ptxas accepts the routed module as the attach assembles it (trampoline, filtered headers, the
device's target). Mutations: the text after the header, or before it, each fail two cases
(`m-after_header.log`, `m-at_start.log`). Plan section 12.

## End to end after the fix (`runs/`)

`pt_victim` (pt_kdiv 100,096 thread runs per launch; pt_kflat through graph replays), the agent
and server from the worktree build:

| run | map h | tool's own counts | companion |
|---|---|---|---|
| CNT, 1 launch | key 0: 91,905 | 91,904 hits + 1 insert; 8,191 failed inserts | EEXIST 8,191 (333 unstable), count 1, live 1 |
| PRE (keys 0, 1 prefilled at 0) | key 0: 100,096 | 100,096 hits | count 2, live 2, EEXIST 0 |
| MISS | empty | 100,096 misses | count 0 |
| CNT, graph replays (pt_kflat) | key 1: 51,457 | 51,456 hits + 1 insert; 6,143 failed | EEXIST 6,143 (800 unstable), syncs covered 2 |
| CNT, 4 launches | key 0: 392,193 | 392,192 hits + 1 insert; 8,191 failed | EEXIST 8,191 |

Every value equals the tool's hits plus its successful inserts, and the companion's EEXIST equals
the tool's failed inserts. In the one-launch runs the in-process delivery covered no
synchronization (word 3 = 0, word 4 = 2): the victim exits right after its sync, and the values
are the exit delivery's (word 1 = 1), as D16 documents for 1531.

## Documentation

- `runtime/src/bpf_map/gpu/README.md` (new; the format of
  `attach/trap_uprobe_attach_impl/README.md`): how it works; for tool authors the plan's 14 items
  (declaring the map and the companion, return codes, first touch and the counters with the
  companion's words, the E2BIG window, sizing, when GPU programs reach the device map, contracts,
  prefill and dumps, takeover, CPU writers and the seal, owners, malformed call blocks, reading
  values, faults); the host process contract; the file map and the regeneration commands; tests.
- `CLAUDE.md`, "GPU Device Hash Map (1533)": the layout header the only definition,
  regeneration, the sm_61 trampoline and the sm_70 device functions, the devhash text's
  placement, the pre-pass rules, the claim check on merged locks, the test conventions.

## Checks

- `bpftime_nv_attach_tests`: 85 of 86 cases pass; `[ptx-failure]` fails, as on plain dev.
- GPU: "Routed 1533 calls agree with the walk on the GPU" passes after the placement change.
- No mutation for the documentation (plan); S8 runs the tools on llama.cpp.
