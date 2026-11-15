#!/usr/bin/env python3
"""Check that the fences, stores and atomics each C4 kernel is meant to have
survived compilation, in the right order (PTX and SASS).

Usage: python3 -I sass_check.py build/kernels.sass build/keep/kernels.ptx \
           build/pubc.sass build/pkeep/pubc.ptx OUT.json
Exit status 1 if any check fails.

SASS lowering on sm_87 (CUDA 13.2), from the probe in README:
  fence.sc.gpu = membar.gl = __threadfence  -> MEMBAR.SC.GPU; ERRBAR; CCTL.IVALL
  fence.acq_rel.gpu                         -> MEMBAR.ALL.GPU; ERRBAR; CCTL.IVALL
  atom.release.gpu                          -> MEMBAR.ALL.GPU; ERRBAR; ATOMG (no CCTL)
  fence.sc.sys = membar.sys                 -> MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL
  fence.acq_rel.sys                         -> MEMBAR.ALL.SYS; ERRBAR; CCTL.IVALL
  st.relaxed.sys                            -> STG.E.64.STRONG.SYS
  atom.relaxed.gpu.add.u64                  -> ATOMG.E.ADD.64.STRONG.GPU
"""
import json
import re
import sys

INS = re.compile(r"/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;")
KINDS = {
    "SCGPU": r"MEMBAR\.SC\.GPU",
    "ALLGPU": r"MEMBAR\.ALL\.GPU",
    "SCSYS": r"MEMBAR\.SC\.SYS",
    "ALLSYS": r"MEMBAR\.ALL\.SYS",
}
ATOM = r"ATOMG\.E\.ADD\.64\.STRONG\.GPU"
FLAG = r"STG\.E\.64\.STRONG\.SYS"
REC = r"^(@!?P\d+ )?STG\.E\.64 "
PRED_EXIT = r"^@!?P\d+ (EXIT|BRA)"
PTX_OPS = ["fence.sc.gpu", "fence.acq_rel.gpu", "fence.sc.sys", "fence.acq_rel.sys",
           "membar", "atom.release.gpu", "atom.acq_rel.gpu", "atom.relaxed.gpu", "st.relaxed.sys",
           "ld.acquire.sys", "ld.relaxed.sys", "bar.warp.sync"]


def parse_sass(path):
    funcs, cur = {}, None
    for line in open(path):
        m = re.search(r"Function : (\S+)", line)
        if m:
            cur = m.group(1)
            funcs[cur] = []
            continue
        if cur is None:
            continue
        m = INS.search(line)
        if m:
            funcs[cur].append((int(m.group(1), 16), m.group(2)))
    return funcs


def parse_ptx(path):
    funcs, cur = {}, None
    for line in open(path):
        m = re.match(r"\.visible \.entry (\w+)\(", line)
        if m:
            cur = m.group(1)
            funcs[cur] = {k: 0 for k in PTX_OPS}
            continue
        if cur is None:
            continue
        s = line.strip()
        for k in PTX_OPS:
            if s.startswith(k):
                funcs[cur][k] += 1
    return funcs


def addrs(ins, pat):
    return [a for a, t in ins if re.search(pat, t)]


def loops(ins):
    out = []
    for addr, text in ins:
        m = re.search(r"\bBRA\b(?:\s+\S+,)?\s+(?:0x)?([0-9a-f]+)", text)
        if m:
            tgt = int(m.group(1), 16)
            if tgt < addr:
                out.append((tgt, addr))
    return out


# cost kernels: (exact MEMBAR counts, contributor kind before the counter
# atomic, kinds after it in order, counter atomic?, flag store?)
COST = {
    "rec": ({}, None, [], False, False),
    "rf_scgpu": ({"SCGPU": 1}, None, [], False, False),
    "rf_argpu": ({"ALLGPU": 1}, None, [], False, False),
    "rf_scsys": ({"SCSYS": 1}, None, [], False, False),
    "rf_arsys": ({"ALLSYS": 1}, None, [], False, False),
    "pw_sc": ({"SCSYS": 1}, None, [], False, True),
    "pw_ar": ({"ALLSYS": 1}, None, [], False, True),
    "nofence": ({}, None, [], True, True),
    "nosys": ({"ALLGPU": 2}, "ALLGPU", ["ALLGPU"], True, True),
    "sysonly": ({"ALLGPU": 1, "SCSYS": 1}, None, ["ALLGPU", "SCSYS"], True, True),
    "tf_sc": ({"SCGPU": 1, "ALLGPU": 1, "SCSYS": 1}, "SCGPU", ["ALLGPU", "SCSYS"], True, True),
    "ar_sc": ({"ALLGPU": 2, "SCSYS": 1}, "ALLGPU", ["ALLGPU", "SCSYS"], True, True),
    "rel_sc": ({"ALLGPU": 2, "SCSYS": 1}, "ALLGPU", ["ALLGPU", "SCSYS"], True, True),
    "ar_ar": ({"ALLGPU": 1, "ALLSYS": 1}, "ALLGPU", ["ALLSYS"], True, True),
    "rel_ar": ({"ALLGPU": 1, "ALLSYS": 1}, "ALLGPU", ["ALLSYS"], True, True),
    "ar32_sc": ({"ALLGPU": 2, "SCSYS": 1}, "ALLGPU", ["ALLGPU", "SCSYS"], True, True),
    "ar32_ar": ({"ALLGPU": 1, "ALLSYS": 1}, "ALLGPU", ["ALLSYS"], True, True),
    "ar_aa": ({"ALLGPU": 2, "ALLSYS": 1}, "ALLGPU", ["ALLGPU", "ALLSYS"], True, True),
    "rmw_ar": ({"ALLGPU": 1, "ALLSYS": 1}, "ALLGPU", ["ALLSYS"], True, True),
    "ar32_aa": ({"ALLGPU": 2, "ALLSYS": 1}, "ALLGPU", ["ALLGPU", "ALLSYS"], True, True),
}
# PTX ops each variant must contain (at least once)
PTX_NEED = {
    "nosys": ["fence.acq_rel.gpu", "atom.relaxed.gpu", "st.relaxed.sys"],
    "sysonly": ["atom.relaxed.gpu", "fence.acq_rel.gpu", "fence.sc.sys"],
    "tf_sc": ["fence.sc.gpu", "atom.relaxed.gpu", "fence.acq_rel.gpu", "fence.sc.sys"],
    "ar_sc": ["fence.acq_rel.gpu", "atom.relaxed.gpu", "fence.sc.sys"],
    "rel_sc": ["atom.release.gpu", "fence.acq_rel.gpu", "fence.sc.sys"],
    "ar_ar": ["fence.acq_rel.gpu", "atom.relaxed.gpu", "fence.acq_rel.sys"],
    "rel_ar": ["atom.release.gpu", "fence.acq_rel.sys"],
    "ar32_sc": ["fence.acq_rel.gpu", "atom.relaxed.gpu", "fence.sc.sys"],
    "ar32_ar": ["fence.acq_rel.gpu", "atom.relaxed.gpu", "fence.acq_rel.sys"],
    "ar_aa": ["fence.acq_rel.gpu", "atom.relaxed.gpu", "fence.acq_rel.sys"],
    "rmw_ar": ["atom.acq_rel.gpu", "fence.acq_rel.sys"],
    "ar32_aa": ["fence.acq_rel.gpu", "atom.relaxed.gpu", "fence.acq_rel.sys"],
    "nofence": ["atom.relaxed.gpu", "st.relaxed.sys"],
    "none": ["atom.relaxed.gpu", "st.relaxed.sys"],
    "early": ["atom.relaxed.gpu", "st.relaxed.sys"],
}


def check_cost(name, ins, ptx):
    m = re.match(r"k_(w32|l8)_(\w+)$", name)
    if not m:
        m2 = re.match(r"k_fonly_(scgpu|argpu|scsys|arsys)$", name)
        if not m2:
            return None
        kind = {"scgpu": "SCGPU", "argpu": "ALLGPU", "scsys": "SCSYS", "arsys": "ALLSYS"}[m2.group(1)]
        res = []
        for k, p in KINDS.items():
            n = len(addrs(ins, p))
            res.append(("%s == %d" % (k, int(k == kind)), n == int(k == kind)))
        return res
    layout, var = m.group(1), m.group(2)
    counts, before, after, atom, flag = COST[var]
    res = []
    for k, p in KINDS.items():
        n = len(addrs(ins, p))
        res.append(("%s == %d (found %d)" % (k, counts.get(k, 0), n), n == counts.get(k, 0)))
    recs = addrs(ins, REC)
    need_rec = 4 if layout == "w32" else 1
    res.append(("record STG >= %d (found %d)" % (need_rec, len(recs)), len(recs) >= need_rec))
    a = addrs(ins, ATOM)
    res.append(("counter ATOMG %s (found %d)" % ("present" if atom else "absent", len(a)),
                (len(a) >= 1) if atom else (len(a) == 0)))
    f = addrs(ins, FLAG)
    res.append(("flag STG.STRONG.SYS %s (found %d)" % ("== 1" if flag else "absent", len(f)),
                (len(f) == 1) if flag else (len(f) == 0)))
    first_fence = min([x for p in KINDS.values() for x in addrs(ins, p)] or [1 << 30])
    if recs:
        res.append(("records before every fence/atomic", max(recs[:need_rec]) < min(first_fence, a[0] if a else 1 << 30)))
    if atom and a:
        atom0 = a[0]
        pre = [(x, k) for k, p in KINDS.items() for x in addrs(ins, p) if x < atom0]
        post = sorted((x, k) for k, p in KINDS.items() for x in addrs(ins, p) if x > atom0)
        res.append(("fence before the counter atomic: %s (found %s)" % (before, [k for _, k in pre]),
                    [k for _, k in pre] == ([before] if before else [])))
        res.append(("fences after the counter atomic, in order: %s (found %s)" % (after, [k for _, k in post]),
                    [k for _, k in post] == after))
        if var == "rmw_ar":
            cc = [x for x, t in ins if t.startswith("CCTL.IVALL") and x > atom0 and (not post or x < post[0][0])]
            res.append(("acq_rel RMW: CCTL.IVALL after the ATOMG, before the completer fence (found %d)" % len(cc), bool(cc)))
        if var in ("rel_sc", "rel_ar") and pre:
            between = [t for x, t in ins if pre[0][0] < x < atom0 and t.startswith("CCTL")]
            res.append(("release RMW: no CCTL between MEMBAR and ATOMG", not between))
        if post:
            gate = [t for x, t in ins if atom0 < x < post[0][0] and re.search(PRED_EXIT, t)]
            res.append(("completer branch (predicated EXIT/BRA) between atomic and completer fences", bool(gate)))
        if f:
            res.append(("flag store after every completer fence", all(f[0] > x for x, _ in post)))
        if var == "nofence":
            res.append(("no CCTL", not addrs(ins, r"^CCTL")))
    if flag and not atom and f:
        allf = [x for p in KINDS.values() for x in addrs(ins, p)]
        res.append(("flag store after the fence", all(f[0] > x for x in allf)))
    # PTX
    p = ptx.get(name)
    if p is None:
        res.append(("PTX entry found", False))
    else:
        for op in PTX_NEED.get(var, []):
            res.append(("PTX has %s (%d)" % (op, p[op]), p[op] >= 1))
        if layout == "l8" and var not in ("rec", "ar32_sc", "ar32_ar", "ar32_aa"):
            res.append(("PTX has bar.warp.sync (%d)" % p["bar.warp.sync"], p["bar.warp.sync"] >= 1))
        if var in ("ar32_sc", "ar32_ar", "ar32_aa"):
            res.append(("PTX has no bar.warp.sync (%d)" % p["bar.warp.sync"], p["bar.warp.sync"] == 0))
    return res


# pubc kernels: minimum MEMBAR counts inside the round loop and order rules
PUBC = {
    "ar_sc": ({"ALLGPU": 2, "SCSYS": 1}, "ALLGPU", "SCSYS"),
    "tf_sc": ({"SCGPU": 1, "ALLGPU": 1, "SCSYS": 1}, "SCGPU", "SCSYS"),
    "rel_sc": ({"ALLGPU": 2, "SCSYS": 1}, "ALLGPU", "SCSYS"),
    "ar_ar": ({"ALLGPU": 1, "ALLSYS": 1}, "ALLGPU", "ALLSYS"),
    "rel_ar": ({"ALLGPU": 1, "ALLSYS": 1}, "ALLGPU", "ALLSYS"),
    "ar32_sc": ({"ALLGPU": 2, "SCSYS": 1}, "ALLGPU", "SCSYS"),
    "ar32_ar": ({"ALLGPU": 1, "ALLSYS": 1}, "ALLGPU", "ALLSYS"),
    "ar_aa": ({"ALLGPU": 2, "ALLSYS": 1}, "ALLGPU", "ALLSYS"),
    "rmw_ar": ({"ALLGPU": 1, "ALLSYS": 1}, "ALLGPU", "ALLSYS"),
    "ar32_aa": ({"ALLGPU": 2, "ALLSYS": 1}, "ALLGPU", "ALLSYS"),
    "none": ({}, None, None),
    "sysonly": ({"ALLGPU": 1, "SCSYS": 1}, None, "SCSYS"),
    "nosys": ({"ALLGPU": 2}, "ALLGPU", None),
    "early": ({}, None, None),
}


# pubc: (contributor fences between the record stores and the counter atomic,
# completer fences between the counter atomic and the flag store), in order
PUBC_SEQ = {
    "ar_sc": (["ALLGPU"], ["ALLGPU", "SCSYS"]),
    "tf_sc": (["SCGPU"], ["ALLGPU", "SCSYS"]),
    "rel_sc": (["ALLGPU"], ["ALLGPU", "SCSYS"]),
    "ar_ar": (["ALLGPU"], ["ALLSYS"]),
    "rel_ar": (["ALLGPU"], ["ALLSYS"]),
    "ar32_sc": (["ALLGPU"], ["ALLGPU", "SCSYS"]),
    "ar32_ar": (["ALLGPU"], ["ALLSYS"]),
    "ar_aa": (["ALLGPU"], ["ALLGPU", "ALLSYS"]),
    "rmw_ar": (["ALLGPU"], ["ALLSYS"]),
    "ar32_aa": (["ALLGPU"], ["ALLGPU", "ALLSYS"]),
    "none": ([], []),
    "sysonly": ([], ["ALLGPU", "SCSYS"]),
    "nosys": (["ALLGPU"], ["ALLGPU"]),
}


def kinds_between(ins, lo, hi):
    return [k for _, k in sorted((x, k) for k, p in KINDS.items() for x in addrs(ins, p) if lo < x < hi)]


def check_pubc_order(layout, var, ins, inloop, a, f):
    """Review addition: the order of the round's operations in SASS."""
    res = []
    need = 4 if layout == "w32" else 1
    votes = [x for x, t in ins if t.startswith("VOTE.ALL") and inloop(x)]
    acq = [x for x, t in ins if re.search(r"LDG\.E\.64\.STRONG\.SYS", t) and inloop(x)]
    cctl = [x for x, t in ins if t.startswith("CCTL.IVALL") and inloop(x)]
    if var == "early":
        if a and votes:
            v = max([x for x in votes if x < a[0]] or [-1])
            res.append(("early: ld.acquire.sys (LDG.STRONG.SYS + CCTL.IVALL) before the warp vote",
                        any(any(x < y < v for y in cctl) for x in acq)))
        return res
    if not (a and f and votes):
        res.append(("order check: atomic, flag store and warp vote found in loop", False))
        return res
    a0 = a[0]
    v = max([x for x in votes if x < a0] or [-1])
    res.append(("warp vote (ack confirm) before the counter atomic", v >= 0))
    res.append(("ld.acquire.sys (LDG.STRONG.SYS + CCTL.IVALL) before the warp vote",
                any(any(x < y < v for y in cctl) for x in acq)))
    recs = [x for x in addrs(ins, REC) if v < x < a0]
    res.append(("record STGs between the ack confirm and the counter atomic >= %d (found %d)" % (need, len(recs)),
                len(recs) >= need))
    want_pre, want_post = PUBC_SEQ[var]
    if recs:
        pre = kinds_between(ins, max(recs), a0)
        res.append(("contributor fences between the last record STG and the atomic == %s (found %s)" % (want_pre, pre),
                    pre == want_pre))
        res.append(("no fence between the ack confirm and the last record STG",
                    not kinds_between(ins, v, max(recs))))
    fl = [x for x in f if x > a0]
    if fl:
        post = kinds_between(ins, a0, fl[0])
        res.append(("completer fences between the atomic and the flag store == %s (found %s)" % (want_post, post),
                    post == want_post))
    else:
        res.append(("flag store after the counter atomic", False))
    if var in ("rel_sc", "rel_ar"):
        m = [x for x in addrs(ins, KINDS["ALLGPU"]) if x < a0 and inloop(x)]
        res.append(("release RMW: no CCTL between its MEMBAR and the ATOMG",
                    bool(m) and not [y for y in cctl if max(m) < y < a0]))
    if var == "rmw_ar" and fl:
        m = [x for k, p in KINDS.items() for x in addrs(ins, p) if a0 < x < fl[0]]
        res.append(("acq_rel RMW: CCTL.IVALL after the ATOMG, before the completer fence",
                    bool(m) and any(a0 < y < min(m) for y in cctl)))
    return res


def check_pubc(name, ins, ptx):
    m = re.match(r"pc_(w32|l8)_(\w+)$", name)
    if not m:
        return None
    layout, var = m.group(1), m.group(2)
    mins, before, sysk = PUBC[var]
    res = []
    ls = loops(ins)
    inloop = lambda x: any(a <= x <= b for a, b in ls)
    for k, p in KINDS.items():
        n = len([x for x in addrs(ins, p) if inloop(x)])
        want = mins.get(k, 0)
        ok = (n >= want) if want else (n == 0)
        res.append(("%s in loop %s %d (found %d)" % (k, ">=" if want else "==", want, n), ok))
    a = [x for x in addrs(ins, ATOM) if inloop(x)]
    f = [x for x in addrs(ins, FLAG) if inloop(x)]
    res.append(("counter ATOMG in loop (found %d)" % len(a), len(a) >= 1))
    res.append(("flag STG.STRONG.SYS in loop (found %d)" % len(f), len(f) >= 1))
    acq = [x for x, t in ins if re.search(r"LDG\.E\.64\.STRONG\.SYS", t) and inloop(x)]
    res.append(("ack loads LDG.STRONG.SYS in loop (found %d)" % len(acq), len(acq) >= 2))
    if a and before:
        pre = [x for x in addrs(ins, KINDS[before]) if x < a[0] and inloop(x)]
        res.append(("contributor %s before the counter atomic" % before, bool(pre)))
    if a and sysk:
        post = [x for x in addrs(ins, KINDS[sysk]) if x > a[0]]
        res.append(("completer %s after the counter atomic and before the flag" % sysk,
                    bool(post) and bool(f) and min(post) < max(f)))
    if var == "early":
        recs = [x for x in addrs(ins, REC) if inloop(x)]
        res.append(("early: a record STG after the flag store", bool(f) and any(x > f[0] for x in recs)))
    res += check_pubc_order(layout, var, ins, inloop, a, f)
    p = ptx.get(name)
    if p is None:
        res.append(("PTX entry found", False))
    else:
        for op in PTX_NEED.get(var, []):
            res.append(("PTX has %s (%d)" % (op, p[op]), p[op] >= 1))
        res.append(("PTX has ld.acquire.sys (%d)" % p["ld.acquire.sys"], p["ld.acquire.sys"] >= 1))
    return res


def main():
    ksass, kptx, psass, pptx, outp = sys.argv[1:6]
    report = {"cost": {}, "pubc": {}, "ptx_counts": {}}
    ok_all = True
    kp, pp = parse_ptx(kptx), parse_ptx(pptx)
    report["ptx_counts"] = {**kp, **pp}
    for part, sass, ptx, fn in (("cost", ksass, kp, check_cost), ("pubc", psass, pp, check_pubc)):
        for name, ins in sorted(parse_sass(sass).items()):
            res = fn(name, ins, ptx)
            if res is None:
                continue
            good = all(ok for _, ok in res)
            ok_all &= good
            report[part][name] = {"ok": good, "checks": [{"check": c, "ok": ok} for c, ok in res]}
            bad = [c for c, ok in res if not ok]
            print("%-24s %s" % (name, "ok" if good else "FAIL: " + "; ".join(bad)))
    n = len(report["cost"]) + len(report["pubc"])
    report["ok"] = ok_all
    json.dump(report, open(outp, "w"), indent=1)
    print("%d kernels checked: %s" % (n, "ALL OK" if ok_all else "SOME CHECKS FAILED"))
    sys.exit(0 if ok_all else 1)


if __name__ == "__main__":
    main()
