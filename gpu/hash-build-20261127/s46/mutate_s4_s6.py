#!/usr/bin/env python3
# S4 and S6 mutations (plan S4.1, S4.2, S6 text and GPU tests): each must
# make a test fail. One at a time: edit, rebuild, run the tests named,
# restore from git. usage: mutate_s4_s6.py OUT [label-prefix ...]
import pathlib, re, subprocess, sys, time
REPO = pathlib.Path('/home/jetson/src/bpftime-proto-70f7')
BUILD = REPO / 'build-cuda'
OUT = pathlib.Path(sys.argv[1])
LOCK = '/tmp/claude-2002/-home-jetson-src/548cc3e6-4b28-4a87-b830-6e00c1ee829e/scratchpad/gpu.lock'
RT = str(BUILD / 'runtime/unit-test/bpftime_runtime_tests')
NV = str(BUILD / 'attach/nv_attach_impl/test/bpftime_nv_attach_tests')
MM = 'runtime/src/bpf_map/gpu/nv_gpu_model_maps.cpp'
RO = 'attach/nv_attach_impl/nv_thread_local_routing.cpp'
NA = 'attach/nv_attach_impl/nv_attach_impl.cpp'
SNAP = [('rt', 'The hash snapshot stages headers and live rows and counters,The hash snapshot acquires the headers')]
ROUTE = [('nv', '[hash-map-routing]'), ('rt', 'Routed 1533 calls agree with the walk on the GPU')]
# (label, file, old, new, count: 1 or 'all', tests)
M = [
 ('S4.1 non-live rows summed', MM, '\t@%p3 bra $L_stage;\n', '', 1, SNAP),
 ('S4.1 claiming staged as empty', MM, '\tand.b64 %rd19, %rd17, 7;\n',
  '\tand.b64 %rd19, %rd17, 7;\n\tsetp.eq.u64 %p7, %rd19, 1;\n\tselp.b64 %rd17, 0, %rd17, %p7;\n', 1, SNAP),
 ('S4.1 a row at or above r read as sharded', MM, '\t@%p5 bra $L_single;\n', '', 1, SNAP),
 ('S4.2 the header load relaxed', MM, 'ld.acquire.gpu.global.b64 %rd17', 'ld.relaxed.gpu.global.b64 %rd17', 1, SNAP),
 ('S6 no state check', RO,
  '\t  << "\\tsetp.eq.u64 \\t" << r("p") << ", " << r("x") << ", "\n\t  << dh::ST_LIVE << ";\\n"',
  '\t  << "\\tsetp.ne.u64 \\t" << r("p") << ", " << r("x") << ", "\n\t  << dh::ST_EMPTY << ";\\n"', 1, ROUTE),
 ('S6 a row at or above r addressed as sharded', RO,
  '\tif (l.r < d.max_entries)\n\t\to << "\\tselp.b64', '\tif (false)\n\t\to << "\\tselp.b64', 1, ROUTE),
 ('S6 row bits shifted by one', RO, '\t  << dh::ROW_SHIFT << ";\\n"', '\t  << (dh::ROW_SHIFT + 1) << ";\\n"', 1, ROUTE),
 ('S6 the update result zero-extended', RO,
  '\t\t\t  ret)\n\t  << "} // bpftime hash-map update\\n";',
  '\t\t\t  ret)\n\t  << "\\tand.b64 \\t" << ret << ", " << ret << ", 4294967295;\\n"\n\t  << "} // bpftime hash-map update\\n";', 1, ROUTE),
 ('S6 the fallback writes the program register', RO,
  '{ r("b"), r("kk"), r("m"), r("n") }, r("v"))', '{ r("b"), r("kk"), r("m"), r("n") }, ret)', 1, ROUTE),
 ('S6 labels without the sequence number', RO, '$Lbph_hit_" << N', '$Lbph_hit_"', 'all', ROUTE),
 ('S6 the text per call site (twice)', RO,
  '\t\t      DEVHASH_PTX + "\\n" + ptx.substr(at);', '\t\t      DEVHASH_PTX + "\\n" + DEVHASH_PTX + "\\n" + ptx.substr(at);', 1, ROUTE),
 ('S6 the text when no 1533 call was rewritten', RO,
  '\tif (st.hash_lookups + st.hash_inserts > 0) {', '\tif (true) {', 1, ROUTE),
 ('S6 the text appended after the probes', RO,
  '\t\tptx = ptx.substr(0, at) +\n\t\t      "\\n// bpftime device hash map functions (devhash_ptx.h)\\n" +\n\t\t      DEVHASH_PTX + "\\n" + ptx.substr(at);',
  '\t\t(void)at;\n\t\tptx = ptx + "\\n// bpftime device hash map functions (devhash_ptx.h)\\n" +\n\t\t      DEVHASH_PTX + "\\n";', 1, ROUTE),
 ('S6 1533 not in the gate', NA, 'm.map_type == 1533 /* device hash */', 'm.map_type == 1599 /* device hash */', 1, ROUTE),
 ('S6 the inline header load weak', RO, '\t  << "\\tld.relaxed.gpu.global.b64 \\t" << r("h")', '\t  << "\\tld.global.b64 \\t" << r("h")', 1, ROUTE),
]


def sh(cmd, timeout, cwd=REPO):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def log(line):
    with OUT.open('a') as f:
        f.write(line + '\n')
    print(line, flush=True)


only = sys.argv[2:]
for label, f, old, new, n, tests in M:
    if only and not any(label.startswith(o) for o in only):
        continue
    p = REPO / f
    s = p.read_text()
    c = s.count(old)
    if c == 0 or (n == 1 and c != 1):
        log(f'{label}: TEXT NOT FOUND ({c})')
        continue
    p.write_text(s.replace(old, new) if n == 'all' else s.replace(old, new, 1))
    try:
        t0 = time.time()
        rc, out = sh(['cmake', '--build', str(BUILD), '--target', 'bpftime_runtime_tests', 'bpftime_nv_attach_tests', '-j4'], 3600)
        if rc != 0:
            errs = [l for l in out.splitlines() if 'error' in l][:4]
            log(f'{label}: BUILD FAILED | ' + ' | '.join(errs)[:700])
            continue
        results = []
        caught = False
        for which, spec in tests:
            binary = RT if which == 'rt' else NV
            cmd = ['timeout', '-s', 'KILL', '600', 'flock', '-o', LOCK, binary, spec]
            try:
                q = subprocess.run(cmd, cwd=BUILD, capture_output=True, text=True, timeout=660,
                                   env={'SPDLOG_LEVEL': 'off', 'PATH': '/usr/bin:/bin'})
                rc2, o2 = q.returncode, q.stdout + q.stderr
            except subprocess.TimeoutExpired:
                rc2, o2 = -9, 'python timeout'
            caught = caught or rc2 != 0
            fails = [l.strip()[:120] for l in o2.splitlines() if 'FAILED' in l][:2]
            summ = [l.strip() for l in o2.splitlines() if 'test cases' in l or 'All tests passed' in l][-1:]
            results.append(f'{which} rc={rc2} ' + ' | '.join(fails + summ))
            (OUT.parent / ('m-' + re.sub(r'[^A-Za-z0-9]+', '-', label)[:50] + f'-{which}.log')).write_text(o2)
        log(f"{label}: {'CAUGHT' if caught else 'MISSED'} [{time.time() - t0:.0f}s] | " + ' || '.join(results))
    finally:
        sh(['git', 'checkout', '--', f], 60)

rc, out = sh(['cmake', '--build', str(BUILD), '--target', 'bpftime_runtime_tests', 'bpftime_nv_attach_tests', '-j4'], 3600)
log(f'rebuilt: rc={rc}')
rc, out = sh(['git', 'status', '--short'], 60)
log('git status after restore: ' + (out.strip() or 'clean'))
