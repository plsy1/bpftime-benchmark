#!/usr/bin/env python3
"""extract_probe_ptx.py PATCHED.ptx FUNC OUT_PREFIX

Cut the PTX function FUNC (e.g. __retprobe_func__e5step0_atomics_kernel) out of
a patched module and list what matters for check 5: every atom/red instruction
(operation, state space, semantics, scope, type), every fence/membar, and for
each atom.cas the instructions just before it (the JIT's CMPXCHG lowering
returns a plain load taken before the cmpxchg: vm/llvm-jit/src/compiler.cpp).
Writes OUT_PREFIX.func.ptx and OUT_PREFIX.atoms.json. Standard library only.
"""
import json
import re
import sys


def cut_function(text, name):
    # header: ".visible .func ... NAME(" or ".func NAME(" possibly with a return param
    m = re.search(r'^[^\n]*\.func[^\n]*\b' + re.escape(name) + r'\b', text, re.M)
    if not m:
        return None
    start = m.start()
    i = text.find('{', m.end())
    depth = 0
    j = i
    while j < len(text):
        c = text[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return text[start:j + 1]
        j += 1
    return None


ATOM_RE = re.compile(r'\b(atom|red)((?:\.[A-Za-z0-9_]+)+)')
SEM = {'relaxed', 'acquire', 'release', 'acq_rel', 'sc'}
SCOPE = {'cta', 'cluster', 'gpu', 'sys'}
SPACE = {'global', 'shared', 'shared::cta', 'shared::cluster'}


def decode(instr, quals):
    q = [x for x in quals.split('.') if x]
    sem = [x for x in q if x in SEM]
    scope = [x for x in q if x in SCOPE]
    space = [x for x in q if x in SPACE]
    ops = [x for x in q if x in ('add', 'cas', 'exch', 'and', 'or', 'xor',
                                 'min', 'max', 'inc', 'dec')]
    types = [x for x in q if re.fullmatch(r'[usbf](8|16|32|64)', x)]
    return {
        'instr': instr,
        'op': ops[0] if ops else None,
        'state_space': space[0] if space else 'generic (none given)',
        'sem': sem[0] if sem else 'none given',
        'scope': scope[0] if scope else 'none given (ptxas: see SASS .STRONG.<scope>)',
        'type': types[0] if types else None,
    }


def main():
    path, name, prefix = sys.argv[1], sys.argv[2], sys.argv[3]
    text = open(path, encoding='utf-8', errors='replace').read()
    func = cut_function(text, name)
    res = {'ptx': path, 'function': name, 'found': func is not None}
    if func is None:
        json.dump(res, open(prefix + '.atoms.json', 'w'), indent=1)
        print(json.dumps(res))
        return 1
    open(prefix + '.func.ptx', 'w').write(func + '\n')
    lines = [l.strip() for l in func.splitlines()]
    atoms, fences, cas_context = [], [], []
    for n, l in enumerate(lines):
        if l.startswith('//'):
            continue
        m = ATOM_RE.search(l)
        if m:
            d = decode(m.group(1), m.group(2))
            d['line'] = l
            atoms.append(d)
            if d['op'] == 'cas':
                ctx = [x for x in lines[max(0, n - 8):n] if x and not x.startswith('//')]
                after = [x for x in lines[n + 1:n + 6] if x and not x.startswith('//')]
                cas_context.append({'before': ctx, 'cas': l, 'after': after})
        if re.search(r'\b(membar|fence)\.', l):
            fences.append(l)
    calls = sorted(set(re.findall(r'call(?:\.uni)?\s*(?:\([^)]*\),\s*)?([A-Za-z_][A-Za-z0-9_]*)', func)))
    loads = [l for l in lines if re.match(r'(@%p\d+\s+)?ld\.', l)]
    res.update({'atoms': atoms, 'fences': fences, 'cas_context': cas_context,
                'calls': calls, 'n_lines': len(lines), 'n_loads': len(loads)})
    json.dump(res, open(prefix + '.atoms.json', 'w'), indent=1)
    for a in atoms:
        print('ATOM', a['line'], '| space', a['state_space'], '| sem', a['sem'], '| scope', a['scope'])
    for c in cas_context:
        print('CAS before:', ' ; '.join(c['before'][-4:]))
        print('CAS       :', c['cas'])
        print('CAS after :', ' ; '.join(c['after'][:4]))
    print('fences:', fences)
    print('calls:', calls)
    return 0


if __name__ == '__main__':
    sys.exit(main())
