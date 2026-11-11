#!/usr/bin/env python3
"""Mutation checks for layer 1: each mutation applied to the worktree, test binary rebuilt, the
new tests run (must fail), source restored byte for byte. mut.py -> log on stdout."""
import subprocess, shutil, sys, time
from pathlib import Path
S = Path("/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate")
B = Path("/home/jetson/src/bpftime-gpu-work/l1/build-cuda")
CU = "attach/nv_attach_impl/trampoline/default_trampoline.cu"
CTX = "runtime/src/attach/bpf_attach_ctx_cuda.cpp"
UT = "attach/nv_attach_impl/nv_attach_utils.cpp"
TESTS = ["The GPU reads and writes ordinary array maps without host calls",
         "GPU threads read bpf_get_current_pid_tgid from their module",
         "The host answers bpf_get_current_pid_tgid with the tgid in both halves",
         "An array map the GPU addresses stays allocated after its fd is closed"]
M = [
 ("M1 no device ARRAY lookup/update", CU, [
   ("if (map_info.map_type == BPF_MAP_TYPE_ARRAY && map_info.extra_buffer) {\n\t\tauto real_key = *(uint32_t *)(uintptr_t)key;\n\t\tif (real_key >=",
    "if (0) {\n\t\tauto real_key = *(uint32_t *)(uintptr_t)key;\n\t\tif (real_key >="),
   ("if (map_info.map_type == BPF_MAP_TYPE_ARRAY && map_info.extra_buffer) {\n\t\tauto real_key = *(uint32_t *)(uintptr_t)key;\n\t\tconst uint64_t base_flags",
    "if (0) {\n\t\tauto real_key = *(uint32_t *)(uintptr_t)key;\n\t\tconst uint64_t base_flags")]),
 ("M2 no device address for arrays", CTX, [("gpu_buffer = gpu_address_of_array_values((int)i);", "(void)0;")]),
 ("M3 BPF_NOEXIST accepted", CU, [("base_flags > 2 /*BPF_EXIST*/ || flags == 1 /*BPF_NOEXIST*/ ||", "base_flags > 2 /*BPF_EXIST*/ ||")]),
 ("M4 helper ignores the module's pid_tgid", CU, [("if (pid_tgid != 0)\n\t\treturn pid_tgid;", "if (pid_tgid == 1)\n\t\treturn pid_tgid;")]),
 ("M5 writer writes nothing", UT, [("\tCUdeviceptr ptr;\n\tsize_t size;\n\tif (cuModuleGetGlobal(&ptr, &size, (CUmodule)cu_module,", "\tCUdeviceptr ptr;\n\tsize_t size;\n\treturn true;\n\tif (cuModuleGetGlobal(&ptr, &size, (CUmodule)cu_module,")]),
 ("M7 array map not pinned", CTX, [("\tgpu_array_bindings.emplace(binding->raw_data(), std::move(binding));\n", "")]),
 ("M6 host answers the watcher's tid", CTX, [("resp.result = attach::trampoline_pid_tgid();", "resp.result = (attach::trampoline_pid_tgid() & ~0xffffffffULL) | (uint32_t)gettid();")]),
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
        err = build(f == CU)
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
