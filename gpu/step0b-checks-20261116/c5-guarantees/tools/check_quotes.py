#!/usr/bin/env python3
"""Check that every quote in a Markdown file is verbatim in the saved document texts.

A quote is a citation tag followed directly by one or more double-quoted pieces:
    [<doc> §<section>] "<text>" "<more text>"
anywhere in a line (block quotes starting with "> " and table cells alike).
<doc> is the basename (without .txt) of a file in the text directory, for
example ptx-isa or cpg-cpp-language-extensions. A piece may be split with
" ... " into fragments, and every fragment must occur in the document. A block
quote line ("> [" at the start) without any quoted piece is an error. Whitespace is normalized on both sides
(runs of whitespace become one space); nothing else is changed, so typographic
quotes, apostrophes and the ^{...} superscript markers must match exactly.

A numeric section (§5.4.4.2, §6.37 ...) is checked too: the nearest numbered heading
before the quoted text must be that section, or its ancestor or descendant. Non-numeric
sections (§atomics.lockfree p5) are not checked. --no-sections skips this (used for the
archived 13.2.2 pages, whose numbering differs).

Usage: python3 -I check_quotes.py <markdown> <textdir> [--expect-bad N] [--no-sections]
Exit 0 when every fragment is found (or, with --expect-bad N, when exactly N
fragments are missing: the control); exit 1 otherwise.
"""
import os
import re
import sys

CHECK_SECTIONS = True
TAG = re.compile(r'\[([A-Za-z0-9._-]+) §([^\]]+)\]((?:\s*(?:\.\.\.|,|;|and|then)?\s*"[^"]+")*)')


HEADING = re.compile(r"^\s*(\d+(?:\.\d+)+)\.\s+\S")


def norm(s):
    return re.sub(r"\s+", " ", s).strip()


def load_doc(path):
    """Normalized text of a page, and the offsets of its numbered headings in it."""
    parts, heads, off = [], [], 0
    for line in open(path, encoding="utf-8"):
        n = norm(line)
        if not n:
            continue
        m = HEADING.match(line)
        if m:
            heads.append((off, m.group(1)))
        parts.append(n)
        off += len(n) + 1
    return " ".join(parts), heads


def section_ok(cited, heads, pos):
    """The nearest numbered heading before pos must equal the cited number, or one of them
    must be an ancestor of the other (6.37 covers 6.37.2)."""
    last = None
    for o, num in heads:
        if o > pos:
            break
        last = num
    if last is None:
        return False, None
    a, b = cited.split("."), last.split(".")
    k = min(len(a), len(b))
    return a[:k] == b[:k], last


def main():
    md, textdir = sys.argv[1], sys.argv[2]
    global CHECK_SECTIONS
    CHECK_SECTIONS = "--no-sections" not in sys.argv
    expect = None
    if "--expect-bad" in sys.argv:
        expect = int(sys.argv[sys.argv.index("--expect-bad") + 1])
    cache = {}
    n = bad = lines = 0
    for i, line in enumerate(open(md, encoding="utf-8"), 1):
        line = line.rstrip("\n")
        for m in TAG.finditer(line):
            doc, sec, rest = m.group(1), m.group(2), m.group(3)
            if not rest.strip():
                if line.lstrip().startswith("> ["):
                    print("%s:%d no quoted piece" % (md, i))
                    bad += 1
                continue
            lines += 1
            n, bad = check(md, i, doc, sec, rest, textdir, cache, n, bad)
    print("%d citations with quotes, %d fragments checked, %d not found" % (lines, n, bad))
    if expect is not None:
        ok = bad == expect and n > 0
        print("control: expected %d not found -> %s" % (expect, "PASS" if ok else "FAIL"))
        sys.exit(0 if ok else 1)
    sys.exit(1 if bad or n == 0 else 0)


def check(md, i, doc, sec, rest, textdir, cache, n, bad):
    path = os.path.join(textdir, doc + ".txt")
    if doc not in cache:
        if not os.path.exists(path):
            print("%s:%d unknown document %s" % (md, i, doc))
            cache[doc] = None
            return n, bad + 1
        cache[doc] = load_doc(path)
    if cache[doc] is None:
        return n, bad + 1
    ref, heads = cache[doc]
    m = re.match(r"(\d+(?:\.\d+)*)", sec)
    cited = m.group(1) if m else None
    for piece in re.findall(r'"([^"]+)"', rest):
        for frag in piece.split(" ... "):
            n += 1
            f = norm(frag)
            pos = ref.find(f)
            if pos < 0:
                bad += 1
                print("%s:%d [%s §%s] NOT FOUND: %s" % (md, i, doc, sec, frag))
                continue
            if cited is None or not CHECK_SECTIONS:
                continue
            ok, last = False, None
            while pos >= 0 and not ok:
                ok, last = section_ok(cited, heads, pos)
                pos = ref.find(f, pos + 1)
            if not ok:
                bad += 1
                print("%s:%d [%s §%s] WRONG SECTION (nearest heading %s): %s" %
                      (md, i, doc, sec, last, frag[:70]))
    return n, bad


if __name__ == "__main__":
    main()
