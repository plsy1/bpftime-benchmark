# Ordinary hash delete regression investigation

Investigation COMPLETE: isolated single-call-site intervention restores native
inlining AND performance. 12 balanced ABC blocks: previous 37.0997 ns,
regression 43.7989 ns, diagnostic 36.2556 ns; diagnostic beats regression in
12/12 blocks. Correctness 42 cases / 110422 assertions and host contract
18 cases / 201 assertions pass. See RESULT.md and native-comparison.json.

User authorized investigation, not promotion of a production fix. Primary
worktree remains unchanged. Previous/current frozen binaries are immutable.

Evidence: isolated 12 AB/BA pairs reproduced 37.0534 -> 43.7309 ns/op (+18.02%).
Native disassembly: previous bpftime_map_delete_elem_helper includes the generic
delete dispatcher (size 0x24f4); current helper is 0xc4 and tail-calls a separate
map_delete_elem.constprop.0 (0x248c). New array_jit_execution_scope::erase also
calls this generic dispatcher. Hypothesis: adding that second caller changed
GCC 13/LTO inlining decisions, affecting the unrelated ordinary-hash hot path.

Diagnostic intervention ONLY in warm independent build:
/home/jetson/src/bpftime-percpu-array-jit-20261020/runtime/src/bpf_map/array_map_binding.hpp
Resolve array erase directly through its known ordinary/per-CPU concrete map
implementation instead of adding a second call to the generic all-map delete
dispatcher. No map layout, locking, FD ownership or default-on feature change.
Disassembly + correctness + paired timing support this causal localization.
Do not promote it to primary or claim other operations/full-suite passed on
the diagnostic version. Retain all data. Warm source differs from archived
current source at ONLY runtime/src/bpf_map/array_map_binding.hpp (erase body).

No perf command available; native/JIT disassembly and isolated paired runtime
experiments are the initial diagnostic method. No full ten-round run planned.
