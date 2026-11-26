#!/usr/bin/env python3
"""C1 (hash design round one): per-access report of what eBPF bytecode does
through bpf_map_lookup_elem (helper 1) results, from `llvm-objdump-15 -dr
--no-show-raw-insn` text.

Usage: analyze_bpf.py OBJDUMP_TXT MAP=TYPE [MAP=TYPE ...]
  TYPE is the map's type number (1 = HASH, 1501 = GPU_HASH_MAP, others are
  reported but not judged).

Abstract interpretation along every path of each function's (loop-free) CFG. Register / stack-slot
values: ('const', n) | ('map', name) | ('ptr', map, lookup_pc, off) |
('stack', off) | None (unknown); each path keeps its own state (no joins),
and on the null edge of a null test the pointer becomes const 0.
Reports, per lookup site: every load / store / atomic through its result
(width, offset, kind; for fetch atomics whether the fetched register is read
afterwards), null tests, other compares, escapes (stored to non-stack
memory, spilled to the stack, passed to a helper within its arity, pointer
arithmetic with a non-constant operand). Per update / delete call: the map
and the flags (r4) value when constant.
"""
import re
import sys

ARITY = {1: 2, 2: 4, 3: 2, 6: 5, 14: 0, 25: 5, 502: 0, 503: 3, 504: 3,
         505: 3, 509: 0, 510: 0, 511: 0, 501: 1}

INS_RE = re.compile(r'^\s*(\d+):\s+(.*)$')
REL_RE = re.compile(r'R_BPF_64_64\s+(\S+)')
FUNC_RE = re.compile(r'^[0-9a-f]+ <([^>]+)>:')
SEC_RE = re.compile(r'^Disassembly of section (\S+):')


def parse(path):
    funcs = []  # (section, name, [(pc, text, reloc)])
    sec = None
    cur = None
    for line in open(path):
        line = line.rstrip('\n')
        m = SEC_RE.match(line)
        if m:
            sec = m.group(1)
            cur = None
            continue
        m = FUNC_RE.match(line)
        if m:
            name = m.group(1)
            if name.startswith('LBB'):
                continue
            cur = (sec, name, [])
            funcs.append(cur)
            continue
        m = REL_RE.search(line)
        if m and cur and cur[2]:
            pc, text, _ = cur[2][-1]
            cur[2][-1] = (pc, text, m.group(1))
            continue
        m = INS_RE.match(line)
        if m and cur is not None:
            cur[2].append((int(m.group(1)), m.group(2).strip(), None))
    return funcs


MEM = r'\*\(u(8|16|32|64) \*\)\((r\d+) ([+-]) (\d+)\)'
RE_LOAD = re.compile(r'^(r\d+) = ' + MEM + '$')
RE_STORE_REG = re.compile(r'^' + MEM + r' = (r\d+)$')
RE_STORE_IMM = re.compile(r'^' + MEM + r' = (-?\d+)$')
RE_XADD = re.compile(r'^lock ' + MEM + r' (\+|&|\||\^)= (r\d+)$')
RE_FETCH = re.compile(r'^(r\d+) = atomic_fetch_(add|and|or|xor)\(\(u(64|32) \*\)\((r\d+) ([+-]) (\d+)\), (r\d+)\)$')
RE_XCHG = re.compile(r'^(r\d+) = xchg(?:32)?_(64|32)\((r\d+) ([+-]) (\d+), (r\d+)\)$')
RE_CMPXCHG = re.compile(r'^(r0|w0) = cmpxchg(?:32)?_(64|32)\((r\d+) ([+-]) (\d+), (r0|w0), (r\d+)\)$')
RE_MOV = re.compile(r'^(r\d+) = (r\d+)$')
RE_MOVI = re.compile(r'^(r\d+) = (-?\d+)(?: ll)?$')
RE_ALU = re.compile(r'^(r\d+) (\+|-|\*|/|%|&|\||\^|<<|>>|s>>)= (r\d+|-?\d+)$')
RE_NEG = re.compile(r'^(r\d+) = -(r\d+)$')
RE_W = re.compile(r'\bw\d+\b')
RE_CALL = re.compile(r'^call (\d+)$')
RE_IF = re.compile(r'^if (r\d+) (\S+) (r\d+|-?\d+) goto ([+-]\d+)')
RE_GOTO = re.compile(r'^goto ([+-]\d+)')


def width_off(sign, n):
    return int(n) if sign == '+' else -int(n)


def join(a, b):
    if a == b:
        return a
    for x, y in ((a, b), (b, a)):
        if x and x[0] == 'ptr' and y == ('const', 0):
            return x
    return None


def join_state(s1, s2):
    r = {}
    for k in set(s1) | set(s2):
        v = join(s1.get(k), s2.get(k))
        if v is not None:
            r[k] = v
    return r


def analyze_func(insns, maptypes):
    idx = {pc: i for i, (pc, _, _) in enumerate(insns)}
    n = len(insns)

    def succs(i):
        pc, t, _ = insns[i]
        nxt = i + 1
        width = (insns[i + 1][0] - pc) if i + 1 < n else 1
        m = RE_GOTO.match(t)
        if m:
            return [idx[pc + 1 + int(m.group(1))]]
        m = RE_IF.match(t)
        if m:
            return [nxt, idx[pc + 1 + int(m.group(4))]]
        if t == 'exit':
            return []
        return [nxt] if nxt < n else []

    events = {}  # keyed by (pc, kind) -> description, de-duplicated
    # path-sensitive: every path of the (loop-free) CFG with its own state
    work = [(0, {'r10': ('stack', 0)}, 0)]
    npaths = 0
    while work:
        i, st0, depth = work.pop()
        if depth > 4 * n:
            events[(-1, 'loop')] = (-1, '', ('note', 'path longer than 4x the function: loop? stopped'))
            continue
        npaths += 1
        if npaths > 200000:
            events[(-1, 'many')] = (-1, '', ('note', 'path limit reached'))
            break
        st = dict(st0)
        pc, t, rel = insns[i]
        ev = []

        def val(r):
            return st.get(r)

        def tagged(r):
            v = val(r)
            return v if v and v[0] == 'ptr' else None

        def slot(base, off):
            b = val(base)
            if b and b[0] == 'stack':
                return ('slot', b[1] + off)
            return None

        if RE_W.search(t):
            ev.append(('note', 'w-register (32-bit ALU) instruction: ' + t))
        m = RE_LOAD.match(t)
        if m:
            dst, w, base, sg, off = m.groups()
            o = width_off(sg, off)
            p = tagged(base)
            if p:
                ev.append(('access', p, 'plain load u%s at %+d' % (w, p[3] + o)))
            s = slot(base, o)
            st[dst] = st.get(s) if (s and w == '64') else None
        elif RE_STORE_REG.match(t):
            w, base, sg, off, src = RE_STORE_REG.match(t).groups()
            o = width_off(sg, off)
            p = tagged(base)
            if p:
                ev.append(('access', p, 'plain store u%s at %+d' % (w, p[3] + o)))
            q = tagged(src)
            s = slot(base, o)
            if q:
                if s:
                    ev.append(('escape', q, 'spilled to stack %s' % (s[1],)))
                else:
                    ev.append(('escape', q, 'stored to memory: ' + t))
            if s:
                st[s] = val(src) if w == '64' else None
        elif RE_STORE_IMM.match(t):
            w, base, sg, off, imm = RE_STORE_IMM.match(t).groups()
            o = width_off(sg, off)
            p = tagged(base)
            if p:
                ev.append(('access', p, 'plain store u%s (imm) at %+d' % (w, p[3] + o)))
            s = slot(base, o)
            if s:
                st[s] = ('const', int(imm)) if w == '64' else None
        elif RE_XADD.match(t):
            w, base, sg, off, op, src = RE_XADD.match(t).groups()
            p = tagged(base)
            opn = {'+': 'add', '&': 'and', '|': 'or', '^': 'xor'}[op]
            if p:
                ev.append(('access', p, 'atomic %s u%s, no fetch (XADD form "lock ... %s=") at %+d' % (opn, w, op, p[3] + width_off(sg, off))))
            if tagged(src):
                ev.append(('escape', tagged(src), 'stored to memory: ' + t))
        elif RE_FETCH.match(t):
            dst, op, w, base, sg, off, src = RE_FETCH.match(t).groups()
            p = tagged(base)
            if p:
                ev.append(('access', p, 'atomic fetch_%s u%s at %+d' % (op, w, p[3] + width_off(sg, off)), ('fetchdst', i, dst)))
            st[dst] = None
        elif RE_XCHG.match(t) or RE_CMPXCHG.match(t):
            ev.append(('note', 'xchg/cmpxchg: ' + t))
            m2 = RE_XCHG.match(t) or RE_CMPXCHG.match(t)
            base = m2.group(3)
            p = tagged(base)
            if p:
                ev.append(('access', p, 'atomic xchg/cmpxchg: ' + t))
            st[m2.group(1).replace('w', 'r')] = None
        elif RE_MOV.match(t):
            d, s = RE_MOV.match(t).groups()
            st[d] = val(s)
        elif RE_MOVI.match(t):
            d, imm = RE_MOVI.match(t).groups()
            if rel:
                st[d] = ('map', rel)
            else:
                st[d] = ('const', int(imm))
        elif RE_NEG.match(t):
            d, s = RE_NEG.match(t).groups()
            if tagged(s):
                ev.append(('escape', tagged(s), 'pointer arithmetic: ' + t))
            st[d] = None
        elif RE_ALU.match(t):
            d, op, s = RE_ALU.match(t).groups()
            dv = val(d)
            if s.startswith('r'):
                sv = val(s)
                if tagged(s):
                    ev.append(('escape', tagged(s), 'pointer used as ALU operand: ' + t))
                if dv and dv[0] == 'ptr':
                    if op in '+-' and sv and sv[0] == 'const':
                        c = sv[1] if op == '+' else -sv[1]
                        st[d] = ('ptr', dv[1], dv[2], dv[3] + c)
                        ev.append(('arith', dv, 'pointer + constant %d: %s' % (c, t)))
                    else:
                        ev.append(('escape', dv, 'pointer arithmetic with a non-constant operand: ' + t))
                        st[d] = None
                elif dv and dv[0] == 'stack' and sv and sv[0] == 'const' and op in '+-':
                    st[d] = ('stack', dv[1] + (sv[1] if op == '+' else -sv[1]))
                elif dv and dv[0] == 'const' and sv and sv[0] == 'const':
                    st[d] = None
                else:
                    st[d] = None
            else:
                c = int(s)
                if dv and dv[0] == 'ptr':
                    if op in '+-':
                        c2 = c if op == '+' else -c
                        st[d] = ('ptr', dv[1], dv[2], dv[3] + c2)
                        ev.append(('arith', dv, 'pointer + constant %d: %s' % (c2, t)))
                    else:
                        ev.append(('escape', dv, 'pointer arithmetic (non-add): ' + t))
                        st[d] = None
                elif dv and dv[0] == 'stack' and op in '+-':
                    st[d] = ('stack', dv[1] + (c if op == '+' else -c))
                elif dv and dv[0] == 'const':
                    a = dv[1]
                    try:
                        r = {'+': a + c, '-': a - c, '*': a * c, '&': a & c, '|': a | c, '^': a ^ c,
                             '<<': a << c, '>>': (a % (1 << 64)) >> c}.get(op)
                    except Exception:
                        r = None
                    st[d] = ('const', r) if r is not None else None
                else:
                    st[d] = None
        elif RE_CALL.match(t):
            h = int(RE_CALL.match(t).group(1))
            ar = ARITY.get(h, 5)
            for k in range(1, ar + 1):
                q = tagged('r%d' % k)
                if q:
                    ev.append(('escape', q, 'passed to helper %d as r%d' % (h, k)))
            mv = val('r1')
            mname = mv[1] if mv and mv[0] == 'map' else None
            if h in (1, 2, 3):
                kv = val('r2')
                key = ('stack%+d' % kv[1]) if kv and kv[0] == 'stack' else repr(kv)
                ev.append(('helper', h, mname, key, val('r4') if h == 2 else None, val('r3') if h == 2 else None))
            for k in range(0, 6):
                st.pop('r%d' % k, None)
            if h == 1 and mname is not None:
                st['r0'] = ('ptr', mname, pc, 0)
        elif RE_IF.match(t):
            a, op, b, _ = RE_IF.match(t).groups()
            p = tagged(a)
            if p:
                if b == '0' and op in ('==', '!='):
                    ev.append(('null', p, 'null test: ' + t))
                else:
                    ev.append(('escape', p, 'pointer compared with non-null: ' + t))
            if b.startswith('r') and tagged(b):
                ev.append(('escape', tagged(b), 'pointer compared: ' + t))
        elif RE_GOTO.match(t) or t == 'exit':
            if t == 'exit' and tagged('r0'):
                ev.append(('escape', tagged('r0'), 'returned'))
        else:
            # unknown form: clobber written reg if obvious
            m = re.match(r'^(r\d+) = ', t)
            if m:
                st[m.group(1)] = None
            ev.append(('note', 'unmodelled: ' + t))

        for e in ev:
            events[(pc, repr(e))] = (pc, t, e)
        # null-test refinement: on the 'r == 0' taken edge the pointer is null
        ss = succs(i)
        for j in ss:
            nst = st
            m = RE_IF.match(t)
            if m and tagged(m.group(1)) and m.group(3) == '0':
                taken = (j == idx.get(pc + 1 + int(m.group(4))))
                isnull = (m.group(2) == '==' and taken) or (m.group(2) == '!=' and not taken)
                if isnull:
                    nst = dict(st)
                    ptr = tagged(m.group(1))
                    for k2, v2 in list(nst.items()):
                        if v2 == ptr:
                            nst[k2] = ('const', 0)
            work.append((j, nst, depth + 1))

    # liveness of fetch destinations: is dst read on some path before a write?
    def reads(t, r):
        return re.search(r'\b%s\b' % r, t.split(' = ', 1)[1] if (' = ' in t and not t.startswith('*') and not t.startswith('lock') and not t.startswith('if')) else t) is not None

    def fetch_used(i0, r):
        seen = set()
        stack = [j for j in succs(i0)]
        while stack:
            j = stack.pop()
            if j in seen:
                continue
            seen.add(j)
            _, t, _ = insns[j]
            m = RE_CALL.match(t)
            if m:
                h = int(m.group(1))
                if int(r[1:]) <= ARITY.get(h, 5) and r != 'r0':
                    return True, insns[j][0], t
                if r in ('r0', 'r1', 'r2', 'r3', 'r4', 'r5'):
                    continue
            if t == 'exit':
                if r == 'r0':
                    return True, insns[j][0], t
                continue
            # written without being read?
            mw = re.match(r'^(r\d+) = (.*)$', t)
            if mw and mw.group(1) == r and not re.search(r'\b%s\b' % r, mw.group(2)):
                continue
            if re.search(r'\b%s\b' % r, t):
                return True, insns[j][0], t
            stack.extend(succs(j))
        return False, None, None

    out = []
    for (pc, _), (pc2, t, e) in sorted(events.items()):
        if e[0] == 'access' and len(e) > 3:
            used, upc, ut = fetch_used(idx[pc], e[3][2])
            e = e[:3] + (('result used at %d: %s' % (upc, ut)) if used else 'result unused',)
        out.append((pc, t, e))
    return out


def main():
    path = sys.argv[1]
    maptypes = {}
    for a in sys.argv[2:]:
        k, v = a.split('=')
        maptypes[k] = int(v)
    for sec, name, insns in parse(path):
        res = analyze_func(insns, maptypes)
        lines = []
        for pc, t, e in res:
            kind = e[0]
            if kind == 'helper':
                h, m, key, flags, valp = e[1:]
                if m is None:
                    mdesc = 'map ?'
                else:
                    mdesc = '%s(type %s)' % (m, maptypes.get(m, '?'))
                if h == 1:
                    lines.append('  %4d lookup  %s key %s' % (pc, mdesc, key))
                elif h == 2:
                    fl = ('flags %d (%s)' % (flags[1], {0: 'BPF_ANY', 1: 'BPF_NOEXIST', 2: 'BPF_EXIST'}.get(flags[1], '?'))) if flags and flags[0] == 'const' else 'flags NOT CONSTANT (%r)' % (flags,)
                    vdesc = ('value = lookup result of %s@%d' % (valp[1], valp[2])) if valp and valp[0] == 'ptr' else ('value stack%+d' % valp[1] if valp and valp[0] == 'stack' else 'value %r' % (valp,))
                    lines.append('  %4d update  %s key %s %s, %s' % (pc, mdesc, key, fl, vdesc))
                else:
                    lines.append('  %4d delete  %s key %s' % (pc, mdesc, key))
            elif kind in ('access', 'escape', 'null', 'arith'):
                p = e[1]
                extra = (' [' + e[3] + ']') if len(e) > 3 else ''
                lines.append('  %4d   %-6s via lookup %s@%d: %s%s   | %s' % (pc, kind, p[1], p[2], e[2], extra, t))
            elif kind == 'note':
                lines.append('  %4d   note: %s' % (pc, e[1]))
        print('== %s  %s' % (sec, name))
        if lines:
            print('\n'.join(lines))
        else:
            print('  (no map helper calls)')


if __name__ == '__main__':
    main()
