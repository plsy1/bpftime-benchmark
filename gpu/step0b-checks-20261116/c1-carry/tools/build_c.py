#!/usr/bin/env python3
"""build_c.py BPFDIR HELPERS_PTX OUTDIR

Part (c): the cost module. Every kernel has bench2's signature
(u64 buf, u64 mask, u32 R, u32 k, u64 aux, u32 mode) and an empty body
(ret;), as step0/e2-cost's reference kernels; the dev kprobe and kretprobe
passes add the probes. One eBPF source (src/probes.bpf.c, PROG_COST): entry
writes %globaltimer into its storage, exit reads it and counts a zero
(mismatch) or a NULL storage in a device counter; the storage differs only
by map type and routing:

  c_none      no probes                              (delta reference)
  c_empty     both probes, `return 0` (PROG_EMPTY)   (empty-probe baseline)
  c_value     entry calls the timer helper, stores nothing (PROG_VALUE): the
              work (1)-(3) share before their storage
  c_local     carry, helper form (wrapper call)      store = 1540
  c_local_d   carry, direct form (no call)           store = 1540
  c_slottag   (SM, warp slot, lane) slot + owner tag, device memory, via the
              wrapper, no carry routing              store = 1542
  c_1512      today's 1512 lookup in registered memory (plain helper 1, with
              its membar.sys), no routing            store = 1512
  c_ref       step0/e2-cost's reference: threadhist's exit-only
              lookup-and-increment (1512, registered), kretprobe only
  *_bad       sabotaged twins (PROG_COST + COST_BAD: entry writes 0), for
              validation only: every exit must count a mismatch
  *_noentry   the carry kernels with the exit probe only (no entry probe),
              validation only: the exit must find the valid mark (0) the
              kernel prologue wrote, in every thread
  *_noinit    the same without the prologue's mark, validation only and
              reported: shows whether stale bytes from earlier launches
              (c_local*, run just before on the same shape) reach the exit
              when nothing writes the mark (local memory is not cleared
              between kernels: measured, not documented)
  *_mark1     the exit-only direct kernel whose prologue writes 1 instead
              of 0: the exit must read it as valid (no mismatch) in every
              thread. With *_noentry (mark 0: a mismatch in every thread)
              it shows deterministically that the exit reads what the
              prologue wrote, so the valid-mark check can fail

Writes OUTDIR/c_module.ptx, c_input.ptx, c_manifest.txt (name store cnt
kind), c_manifest.json."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import c1lib as L  # noqa: E402

THREADHIST = "/home/jetson/src/bpftime-gpu-work/example/gpu/threadhist-llama/.output/threadhist.bpf.o"
CARRY_BYTES = 16
SIG = """(
	.param .u64 {n}_param_0,
	.param .u64 {n}_param_1,
	.param .u32 {n}_param_2,
	.param .u32 {n}_param_3,
	.param .u64 {n}_param_4,
	.param .u32 {n}_param_5
)"""
# name, program, kind (routing), store fd, cnt fd
KERNELS = [
    ("c_none", None, "none", 0, 0),
    ("c_empty", "empty", "plain", 8, 9),
    ("c_value", "value", "plain", 28, 29),
    ("c_local", "cost", "helper", 10, 11),
    ("c_local_d", "cost", "direct", 12, 13),
    ("c_slottag", "cost", "wrapper", 14, 15),
    ("c_1512", "cost", "plain", 16, 17),
    ("c_ref", "threadhist", "plain", 18, 0),
    ("c_local_bad", "cost_bad", "helper", 20, 21),
    ("c_local_d_bad", "cost_bad", "direct", 22, 23),
    ("c_slottag_bad", "cost_bad", "wrapper", 24, 25),
    ("c_1512_bad", "cost_bad", "plain", 26, 27),
    ("c_local_noentry", "cost", "helper-exitonly", 30, 31),
    ("c_local_d_noentry", "cost", "direct-exitonly", 32, 33),
    ("c_local_d_noentry_noinit", "cost", "direct-exitonly-noinit", 34, 35),
    ("c_local_d_noentry_mark1", "cost", "direct-exitonly-mark1", 36, 37),
]


def main():
    bpfdir, hptx, out = sys.argv[1:4]
    os.makedirs(out, exist_ok=True)
    inp = ".version 8.1\n.target sm_61\n.address_size 64\n\n"
    for k in KERNELS:
        inp += ".visible .entry " + k[0] + SIG.format(n=k[0]) + "\n{\n\tret;\n}\n\n"
    open(os.path.join(out, "c_input.ptx"), "w").write(inp)
    ptx = inp
    for name, prog, kind, sfd, cfd in KERNELS:
        if prog is None:
            continue
        if prog == "threadhist":
            _, wx, _ = L.program_words(THREADHIST, "kretprobe/", {"call_count": sfd})
            ptx = L.run_pass("kretprobe", ptx, name, wx)
            continue
        obj = os.path.join(bpfdir, prog + ".bpf.o")
        fds = {"store": sfd, "cnt": cfd, "rec": 0}
        _, we, _ = L.program_words(obj, "kprobe/", fds)
        _, wx, _ = L.program_words(obj, "kretprobe/", fds)
        if "-exitonly" not in kind:
            ptx = L.run_pass("kprobe", ptx, name, we)
        ptx = L.run_pass("kretprobe", ptx, name, wx)
    open(os.path.join(out, "c_pass_output.ptx"), "w").write(ptx)
    man = []
    for name, prog, kind, sfd, cfd in KERNELS:
        exitonly = "-exitonly" in kind
        init = "-noinit" not in kind
        mark = 1 if "-mark1" in kind else 0
        kind = kind.replace("-exitonly", "").replace("-noinit", "").replace("-mark1", "")
        probes = (["__probe_func__" + name] if not exitonly else []) + ["__retprobe_func__" + name]
        ent = {"name": name, "program": prog, "kind": kind, "store": sfd, "cnt": cfd,
               "routed": kind in ("helper", "direct"), "direct": kind == "direct",
               "has_entry": prog not in (None, "threadhist") and not exitonly}
        if kind in ("helper", "direct"):
            ptx, sites = L.route_kernel(ptx, name, CARRY_BYTES, probes, init=init, mark=mark)
            if sites != {p: 1 for p in probes}:
                raise SystemExit("%s: unexpected probe call sites %s" % (name, sites))
            ent["init_mark"] = init
            ent["mark_value"] = mark
            st = {}
            for p in probes:
                ptx, st[p] = L.route_probe_function(ptx, p, True, {sfd: L.MT_CARRY}, kind == "direct")
                n = st[p]["direct"] if kind == "direct" else st[p]["redirected"]
                if n != 1:
                    raise SystemExit("%s: %d store lookups routed" % (p, n))
            ent.update(call_sites=sites, probe_stats=st, carry_bytes=CARRY_BYTES)
        elif kind == "wrapper":
            st = {}
            for p in probes:
                ptx, st[p] = L.route_probe_function(ptx, p, False, {sfd: L.MT_SLOTTAG}, False)
                if st[p]["redirected"] != 1:
                    raise SystemExit("%s: store lookup not redirected" % p)
            ent.update(probe_stats=st)
        ent["kind"] = "none" if prog is None else ent["kind"]
        man.append(ent)
    final = L.assemble(ptx, L.helpers_ptx(hptx))
    open(os.path.join(out, "c_module.ptx"), "w").write(final)
    with open(os.path.join(out, "c_manifest.txt"), "w") as f:
        for m in man:
            f.write("%s %d %d %s\n" % (m["name"], m["store"], m["cnt"], m["kind"]))
    json.dump(man, open(os.path.join(out, "c_manifest.json"), "w"), indent=1)
    for m in man:
        print(m["name"], m["kind"], m.get("probe_stats", ""))


if __name__ == "__main__":
    main()
