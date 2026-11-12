#!/usr/bin/env python3
# Mutation checks for the AC3 watcher: each mutation must make a test fail.
import subprocess, shutil, sys, os
W = "/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate"
H = W + "/runtime/src/attach/cuda_watcher_policy.hpp"
C = W + "/runtime/src/attach/bpf_attach_ctx_cuda.cpp"
T = W + "/build-cuda/runtime/unit-test/bpftime_runtime_tests"
OUT = "/home/jetson/src/bpftime-gpu-work/ac3dev/mut3"
M = [
 ("M1 window closes on one long gap", H, "if (long_gaps >= 2)", "if (long_gaps >= 1)", ["[cuda-watcher-policy]"]),
 ("M2 window max 300 us", H, "constexpr uint64_t window_max_us = 50;", "constexpr uint64_t window_max_us = 300;", ["[cuda-watcher-policy]"]),
 ("M3 no late-sleep guard", H, "if (late_ewma > late_guard_us) {", "if (false) {", ["[cuda-watcher-policy]"]),
 ("M4 hits latch at any distance", H, "now_us - last_hit <= unseen_hit_window_us", "true", ["[cuda-watcher-policy]"]),
 ("M5 never unlatch", H, "if (is_latched && now_us - last_evidence > unlatch_after_us) {", "if (false) {", ["[cuda-watcher-policy]", "Requests from unnoted launches keep the watcher from parking until they stop"]),
 ("M6 noted launches ignored", C, "return inflight || latched;", "return latched;", ["[cuda-host-call]"]),
 ("M7 a note does not end the park", C, "\t\t\tactivity.wait_for_note(\n\t\t\t\tg0, std::chrono::microseconds(\n\t\t\t\t\t    w::park_backstop_us));", "\t\t\tstd::this_thread::sleep_for(std::chrono::microseconds(\n\t\t\t\tw::park_backstop_us));", ["[cuda-host-call]"]),
 ("M8 WFE on by default", C, 'if (v == nullptr || strcmp(v, "1") != 0)', 'if (v != nullptr && strcmp(v, "0") == 0)', ["[cuda-host-call]"]),
 ("M9 the loop never sizes the window", C, "window.on_request(gap);", "(void)gap;", ["[cuda-host-call]"]),
 ("M10 polls instead of 100 us sleeps", C, "sleep_us(w::slow_sleep_us);", "cpu_relax();", ["[cuda-host-call]"]),
 ("M11 the latch does not keep it from parking", C, "return inflight || latched;", "return inflight;", ["[cuda-host-call]"]),
 ("M12 WFE while latched", C, "const bool on_flag = inflight;", "const bool on_flag = inflight || was_latched;", ["[cuda-host-call]"]),
 ("M13 WFE queries kernel ends every 1 ms", C, "kernels_may_run(t, wfe_event_query_every_us);", "kernels_may_run(t, w::event_query_every_us);", ["[cuda-host-call]"]),
]
def build():
    r = subprocess.run(["nice","-n","5","timeout","3000","cmake","--build",W+"/build-cuda","--parallel","5","--target","bpftime_runtime_tests"], capture_output=True, text=True)
    return r.returncode, r.stdout[-2000:] + r.stderr[-2000:]
res = []
for name, f, a, b, tests in [m for m in M if m[0].split()[0] in (sys.argv[1:] or [m[0].split()[0] for m in M])]:
    src = open(f).read()
    assert src.count(a) == 1, (name, src.count(a))
    shutil.copy(f, f + ".mutbak")
    try:
        open(f, "w").write(src.replace(a, b))
        rc, log = build()
        if rc != 0:
            res.append((name, "BUILD FAILED", log[-500:])); continue
        outs = []
        failed = False
        for t in tests:
            r = subprocess.run(["timeout","600",T,t], capture_output=True, text=True)
            tail = [l for l in (r.stdout+r.stderr).splitlines() if "test cases" in l or "FAILED" in l or "with expansion" in l][-6:]
            outs.append(f"{t}: rc={r.returncode} " + " | ".join(tail))
            failed |= r.returncode != 0
            open(f"{OUT}/{name.split()[0]}.log","a").write(r.stdout+r.stderr)
        res.append((name, "CAUGHT" if failed else "NOT CAUGHT", "\n    ".join(outs)))
    finally:
        shutil.copy(f + ".mutbak", f); os.remove(f + ".mutbak")
    print(res[-1][0], res[-1][1], flush=True)
rc, log = build()
print("rebuild original rc", rc)
for r in res: print(r[0], "->", r[1], "\n   ", r[2])
