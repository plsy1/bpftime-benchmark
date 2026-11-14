#!/usr/bin/env python3
"""check_targets.py: the probed kernel names are the ones the census counts.

For every llama object in bpf/ (e7_rms, e7_mmq, e7_mmvq, e7_ropek, e7_all4),
read its kretprobe/ section names (llvm-objdump-15 -h), and check that
  - each single-kernel object has exactly one kretprobe section, all4 has the four;
  - the name equals analyze.py's MANGLED entry for that kernel (text check);
  - the census sqlite counts that name with the README's launches and threads
    (pp512: rms 45/11,273,216, mmq 21/1,892,352, mmvq 2/983,040,
    ropek 22/11,534,336; tg128: rms 5,760/2,949,120, mmq 0/0,
    mmvq 7,040/3,137,339,392, ropek 2,816/2,883,584).
A probe whose name matches nothing never fires, and its census row is 0/0.
Exit 0 iff all pass. No GPU. Standard library only (python3 -I).
"""
import os
import re
import sqlite3
import subprocess
import sys

E = os.path.dirname(os.path.abspath(__file__))
CENSUS = '/home/jetson/src/bpftime-gpu-work/llama/census'
EXPECTED = {
    ('pp512', 'rms'): (45, 11273216), ('pp512', 'mmq'): (21, 1892352),
    ('pp512', 'mmvq'): (2, 983040), ('pp512', 'ropek'): (22, 11534336),
    ('tg128', 'rms'): (5760, 2949120), ('tg128', 'mmq'): (0, 0),
    ('tg128', 'mmvq'): (7040, 3137339392), ('tg128', 'ropek'): (2816, 2883584),
}
KERNELS = ('rms', 'mmq', 'mmvq', 'ropek')


def sections(obj):
    out = subprocess.run(['llvm-objdump-15', '-h', obj], capture_output=True, text=True, timeout=60).stdout
    return re.findall(r'\bkretprobe/(\S+)', out)


def main():
    ok = True
    analyze = open(os.path.join(E, 'analyze.py')).read()
    names = {}
    for k in KERNELS:
        s = sections(os.path.join(E, 'bpf', 'e7_%s.bpf.o' % k))
        good = len(s) == 1
        print('%s %-6s object sections: %d' % ('ok  ' if good else 'FAIL', k, len(s)))
        ok &= good
        if s:
            names[k] = s[0]
            m = re.search(r"'%s':\s*'([^']+)'" % k, analyze)
            same = bool(m) and m.group(1) == s[0]
            print('%s %-6s SEC name == analyze.py MANGLED' % ('ok  ' if same else 'FAIL', k))
            ok &= same
    a4 = sections(os.path.join(E, 'bpf', 'e7_all4.bpf.o'))
    good = sorted(a4) == sorted(names.values()) and len(a4) == 4
    print('%s all4 object has the four names' % ('ok  ' if good else 'FAIL'))
    ok &= good
    for test in ('pp512', 'tg128'):
        c = sqlite3.connect('file:%s?mode=ro' % os.path.join(CENSUS, '%s-graph.sqlite' % test), uri=True)
        for k, n in names.items():
            r = c.execute('select count(*), coalesce(sum(gridX*gridY*gridZ*blockX*blockY*blockZ),0) '
                          'from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on s.id=k.mangledName '
                          'where s.value=?', (n,)).fetchone()
            want = EXPECTED[(test, k)]
            good = tuple(r) == want
            print('%s %-6s %-6s census %d/%d, README %d/%d' % ('ok  ' if good else 'FAIL', test, k, r[0], r[1], want[0], want[1]))
            ok &= good
    print('TARGETS PASS' if ok else 'TARGETS FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
