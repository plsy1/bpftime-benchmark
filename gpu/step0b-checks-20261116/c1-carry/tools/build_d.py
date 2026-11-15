#!/usr/bin/env python3
"""build_d.py BPFDIR HELPERS_PTX OUTDIR

Part (d), compile only: the carry in a real llama.cpp module. Inputs (read
only) are the module with rms_norm that step 0 check 8 kept from a
threadhist-llama run (bpftime's own patching, BPFTIME_CUDA_KEEP_PTX=1):
  E8/rmsnorm-module-20261115-061844.unpatched.ptx  (as llama.cpp shipped it)
  E8/rmsnorm-module-20261115-061844.1.patched.ptx  (as bpftime patched it:
      trampoline, map_info, threadhist's exit probe on rms_norm<512,...>)

Variants, all on the kernel K below (rms_norm_f32<512,1,true,true,false,false>,
the one threadhist probed; launched with 512 threads per block):
  U         unpatched module, ptxas as is (the application's own numbers)
  P0        the kept patched module as is (today's probe, 1512 lookup)
  P0e       P0 + the dev kprobe pass with an empty program (no routing)
  P0c       P0 + the dev kprobe pass with the cost program (entry writes the
            carry) + routing to both probe calls, helper form: injection into
            the kept patched module itself; the existing exit probe gets the
            address and does not use it
  P0cd      the same, direct form
  E         unpatched + trampoline + kprobe(empty) + kretprobe(empty)
  N         unpatched + trampoline + kprobe(cost) + kretprobe(cost), no
            routing (store lookups stay plain helper-1 calls): same eBPF
  H16       N + routing, helper form, 16-B carry
  D16, D64, D256   N + routing, direct form, 16/64/256-B carry
Every variant is compiled with ptxas -arch=sm_87 -v (as bpftime's
nvPTXCompiler: same compiler, same target, default -O3); the kernel's
registers, stack frame, cumulative stack and spills are parsed from the log.
Writes OUTDIR/<variant>.ptx, .ptxas.txt, .cubin and OUTDIR/d_compile.json,
d_compile.txt. The occupancy is queried on the GPU later (build/occ, run_all.sh).
"""
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import c1lib as L  # noqa: E402

E8 = "/home/jetson/src/bpftime-gpu-work/step0/e8-package/ptx"
UNPATCHED = E8 + "/rmsnorm-module-20261115-061844.unpatched.ptx"
PATCHED = E8 + "/rmsnorm-module-20261115-061844.1.patched.ptx"
K = "_Z12rms_norm_f32ILi512ELi1ELb1ELb1ELb0ELb0EEvPKfPfiiilllfS1_lll5uint3S3_S3_S3_S1_lllS3_S3_S3_S3_f"
PTXAS = "/usr/local/cuda-13.2/bin/ptxas"
# fds of the C1 programs (the kept module's threadhist uses its own fd)
FDS = {"store": 901, "cnt": 902, "rec": 903}


def ptxas(ptx_path, cubin):
    p = subprocess.run([PTXAS, "-arch=sm_87", "-v", ptx_path, "-o", cubin],
                       capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def kernel_stats(log, kernel):
    """registers, stack frame, cumulative stack, spills of one kernel."""
    i = log.find("Compiling entry function '%s'" % kernel)
    if i < 0:
        return None
    seg = log[i:i + 2000]
    m1 = re.search(r"(\d+) bytes stack frame, (\d+) bytes spill stores, (\d+) bytes spill loads", seg)
    m2 = re.search(r"Used (\d+) registers[^\n]*", seg)
    if not (m1 and m2):
        return None
    m3 = re.search(r"(\d+) bytes cumulative stack size", m2.group(0))
    return {"regs": int(m2.group(1)), "stack_frame": int(m1.group(1)),
            "cumulative_stack": int(m3.group(1)) if m3 else 0, "spill_stores": int(m1.group(2)),
            "spill_loads": int(m1.group(3))}


def occupancy_blocks(regs, tpb=512, threads_sm=1536, regs_sm=65536, blocks_sm=16,
                     reg_unit=256, warp_alloc_unit=4):
    """Blocks per SM from registers and threads only (sm_87: 64 K registers,
    1,536 threads, 16 blocks per SM; registers allocated per warp in units of
    256). Shared memory is not a limit for this kernel (128 B static)."""
    per_warp = -(-regs * 32 // reg_unit) * reg_unit
    warps_regs = regs_sm // per_warp
    wpb = -(-tpb // 32)
    return min(threads_sm // tpb, warps_regs // wpb, blocks_sm)


def check_routed(name, sites, st, direct, entry_only):
    """As build_b/build_c: one entry-probe call site, at least one exit-probe
    call site, and exactly one store lookup routed per probe function that
    has the C1 program (entry only when the exit probe is the kept one)."""
    pe, px = "__probe_func__" + K, "__retprobe_func__" + K
    if sites.get(pe) != 1 or sites.get(px, 0) < 1:
        raise SystemExit("%s: unexpected probe call sites %s" % (name, sites))
    key = "direct" if direct else "redirected"
    want = {pe: 1, px: 0 if entry_only else 1}
    got = {p: st[p][key] for p in (pe, px)}
    if got != want:
        raise SystemExit("%s: routed store lookups %s, want %s" % (name, got, want))


def main():
    bpfdir, hptx, out = sys.argv[1:4]
    os.makedirs(out, exist_ok=True)
    cost = os.path.join(bpfdir, "cost.bpf.o")
    empty = os.path.join(bpfdir, "empty.bpf.o")
    helpers = L.helpers_ptx(hptx)
    unp = open(UNPATCHED).read()
    pat = open(PATCHED).read()
    if K not in pat or ("call __retprobe_func__" + K) not in pat:
        raise SystemExit("the kept patched module does not probe " + K)
    probes = ["__probe_func__" + K, "__retprobe_func__" + K]

    def words(obj, sec):
        return L.program_words(obj, sec, FDS)[1]

    variants = {}
    variants["U"] = unp
    variants["P0"] = pat
    # into the kept patched module (it already holds trampoline + switch):
    # the kprobe pass puts the new probe function in front of everything,
    # before the module header; as in bpftime's own order (trampoline,
    # switch, probe functions, kernels), it is moved behind the probe switch,
    # with the C1 helpers (helper form) in front of it.
    switch = L.PROBE_SWITCH_PTX.strip()

    def after_switch(x, extra):
        i = x.index("\n.version ")
        head, rest = x[:i], x[i:]
        j = rest.index(switch) + len(switch)
        return rest[:j] + "\n" + extra + "\n" + head + "\n" + rest[j:]

    variants["P0e"] = after_switch(L.run_pass("kprobe", pat, K, words(empty, "kprobe/")), "")
    p0c_in = L.run_pass("kprobe", pat, K, words(cost, "kprobe/"))
    stats = {}
    for name, direct in (("P0c", False), ("P0cd", True)):
        x = after_switch(p0c_in, "" if direct else helpers)
        x, sites = L.route_kernel(x, K, 16, probes)
        st = {}
        for p in probes:
            x, st[p] = L.route_probe_function(x, p, True, {FDS["store"]: L.MT_CARRY}, direct)
        # the new entry probe's store lookup is routed; the kept exit probe
        # (threadhist, its own map) is called with the address and has none
        check_routed(name, sites, st, direct, entry_only=True)
        variants[name] = x
        stats[name] = {"call_sites": sites, "probe_stats": st}
    # from the unpatched module, as bpftime builds a module
    e = L.run_pass("kprobe", unp, K, words(empty, "kprobe/"))
    e = L.run_pass("kretprobe", e, K, words(empty, "kretprobe/"))
    variants["E"] = L.assemble(e, "")
    n = L.run_pass("kprobe", unp, K, words(cost, "kprobe/"))
    n = L.run_pass("kretprobe", n, K, words(cost, "kretprobe/"))
    variants["N"] = L.assemble(n, helpers)
    for name, direct, cb in (("H16", False, 16), ("D16", True, 16), ("D64", True, 64), ("D256", True, 256)):
        x, sites = L.route_kernel(n, K, cb, probes)
        st = {}
        for p in probes:
            x, st[p] = L.route_probe_function(x, p, True, {FDS["store"]: L.MT_CARRY}, direct)
        check_routed(name, sites, st, direct, entry_only=False)
        variants[name] = L.assemble(x, helpers)
        stats[name] = {"call_sites": sites, "probe_stats": st, "carry_bytes": cb}
    res = {}
    for name, ptx in variants.items():
        path = os.path.join(out, name + ".ptx")
        open(path, "w").write(ptx)
        rc, log = ptxas(path, os.path.join(out, name + ".cubin"))
        open(os.path.join(out, name + ".ptxas.txt"), "w").write(log)
        ks = kernel_stats(log, K) if rc == 0 else None
        res[name] = {"ptxas_rc": rc, "kernel": ks,
                     "occ_blocks_512_arith": occupancy_blocks(ks["regs"]) if ks else None,
                     "errors": [l for l in log.split("\n") if "error" in l][:5]}
        res[name].update(stats.get(name, {}))
    json.dump({"kernel": K, "variants": res}, open(os.path.join(out, "d_compile.json"), "w"), indent=1)
    lines = ["variant  ptxas  regs  frame  cumulative_stack  spills  blocks/SM@512 (arithmetic)"]
    for name, r in res.items():
        k = r["kernel"] or {}
        lines.append("%-8s %-6s %-5s %-6s %-17s %-7s %s" % (
            name, "ok" if r["ptxas_rc"] == 0 else "FAIL", k.get("regs"), k.get("stack_frame"),
            k.get("cumulative_stack"), (k.get("spill_stores", 0) or 0) + (k.get("spill_loads", 0) or 0),
            r["occ_blocks_512_arith"]))
    open(os.path.join(out, "d_compile.txt"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
