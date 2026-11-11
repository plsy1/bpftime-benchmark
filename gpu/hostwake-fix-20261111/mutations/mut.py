#!/usr/bin/env python3
"""Mutation checks for the watcher change: each mutation applied to the worktree, test binary rebuilt, the
new tests run (must fail), source restored byte for byte. mut.py -> log on stdout."""
import subprocess, shutil, sys, time
from pathlib import Path
S = Path("/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate")
B = Path("/home/jetson/src/bpftime-gpu-work/wake/build-cuda")
CU = "attach/nv_attach_impl/trampoline/default_trampoline.cu"
CTX = "runtime/src/attach/bpf_attach_ctx_cuda.cpp"
UT = "attach/nv_attach_impl/nv_attach_utils.cpp"
ACT = "attach/nv_attach_impl/probed_launch_activity.cpp"
TESTS = ["The host-call watcher answers at once while a probed kernel runs",
         "A noted launch into a stream being captured leaves the capture valid",
         "Every GPU thread's host call carries its own arguments"]
M = [
 ("W1 no WFE waiting", CTX, [("const bool wfe = cuda_watcher_can_wait_on_flag();", "const bool wfe = false;")]),
 ("W2 never back to sleeping", CTX, [("if (!activity.any_running()) {", "if (false) {")]),
 ("W3 a noted launch does not end the sleep", CTX, [("activity.wait_for_note(seen_launches, idle_sleep);", "std::this_thread::sleep_for(idle_sleep);")]),
 ("C1 notes a stream being captured", ACT, [("	if (cuStreamIsCapturing(stream, &capture) != CUDA_SUCCESS ||\n	    capture != CU_STREAM_CAPTURE_STATUS_NONE)\n		return;", "	(void)capture;")]),
 ("W4 note() does not count", ACT, [("notes.fetch_add(1, std::memory_order_seq_cst);", "(void)0;")]),
]
def run(cmd, **kw):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, **kw)
def build(regen):
    if regen:
        r = run("make -C attach/nv_attach_impl/trampoline CLANG=clang++-16 ../trampoline_ptx.h", cwd=S)
        if r.returncode: return "regen failed: " + r.stderr[-500:]
    r = run(f"nice -n 5 timeout 1800 cmake --build {B} --parallel 5 --target bpftime_runtime_tests")
    return None if r.returncode == 0 else "build failed: " + r.stdout[-1500:]
orig = {}
ONLY = sys.argv[1:]
for name, f, reps in M:
    if ONLY and name.split()[0] not in ONLY: continue
    p = S / f
    text = p.read_text(); orig.setdefault(f, text)
    ptx_before = (S / "attach/nv_attach_impl/trampoline_ptx.h").read_text()
    new = text
    for a, b in reps:
        assert new.count(a) == 1, (name, a[:60])
        new = new.replace(a, b)
    p.write_text(new)
    try:
        err = build(False)
        if err:
            print(name, "BUILD-ERROR", err, flush=True); continue
        for t in TESTS:
            r = run(f'timeout 300 {B}/runtime/unit-test/bpftime_runtime_tests "{t}" 2>&1 | tail -3')
            res = "PASS" if "All tests passed" in r.stdout else ("FAIL" if "failed" in r.stdout else "OTHER:" + r.stdout[-200:])
            print(f"{name} | {t} | {res}", flush=True)
    finally:
        p.write_text(text)
        (S / "attach/nv_attach_impl/trampoline_ptx.h").write_text(ptx_before)
# rebuild the real thing
err = build(False)
print("restored build:", err or "ok", flush=True)
r = run("git -C %s diff --stat | tail -1" % S); print(r.stdout)
