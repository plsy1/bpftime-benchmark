#!/usr/bin/env python3
"""Print the forms the C6 test kernel compiled to, per variant, from build/c6_<v>.sass and .ptx.

For each variant: instruction counts in c6_kernel (test kernel only), the writer's two writes
with everything between them, the reader's flag poll through its data load, and every MEMBAR
with its neighbours. Then the PTX differences between variants, ignoring .loc / comment lines.
"""
import difflib
import os
import re
import sys

D = os.path.dirname(os.path.abspath(__file__))
B = os.path.join(D, "build")
VARIANTS = sys.argv[1:] or ["A", "B", "C", "D", "S", "SD"]


def kernel_sass(v):
    out, on = [], False
    for line in open(os.path.join(B, f"c6_{v}.sass")):
        if "Function :" in line:
            on = "c6_kernel" in line
            continue
        if not on:
            continue
        m = re.match(r"\s*/\*([0-9a-f]{4,})\*/\s*(.*?)\s*;?\s*/\*", line)
        if m:
            out.append((m.group(1), m.group(2).rstrip(" ;")))
    return out


def show(ins, lo, hi):
    for a, t in ins[lo:hi + 1]:
        print(f"    /*{a}*/ {t}")


def main():
    for v in VARIANTS:
        ins = kernel_sass(v)
        txt = [t for _, t in ins]
        print(f"=== variant {v}: c6_kernel, {len(ins)} instructions")
        pats = {
            "RED.E.ADD.64.STRONG.GPU": r"RED\.E\.ADD\.64\.STRONG\.GPU",
            "STG.E.64.STRONG.GPU": r"STG\.E\.64\.STRONG\.GPU",
            "LDG.E.64.STRONG.GPU": r"LDG\.E\.64\.STRONG\.GPU",
            "MEMBAR (any)": r"\bMEMBAR",
            "ERRBAR": r"\bERRBAR",
            "CCTL.IVALL": r"CCTL\.IVALL",
            "LDG.E (32-bit, weak: role/cell tables)": r"LDG\.E\s",
            "STG.E / STG.E.64 (weak: records)": r"STG\.E(\.64)?\s",
            "generic LD/ST/ATOM (none expected)": r"^(@!?U?P\w+\s+)?(LD|ST|ATOM)\.",
        }
        for name, p in pats.items():
            print(f"  count {name}: {sum(1 for t in txt if re.search(p, t))}")
        # writer: the test RED and the publish STG (or the reverse in S / SD), all between them
        for i, t in enumerate(txt):
            if v in ("A", "B", "C", "D") and re.search(r"RED\.E\.ADD\.64\.STRONG\.GPU \[R\d+\.64\], R\d+$", t) and not t.startswith("@"):
                for j in range(i + 1, min(i + 6, len(txt))):
                    if re.search(r"STG\.E\.64\.STRONG\.GPU", txt[j]):
                        print("  writer (add, then publish):")
                        show(ins, i, j)
                        break
            if v in ("S", "SD") and re.search(r"STG\.E\.64\.STRONG\.GPU", t) and not t.startswith("@"):
                for j in range(i + 1, min(i + 16, len(txt))):
                    if re.search(r"RED\.E\.ADD\.64\.STRONG\.GPU", txt[j]):
                        print("  writer (publish, then add; swapped order):")
                        show(ins, i, j)
                        break
        # reader: the flag poll that loops back on itself, through the next 64-bit strong load
        for i, t in enumerate(txt):
            if not re.search(r"LDG\.E\.64\.STRONG\.GPU", t):
                continue
            addr = int(ins[i][0], 16)
            back = None
            for j in range(i + 1, min(i + 8, len(txt))):
                m = re.search(r"BRA (?:P\d, )?0x([0-9a-f]+)", txt[j])
                if m and int(m.group(1), 16) == addr:
                    back = j
                    break
            if back is None:
                continue
            for k in range(back + 1, min(back + 20, len(txt))):
                if re.search(r"LDG\.E\.64\.STRONG\.GPU", txt[k]):
                    if any("SEL" in x for x in txt[back:k]) or k - back < 20:
                        print("  reader (flag poll loop, then the data load):")
                        show(ins, i, k)
                    break
        for i, t in enumerate(txt):
            if re.search(r"\bMEMBAR", t):
                print(f"  MEMBAR at /*{ins[i][0]}*/ with neighbours:")
                show(ins, max(0, i - 2), min(len(ins) - 1, i + 3))
        print()

    def ptx(v):
        keep = []
        for line in open(os.path.join(B, f"c6_{v}.ptx")):
            s = line.strip()
            if s.startswith(".section") and ".debug_str" in s:
                break  # -lineinfo name strings (inlined helper names) follow; code ends here
            if not s or s.startswith(".loc") or s.startswith("//") or s.startswith(".file"):
                continue
            keep.append(re.sub(r"\s+", " ", s))
        return keep

    for a, b in [("A", "B"), ("A", "C"), ("A", "D"), ("D", "S"), ("S", "SD")]:
        if a not in VARIANTS or b not in VARIANTS:
            continue
        d = [l for l in difflib.unified_diff(ptx(a), ptx(b), lineterm="", n=0)
             if not l.startswith("@@") and not l.startswith("---") and not l.startswith("+++")]
        print(f"=== PTX diff {a} -> {b} (code only: ignoring .loc, .file, comments, blank lines, .debug_str): {len(d)} lines")
        for l in d[:40]:
            print("  " + l)
        print()


if __name__ == "__main__":
    main()
