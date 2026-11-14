#!/usr/bin/env python3
"""check_sass.py [BUILD_DIR]: confirm that the register reads, the spin load and the atomics of
check 1 survive compilation. Reads build/ids.sass, build/idmod.sass, build/hog.sass and the PTX in
build/keep and build/idmod.ptx. Prints one PASS/FAIL line per requirement; exit 1 on any FAIL.
"""
import re
import sys

B = sys.argv[1] if len(sys.argv) > 1 else "build"


def functions(path):
    out, cur, name = {}, [], None
    for line in open(path):
        m = re.search(r"Function : (\S+)", line)
        if m:
            if name:
                out[name] = "".join(cur)
            name, cur = m.group(1), []
        elif name:
            cur.append(line)
    if name:
        out[name] = "".join(cur)
    return out


def find(funcs, sub):
    for k, v in funcs.items():
        if sub in k:
            return k, v
    return None, ""


ids = functions(f"{B}/ids.sass")
mod = functions(f"{B}/idmod.sass")
hog = functions(f"{B}/hog.sass")
fails = 0


def req(where, text, pattern, minimum, what):
    global fails
    n = len(re.findall(pattern, text))
    ok = n >= minimum
    fails += 0 if ok else 1
    print(f"{'PASS' if ok else 'FAIL'} {where}: {what}: {n} (need >= {minimum})")


ID_READS = [
    (r"S2R R\d+, SR_VIRTUALSMID", "%smid -> S2R SR_VIRTUALSMID"),
    (r"S2R R\d+, SR_VIRTID", "%warpid -> S2R SR_VIRTID (then bits 8..14)"),
    (r"S2R R\d+, SR_LANEID", "%laneid -> S2R SR_LANEID"),
    (r"CS2R R\d+, SR_GLOBALTIMERLO", "%globaltimer -> one 64-bit CS2R (no .32)"),
    (r"c\[0x0\]\[0x30\]", "%gridid low word = constant bank c[0x0][0x30]"),
    (r"c\[0x0\]\[0x34\]", "%gridid high word = c[0x0][0x34]"),
    (r"c\[0x0\]\[0x10c\]", "%nsmid = constant bank c[0x0][0x10c]"),
]
for fname, need_reads, extra in [
    ("k_barrier", 2, [(r"LDG\.E\.STRONG\.(SYS|GPU)", 1, "volatile spin load"),
                      (r"(RED|ATOMG)\.E\.ADD", 1, "arrival atomic add")]),
    ("k_long", 2, [(r"LDG\.E\.STRONG\.(SYS|GPU)", 1, "volatile spin load"),
                   (r"(RED|ATOMG)\.E\.ADD", 2, "arrival atomic adds (entry, exit)"),
                   (r"NANOSLEEP", 1, "per-iteration nanosleep")]),
    ("k_plain", 1, []),
    ("k_node", 1, [(r"ATOMG\.E\.ADD", 1, "replay counter fetch-add (result used)")]),
]:
    name, text = find(ids, fname)
    if not name:
        print(f"FAIL ids: function {fname} not found")
        fails += 1
        continue
    for pat, what in ID_READS:
        req(name, text, pat, need_reads if "GLOBALTIMER" not in pat else 1, what)
    for pat, m, what in extra:
        req(name, text, pat, m, what)
    # %globaltimer must never be read as a 32-bit half only
    n32 = len(re.findall(r"CS2R\.32 R\d+, SR_GLOBALTIMER", text))
    print(f"INFO {name}: 32-bit-only timer reads: {n32}")

# k_long: the per-iteration reads must sit inside the timed loop (between the back-branch target
# and the back-branch that also encloses the per-iteration NANOSLEEP c[0x0][...]), not hoisted out
name, text = find(ids, "k_long")
ins = []
for line in text.splitlines():
    m = re.search(r"/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;", line)
    if m:
        ins.append((int(m.group(1), 16), m.group(2)))
loops = []
for a, t in ins:
    m = re.search(r"BRA (?:!?P\d, )?(0x[0-9a-f]+)", t)
    if m and int(m.group(1), 16) < a:
        lo = int(m.group(1), 16)
        body_t = [x for b, x in ins if lo <= b <= a]
        if any(re.search(r"NANOSLEEP c\[0x0\]", x) for x in body_t):
            loops.append((lo, a, body_t))
if not loops:
    print("FAIL k_long: no loop containing the per-iteration NANOSLEEP")
    fails += 1
else:
    lo, hi, bt = min(loops, key=lambda x: x[1] - x[0])
    for pat, what in [(r"S2R R\d+, SR_VIRTUALSMID", "%smid read inside the loop"),
                      (r"S2R R\d+, SR_VIRTID", "%warpid read inside the loop"),
                      (r"CS2R R\d+, SR_GLOBALTIMERLO", "%globaltimer read inside the loop")]:
        req(f"k_long loop {hex(lo)}-{hex(hi)}", "\n".join(bt), pat, 1, what)

name, text = find(mod, "idmod_plain")
for pat, what in ID_READS:
    req("idmod.cubin " + (name or "?"), text, pat, 1, what)
name, text = find(hog, "k_hog")
hname, htext = find(hog, "hog_warp")
req("hog", text + htext, r"CS2R R\d+, SR_GLOBALTIMERLO", 1, "64-bit timer read")
req("hog", text + htext, r"NANOSLEEP", 1, "per-iteration nanosleep")

# PTX: every special register is read with the intended width
import glob
ptx = "".join(open(p).read() for p in glob.glob(f"{B}/keep/ids.ptx") + [f"{B}/idmod.ptx"])
for pat, what in [
    (r"mov\.u64\s+%rd\d+,\s*%gridid", "mov.u64 %gridid"),
    (r"mov\.u32\s+%r\d+,\s*%gridid", "mov.u32 %gridid (accepted by ptxas too)"),
    (r"mov\.u32\s+%r\d+,\s*%nwarpid", "mov.u32 %nwarpid (SASS: an immediate, see README)"),
    (r"mov\.u32\s+%r\d+,\s*%nsmid", "mov.u32 %nsmid"),
    (r"mov\.u64\s+%rd\d+,\s*%globaltimer", "mov.u64 %globaltimer"),
]:
    req("ptx", ptx, pat, 1, what)
print("RESULT", "PASS" if fails == 0 else f"FAIL ({fails})")
sys.exit(1 if fails else 0)
