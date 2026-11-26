"""Print the probe subroutines of a kernel's SASS (cuobjdump -sass -fun K):
every CALL.REL target, from the target address to the first RET after it,
one instruction per line (address, text). Also counts per subroutine.

usage: python3 -I sassfn.py FILE.sass [--ops]
"""
import re, sys

lines = open(sys.argv[1]).read().split('\n')
ins = []
for l in lines:
    m = re.match(r'\s*/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;', l)
    if m:
        ins.append((int(m.group(1), 16), m.group(2).strip()))
addr = {a: i for i, (a, _) in enumerate(ins)}
targets = []
for a, t in ins:
    m = re.search(r'CALL\.REL(?:\.NOINC)?\s+(?:`\(([^)]*)\)|0x([0-9a-f]+))', t)
    if m and m.group(2):
        tgt = int(m.group(2), 16)
        if tgt not in targets:
            targets.append(tgt)
for tgt in targets:
    if tgt not in addr:
        print('target 0x%x not in listing' % tgt)
        continue
    # the subroutine spans up to the next call target (or the end of the
    # listing, without the trailing self-branch and NOP padding)
    nxt = min([t for t in targets if t > tgt] + [ins[-1][0] + 1])
    body = [x for x in ins if tgt <= x[0] < nxt]
    while body and (body[-1][1].startswith('NOP') or re.match(r'BRA 0x%x$' % body[-1][0], body[-1][1])):
        body.pop()
    ops = [re.sub(r'^@!?U?P[T\d]+\s+', '', t).split()[0] for _, t in body]
    print('== subroutine at 0x%x: %d instructions; c[0x3] refs=%d CS2R=%d ATOM=%d QSPC=%d BPT=%d LDC=%d SEL=%d ISETP=%d' % (
        tgt, len(body), sum('c[0x3]' in t for _, t in body), sum(o.startswith('CS2R') for o in ops), sum(o.startswith(('ATOM', 'RED')) for o in ops),
        sum(o.startswith('QSPC') for o in ops), sum(o.startswith('BPT') for o in ops), sum(o.startswith('LDC') for o in ops),
        sum(o.startswith(('SEL', 'FSEL')) for o in ops), sum(o.startswith('ISETP') for o in ops)))
    if '--ops' in sys.argv:
        for a, t in body:
            print('  %05x  %s' % (a, t))
