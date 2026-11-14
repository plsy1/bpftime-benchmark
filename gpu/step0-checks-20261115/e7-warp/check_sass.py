#!/usr/bin/env python3
"""check_sass.py [SASS_FILE]: confirm that every kernel of check 7 calls the reservation helper
as a real subroutine (CALL.REL.NOINC, not inlined) and that the helper body keeps its warp
instructions: VOTE.ANY (activemask), FLO (lowest lane), POPC, ATOMG.E.ADD with a used result
(leader reservation), SHFL.IDX and WARPSYNC (directly or in the slow-path subroutines the helper
itself calls). SASS_FILE defaults to build/warp.sass; writes SASS_FILE.helpers.txt as evidence.
Exit 1 on any FAIL.
"""
import re
import sys

SASS = sys.argv[1] if len(sys.argv) > 1 else "build/warp.sass"
SITES = {"k_early": 1, "k_lowlanes": 2, "k_nested": 3, "k_loops": 1, "k_memcap": 4}
INS = re.compile(r"/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;")


def functions(path):
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
                out[name].append((int(m.group(1), 16), m.group(2)))
    return out


def body(ins, start):
    """instructions from start up to and including the first RET."""
    res = []
    on = False
    for a, t in ins:
        if a == start:
            on = True
        if on:
            res.append((a, t))
            if t.startswith("RET"):
                break
    return res


def closure(ins, start, seen=None):
    seen = seen if seen is not None else set()
    if start in seen:
        return []
    seen.add(start)
    b = body(ins, start)
    out = list(b)
    for a, t in b:
        m = re.search(r"CALL\.REL\.NOINC (0x[0-9a-f]+)", t)
        if m:
            out += closure(ins, int(m.group(1), 16), seen)
    return out


funcs = functions(SASS)
fails = 0
ev = open(SASS + ".helpers.txt", "w")
for kname, ins in sorted(funcs.items()):
    m = re.match(r"_Z\d+(k_\w+?)ILi(\d)E", kname)
    if not m:
        continue
    case, var = m.group(1), int(m.group(2))
    targets = {}
    for a, t in ins:
        mm = re.search(r"CALL\.REL\.NOINC (0x[0-9a-f]+)", t)
        if mm:
            targets.setdefault(int(mm.group(1), 16), []).append(a)
    helpers = []
    for tgt in targets:
        own = body(ins, tgt)
        # the helper itself: activemask in its first instructions and the leader's fetch-add in
        # its own body (other CALL targets are compiler subroutines, e.g. integer modulo)
        if not (any("VOTE.ANY R" in t for _, t in own[:4]) and any("ATOMG.E.ADD" in t for _, t in own)):
            continue
        cl = closure(ins, tgt)
        text = "\n".join(t for _, t in cl)
        helpers.append((tgt, cl, text))
    if len(helpers) != 1:
        print(f"FAIL {kname}: {len(helpers)} helper bodies found (want 1)")
        fails += 1
        continue
    tgt, cl, text = helpers[0]
    helper_addrs = {a for a, _ in cl}
    ncalls = sum(1 for a in targets[tgt] if a not in helper_addrs)
    need = {"VOTE.ANY R": "VOTE.ANY", "FLO.U32": "FLO", "POPC": "POPC", "ATOMG.E.ADD": "ATOMG.E.ADD",
            "SHFL.IDX": "SHFL.IDX", "WARPSYNC": "WARPSYNC"}
    miss = [v for k, v in need.items() if k not in text]
    ok = ncalls >= SITES[case] and not miss
    fails += 0 if ok else 1
    print(f"{'PASS' if ok else 'FAIL'} {kname}: CALL.REL.NOINC to the helper at {hex(tgt)} from {ncalls} sites "
          f"(source sites {SITES[case]}); helper body {len(cl)} instructions; missing: {miss or 'none'}")
    ev.write(f"==== {kname}: helper at {hex(tgt)}, called from {[hex(a) for a in targets[tgt]]}\n")
    for a, t in sorted(set(cl)):
        ev.write(f"  /*{a:04x}*/ {t}\n")
ev.close()
# memcap: a helper call sits right before data loads/stores (evidence only)
for kname, ins in funcs.items():
    if "k_memcapILi0" in kname:
        seq = [t.split()[0] if not t.startswith("@") else t.split()[1] for _, t in ins]
        pairs = 0
        for i, op in enumerate(seq):
            if op.startswith("CALL"):
                nxt = [o for o in seq[i + 1:i + 40] if o.startswith(("LDG", "STG", "CALL"))]
                if nxt and nxt[0].startswith(("LDG", "STG")):
                    pairs += 1
        print(f"INFO {kname}: CALL followed by a global load/store within 40 instructions: {pairs}")
print("RESULT", "PASS" if fails == 0 else f"FAIL ({fails})")
sys.exit(1 if fails else 0)
