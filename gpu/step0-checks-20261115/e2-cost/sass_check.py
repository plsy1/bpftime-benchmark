#!/usr/bin/env python3
"""Check that the operations each e2-cost kernel is meant to time survived
compilation, inside the R loop.

Usage: python3 -I sass_check.py build/kernels.sass build/ref/ref1512.sass OUT.json
Exit status 1 if any check fails.

A loop is a backward branch (BRA to a lower address); an instruction is "in
the loop" when its address lies between the branch target and the branch.
"""
import json
import re
import sys

INS = re.compile(r"/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;")


def parse(path):
    funcs = {}
    cur = None
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


def loops(ins):
    out = []
    for addr, text in ins:
        m = re.search(r"\bBRA\b(?:\s+\S+,)?\s+(?:0x)?([0-9a-f]+)", text)
        if m:
            tgt = int(m.group(1), 16)
            if tgt < addr:
                out.append((tgt, addr))
    return out


def count_in_loops(ins, pat):
    ls = loops(ins)
    n = 0
    for addr, text in ins:
        if re.search(pat, text) and any(a <= addr <= b for a, b in ls):
            n += 1
    return n


def count_all(ins, pat):
    return sum(1 for _, t in ins if re.search(pat, t))


STG = r"^(@!?P\d+ )?STG\.E\.64 "
MEMBAR = r"MEMBAR\.SC\.SYS"


def spec_for(name):
    """[(pattern, min in loop, exact?)], [forbidden anywhere]"""
    m = re.match(r"k_(.*)_f([01])$", name)
    if not m:
        return None
    case, f = m.group(1), int(m.group(2))
    need, forbid = [], []
    if f:
        need.append((MEMBAR, 1))
    else:
        forbid.append("MEMBAR")
    if case in ("st_lane32", "st_lane1", "rec_lane4"):
        need.append((STG, 1))
    elif case in ("rec_lane0", "rec_thread"):
        need.append((STG, 4))
    elif case in ("atom_lane", "atom_warp_all", "atom_warp_leader", "atom_sm"):
        need.append((r"ATOMG\.E\.ADD\.64\.STRONG\.GPU", 1))
    elif case == "atom_warp_leader_sys":
        need.append((r"ATOMG\.E\.ADD\.64\.STRONG\.SYS", 1))
    elif case == "red_warp_leader":
        need.append((r"^(@!?P\d+ )?RED\.E\.ADD\.64\.STRONG\.GPU", 1))
        forbid.append("ATOMG")
    elif case == "cas_sm":
        need.append((r"ATOMG\.E\.CAS\.64\.STRONG\.GPU", 1))
        need.append((r"LDG\.E\.64\.STRONG\.SYS", 1))
    elif case == "comp_rec":
        need.append((r"ATOMG\.E\.ADD\.64\.STRONG\.GPU", 1))
        need.append((STG, 1))
        need.append((r"STG\.E\.64\.STRONG\.SYS", 1))
    return need, forbid


def main():
    kfuncs = parse(sys.argv[1])
    rfuncs = parse(sys.argv[2])
    report = {"kernels": {}, "ref": {}}
    ok_all = True
    for name, ins in sorted(kfuncs.items()):
        sp = spec_for(name)
        if sp is None:
            continue
        need, forbid = sp
        res = {"loops": loops(ins), "checks": []}
        for pat, mn in need:
            n = count_in_loops(ins, pat)
            good = n >= mn
            res["checks"].append({"need_in_loop": pat, "min": mn, "found": n, "ok": good})
            ok_all &= good
        for pat in forbid:
            n = count_all(ins, pat)
            good = n == 0
            res["checks"].append({"forbid": pat, "found": n, "ok": good})
            ok_all &= good
        res["membar_total"] = count_all(ins, MEMBAR)
        report["kernels"][name] = res
    # reference: today's path. r1 calls the probe; rn calls it in a loop;
    # the helper (inlined into the call tree of each entry) holds the
    # membar.sys of the 1512 branch; the probe body does 2 x LD.E + 2 x ST.E.
    for name in ("ref_host_r1", "ref_dev_r1", "ref_host_rn", "ref_dev_rn"):
        ins = rfuncs.get(name, [])
        res = {
            "calls": count_all(ins, r"CALL\.REL"),
            "membar_sc_sys": count_all(ins, MEMBAR),
            "ld_e_32": count_all(ins, r"^LD\.E R\d+, \[R\d+\.64(\+0x4)?\]$"),
            "st_e_32": count_all(ins, r"^ST\.E \[R\d+\.64(\+0x4)?\], R\d+$"),
            "probe_switch_ldg": count_all(ins, r"^LDG\.E R\d+, \[R\d+\.64\]$"),
        }
        if name.endswith("rn"):
            res["call_in_loop"] = count_in_loops(ins, r"CALL\.REL")
        good = res["calls"] >= 2 and res["membar_sc_sys"] >= 1 and \
            res["ld_e_32"] >= 2 and res["st_e_32"] >= 2 and \
            (not name.endswith("rn") or res["call_in_loop"] >= 1)
        res["ok"] = good
        ok_all &= good
        report["ref"][name] = res
    report["ok"] = ok_all
    json.dump(report, open(sys.argv[3], "w"), indent=1)
    for n, r in report["kernels"].items():
        bad = [c for c in r["checks"] if not c["ok"]]
        print("%-30s %s membar=%d" % (n, "ok" if not bad else "FAIL " + json.dumps(bad), r["membar_total"]))
    for n, r in report["ref"].items():
        print("%-30s %s" % (n, json.dumps(r)))
    print("ALL OK" if ok_all else "SOME CHECKS FAILED")
    sys.exit(0 if ok_all else 1)


if __name__ == "__main__":
    main()
