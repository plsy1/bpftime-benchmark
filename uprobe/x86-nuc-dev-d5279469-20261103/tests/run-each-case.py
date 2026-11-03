#!/usr/bin/env python3
"""Run every Catch2 test case of one binary in its own process, serially.

A crash or order dependence in one case then cannot hide the others.
Usage: run-each-case.py <binary> <out-name> [timeout-seconds, default 600]
(copied from ../../x86-nuc-dev-6ab9cbfe-20261102/tests; the timeout argument is new, so a4ac0d31's
hanging cases do not take 10 minutes each: the slowest case on 6ab9cbfe took 20.5 s)
"""
import json, os, subprocess, sys, time
from pathlib import Path

binary, name = Path(sys.argv[1]), sys.argv[2]
limit = int(sys.argv[3]) if len(sys.argv) > 3 else 600
label, root = 'dev-d5279469', '/home/y/src/bpftime-wt-uprobe-177a1dc'
out = Path(__file__).resolve().parent / name
out.mkdir(parents=True, exist_ok=False)
env = {k: v for k, v in os.environ.items() if k != 'BPFTIME_ARRAY_JIT_INLINE'}
names = subprocess.run([binary, '--list-tests', '--verbosity', 'quiet'], cwd=root,
                       capture_output=True, text=True, check=True).stdout.splitlines()
# Static constructors log to stdout before the list; those lines are not cases.
names = [n for n in names if n.strip() and not n.startswith('[20')]

def spec(n):
    for ch in '\\,[]*"':
        n = n.replace(ch, '\\' + ch)
    return n

results = []
for i, n in enumerate(names):
    t0 = time.monotonic()
    try:
        p = subprocess.run([binary, spec(n)], cwd=root, env=env, capture_output=True,
                           text=True, errors='replace', timeout=limit)
        rc, log = p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired as e:
        rc, log = 'timeout', (e.stdout or b'').decode(errors='replace') if isinstance(e.stdout, bytes) else (e.stdout or '')
    dt = time.monotonic() - t0
    # Catch2 exits non-zero on any failure; cases without assertions print no
    # 'All tests passed', so the exit status alone decides.
    ok = rc == 0
    results.append({'case': n, 'rc': rc, 'passed': ok, 'seconds': round(dt, 2)})
    if not ok:
        (out / f'case-{i:03d}.log').write_text(n + '\n' + log)
(out / 'results.json').write_text(json.dumps(results, indent=1, ensure_ascii=False))
bad = [r for r in results if not r['passed']]
print(f'{label} {name}: {len(results)} cases, {len(results) - len(bad)} passed, {len(bad)} failed')
for r in bad:
    print(f"  FAIL rc={r['rc']} {r['case']}")
