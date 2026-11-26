#!/usr/bin/env python3
"""C1: for every atom/red instruction inside the probe .func bodies
(__probe_func__*, __retprobe_func__*) of a dumped patched module, report its
destination register and every other line of the same function body that
mentions that register (raw dump text, nothing condensed).
Usage: atom_dest_check.py DUMP.ptx [...]"""
import re, sys
for path in sys.argv[1:]:
    src = open(path).read().split('\n')
    i = 0
    while i < len(src):
        m = re.match(r'^\.visible \.func (__(?:ret)?probe_func__\S+)\(\)\s*$', src[i])
        if not m:
            i += 1
            continue
        name = m.group(1)
        depth, j, body = 0, i + 1, []
        while j < len(src):
            body.append((j + 1, src[j]))
            depth += src[j].count('{') - src[j].count('}')
            if depth == 0 and src[j].strip() == '}':
                break
            j += 1
        for ln, t in body:
            a = re.search(r'\b(atom|red)\.(\S+)\s+(%\w+),', t)
            if not a:
                continue
            d = a.group(3)
            others = [(l2, t2.strip()) for l2, t2 in body
                      if l2 != ln and re.search(re.escape(d) + r'\b', t2)]
            print('%s:%d %s | %s | dest %s other mentions %d %s' % (
                path.split('/')[-2], ln, name[:45], t.strip(), d, len(others), others[:3]))
        i = j + 1
