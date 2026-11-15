#!/usr/bin/env python3
"""Check that every quote in README.md is verbatim in the saved excerpts AND
sits in the section it is attributed to.

A quote is a README line (any indentation) of the form
    > PTX §<section>: "<text>"        (checked against the PTX excerpts)
    > CUDA-PG §<section>: "<text>"    (checked against the CUDA Programming
                                       Guide excerpts)
Several quoted pieces per line are allowed, joined by ' ... '. Whitespace is
normalized on both sides; nothing else is. The excerpt files are split into
sections at heading lines ("8.9.4. Memory synchronization"); a piece passes
only if it occurs in the text of the section it names (text after a
"====" separator and before the next heading belongs to no section).

Usage: python3 -I check_quotes.py README.md docs/ptx-isa-9.5-excerpts.txt \
           [docs/cuda-programming-guide-excerpts.txt]
"""
import re
import sys

HEAD = re.compile(r"^(\d+(?:\.\d+)+)\. \S")


def norm(s):
    return re.sub(r"\s+", " ", s).strip()


def sections(path):
    secs, cur = {}, None
    for line in open(path, encoding="utf-8"):
        if line.startswith("===================="):
            cur = None
            continue
        m = HEAD.match(line)
        if m:
            cur = m.group(1)
            secs.setdefault(cur, [])
            continue
        if cur is not None:
            secs[cur].append(line)
    return {k: norm("".join(v)) for k, v in secs.items()}


def main():
    readme, ptx = sys.argv[1], sys.argv[2]
    srcs = {"PTX": sections(ptx)}
    if len(sys.argv) > 3:
        srcs["CUDA-PG"] = sections(sys.argv[3])
    n = bad = 0
    for i, line in enumerate(open(readme, encoding="utf-8"), 1):
        m = re.match(r"^\s*> (PTX|CUDA-PG) §([\d.]+): (.*)$", line.rstrip("\n"))
        if not m:
            continue
        src, sec = m.group(1), m.group(2)
        secs = srcs.get(src)
        for piece in re.findall(r'"([^"]+)"', m.group(3)):
            for part in piece.split(" ... "):
                n += 1
                q = norm(part)
                if secs is None:
                    bad += 1
                    print("README:%d %s §%s: no excerpt file given" % (i, src, sec))
                    continue
                if q in secs.get(sec, ""):
                    continue
                bad += 1
                where = [k for k, v in secs.items() if q in v]
                print("README:%d %s §%s %s: %s" % (
                    i, src, sec, "MISATTRIBUTED (found in §%s)" % ", §".join(where) if where else "NOT FOUND",
                    part))
    print("%d quoted pieces checked, %d not found in the section named" % (n, bad))
    sys.exit(1 if bad or n == 0 else 0)


if __name__ == "__main__":
    main()
