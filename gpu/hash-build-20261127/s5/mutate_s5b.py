# S5 mutations, second batch (after the review): each applied alone, the
# build rebuilt, then the named children (in order, sharing a segment) or a
# whole test case of the parent; caught when any exits non-zero.
import pathlib, subprocess, sys, os
W = pathlib.Path('/home/jetson/src/bpftime-proto-70f7')
LOCK = '/tmp/claude-2002/-home-jetson-src/548cc3e6-4b28-4a87-b830-6e00c1ee829e/scratchpad/gpu.lock'
OUT = pathlib.Path(sys.argv[1])
CC = 'runtime/src/attach/bpf_attach_ctx_cuda.cpp'
AC = 'runtime/src/attach/bpf_attach_ctx.cpp'
MM = 'runtime/src/bpf_map/gpu/nv_gpu_model_maps.cpp'
HM = 'runtime/src/bpf_map/gpu/nv_gpu_device_hash_map.cpp'
PREPASS = '#ifdef BPFTIME_ENABLE_CUDA_ATTACH\n\t// 1533 copies are made outside the binding guard: the seal waits up\n\t// to a second, and the guard is a robust mutex a death would break\n\tprepare_device_hash_copies(manager);\n#endif\n'
M = [
 ('N1 a claim that always succeeds',
  [(HM, '\t\t\tgot = ofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE,\n\t\t\t\t       F_WRLCK) == 0;',
        '\t\t\tofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE, F_WRLCK);\n\t\t\tgot = true;'),
   (HM, '\t\t\tif (got && was != 0 && was != me &&\n\t\t\t    holder_alive(was, e->ino)) {', '\t\t\tif (false) {')],
  ('case', 'S5: no copy for another agent while the owner runs or is stopped and a takeover once it execs')),
 ('N2 the path chosen by the claim alone',
  [(HM, '\tcase DEVHASH_PHASE_SEALING:\n\t\treturn devhash_path::recover_sealing;', '\tcase DEVHASH_PHASE_SEALING:\n\t\treturn devhash_path::takeover;')],
  ('case', 'S5: owner deaths after the claim or in the seal wait or after the seal or between passes B and C')),
 ('N3 the copy made under map_binding_mutex (deaths)',
  [(AC, PREPASS, ''),
   (AC, '\t\tbinding_guard.emplace(map_binding_mutex(*memory));\n', '\t\tbinding_guard.emplace(map_binding_mutex(*memory));\n' + PREPASS)],
  ('case', 'S5: owner deaths after the claim or in the seal wait or after the seal or between passes B and C')),
 ('N4 the stored companion slot not checked at resolve',
  [(CC, '\t\tif (m == nullptr || !named(*m, name) || !companion_shape(*m)) {', '\t\tif (m == nullptr) {')],
  ('case', 'S5: a companion closed and its slot reused before a takeover is not bound')),
 ('N5 1533 left out of the pin list',
  [(CC, '\t\t\t\tm->inc_map_refcount();\n\t\t\t\tdevhash_pinned.push_back(*impl);', '\t\t\t\tdevhash_pinned.push_back(*impl);')],
  ['s5-close']),
 ('N6 word 15 not written',
  [(HM, '\t__atomic_store_n(status_word(DEVHASH_STATUS_REFUSED), refused,', '\t__atomic_store_n(status_word(DEVHASH_STATUS_REFUSED), 0,')],
  ['s5-refused']),
 ('N7 the copies made at the end of init (plan 1c)',
  [(AC, PREPASS, ''),
   (AC, '\t\tconst std::uint64_t epoch_after =\n', PREPASS.replace('\n\t', '\n\t\t') + '\t\tconst std::uint64_t epoch_after =\n')],
  ['s5-first']),
 ('N8 the gate ignores whether the agent handles CUDA attaches',
  [(CC, '\tif (attach_impls.count(attach::ATTACH_CUDA_PROBE) == 0 &&\n\t    attach_impls.count(attach::ATTACH_CUDA_RETPROBE) == 0)\n\t\treturn;\n', '')],
  ['s5-gate']),
 ('N9 a non-owner records the companion binding',
  [(CC, '\t\t\tif (gpu_model::make_device_hash_copy(t.impl,\n\t\t\t\t\t\t\t     t.comp.words) == 0)\n\t\t\t\tcontinue;',
        '\t\t\tif (t.comp.index >= 0)\n\t\t\t\tt.impl->set_companion(t.comp.index);\n\t\t\tif (gpu_model::make_device_hash_copy(t.impl,\n\t\t\t\t\t\t\t     t.comp.words) == 0)\n\t\t\t\tcontinue;')],
  ('case', 'S5: another agent leaves an unbound companion unbound')),
 ('N10 an exception in the copy not rolled back',
  [(MM, '\ttry {\n\t\tif (auto hook = devhash_copy_hook.load())\n\t\t\thook(map); // tests: after the claim and the seal\n',
        '\tif (auto hook = devhash_copy_hook.load())\n\t\thook(map);\n\ttry {\n')],
  ['s5-fail']),
 ('N12 the claim check takes a lock line\'s start only (adjacent locks merged)',
  [(HM, '\t\tif (start <= byte && byte <= end)\n\t\t\theld = true;', '\t\tif (start == byte)\n\t\t\theld = true;')],
  ('case', 'The claim stays held beside this process\'s writer slot 255')),
 ('N11 the dead owner\'s pass B occupancy ignored by the takeover image',
  [(HM, '\t\t\t// claiming, refused, opaque: occupied, no key\n\t\t\timg.headers[s] = OPAQUE_WORD;', '\t\t\t// claiming, refused, opaque: occupied, no key\n\t\t\timg.headers[s] = 0;')],
  ('case', 'S5: owner deaths after the claim or in the seal wait or after the seal or between passes B and C')),
]
def build():
    r = subprocess.run(['timeout', '1800', 'cmake', '--build', 'build-cuda', '--target', 'bpftime_runtime_tests', '-j6'],
                       cwd=W, capture_output=True, text=True)
    return r.returncode == 0, r.stdout[-3000:] + r.stderr[-3000:]
def run_target(target):
    out = []
    if isinstance(target, tuple):
        r = subprocess.run(['timeout', '900', 'flock', '-o', LOCK, './build-cuda/runtime/unit-test/bpftime_runtime_tests',
                            target[1]], cwd=W, env=dict(os.environ, SPDLOG_LEVEL='off'), capture_output=True, text=True)
        tail = [l for l in r.stdout.splitlines() if 'FAILED' in l or 'passed' in l or 'failed' in l or 'RESULT' in l or 'got' in l][:8]
        out.append('  case exit %d: %s' % (r.returncode, ' | '.join(tail)))
        return r.returncode != 0, out
    name = 'bpftime-s5-mutb-%d' % os.getpid()
    caught = False
    for w in target:
        env = dict(os.environ, BPFTIME_DEVHASH_GPU_CHILD=w, SPDLOG_LEVEL='off')
        r = subprocess.run(['timeout', '150', 'flock', '-o', LOCK, './build-cuda/runtime/unit-test/bpftime_runtime_tests',
                            '[.devhash-gpu-child]', '-o', '/dev/null'], cwd=W, env=env, capture_output=True, text=True)
        lines = [l for l in r.stdout.splitlines() if 'RESULT' in l or 'got' in l or 'false' in l][:6]
        out.append('  %s exit %d: %s' % (w, r.returncode, ' | '.join(lines)))
        if r.returncode != 0:
            caught = True
            break
    return caught, out
log = open(OUT, 'w')
def say(s):
    log.write(s + '\n'); log.flush()
only = sys.argv[2:]
for title, edits, target in M:
    if only and title.split()[0] not in only:
        continue
    saved = {}
    ok = True
    for f, a, b in edits:
        p = W / f
        if f not in saved:
            saved[f] = p.read_text()
        s = p.read_text()
        if s.count(a) != 1:
            say('%s: edit not found in %s (%d)' % (title, f, s.count(a)))
            ok = False
            break
        p.write_text(s.replace(a, b))
    if ok:
        built, msg = build()
        if not built:
            say('%s: BUILD FAILED\n%s' % (title, msg))
        else:
            caught, out = run_target(target)
            say('%s: %s' % (title, 'CAUGHT' if caught else 'MISSED'))
            for l in out:
                say(l)
    for f, text in saved.items():
        (W / f).write_text(text)
built, msg = build()
say('restored build: %s' % ('ok' if built else 'FAILED\n' + msg))
