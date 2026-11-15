#!/usr/bin/env python3
"""C3 SASS checks: a copy of step 0's e2-cost/sass_check.py (parse, loops,
count_in_loops, count_all and spec_for unchanged) with C3's checks added.

Usage: python3 -I sass_check.py build/kernels.sass build/ref OUT.json
  build/ref holds refF.sass, refN.sass, refX.sass.
Exit status 1 if any check fails.

A loop is a backward branch (BRA to a lower address); an instruction is "in
the loop" when its address lies between the branch target and the branch.
"""
import json
import os
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


FENCE_PAT = {1: r"MEMBAR\.SC\.SYS", 2: r"MEMBAR\.SC\.GPU", 3: r"MEMBAR\.ALL\.GPU",
             4: r"MEMBAR\.ALL\.SYS"}
LD32 = r"^LD\.E R\d+, \[R\d+\.64(\+0x4)?\]$"
ST32 = r"^ST\.E \[R\d+\.64(\+0x4)?\], R\d+$"
LD64 = r"^LD\.E\.64 R\d+, \[R\d+\.64\]$"
ST64 = r"^ST\.E\.64 \[R\d+\.64\], R\d+$"
SWITCH_LDG = r"^LDG\.E R\d+, \[R\d+\.64\]$"


def spec_c3(name):
    """C3 kernels in kernels.cu: [(pattern, min in loop)], [(pattern, min anywhere)], [forbidden]"""
    m = re.match(r"k_(fal32|fal1|st32|st1)_fk(\d)$", name)
    if m:
        kind, fk = m.group(1), int(m.group(2))
        # the fence, and the ERRBAR; CCTL.IVALL that ptxas emits after every
        # fence kind (README "SASS checks")
        need = [(FENCE_PAT[fk], 1), (r"^ERRBAR$", 1), (r"^CCTL\.IVALL$", 1)]
        forbid = [p for k, p in FENCE_PAT.items() if k != fk]
        if kind.startswith("st"):
            need.append((STG, 1))
        else:
            forbid.append(r"^(@!?P\d+ )?STG\.E\.64 \[R\d+\.64\], R\d+$")  # no store in the fence-alone loop
        return need, [], forbid
    m = re.match(r"k_slot_(cx|cxr)$", name)
    if m:
        acq = m.group(1) == "cx"
        # exit: the tag with a strong 8-B load (ld.relaxed.gpu), the state
        # with a weak 8-B load; no 16-B load of {tag, state}
        need = [(r"ATOMG\.E\.CAS\.64\.STRONG\.GPU", 1), (r"LDG\.E\.64\.STRONG\.GPU R", 1),
                (r"^(@!?P\d+ )?LDG\.E\.64 R", 1),
                (r"STG\.E\.64\.STRONG\.GPU \[R\d+\.64(\+-?0x[0-9a-f]+)?\], RZ", 1),
                (r"S2R R\d+, SR_VIRTUALSMID", 2), (r"S2R R\d+, SR_VIRTID", 2)]
        forbid = [r"LDG\.E\.128"]
        if acq:  # acquire CAS: L1 invalidate after it; release store: gpu fence before it
            need += [(r"CCTL\.IVALL", 1), (r"MEMBAR\.ALL\.GPU", 1)]
        else:
            forbid += [r"MEMBAR", r"CCTL"]
        return need, [(r"S2R R\d+, SR_LANEID", 2)], forbid
    m = re.match(r"k_(slot|tid|tidw)_(e|ex|excg)$", name)
    if m:
        addr, ex = m.group(1), m.group(2)
        need = [(r"STG\.E\.128 ", 1)]
        anywhere = []
        forbid = []
        n_ids = 2 if ex != "e" else 1
        if addr == "slot":
            need += [(r"S2R R\d+, SR_VIRTUALSMID", n_ids), (r"S2R R\d+, SR_VIRTID", n_ids)]
            anywhere.append((r"S2R R\d+, SR_LANEID", n_ids))
        else:
            forbid += [r"SR_VIRTUALSMID", r"SR_VIRTID"]
        if ex == "ex":
            need += [(r"LDG\.E\.128 R", 1), (r"ISETP\.NE\.U32\.AND", 1)]
        elif ex == "excg":
            need += [(r"LDG\.E\.128\.STRONG\.GPU R", 1), (r"ISETP\.NE\.U32\.AND", 1)]
        else:
            forbid.append(r"LDG\.E\.128")
        return need, anywhere, forbid
    return None


def ref_counts(ins):
    return {
        "calls": count_all(ins, r"CALL\.REL"),
        "membar_sc_sys": count_all(ins, MEMBAR),
        "membar_any": count_all(ins, r"MEMBAR"),
        "ld_e_32": count_all(ins, LD32),
        "st_e_32": count_all(ins, ST32),
        "ld_e_64": count_all(ins, LD64),
        "st_e_64": count_all(ins, ST64),
        "probe_switch_ldg": count_all(ins, SWITCH_LDG),
        "stl_key": count_all(ins, r"^STL \[R1\+0x1fc\], RZ$"),
        "calls_in_loop": count_in_loops(ins, r"CALL\.REL"),
        "ld_e_32_in_loop": count_in_loops(ins, LD32),
        "st_e_32_in_loop": count_in_loops(ins, ST32),
        "ld_e_64_in_loop": count_in_loops(ins, LD64),
        "st_e_64_in_loop": count_in_loops(ins, ST64),
        "switch_ldg_in_loop": count_in_loops(ins, SWITCH_LDG),
    }


def ref_ok(mod, name, c, F):
    """Expected SASS of each reference-module kernel (see ref/build_variants.py)."""
    rn = name.endswith("_rn")
    base = name.split("_")[0]
    memop32 = c["ld_e_32"] == 2 and c["st_e_32"] == 2
    memop32_loop = c["ld_e_32_in_loop"] >= 2 and c["st_e_32_in_loop"] >= 2
    if name == "ref_empty":
        return c["calls"] == 0 and c["membar_any"] == 0
    if mod in ("F", "N"):  # today's op (e2's check) / the same without the 1512 fence
        ok = c["calls"] >= 2 and c["ld_e_32"] >= 2 and c["st_e_32"] >= 2 and \
            (not rn or c["calls_in_loop"] >= 1)
        if mod == "F":
            ok = ok and c["membar_sc_sys"] >= 1
        else:
            ok = ok and c["membar_sc_sys"] == F[name]["membar_sc_sys"] - 1
        return ok
    if base == "E":
        return c["calls"] == 1 and c["membar_any"] == 0 and c["ld_e_32"] == 0 and \
            c["st_e_32"] == 0 and (not rn or c["calls_in_loop"] == 1)
    if base in ("I", "IF"):
        ok = c["calls"] == 1 and memop32 and c["probe_switch_ldg"] >= 1 and c["stl_key"] == 1 and \
            (not rn or c["calls_in_loop"] == 1)
        return ok and (c["membar_any"] == 0 if base == "I" else
                       (c["membar_sc_sys"] == 1 and c["membar_any"] == 1))
    if base == "IC":  # the helper is linked in for the counted slow path
        fname = "ref_%s_%s" % (name.split("_")[1], name.split("_")[2])
        return c["calls"] >= 2 and c["ld_e_32"] >= 2 and c["st_e_32"] >= 2 and c["probe_switch_ldg"] >= 1 and \
            c["membar_sc_sys"] == F[fname]["membar_sc_sys"] and (not rn or c["calls_in_loop"] >= 1)
    if base == "K":
        ok = c["calls"] == 0 and c["membar_any"] == 0 and c["probe_switch_ldg"] >= 1 and c["stl_key"] >= 1
        return ok and (memop32_loop and c["switch_ldg_in_loop"] >= 1 if rn else memop32)
    if base == "M":
        ok = c["calls"] == 0 and c["membar_any"] == 0 and c["probe_switch_ldg"] == 0
        return ok and (memop32_loop if rn else memop32)
    if base == "M64":
        ok = c["calls"] == 0 and c["membar_any"] == 0 and c["ld_e_32"] == 0
        return ok and (c["ld_e_64_in_loop"] >= 1 and c["st_e_64_in_loop"] >= 1 if rn else
                       c["ld_e_64"] == 1 and c["st_e_64"] == 1)
    return False


REF_KERNELS = {
    "F": ["ref_host_r1", "ref_dev_r1", "ref_host_rn", "ref_dev_rn", "ref_empty"],
    "N": ["ref_host_r1", "ref_dev_r1", "ref_host_rn", "ref_dev_rn", "ref_empty"],
    "X": ["E_r1", "E_rn"] + ["%s_%s_%s" % (v, h, f) for v in ("I", "IF", "IC", "K")
                             for h in ("host", "dev") for f in ("r1", "rn")] +
         ["M_r1", "M_rn", "M64_r1", "M64_rn", "ref_empty"],
}


def main():
    kfuncs = parse(sys.argv[1])
    refdir = sys.argv[2]
    report = {"kernels": {}, "ref": {}}
    ok_all = True
    for name, ins in sorted(kfuncs.items()):
        sp = spec_for(name)
        c3 = spec_c3(name) if sp is None else None
        if sp is None and c3 is None:
            continue
        if sp is not None:
            need, forbid = sp
            anywhere = []
        else:
            need, anywhere, forbid = c3
        res = {"loops": loops(ins), "checks": []}
        for pat, mn in need:
            n = count_in_loops(ins, pat)
            good = n >= mn
            res["checks"].append({"need_in_loop": pat, "min": mn, "found": n, "ok": good})
            ok_all &= good
        for pat, mn in anywhere:
            n = count_all(ins, pat)
            good = n >= mn
            res["checks"].append({"need_anywhere": pat, "min": mn, "found": n, "ok": good})
            ok_all &= good
        for pat in forbid:
            n = count_all(ins, pat)
            good = n == 0
            res["checks"].append({"forbid": pat, "found": n, "ok": good})
            ok_all &= good
        res["membar_total"] = count_all(ins, r"MEMBAR")
        report["kernels"][name] = res
    counts = {}
    for mod in ("F", "N", "X"):
        funcs = parse(os.path.join(refdir, "ref%s.sass" % mod))
        counts[mod] = {n: ref_counts(funcs.get(n, [])) for n in REF_KERNELS[mod]}
        missing = [n for n in REF_KERNELS[mod] if n not in funcs]
        assert not missing, (mod, missing)
    for mod in ("F", "N", "X"):
        for n, c in counts[mod].items():
            good = ref_ok(mod, n, c, counts["F"])
            c["ok"] = good
            ok_all &= good
            report["ref"]["%s:%s" % (mod, n)] = c
    # Controls: each check applied to a kernel that lacks what it looks for
    # must fail, or the check could not detect a missing operation.
    def c3_passes(spec_name, func):
        need, anywhere, forbid = spec_c3(spec_name)
        ins = kfuncs[func]
        return all(count_in_loops(ins, p) >= m for p, m in need) and \
            all(count_all(ins, p) >= m for p, m in anywhere) and \
            all(count_all(ins, p) == 0 for p in forbid)
    controls = {
        "gl check on the acq_rel.gpu kernel": c3_passes("k_st32_fk2", "k_st32_fk3"),
        "sys check on the gl kernel": c3_passes("k_fal32_fk1", "k_fal32_fk2"),
        "exit check on the entry-only kernel": c3_passes("k_slot_ex", "k_slot_e"),
        "hardware-slot check on the tid kernel": c3_passes("k_slot_e", "k_tid_e"),
        "acquire/release claim check on the relaxed claim": c3_passes("k_slot_cx", "k_slot_cxr"),
        "no-fence N check on F": ref_ok("N", "ref_host_r1", counts["F"]["ref_host_r1"], counts["F"]),
        "fence-free I check on IF": ref_ok("X", "I_host_r1", counts["X"]["IF_host_r1"], counts["F"]),
        "no-call K check on I": ref_ok("X", "K_host_r1", counts["X"]["I_host_r1"], counts["F"]),
        "memop-alone M check on K": ref_ok("X", "M_r1", counts["X"]["K_host_r1"], counts["F"]),
    }
    report["controls_pass_when_they_must_fail"] = controls
    for k, v in controls.items():
        print("control %-40s %s" % (k, "FAIL (check passed a kernel it must reject)" if v else "ok (rejected)"))
        ok_all &= not v
    report["ok"] = ok_all
    json.dump(report, open(sys.argv[3], "w"), indent=1)
    for n, r in report["kernels"].items():
        bad = [c for c in r["checks"] if not c["ok"]]
        print("%-30s %s membar=%d" % (n, "ok" if not bad else "FAIL " + json.dumps(bad), r["membar_total"]))
    for n, r in report["ref"].items():
        print("%-22s %s calls=%d membar.sc.sys=%d ld32=%d st32=%d ld64=%d st64=%d switch=%d stl=%d loop(call=%d ld32=%d st32=%d)" % (
            n, "ok" if r["ok"] else "FAIL", r["calls"], r["membar_sc_sys"], r["ld_e_32"], r["st_e_32"],
            r["ld_e_64"], r["st_e_64"], r["probe_switch_ldg"], r["stl_key"], r["calls_in_loop"],
            r["ld_e_32_in_loop"], r["st_e_32_in_loop"]))
    print("ALL OK" if ok_all else "SOME CHECKS FAILED")
    sys.exit(0 if ok_all else 1)


if __name__ == "__main__":
    main()
