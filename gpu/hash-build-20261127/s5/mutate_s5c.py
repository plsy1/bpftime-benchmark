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
 ('C1 the claim check takes a lock line\'s start only (GPU, slot 255)',
  [(HM, '\t\tif (start <= byte && byte <= end)\n\t\t\theld = true;', '\t\tif (start == byte)\n\t\t\theld = true;')],
  ['s5-slot255']),
 ('C2 unreadable fdinfo taken as a lost claim',
  [(HM, '\tif (!opened || !lines)\n\t\treturn true;', '\tif (!opened || !lines)\n\t\treturn false;')],
  ('case', 'The claim counts as held when fdinfo cannot be read')),
 ('C3 the number not checked against the segment file first',
  [(HM, '\tif (!raw_fstat(fd, &st) || st.st_ino != ino)\n\t\treturn false;\n\tchar path[64];\n\tsnprintf(path, sizeof(path), "/proc/self/fdinfo/%d", fd);\n\tbool held = false, lines = false;',
        '\t(void)st;\n\tchar path[64];\n\tsnprintf(path, sizeof(path), "/proc/self/fdinfo/%d", fd);\n\tbool held = false, lines = false;')],
  ('case', 'Giving the claim back leaves another file at its number alone')),
 ('C4 the exit words not written for hash maps',
  [(MM, '\t\t\t__atomic_store_n(\n\t\t\t\th.map->status_word(DEVHASH_STATUS_SYNCS_AT_EXIT),\n\t\t\t\tsyncs, __ATOMIC_RELEASE);',
        '\t\t\t(void)h;')],
  ('case', 'S5: a real exit with a device hash map registered')),
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
    name = 'bpftime-s5-mutc-%d' % os.getpid()
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
