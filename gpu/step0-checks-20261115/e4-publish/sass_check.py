#!/usr/bin/env python3
"""Check that each pub4 variant compiled to the fences and stores it claims.

Usage: python3 -I sass_check.py build/pub4.sass OUT.json [build/pub4.ptx]   (exit 1 on failure)
"""
import json
import re
import sys

INS = re.compile(r"/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;")


def parse(path):
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


def count(ins, pat):
    return sum(1 for _, t in ins if re.search(pat, t))


def first(ins, pat):
    for a, t in ins:
        if re.search(pat, t):
            return a
    return None


FLAG = r"STG\.E\.64\.STRONG\.SYS"          # flag store (st.relaxed/release.sys)
PAYLOAD = r"STG\.E\.64 \[R\d+\.64\+0x100\]"  # payload store (slot + 256)
ACKWAIT = r"LDG\.E\.64\.STRONG\.SYS R\d+, \[R\d+\.64\+0x80\]"  # ld.volatile ack

T1 = {  # variant: (MEMBAR.SC.SYS, MEMBAR.ALL.SYS, CCTL.IVALL)
    0: ("A", 1, 0, 1),
    1: ("B", 1, 0, 1),
    2: ("C", 0, 0, 0),
    3: ("D1", 0, 1, 0),
    4: ("D2", 0, 1, 1),
    5: ("E", 0, 0, 0),
    6: ("Cd", 0, 0, 0),
    7: ("Am", 1, 0, 1),
}
T2 = {  # variant: (CAS pattern, MEMBAR.SC.SYS count, CCTL count)
    0: ("cas", r"ATOMG\.E\.CAS\.64\.STRONG\.GPU", 1, 1),
    1: ("casfence", r"ATOMG\.E\.CAS\.64\.STRONG\.SYS", 2, 2),
    2: ("casacq", r"ATOMG\.E\.CAS\.64\.STRONG\.SYS", 1, 2),
}


PTX_OPS = {
    "bar.warp.sync": r"\bbar\.warp\.sync\b",
    "membar.sys": r"\bmembar\.sys\b",
    "st.release.sys": r"\bst\.release\.sys",
    "st.relaxed.sys": r"\bst\.relaxed\.sys",
    "fence.acq_rel.sys": r"\bfence\.acq_rel\.sys",
    "atom.acquire": r"\batom\.acquire",
    "ld.volatile": r"\bld\.volatile",
}


def ptx_counts(path):
    """Per .entry: how many times each memory-ordering op appears in the PTX."""
    out, cur = {}, None
    for line in open(path):
        m = re.search(r"\.entry\s+(\S+?)\(", line)
        if m:
            cur = m.group(1)
            out[cur] = {k: 0 for k in PTX_OPS}
            continue
        if cur:
            for k, pat in PTX_OPS.items():
                if re.search(pat, line):
                    out[cur][k] += 1
    return out


def main():
    funcs = parse(sys.argv[1])
    ptx = ptx_counts(sys.argv[3]) if len(sys.argv) > 3 else {}
    rep, ok = {}, True
    for name, ins in funcs.items():
        m = re.match(r"_Z9t([12])_kernelILi(\d)E", name)
        if not m:
            continue
        test, v = int(m.group(1)), int(m.group(2))
        r = {"membar_sc_sys": count(ins, r"MEMBAR\.SC\.SYS"),
             "membar_all_sys": count(ins, r"MEMBAR\.ALL\.SYS"),
             "cctl_ivall": count(ins, r"CCTL\.IVALL"),
             "flag_stores": count(ins, FLAG)}
        if test == 1:
            vn, sc, al, cc = T1[v]
            r["variant"] = vn
            r["ack_wait_volatile_loads"] = count(ins, ACKWAIT)
            r["payload_stores"] = count(ins, PAYLOAD)
            fa, pa = first(ins, FLAG), first(ins, PAYLOAD)
            r["flag_before_payload"] = fa is not None and pa is not None and fa < pa
            if vn == "Am":  # the fence is timed: globaltimer read before and after it
                mb = first(ins, r"MEMBAR\.SC\.SYS")
                gts = [a for a, t in ins if re.search(r"SR_GLOBALTIMER", t)]
                r["timer_before_fence"] = any(a < mb for a in gts) if mb is not None else False
                r["timer_after_fence_before_flag"] = (mb is not None and fa is not None and
                                                      any(mb < a < fa for a in gts))
            good = (r["membar_sc_sys"] == sc and r["membar_all_sys"] == al and
                    r["cctl_ivall"] == cc and r["flag_stores"] == 1 and
                    r["ack_wait_volatile_loads"] >= 1 and r["payload_stores"] >= 1 and
                    r["flag_before_payload"] == (vn == "E") and
                  (vn != "Am" or (r["timer_before_fence"] and r["timer_after_fence_before_flag"])))
        else:
            vn, cas, sc, cc = T2[v]
            r["variant"] = vn
            r["cas"] = count(ins, cas)
            good = r["cas"] >= 1 and r["membar_sc_sys"] == sc and r["cctl_ivall"] == cc
        if name in ptx:
            r["ptx"] = ptx[name]
            # the protocol's bar.warp.sync must be in the PTX even where
            # the converged SASS path has no barrier instruction
            good = good and ptx[name]["bar.warp.sync"] >= 2
        r["ok"] = good
        ok &= good
        rep["t%d_%s" % (test, vn)] = r
    rep["ok"] = ok
    json.dump(rep, open(sys.argv[2], "w"), indent=1)
    if len(sys.argv) > 3 and not ptx:
        print("no .entry found in", sys.argv[3])
    for k in sorted(rep):
        if k != "ok":
            print(k, json.dumps(rep[k]))
    print("ALL OK" if ok else "SOME CHECKS FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
