# GPU device hash map (type 1533)

A hash map that GPU programs (`cuda__*` probes) use in device memory, without a host call per
operation, for the common counter idiom of bpf tools: look a key up, add to its value, and insert
the key with `BPF_NOEXIST` when it is missing. Keys are 32 bits, values 64 bits. The CPU sees the
values through a mirror that the agent's delivery refreshes after the application's
synchronizations, as it does for the sum array (1531).

Stage one of the design in the plan (results `gpu/hash-build-plan-20261127`, `PLAN-stage1.md`;
decisions D1-D16 there). This file is for tool authors and for whoever changes the code.

## How it works

1. **CPU phase.** The tool creates the map and may fill it with `bpf()` (updates, deletes,
   lookups). Writes go to a CPU table in the shared segment.
2. **The copy.** The first agent init that will instantiate a CUDA link claims the map (an open
   file description lock on a byte of the segment's file), seals it (CPU writes now fail with
   `EPERM`; writers in flight are waited for, at most a second) and copies the CPU table into a
   device block: an open-addressing directory of `key << 32 | row << 3 | state` headers, the values
   of rows below `r = min(max_entries, 512)` sharded over (SM, lane), the other rows one word each,
   a reservation count and loss counters. This runs in a pre-pass before init takes
   `map_binding_mutex` (`bpf_attach_ctx::prepare_device_hash_copies`).
3. **GPU programs.** The routing pass rewrites each constant-fd lookup into an inline hit path
   (the key's home slot, one load; a live header gives the thread's shard of the row) with a call
   of `__bpftime_devhash_lookup` behind it, and each update into a call of
   `__bpftime_devhash_insert`. Both functions are inserted once into the patched module.
4. **Delivery.** After the application's synchronizations (and every 20 ms while values change,
   200 ms otherwise) the owner snapshots the block on its own stream and writes the mirror, the
   status words and the companion status array. CPU lookups read the mirror.
5. **Takeover.** If the owner exits or dies, the next agent init that needs the map takes it over
   from the mirror.

## Writing a tool

1. **Declare the map and, optionally, its companion status array:**

   ```c
   struct { __uint(type, 1533); __uint(max_entries, 65536);
            __type(key, u32); __type(value, u64); } counts SEC(".maps");
   struct { __uint(type, BPF_MAP_TYPE_ARRAY); __uint(max_entries, 16);
            __type(key, u32); __type(value, u64); } counts_st SEC(".maps");
   ```

   Key 4 bytes, value 8, `max_entries` at most 2^22. The companion is an ARRAY (key 4, value 8,
   at least 16 entries) named the hash map's name cut to 12 bytes plus `_st`. It binds only when
   exactly one such array exists and exactly one 1533 map derives that name: two hash maps whose
   names share their first 12 bytes get no companion, nor does a name with two arrays.

2. **Return codes on the GPU.** An update returns 0, -17 (EEXIST: the key is there), -7 (E2BIG:
   no room), -22 (EINVAL: flags other than `BPF_NOEXIST`) or -1 (no device copy in this process,
   see 6). A delete returns -22. A lookup returns the value's address or NULL. Of the GPU maps,
   only 1533 gives GPU programs errno values.

3. **First touch and the counters.** When threads insert one new key at once, one wins and the
   others get EEXIST. Their increments are lost unless the program adds through a lookup after
   EEXIST, as the examples' counters do not. Each EEXIST and E2BIG is counted. An *unstable*
   EEXIST (or E2BIG) means the result came while the winner's slot was not yet published, so a
   lookup right after it may still return NULL. Companion words:

   | word | content |
   |---|---|
   | 0 | deliveries |
   | 1 | 1 when the exit delivery ran (best effort, as for 1531) |
   | 2 | the owner's pid |
   | 3 | application synchronizations the latest in-process delivery covered |
   | 4 | application synchronizations counted at exit |
   | 5 | probed launches noted after the last synchronization (at exit) |
   | 6-12 | EEXIST, E2BIG, unstable EEXIST, unstable E2BIG, bad flags, lookups and inserts that walked the whole directory |
   | 13 | the reservation count: rows handed out, never lower; after a takeover it includes rows the new image does not use, so it is reserved capacity, not live keys |
   | 14 | live slots |
   | 15 | refused slots |

   Words 3 and 4 equal means a delivery covered the last synchronization; with word 5 also 0, the
   values hold every add of the noted launches.

4. **The E2BIG window.** As in the kernel's htab, an insert can return E2BIG while the key that
   took the last place is not yet visible to lookups. Within one launch, whether a thread sees
   another thread's insert rests on the hardware's ordering (the headers are published with
   release stores and read with acquire or strong loads); across launches, stream order holds.

5. **Size the map above the number of keys.** An insert past `max_entries` leaves a refused slot
   in the directory. Refused slots stay, so a map that receives many more distinct keys than it
   holds fills its directory, and lookups of absent keys then walk all of it (word 11 counts
   them).

6. **When GPU programs reach the device map.** Only through constant map fds, and only in modules
   patched after this process made its copy. A map without a copy when the process initialized its
   attach (another process owns it, the copy failed, or the map was created during that init) and
   any non-constant fd go to the trampoline, which answers NULL, -1 and -22 without a host call,
   for the modules patched then. Unlike 1531, the trampoline never serves 1533 through the host.
   `bpftimetool cuda` (the direct run) never uses the device map.

7. **Contracts.** Values change only by 64-bit adds (`__sync_fetch_and_add` on the u64) through a
   lookup's pointer. No stores through lookup pointers, on the GPU or the CPU.

8. **Prefill and dumps.** Fill the map before the first agent init that instantiates a CUDA link
   (the pre-pass seals it then). From the seal on, CPU updates and deletes fail with `EPERM`. A dump
   with `get_next_key` that crosses the copy may skip keys.

9. **Takeover.** The new owner keeps the same slots and renumbers the rows in slot order. Keys and
   adds made after the last delivery are lost with the old owner.

10. **CPU writers and the seal.** A writer of a running process that stays inside an update for
    more than 1 s during a seal delays the device copy (`SEAL_DEAD`) until it has finished. A
    writer whose process died, was killed or exec'd (into an image without the segment) is
    discounted: the kernel releases its slot's lock. Not discounted, so the copy stays disabled
    until the writer's process exits or the segment is recreated, if one dies in flight:
    - a writer in the overflow slot: more than 256 writing processes at once, a process without
      the segment file, or one whose locks the system refuses;
    - a vfork-style child killed in flight while its parent lives;
    - the threads an exec killed when the new image maps the segment again, unless it takes the
      same slot, as it usually does (the scan starts at its pid);
    - a holder whose lock a host released while it runs.

    A writer that died inside the table also leaves its stripe locked, as for HASH. Each process
    keeps one descriptor of the segment's file per 1533 map it writes or owns, until it exits or
    destroys the map.

11. **Owners.** An owner that died, was killed or exec'd is replaced at the next agent init that
    needs the map, in any pid namespace. An owner whose descriptor a host closed keeps its claim:
    no process of its pid namespace takes over while it runs and maps the segment, and it takes its
    lock back at its next delivery check. An owner of another pid namespace whose descriptor a host
    closed can be taken over while it runs, and both deliver until its next check. Children made
    without `fork` (`clone`, `clone3`) keep their parent's descriptors, so a dead owner's claim and
    a dead writer's slot stay held while such a child runs. The claim stays with the process until
    it exits or execs, even after the agent detaches.

12. **A malformed call block** (a 1533 helper call the routing pass cannot parse) leaves the whole
    module unprobed.

13. **Reading values.** The delivery writes the mirror one word at a time, so a `bpf()` lookup can
    copy a value while the next delivery writes it, as for 1531. Companion words are independent
    counters.

14. **A device fault in the hash snapshot** stops the CUDA context.

## Host process contract

- The copy and the claim never run under `map_binding_mutex` (a robust interprocess mutex that a
  death breaks for good), and every copy exists before any CUDA link of that init is
  instantiated: `map_basic_info` never changes after a link exists. The pre-pass runs only when
  the init will instantiate a CUDA link and this process handles the CUDA attach types; an agent
  without one never claims or seals a map, so a tool's later prefill is not refused.
- Any failure after the seal rolls the copy back: a first copy unseals (CPU writes work again) and
  empties the mirror, a takeover keeps PHASE 2 and the mirror, and the claim is given back.
  Nothing the pre-pass does throws out of init.
- Only the process whose copy is made stores the companion's index. A stored index is checked
  (an ARRAY with the derived name, key 4, value 8, at least 16 entries) before it is used, so a
  closed and reused slot is never written.
- The claim check reads the description's own lock lines in `/proc/self/fdinfo`, testing that the
  claim's byte lies in a line's range: the kernel merges one description's adjacent locks (writer
  slot 255 and the claim's byte 256 show as one line). An unreadable fdinfo, with the segment's
  file still at that number, counts as held.
- Not handled, as for 1531: the delivery holds the map object in the segment, so a process that
  unmaps its segment while a device map is registered crashes the delivery thread. The agent
  unmaps only at exit, after the exit delivery.

## Layout

| file | contents |
|---|---|
| `attach/nv_attach_impl/trampoline/gpu_devhash_layout.h` | the block's and the staging's layout, states, hash, constants: the only definition (host, device, routing pass) |
| `nv_gpu_device_hash_map.{hpp,cpp}` | the CPU object: phases, CPU table, writer slots and claim on OFD locks, seal, images, mirror, takeover |
| `attach/nv_attach_impl/devhash/devhash.cu` | the device functions, shipped as `devhash_ptx.h`; `devhash_harness.cu`, the test kernels (`devhash_test_ptx.h`) |
| `nv_gpu_model_maps.cpp` | the copy (`make_device_hash_copy`), the hash snapshot module, the delivery |
| `attach/nv_attach_impl/nv_thread_local_routing.cpp` | the routing of 1533 calls and the devhash text's insertion |
| `runtime/src/attach/bpf_attach_ctx_cuda.cpp` | the pre-pass and the companion's binding |
| `attach/nv_attach_impl/trampoline/default_trampoline.cu` | the trampoline's 1533 answers (NULL, -1, -22) |

Regenerate the PTX headers after a change to their sources or to the layout header:

```bash
make -C attach/nv_attach_impl/devhash
make -C attach/nv_attach_impl/trampoline CLANG=clang++-16 ../trampoline_ptx.h
```

Tests: `[gpu-devhash-cpu]` (`runtime/unit-test/maps/test_gpu_device_hash.cpp`), `[devhash-layout]`
and `[hash-map-routing]` (`bpftime_nv_attach_tests`), `[gpu-devhash-device]` and its S5 part
`[gpu-devhash-s5]` (`runtime/unit-test/cuda/test_gpu_devhash_device.cpp`; every GPU configuration
runs in a child process under a deadline).
