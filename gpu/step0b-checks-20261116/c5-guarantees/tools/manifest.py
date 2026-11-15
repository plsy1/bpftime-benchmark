#!/usr/bin/env python3
"""Write or verify the manifest of the saved documentation pages.

Usage: python3 -I manifest.py write  <docs dir>   (writes <docs dir>/MANIFEST.tsv)
       python3 -I manifest.py verify <docs dir>   (exit 1 on any mismatch)
Columns: name, sha256 of raw/<name>.html, bytes, version or last-updated
string found in the page text, URL (from SOURCES.tsv), fetch date.
"""
import hashlib
import os
import re
import sys

FETCHED = "2026-11-15"


def version_of(text):
    found = []
    for pat in (r"PTX ISA [0-9.]+ documentation", r"\bv1[0-9]\.[0-9]+\b", r"Last updated on [A-Z][a-z]+ [0-9]+, [0-9]{4}"):
        m = re.search(pat, text)
        if m:
            found.append(m.group(0))
    if found:
        return "; ".join(found)
    for pat in (r"PTX ISA [0-9.]+ documentation", r"v1[0-9]\.[0-9]+ \| PDF",
                r"Last updated on [A-Z][a-z]+ [0-9]+, [0-9]{4}", r"CUDA Toolkit Documentation [0-9.]+",
                r"Release [0-9.]+", r"Generated on [^\n]*"):
        m = re.search(pat, text)
        if m:
            return m.group(0)
    return "-"


def rows(d):
    src = dict(l.rstrip("\n").split("\t") for l in open(os.path.join(d, "SOURCES.tsv")))
    out = []
    for name in sorted(src):
        raw = open(os.path.join(d, "raw", name + ".html"), "rb").read()
        text = open(os.path.join(d, "text", name + ".txt"), encoding="utf-8").read()
        out.append((name, hashlib.sha256(raw).hexdigest(), str(len(raw)), version_of(text),
                    src[name], FETCHED))
    return out


def main():
    mode, d = sys.argv[1], sys.argv[2]
    path = os.path.join(d, "MANIFEST.tsv")
    cur = rows(d)
    if mode == "write":
        with open(path, "w") as f:
            f.write("name\tsha256\tbytes\tversion\turl\tfetched\n")
            for r in cur:
                f.write("\t".join(r) + "\n")
        print("wrote %d rows to %s" % (len(cur), path))
        return
    old = [tuple(l.rstrip("\n").split("\t")) for l in open(path)][1:]
    bad = 0
    oldd = {r[0]: r for r in old}
    for r in cur:
        o = oldd.get(r[0])
        if o is None or o[1] != r[1]:
            print("MISMATCH %s" % r[0])
            bad += 1
    if len(old) != len(cur):
        print("row count %d != %d" % (len(old), len(cur)))
        bad += 1
    print("manifest: %d pages, %d mismatches" % (len(cur), bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
