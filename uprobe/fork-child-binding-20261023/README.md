# a90fa447: fork children execute inherited array JIT programs (2026-10-23)

## Bug

Since the array lookup JIT became the default (`c0939bd`, 10/21), a program bound to arrays
returned ENOTSUP in fork children: an atfork marker made inherited bindings "borrowed".
Forking servers that attach in the parent and serve in children lost every event. Found on
ssl-nginx: sslsniff (it has a per-CPU array) recorded 0 events/request with 72aebadd and
20870a03; the agent logged `Unable to run ebpf callback: -95` in the nginx worker.

## Fix

The atfork prepare handler takes one shared map reference per live binding for the child,
under a process-local lock that also serializes binding creation and release. The reference
counter is in shared memory, so the parent keeps the extra reference; the child releases its
inherited copies normally. A failed fork, or a child that execs or _exits without destructors,
strands those references (same class as abrupt termination). The ENOTSUP check in
`bpftime_prog_exec` is removed. `CLAUDE.md` contract updated.

## Validation

| Check | Result |
|---|---|
| `[array-jit-lifetime]` | 26 cases / 2651 assertions (two fork tests: child executes, counts balance; child keeps the map after the parent releases it) |
| Daily filter (`tests.log`) | 78 cases / 239653 assertions |
| `[perf_event]` | 4 cases / 8335 assertions |
| ssl-nginx audit | 2.000 events/request, same as kernel (`../ssl-nginx-2x2-20261023`) |
| Full ten rounds (`full10-a90fa447`) | map ops and probes within ±1% of 20870a03, except hash lookup +2.2%, hash update +5.0% |
| AB/BA screen vs 20870a03 (`screen/`, 12 rounds) | hash update +2.5% (3/12 faster), hash lookup +1.2%, uprobe +1.2%, rest within ±1% |

The hash helpers and `bpftime_hash_map::elem_update` have identical instructions in both
builds; only their addresses moved (e.g. the hash update helper from a 64-byte boundary to
offset 48). `bpftime_prog_exec` is 80 bytes shorter. This is the known code-layout effect, not
a logic cost; kept because the fix is a correctness fix.
