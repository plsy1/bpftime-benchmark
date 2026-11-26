# What the code is meant to provide (the contract, not an argument)

A hash map whose keys live in a device "key directory" (open addressing, 2^k slots,
at least twice max_entries) and whose values are u64 counters that GPU threads
only add to (each key's value is a row of 256 shards; the value is the shards' sum).

Operations, as in bpf (Linux htab), restricted for this stage:
- lookup(K): a pointer to the calling thread's shard of K's value if K is in the
  map, else NULL. bpf: a key inserted before the lookup started must be found; a
  key never inserted must not be found.
- update(K, 1, BPF_NOEXIST): EEXIST if K is in the map; E2BIG if K is not in the
  map and the map holds max_entries keys; otherwise K is inserted with value 1.
  In bpf an insert and the lookups/inserts around it behave as if they happened
  one at a time in some order consistent with real time (each bucket is locked).
- No deletes from GPU code in this stage. After the device copy is made, CPU
  update and delete return EPERM. So keys are never removed.
- Values are changed only by 64-bit atomic adds through lookup results.
- Documented deviation: an insert that meets K mid-insertion by another thread
  waits a bounded time; if the bound runs out it returns EEXIST, counted as
  "unstable" (the caller may then get NULL from lookup).
- A CPU-side reader (not in this file) sums the shards of live rows while kernels
  still run; k_reader in this file plays that role in the stress test.

The file under review is c5-device-part.cu (FAULT == 0 is the design; the other
FAULT values are deliberately broken controls; ignore them except as contrast).
The memory model is PTX ISA 9.5 chapter 8, in ptx-isa-9.5-ch8-memory-model.txt.
Target: sm_87, all accesses on .global addresses, scope .gpu.
