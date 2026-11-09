# fixes-20261109/daily-list: the CUDA attach round in the daily list (236 commits)

2026-11-10 07:10-07:56 JST, Orin Nano. The daily of 11-10 03:32 had migrated the 222-commit list to
official 4708f409 (upstream #709 merged) and pushed `dev/uprobe-official-4708f4092d1e` (d52301f2).
The round's 14 dev commits, e94e2c04 to 0decdfab, were not in the list.

## Replay

Cherry-picked onto d52301f2 in a separate worktree: 13 apply cleanly; acdd7a77 (load-time
substitution) conflicts with #709, which adds `clear_patched_state_for_next_session()` to the
late-bootstrap rerun in `create_attach_with_ebpf_callback`, the block acdd7a77 moves after
`load_guard.unlock()`. Resolved by keeping acdd7a77's structure with #709's call in the moved block
(it takes `late_bootstrap_mutex` and `load_substitution_mutex`, so it must run after the guard is
released): `resolve_acdd7a77.py`; the adapted commit 811b841f says so in its message. In the code
directories the result differs from dev 0decdfab by #709's two changes only.

The list names 811b841f in place of acdd7a77 and the dev commits for the other thirteen (they apply
cleanly), as the README's rule for adapted commits says; `[syscall-server-exit]` (17427685's test)
joins the focused filter (the frame test of e94e2c04 is in `[jit_load]`, already there). The daily
candidate branch was fast-forwarded to the replay, c115b33d; `migrate --official-sha 4708f409`
then finds all 236 applied and changes nothing. A replay of the whole list from scratch onto
4708f409 with rerere off applies all 236 and gives c115b33d's tree exactly.

## Candidate gate (`test-candidate`, daily settings: CUDA off, LLVM 15, LTO, checks off)

- First run (`test-candidate.log`): suites green (runtime 196 + 1 skipped, kernel_user, daemon 10,
  attach replace, Frida 26, transformer 54 + 1 expected failure, syscall trace 7); the smoke
  failed: funclatency counted 0 of 1,124,174,000 calls. The tool set its probes up at 07:26:35, 4 s
  after it started (shm at 07:26:31), and the smoke starts the workload 3 s after the tool, so the
  agent attached with nothing to attach. Nothing in the kernel log or the journal then; not
  explained.
- The smoke alone (`smoke-rerun1.log`): funclatency 4,042,000 of 4,042,000, memleak 9, detach rc 0;
  the tool's probes in the second it started.
- Second full run (`test-candidate-2.log`): all green, smoke funclatency 4,251,000 of 4,251,000,
  memleak 9, detach rc 0.

The daily's 04:14 smoke on the 222-commit candidate set the probes up in the same second too.

## CUDA side (`gpu-check.sh`, `gpu-check.out`, `gpu/`)

The daily builds without CUDA, so its gate checks only these commits' CPU side and that the CUDA
code applies. c115b33d built with CUDA (its own build-cuda, the dev build-cuda's settings):
`bpftime_nv_attach_tests` 32/32; `[cuda-no-device]`, `[cuda-host-call]`, `[cuda-ringbuf]` and the
watcher test pass. The example matrix (`fix-matrix.sh`, run.sh pointed at this build with
`GPU_BUILD`; the loader's maps checked to come from it):

| | dev af4a1a8d (`../robustness/runs`) | c115b33d (#709 in) |
|---|---|---|
| default path | 19/19 | 19/19 |
| late path (`BPFTIME_CUDA_LOAD_SUBSTITUTION=0`) | 8/19 | 10/19 |

On the late path #709 brings hmt-legacy and cc-legacy (kprobe + kretprobe on one kernel, problem 1)
to C[1] = 3.00, and glob-shared from globals 0 0 0 to 0 5 0 (its demangled lookup in the
memcpy-from-symbol mirror; the separate copies of problem 16 remain). The other late-path failures
are the known ones (kernel handles, stripped, static cudart).

Not done: the quiesce the maintainer asked for in #709 (waiting for in-flight patched launches
before the rerun unloads the previous module set; the last detach already waits).
