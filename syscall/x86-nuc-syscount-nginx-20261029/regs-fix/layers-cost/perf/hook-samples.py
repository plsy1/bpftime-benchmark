#!/usr/bin/env python3
"""Split perf samples of a rewrite-leg profile: page zero (zpoline nop sled below
0x200, jump stub above), and syscall_hooker_asm by instruction (offset from the
symbol, attributed to the instruction that starts at or before it). Reads
`perf script -F ip,sym,symoff -G` on stdin; argv[1] = objdump lines of
syscall_hooker_asm (to name the offsets), optional.
Usage: perf script -i X.perf.data -F ip,sym,symoff -G | hook-samples.py [dis.txt]"""
import collections, re, sys
insns = []
if len(sys.argv) > 1:
    lines = [l for l in open(sys.argv[1]) if re.match(r'^\s+[0-9a-f]+:', l)]
    base = int(lines[0].split(':')[0], 16)
    for l in lines:
        a, rest = l.split(':', 1)
        insns.append((int(a, 16) - base, rest.strip()))
n = 0
c = collections.Counter()
hook = collections.Counter()
for line in sys.stdin:
    p = line.split()
    if not p:
        continue
    n += 1
    ip = int(p[0], 16)
    if ip < 0x200:
        c['page zero: nop sled'] += 1
    elif ip < 0x1000:
        c['page zero: jump stub'] += 1
    elif len(p) > 1 and p[1].startswith('syscall_hooker_asm+0x'):
        off = int(p[1].split('+')[1], 16)
        c['syscall_hooker_asm'] += 1
        name = '?'
        for o, text in insns:
            if o <= off:
                name = f'+{o:#x} {text}'
        hook[name] += 1
    elif len(p) > 1:
        c[p[1].split('+')[0]] += 1
print(f'samples {n}')
for k, v in c.most_common(8):
    print(f'{100 * v / n:5.1f}%  {k}')
print('syscall_hooker_asm by instruction (sample at or after it):')
for k, v in hook.most_common(8):
    print(f'{100 * v / n:5.1f}%  {k}')
