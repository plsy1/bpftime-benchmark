#!/usr/bin/env python3
"""build_b.py BPFDIR KERNELS_PTX HELPERS_PTX OUTDIR

Part (b): the correctness module. For every kernel below, the dev kprobe
pass and then the dev kretprobe pass insert the probes (eBPF of
src/probes.bpf.c, PROG_CHK, one SALT per kernel family), and the routing
(tools/c1lib.py) adds the kernel-scope carry and passes its address to both
probe functions.

  helper form (kmr, kdiv, kspin, ks2a, ks2b): the probes' lookups of the
      store map go to _bpf_helper_ext_0001_c1, which picks the storage by
      map_info[store].map_type at run time (carry, slot, slot+tag, shared,
      lane, frame): one PTX, every variant.
  direct form (<name>_d): the store lookups are replaced by the carry address.

Writes OUTDIR/b_module.ptx, b_input.ptx, b_pass_output.ptx, b_manifest.txt
(one line per kernel: name rec_fd store_fd cnt_fd direct carry_bytes) and
b_manifest.json (with the routing statistics)."""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import c1lib as L  # noqa: E402

CARRY_BYTES = 16
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
# name, eBPF family, direct form
KERNELS = [
    ("kmr", "kmr", False), ("kdiv", "kdiv", False), ("kspin", "kspin", False),
    ("ks2a", "ks2a", False), ("ks2b", "ks2b", False),
    ("kmr_d", "kmr", True), ("kdiv_d", "kdiv", True), ("kspin_d", "kspin", True),
    ("ks2a_d", "ks2a", True), ("ks2b_d", "ks2b", True),
]
NVCC_KERNELS = ("kdiv", "kspin", "ks2a", "ks2b")


def fds_of(i):
    return {"rec": 16 + 4 * i, "store": 17 + 4 * i, "cnt": 18 + 4 * i}


def clone_entry(ptx, src, dst):
    b0, b1 = L.find_entry_body(ptx, src)
    m = re.search(r"\.visible\s+\.entry\s+" + re.escape(src) + r"\s*\(", ptx)
    text = ptx[m.start():b1]
    text = text.replace(".entry " + src + "(", ".entry " + dst + "(")
    text = text.replace(src + "_param_", dst + "_param_")
    return "\n" + text + "\n"


def main():
    bpfdir, kptx, hptx, out = sys.argv[1:5]
    os.makedirs(out, exist_ok=True)
    nv = L.strip_headers(open(kptx).read())
    kmr_t = open(os.path.join(SRC, "kmr.ptx.in")).read()
    inp = ".version 8.1\n.target sm_61\n.address_size 64\n\n" + nv
    for name, fam, direct in KERNELS:
        if name != fam and fam in NVCC_KERNELS:
            inp += clone_entry(nv, fam, name)
        elif fam == "kmr":
            inp += "\n" + kmr_t.replace("@K@", name) + "\n"
    open(os.path.join(out, "b_input.ptx"), "w").write(inp)
    ptx = inp
    for i, (name, fam, direct) in enumerate(KERNELS):
        obj = os.path.join(bpfdir, "chk_%s.bpf.o" % fam)
        fds = fds_of(i)
        _, we, _ = L.program_words(obj, "kprobe/", fds)
        _, wx, _ = L.program_words(obj, "kretprobe/", fds)
        ptx = L.run_pass("kprobe", ptx, name, we)
        ptx = L.run_pass("kretprobe", ptx, name, wx)
    open(os.path.join(out, "b_pass_output.ptx"), "w").write(ptx)
    man = []
    for i, (name, fam, direct) in enumerate(KERNELS):
        fds = fds_of(i)
        probes = ["__probe_func__" + name, "__retprobe_func__" + name]
        ptx, sites = L.route_kernel(ptx, name, CARRY_BYTES, probes)
        if sites[probes[0]] != 1 or sites[probes[1]] < 1:
            raise SystemExit("%s: unexpected probe call sites %s" % (name, sites))
        # kmr has one exit of each form the kretprobe pass handles (7 sites);
        # nvcc merges kdiv's returns into one ret (1 site)
        if fam == "kmr" and sites[probes[1]] != 7:
            raise SystemExit("%s: %d exit-probe sites, want 7" % (name, sites[probes[1]]))
        st = {}
        for p in probes:
            ptx, st[p] = L.route_probe_function(ptx, p, True, {fds["store"]: L.MT_CARRY}, direct)
            n = st[p]["direct"] if direct else st[p]["redirected"]
            if n != 1:
                raise SystemExit("%s: %d store lookups routed (want 1): %s" % (p, n, st[p]))
        man.append({"name": name, "family": fam, "direct": direct, "fds": fds,
                    "carry_bytes": CARRY_BYTES, "call_sites": sites, "probe_stats": st})
    helpers = L.helpers_ptx(hptx)
    final = L.assemble(ptx, helpers)
    open(os.path.join(out, "b_module.ptx"), "w").write(final)
    with open(os.path.join(out, "b_manifest.txt"), "w") as f:
        for m in man:
            f.write("%s %d %d %d %d %d\n" % (m["name"], m["fds"]["rec"], m["fds"]["store"],
                                             m["fds"]["cnt"], int(m["direct"]), m["carry_bytes"]))
    json.dump(man, open(os.path.join(out, "b_manifest.json"), "w"), indent=1)
    for m in man:
        print(m["name"], "direct" if m["direct"] else "helper", "exit sites",
              m["call_sites"]["__retprobe_func__" + m["name"]])


if __name__ == "__main__":
    main()
