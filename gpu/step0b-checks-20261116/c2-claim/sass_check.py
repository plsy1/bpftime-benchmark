#!/usr/bin/env python3
"""sass_check.py BUILD_DIR: confirm that the operations check C2 means survive compilation.

PTX (BUILD_DIR/kernels.ptx, per helper instantiation c2_entry<P,ORD,X,CHK> / c2_exit<...>):
  - ORD = 1: the claim's acquire (atom.acquire ...or / ld.acquire for stacks / atom.acquire ...cas.b32
    for lf), the leader's release add, the lanes' ld.acquire wait, the acq_rel decrement and the
    release (atom.release ...and / atom.release ...cas / st.release ...u32) are present;
  - ORD = 0: no .acquire/.release/.acq_rel qualifier at all;
  - warp claims: activemask.b32 present unless X = 3 or 4 (constructed mask), shfl.sync.idx present;
  - X = 1 (agg): match.any.sync and bar.warp.sync present;
  - checker builds (CHK = 1) of warp claims: the leader's ledger atom.exch comes before its claim
    add, so each lane's ld.acquire of that add also orders the ledger value.
SASS (BUILD_DIR/kernels.sass, per kernel; noinline helpers sit inside the kernel's section):
  - every c_* / x_div|full|hold_* kernel calls its helpers with CALL.REL.NOINC (not inlined);
  - ORD kernels hold MEMBAR.ALL.GPU and CCTL.IVALL, relaxed kernels hold neither;
  - the expected atomics: ATOMG.E.OR.64 + ATOMG.E.AND.64 (bitmaps), ATOMG.E.CAS.64 (stacks),
    ATOMG.E.CAS (32-bit, lf) + STG.E.STRONG.GPU (lf release);
  - warp claims with per-lane exit accounting: at least one ATOMG.E.ADD.64 that is NOT the
    compiler's warp-aggregated form (no VOTEU.ANY/UFLO election in the 8 instructions before it),
    i.e. the per-lane decrement is one atomic per lane;
  - the owner-tag check loads are strong: LDG.E.64.STRONG.GPU present;
  - c_wid reads SR_VIRTUALSMID and SR_VIRTID; x_mask_loop_inl has no CALL to the reproducer helper.
Also BUILD_DIR/kernels_jit.sass (the JIT-path PTX assembled offline): the same SASS rules.
Writes BUILD_DIR/sass_check.json; prints one line per item and RESULT PASS/FAIL. Exit 1 on FAIL.
"""
import json
import os
import re
import sys

B = sys.argv[1] if len(sys.argv) > 1 else "build"
INS = re.compile(r"/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;")
POOLS = {0: "bmsm", 1: "bmglob", 2: "stksm", 3: "stkglob", 4: "stksmnv", 5: "stkglobnv",
         6: "lnsm", 7: "lnglob", 8: "lfsm", 9: "lfglob"}
results = []
fails = 0


def rep(ok, what, detail=""):
    global fails
    results.append({"ok": ok, "item": what, "detail": detail})
    if not ok:
        fails += 1
    print(("PASS " if ok else "FAIL ") + what + (": " + detail if detail else ""))


# ---- PTX, per helper instantiation -----------------------------------------------------------
def ptx_funcs(path):
    text = open(path).read()
    out = {}
    for m in re.finditer(r"^\.func[^\n]*?(_Z\d+c2_(entry|exit)ILi(\d+)ELi(\d+)ELi(\d+)ELi(\d+)E\w*)\(",
                         text, re.M):
        start = m.start()
        end = text.find("\n}\n", start)
        out[m.group(1)] = (m.group(2), int(m.group(3)), int(m.group(4)), int(m.group(5)),
                           int(m.group(6)), text[start:end])
    return out


def check_ptx(path):
    funcs = ptx_funcs(path)
    rep(len(funcs) > 0, "ptx: helper instantiations found", "%d in %s" % (len(funcs), path))
    for name, (kind, P, ORD, X, CHK, body) in sorted(funcs.items()):
        tag = "ptx %s<%s,ORD=%d,X=%d,CHK=%d>" % (kind, POOLS.get(P, P), ORD, X, CHK)
        lane = P >= 6
        lf = P >= 8
        stack = 2 <= P <= 5
        need, forbid = [], []
        if ORD == 0:
            forbid += [".acquire", ".release", ".acq_rel"]
        if kind == "entry":
            if lf:
                need += ["atom.acquire.gpu.global.cas.b32" if ORD else "atom.relaxed.gpu.global.cas.b32"]
            elif lane:
                need += ["atom.acquire.gpu.global.or.b64" if ORD else "atom.relaxed.gpu.global.or.b64"]
            else:
                need += ["shfl.sync.idx"]
                if X not in (3, 4):  # 3 = constructed mask, 4 = its first-exit control
                    need += ["activemask.b32"]
                else:
                    forbid += ["activemask.b32"]
                if stack:
                    need += ["ld.acquire.gpu.global.u64" if ORD else "ld.relaxed.gpu.global.u64",
                             "atom.relaxed.gpu.global.cas.b64"]
                else:
                    need += ["atom.acquire.gpu.global.or.b64" if ORD else "atom.relaxed.gpu.global.or.b64"]
                need += ["atom.release.gpu.global.add.u64" if ORD else "atom.relaxed.gpu.global.add.u64"]
                if ORD:
                    need += ["ld.acquire.gpu.global.u64"]  # the lanes' wait for the claim
            need += ["st.relaxed.gpu.global.u64"]  # owner tag
        else:
            need += ["ld.relaxed.gpu.global.u64"]  # owner tag check (strong)
            if lf:
                if X != 2:
                    need += ["st.release.gpu.global.u32" if ORD else "st.relaxed.gpu.global.u32"]
            elif lane:
                if X != 2:
                    need += ["atom.release.gpu.global.and.b64" if ORD else "atom.relaxed.gpu.global.and.b64"]
            else:
                if X in (0, 1, 3):
                    need += ["atom.acq_rel.gpu.global.add.u64" if ORD else "atom.relaxed.gpu.global.add.u64"]
                if stack:
                    need += ["atom.release.gpu.global.cas.b64" if ORD else "atom.relaxed.gpu.global.cas.b64"]
                else:
                    need += ["atom.release.gpu.global.and.b64" if ORD else "atom.relaxed.gpu.global.and.b64"]
                if X == 1:
                    need += ["match.any.sync", "bar.warp.sync"]
        miss = [n for n in need if n not in body]
        bad = [f for f in forbid if f in body]
        order = ""
        if kind == "entry" and CHK and not lane:
            # checker: the leader's ledger exch must precede its release add, so that each lane's
            # acquire of the add also orders the ledger value (no false freed_live / bad_rel)
            ix = body.find("atom.relaxed.gpu.global.exch.b64")
            ia = min([i for i in (body.find("atom.release.gpu.global.add.u64"), body.find("atom.relaxed.gpu.global.add.u64")) if i >= 0] or [-1])
            if not (0 <= ix < ia):
                bad = bad + ["ledger exch not before the claim add"]
            order = "; ledger exch before the add"
        rep(not miss and not bad, tag, "missing %s; forbidden present %s%s" % (miss or "none", bad or "none", order))


# ---- SASS, per kernel ----------------------------------------------------------------------------
def sass_kernels(path):
    out, name = {}, None
    for line in open(path):
        m = re.search(r"Function : (\S+)", line)
        if m:
            name = m.group(1)
            out[name] = []
            continue
        if name:
            m = INS.search(line)
            if m:
                out[name].append(m.group(2))
    return out


def _target_is_subroutine(path, kernel, target):
    """True if the instructions at `target` in `kernel` do not begin with a VOTE (activemask)."""
    on, first = False, []
    want = "/*%04x*/" % int(target, 16)
    for line in open(path):
        m = re.search(r"Function : (\S+)", line)
        if m:
            on = m.group(1) == kernel
            continue
        if on and want in line:
            first.append(line)
            continue
        if on and first and len(first) < 4:
            first.append(line)
    return not any("VOTE" in x for x in first)


def unfolded_add64(ins):
    n = 0
    for i, t in enumerate(ins):
        if "ATOMG.E.ADD.64" in t:
            window = " ".join(ins[max(0, i - 8):i])
            if "VOTEU.ANY" not in window and "UFLO" not in window:
                n += 1
    return n


def check_sass(path, label):
    ks = sass_kernels(path)
    checked = 0
    for k, ins in sorted(ks.items()):
        m = re.match(r"(c|x_div|x_full|x_hold)_(\w+?)_(o|r|oc)_(lane|agg|first)$", k)
        if not m:
            continue
        checked += 1
        pool, ordk, ex = m.group(2), m.group(3), m.group(4)
        text = "\n".join(ins)
        need, forbid = ["CALL.REL.NOINC", "LDG.E.64.STRONG.GPU"], []
        ORD = ordk in ("o", "oc")
        if ORD:
            need += ["MEMBAR.ALL.GPU", "CCTL.IVALL"]
        else:
            forbid += ["MEMBAR", "CCTL.IVALL"]
        if pool.startswith("lf"):
            need += ["ATOMG.E.CAS.STRONG.GPU"]
            if ex != "first":
                need += ["STG.E.STRONG.GPU"]
        elif pool.startswith("ln"):
            need += ["ATOMG.E.OR.64", "ATOMG.E.AND.64"]
        elif pool.startswith("stk"):
            need += ["ATOMG.E.CAS.64"]
        else:
            need += ["ATOMG.E.OR.64", "ATOMG.E.AND.64"]
        if not pool.startswith("l"):
            need += ["SHFL.IDX"]
            if ordk != "oc":
                # __activemask(): VOTE.ANY R, or VOTEU.ANY UR where ptxas treats the call site as
                # uniform (single top-level call); presence of activemask is checked in the PTX
                need += ["VOTE"]
            if ex == "agg":
                need += ["MATCH.ANY"]
        miss = [n for n in need if n not in text]
        bad = [f for f in forbid if f in text]
        extra = ""
        ok = not miss and not bad
        if not pool.startswith("l") and ex in ("lane",):
            u = unfolded_add64(ins)
            extra = "; per-lane (unfolded) ATOMG.E.ADD.64: %d" % u
            ok = ok and u >= 1
        rep(ok, "%s %s" % (label, k), "missing %s; forbidden %s%s" % (miss or "none", bad or "none", extra))
    rep(checked > 0, "%s kernels checked" % label, str(checked))
    if "c_wid" in ks:
        t = "\n".join(ks["c_wid"])
        rep("SR_VIRTUALSMID" in t and "SR_VIRTID" in t and "CALL.REL.NOINC" in t, "%s c_wid reads %%smid and %%warpid" % label)
    for k in ("x_mask_loop_ni", "x_mask_one_ni"):
        if k in ks:
            t = "\n".join(ks[k])
            rep("CALL.REL.NOINC" in t and "VOTE.ANY R" in t and "SHFL.IDX" in t, "%s %s calls the noinline helper" % (label, k))
    if "x_mask_loop_inl" in ks:
        t = "\n".join(ks["x_mask_loop_inl"])
        # inlined: no CALL target starts the pattern (the remaining CALLs are the compiler's own
        # subroutines, e.g. the WARPSYNC + SHFL.IDX slow path of __shfl_sync)
        targets = set(re.findall(r"CALL\.REL\.NOINC (0x[0-9a-f]+)", t))
        rep("VOTE.ANY R" in t and "SHFL.IDX" in t and all(_target_is_subroutine(path, "x_mask_loop_inl", x) for x in targets),
            "%s x_mask_loop_inl has the pattern inlined" % label,
            "%d CALL targets, none of them starts with VOTE.ANY" % len(targets))


check_ptx(os.path.join(B, "kernels.ptx"))
check_sass(os.path.join(B, "kernels.sass"), "native")
if os.path.exists(os.path.join(B, "kernels_jit.sass")):
    check_sass(os.path.join(B, "kernels_jit.sass"), "jit")
json.dump({"fails": fails, "results": results}, open(os.path.join(B, "sass_check.json"), "w"), indent=1)
print("RESULT", "PASS" if fails == 0 else "FAIL (%d)" % fails)
sys.exit(1 if fails else 0)
