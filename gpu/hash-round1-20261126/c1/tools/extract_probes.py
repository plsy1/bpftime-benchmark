#!/usr/bin/env python3
"""Extract the probe .func definitions (__probe_func__*, __retprobe_func__*)
from a dumped patched module, condensed: call sequences shown as
'CALL helper(param0..param4 regs)' with constant movs kept.
Usage: extract_probes.py DUMP.ptx > out.txt"""
import re, sys
src = open(sys.argv[1]).read().split('\n')
i = 0
out = []
while i < len(src):
    l = src[i]
    m = re.match(r'^\.visible \.func (__(?:ret)?probe_func__\S+)\(\)\s*$', l)
    if m:
        out.append('#### %s  (dump line %d)' % (m.group(1), i + 1))
        depth = 0
        j = i + 1
        body = []
        while j < len(src):
            body.append((j + 1, src[j]))
            depth += src[j].count('{') - src[j].count('}')
            if depth == 0 and src[j].strip() == '}':
                break
            j += 1
        # condense callseqs
        k = 0
        while k < len(body):
            ln, t = body[k]
            if '// callseq' in t and '{' in t:
                params = {}
                helper = None
                ret = None
                k += 1
                while k < len(body) and '} // callseq' not in body[k][1]:
                    tt = body[k][1].strip()
                    pm = re.match(r'st\.param\.b64\s+\[(param\d)\+0\], (%rd\d+);', tt)
                    if pm:
                        params[pm.group(1)] = pm.group(2)
                    hm = re.match(r'(_bpf_helper_ext_\d+),?', tt)
                    if hm:
                        helper = hm.group(1)
                    rm = re.match(r'ld\.param\.b64\s+(%rd\d+), \[retval0\+0\];', tt)
                    if rm:
                        ret = rm.group(1)
                    k += 1
                out.append('%6d\tCALL %s(%s) -> %s' % (ln, helper, ', '.join(params.get('param%d' % x, '-') for x in range(5)), ret))
                k += 1
                continue
            out.append('%6d%s' % (ln, t))
            k += 1
        i = j
    i += 1
print('\n'.join(out))
