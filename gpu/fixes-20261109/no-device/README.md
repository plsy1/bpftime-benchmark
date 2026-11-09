# fixes-20261109/no-device: a CUDA build's agent without a usable GPU

Board: Orin Nano 8 GB, CUDA 13.2, sm_87. Code: dev 0decdfab on top of af4a1a8d (the runs used
the same sources before the commit), build-cuda, never installed. Runs 2026-11-10 06:40-07:05 JST.
Found in the round's verification (`../verification/cuda-no-device`, `../verification/regression`).

## The problem

`bpf_attach_ctx`'s constructor makes the CUDA context in a CUDA build. `CUDAContext` asked the
driver for `CommSharedMem`'s device address and threw when there was none: in a process without a
usable GPU (cuInit fails: `CUDA_VISIBLE_DEVICES=` empty, a container without the device nodes) and
in a process that did not register the segment with CUDA (only an agent does). In the agent the
exception reached the catch-all around its initialization, which disabled the whole agent: CPU
probes too. funclatency under build-cuda's agent and server with `CUDA_VISIBLE_DEVICES=`:
1,123,346,000 calls, 0 counted (`../verification/cuda-no-device/run.out`). Before the round the
constructor threw there too (`cudaHostGetDevicePointer` on memory nobody registered: invalid
argument, `../verification/cuda-no-device/hostptr.out`). It also cleared `CommSharedMem`, the request
slot shared with the other processes, before knowing it could use it.

## The fix

`create_cuda_context` asks for the device address first; without one it logs a warning and
returns nothing, and the attach context comes up without CUDA attach ("CUDA attach is disabled in
this process; other probes run as usual"). The request slot is cleared only when the context is
made. `CUDAContext` takes the device address and no longer throws. A CUDA link in such a process
was already refused (`-ENODEV`).

## Test (`bpftime_runtime_tests`, `[cuda-no-device]`)

The test creates the segment as the syscall server does, with a CPU program and a request in
`CommSharedMem` (`flag1` 1, `request_id` 1234), and runs a child image with `CUDA_VISIBLE_DEVICES=`
empty that opens the segment as an agent (its registration fails at cuInit), makes the attach
context, instantiates the program from the handlers and runs it. Checked: the child exits 0, cuInit
failed there, the program returned 42, no device address was set, the request is untouched. It
needs no GPU.

Mutations (`mutations/run.sh`, `mutate.py`, `mut.out`; the source restored byte for byte):

| mutation | result |
|---|---|
| none | passes (15 assertions) |
| throw-again (no device address throws, as before) | the child exits 1 (the constructor threw) |
| memset-first (the slot cleared before the check) | `flag1` 0, `request_id` 0 |

## Other checks (`tests/`, `e2e/`)

On the rebuilt build-cuda, each alone (`tests/run.out`): `[cuda-no-device]`, `[cuda-host-call]`
(3 cases), `[cuda-ringbuf]` (2), the watcher test, and the four CPU tests that failed in the CUDA
build in the verification (two typed hash helper cases, two `[aot-cache]` cases): all pass now;
`bpftime_nv_attach_tests` 32/32.

funclatency on callloop under build-cuda's agent and server (`e2e/run.out`, the verification's
script again): GPU visible 6,815,000 calls counted 6,815,000; `CUDA_VISIBLE_DEVICES=` 6,845,000
counted 6,845,000 (0 before the fix), with the three lines the agent logs now (cuInit failed, no
device address, CUDA attach disabled).

The example matrix (`fix-matrix.sh nodev1`, default path, `matrix-summary.txt`): 19 of 19, as
before the fix.
