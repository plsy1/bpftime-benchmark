# S5 working steps (code level; plan PLAN-stage1.md S5, D11a, D12, D14, D15)

Base: bpftime-proto-70f7 at c42a843cb (S1-S4, S6, slots/claim on OFD locks). Each step builds,
passes its tests, and is committed locally before the next.

## A. Delivery of 1533 maps (`runtime/src/bpf_map/gpu/nv_gpu_model_maps.{hpp,cpp}`)

- `registered_hash` beside `registered_map`: impl (`nv_gpu_device_hash_map_impl *`), block
  (`CUdeviceptr`), max_entries, shards, pinned staging (`cuMemHostAlloc` DEVICEMAP,
  `staging_for(max).total` bytes) and its device pointer, the companion's value words
  (`uint64_t *`, or null), the same schedule fields as 1531 (busy/idle, covered syncs).
- `delivery_state`: `hash_module`, `hash_fn`, `hash_broken` (D14: own module, own warm-up launch,
  own broken flag; a hash failure never sets `broken`, so 1531 maps keep being delivered).
- `deliver_locked`: after the 1531 loop, each due hash map:
  1. `impl->still_holds_claim()` (D11c), with probes suppressed on application threads; false:
     unregister it, log once;
  2. `launch_hash_snapshot` on `s.stream`, `wait_stream`;
  3. `impl->apply_delivery(staging)`;
  4. status words of the impl: SYNCS_COVERED (not at exit), EXIT_DELIVERY (at exit),
     `finish_delivery()` (DELIVERIES);
  5. companion: copy impl status words 0..15 into its values (relaxed stores).
- `gpu_model::register_device_hash(impl, block, max_entries, shards, companion_words)` returns
  bool (D15: registration has a result); test hook to fail it.
- Exit delivery covers hash maps too.

## B. The copy (`gpu_model::make_device_hash_copy`)

Under a process-wide mutex held from the claim to the cache insert:
1. `impl->make_image(has_cached_copy=false, img, path)`; not_owner/have_copy: return 0;
2. shards = pow2(SMs) * 32 (as 1531); `layout_for(max, shards)`; geometry_valid or fail;
3. allocate `l.total`, zero (`cuMemsetD8Async`) on an own non-blocking stream in relaxed capture
   mode;
4. upload: headers into the directory, each live slot's value into shard 0 of its row (row < r) or
   its single word, count word, counters into shard 0 of each counter (the image's totals);
   `cuMemcpyHtoDAsync`; `cuStreamSynchronize` under `agent_sync_scope`;
5. first copy: `impl->seed_mirror(img)`;
6. register (A); 7. first copy: `impl->publish_device_phase()`; 8. cache (impl, generation) ->
   (block, shards).
Any failure after the seal (D15): free the block; first copy: `reset_mirror()`, `unseal()`;
takeover: keep PHASE 2 and the mirror; `release_claim()`; return 0. Test hooks: fail the copy,
fail registration, stop points (after the claim, after the seal).
`gpu_model::find_device_hash_copy(impl)` -> (block, shards) or 0 (process-local, keyed by impl
pointer and generation).

## C. Handler accessors (`runtime/src/handler/map_handler.cpp`)

1533 in `get_gpu_map_extra_buffer` (the cached block, or null: no creation here) and
`get_gpu_map_max_thread_count` (the cached shards, or 0).

## D. The pre-pass (`runtime/src/attach/bpf_attach_ctx.cpp`, D11a)

`prepare_device_hash_copies(manager)` at the start of `init_attach_ctx_from_handlers`, inside
`ctx_mutex`, before the binding guard:
- gate: CUDA context, nv attach handled, not the direct run, and a link handler whose program's
  name starts with `cuda__`;
- under the guard: 1533 handlers without a cached copy: pin each (`inc_map_refcount`, once per
  process), record the impl pointer; bind the companion (D12: exactly one ARRAY named the hash's
  name cut to 12 bytes + `_st`, key 4, value 8, >= 16 entries; pinned too; its index stored in the
  impl);
- without the guard: B for each recorded impl.
Test check: `map_binding_mutex`'s thread-local depth is 0 during B.

## E. `bpftimetool cuda`

The direct run sets a flag that skips the pre-pass (1533 calls go to the trampoline with base 0).

## F. Tests (`runtime/unit-test/cuda/test_gpu_devhash_device.cpp`, children)

Plan S5 tests 1-11 (test 2 with lock semantics). Start with 1 (first copy), 1b (later links keep
the base), 4 (takeover after a clean exit), 5 (failures and rollback), 6 (end to end: GPU inserts,
sync, delivery, mirror and companion exact), 10 (companion binding), 11 (isolation); then 3 (owner
deaths at the stop points), 7, 8, 9, 1c.
