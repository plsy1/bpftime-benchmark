#!/usr/bin/env python3
"""check_ptx.py PATCHED.ptx KERNEL OUT_PREFIX [--ptxas PATH | --sass FILE] [--min-sass-probe-calls N]

Checks that the e7 probe survived compilation in a patched module kept with
BPFTIME_CUDA_KEEP_PTX=1 (the control run):
  PTX
  - KERNEL calls __retprobe_func__KERNEL before each of its ret/exit;
  - the probe function calls helpers 520 (active mask), 511 (lane), 509 (smid)
    and 1 (map lookup), and has an atom/red add on 64 bits;
  - the module defines _bpf_helper_ext_0520 with activemask.b32.
  SASS (with --ptxas: assemble the module for sm_87 and disassemble it; with
  --sass: read an existing cuobjdump -sass dump instead), scoped by address
  inside KERNEL's function, because ptxas puts the whole trampoline into the
  kernel's section and other helpers have their own VOTE and ATOM:
  - subroutine starts are the CALL.REL targets; the kernel body is the code
    before the first of them; the probe is the subroutine the body calls
    (or the body itself when ptxas inlined it);
  - the body calls the probe at N sites (N >= --min-sass-probe-calls, default
    1; the divergence control asks for 2: its two exits must stay separate);
  - the probe's first CALL goes to a subroutine with VOTE (helper 520:
    activemask), or the probe itself has a VOTE before any CALL when 520 was
    inlined; some CALL of the probe goes to a subroutine reading SR_LANEID
    (helper 511), or the probe reads it itself;
  - the probe's own range has an ATOM/RED .ADD (the histogram add).
Writes OUT_PREFIX.probe.ptx and OUT_PREFIX.check.json; exit 0 iff all pass.
Standard library only (python3 -I).
"""
import json
import os
import re
import subprocess
import sys


def cut_function(text, header_re):
    m = re.search(header_re, text, re.M)
    if not m:
        return None
    i = text.find('{', m.end())
    depth, j = 0, i
    while j < len(text):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                return text[m.start():j + 1]
        j += 1
    return None


SASS_LINE = re.compile(r'/\*([0-9a-f]{4,})\*/\s+(.*?)\s*;')
CALL_REL = re.compile(r'\bCALL\.REL(?:\.NOINC)?\s+0x([0-9a-f]+)\b')


def sass_function(sass, name):
    """[(addr, instruction)] of the function section named exactly NAME."""
    out = None
    for sec in sass.split('Function : ')[1:]:
        first = sec.split('\n', 1)[0].strip()
        if first == name:
            out = []
            for m in SASS_LINE.finditer(sec):
                out.append((int(m.group(1), 16), m.group(2)))
    return out


def check_sass(sass, kernel, min_probe_calls):
    """Scoped SASS checks; returns a dict of named results."""
    c = {}
    ins = sass_function(sass, kernel)
    c['sass_kernel_found'] = ins is not None
    if not ins:
        return c, False
    end = ins[-1][0] + 16
    targets = sorted(set(int(m.group(1), 16) for _, t in ins for m in [CALL_REL.search(t)] if m))
    starts = [0] + targets

    def range_of(start):
        nxt = [s for s in starts if s > start]
        return start, (nxt[0] if nxt else end)

    def instrs(lo, hi):
        return [(a, t) for a, t in ins if lo <= a < hi]

    body_lo, body_hi = range_of(0)
    body = instrs(body_lo, body_hi)
    body_calls = [int(CALL_REL.search(t).group(1), 16) for _, t in body if CALL_REL.search(t)]
    if body_calls:
        probe_start = body_calls[0]
        c['sass_body_probe_calls'] = sum(1 for x in body_calls if x == probe_start)
        c['sass_body_calls_one_target'] = len(set(body_calls)) == 1
        plo, phi = range_of(probe_start)
        c['sass_probe_inlined'] = False
    else:
        # ptxas inlined the probe into the kernel body
        c['sass_body_probe_calls'] = 0
        c['sass_body_calls_one_target'] = True
        plo, phi = body_lo, body_hi
        c['sass_probe_inlined'] = True
    c['sass_probe_range'] = '0x%x-0x%x' % (plo, phi)
    probe = instrs(plo, phi)
    pcalls = [int(CALL_REL.search(t).group(1), 16) for _, t in probe if CALL_REL.search(t)]
    c['sass_probe_call_targets'] = ['0x%x' % x for x in pcalls]

    def sub_has(start, pat):
        lo, hi = range_of(start)
        return any(re.search(pat, t) for _, t in instrs(lo, hi))

    vote = r'\bVOTEU?\.ANY\b'
    first_call_vote = bool(pcalls) and sub_has(pcalls[0], vote)
    first_call_addr = next((a for a, t in probe if CALL_REL.search(t)), phi)
    inline_vote = any(re.search(vote, t) for a, t in probe if a < first_call_addr)
    c['sass_0520_vote_first_call'] = first_call_vote
    c['sass_0520_vote_inlined'] = inline_vote
    c['sass_0520_vote'] = first_call_vote or inline_vote
    lane = r'\bSR_LANEID\b'
    c['sass_0511_laneid'] = any(sub_has(x, lane) for x in pcalls) or any(re.search(lane, t) for _, t in probe)
    c['sass_probe_atom_or_red'] = any(re.search(r'\b(?:ATOM|RED|ATOMG)\b[^;]*\.ADD', t) for _, t in probe)
    if body_calls:
        n_probe_calls_ok = c['sass_body_probe_calls'] >= min_probe_calls and c['sass_body_calls_one_target']
    else:
        n_probe_calls_ok = min_probe_calls <= 1
    c['sass_probe_call_sites_ok'] = n_probe_calls_ok
    ok = c['sass_0520_vote'] and c['sass_0511_laneid'] and c['sass_probe_atom_or_red'] and n_probe_calls_ok
    return c, ok


def main():
    args = sys.argv[1:]
    ptxas = sass_file = None
    min_calls = 1
    for opt in ('--ptxas', '--sass', '--min-sass-probe-calls'):
        if opt in args:
            k = args.index(opt)
            v = args[k + 1]
            del args[k:k + 2]
            if opt == '--ptxas':
                ptxas = v
            elif opt == '--sass':
                sass_file = v
            else:
                min_calls = int(v)
    if len(args) != 3:
        print(__doc__)
        return 2
    path, kernel, out = args
    text = open(path, errors='replace').read()
    probe = '__retprobe_func__' + kernel
    res = {'ptx': path, 'kernel': kernel, 'checks': {}}
    c = res['checks']

    entry = cut_function(text, r'^\s*\.visible\s+\.entry\s+' + re.escape(kernel) + r'\s*\(')
    c['kernel_found'] = entry is not None
    if entry:
        rets = re.findall(r'^\s*(?:[\w$.]+:\s*)?(?:@\S+\s+)?(?:ret|exit);', entry, re.M)
        calls = re.findall(r'\bcall(?:\.uni)?\s+' + re.escape(probe) + r'\s*;', entry)
        c['kernel_rets'] = len(rets)
        c['kernel_probe_calls'] = len(calls)
        c['probe_before_every_ret'] = len(rets) > 0 and len(calls) == len(rets)

    fn = cut_function(text, r'^[^\n]*\.func[^\n]*\b' + re.escape(probe) + r'\b')
    c['probe_found'] = fn is not None
    if fn:
        open(out + '.probe.ptx', 'w').write(fn + '\n')
        for h in ('0520', '0511', '0509', '0001'):
            c['calls_helper_' + h] = bool(re.search(r'\bcall(?:\.uni)?\s*(?:\([^)]*\),\s*)?_bpf_helper_ext_' + h + r'\b', fn))
        adds = re.findall(r'\b(?:atom|red)(?:\.[a-z0-9_:]+)*\.add(?:\.[a-z0-9_:]+)*\.(?:u64|s64|b64)\b', fn)
        c['atomic_adds_u64'] = len(adds)
        c['atomic_forms'] = sorted(set(adds))
        c['probe_membar_sys'] = len(re.findall(r'\b(?:membar\.sys|fence\.sc\.sys|fence\.acq_rel\.sys)\b', fn))
    h520 = cut_function(text, r'^[^\n]*\.func[^\n]*\b_bpf_helper_ext_0520\b')
    c['helper_0520_defined'] = h520 is not None
    c['helper_0520_activemask'] = bool(h520 and 'activemask.b32' in h520)

    ok = all([c.get('kernel_found'), c.get('probe_before_every_ret'), c.get('probe_found'),
              c.get('calls_helper_0520'), c.get('calls_helper_0511'), c.get('calls_helper_0509'),
              c.get('calls_helper_0001'), c.get('atomic_adds_u64', 0) >= 1,
              c.get('helper_0520_activemask')])

    sass = None
    if sass_file:
        sass = open(sass_file, errors='replace').read()
        c['sass_source'] = sass_file
    elif ptxas:
        cubin = out + '.sm_87.cubin'
        tgt = re.sub(r'^\.target\s+\S+', '.target sm_87', text, count=1, flags=re.M)
        tmp_ptx = out + '.sm_87.ptx'
        open(tmp_ptx, 'w').write(tgt)
        r = subprocess.run([ptxas, '-arch=sm_87', '-o', cubin, tmp_ptx], capture_output=True, text=True, timeout=300)
        c['ptxas_rc'] = r.returncode
        if r.returncode == 0:
            cuobjdump = os.path.join(os.path.dirname(ptxas), 'cuobjdump')
            sass = subprocess.run([cuobjdump, '-sass', cubin], capture_output=True, text=True, timeout=300).stdout
            open(out + '.sass', 'w').write(sass)
            c['sass_source'] = out + '.sass'
        else:
            c['ptxas_err'] = r.stderr[-2000:]
            ok = False
    if sass is not None:
        sc, sok = check_sass(sass, kernel, min_calls)
        c.update(sc)
        ok = ok and sok
    elif not ptxas:
        c['sass_checked'] = False
    res['pass'] = bool(ok)
    json.dump(res, open(out + '.check.json', 'w'), indent=1)
    print(json.dumps(res, indent=1))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
