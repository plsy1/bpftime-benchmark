# S5 mutations: each applied alone, the build rebuilt, the named children
# run; a mutation is caught when any child exits non-zero.
import pathlib, subprocess, sys, time, os
W = pathlib.Path('/home/jetson/src/bpftime-proto-70f7')
LOCK = '/tmp/claude-2002/-home-jetson-src/548cc3e6-4b28-4a87-b830-6e00c1ee829e/scratchpad/gpu.lock'
OUT = pathlib.Path(sys.argv[1])
MH = 'runtime/src/handler/map_handler.cpp'
CC = 'runtime/src/attach/bpf_attach_ctx_cuda.cpp'
AC = 'runtime/src/attach/bpf_attach_ctx.cpp'
MM = 'runtime/src/bpf_map/gpu/nv_gpu_model_maps.cpp'
HM = 'runtime/src/bpf_map/gpu/nv_gpu_device_hash_map.cpp'
TAKEOVER = ['s5-owner-exit', 's5-takeover-fail', 's5-takeover']
M = [
 ('M1 1533 missing from get_gpu_map_max_thread_count',
  [(MH, '\t\t\treturn 0;\n\t\treturn shards;', '\t\t\treturn 0;\n\t\treturn 0;')], ['s5-first']),
 ('M2 no cache in the pre-pass or the copy',
  [(CC, 'gpu_model::find_device_hash_copy(*impl, nullptr) != 0)', 'false)'),
   (MM, '\t\t\treturn c.block;\n\tdevhash_image img;', '\t\t\t(void)c;\n\tdevhash_image img;')], ['s5-first']),
 ('M3 the copy made under map_binding_mutex',
  [(AC, '#ifdef BPFTIME_ENABLE_CUDA_ATTACH\n\t// 1533 copies are made outside the binding guard: the seal waits up\n\t// to a second, and the guard is a robust mutex a death would break\n\tprepare_device_hash_copies(manager);\n#endif\n', ''),
   (AC, '\t\tbinding_guard.emplace(map_binding_mutex(*memory));\n', '\t\tbinding_guard.emplace(map_binding_mutex(*memory));\n#ifdef BPFTIME_ENABLE_CUDA_ATTACH\n\tprepare_device_hash_copies(manager);\n#endif\n')], ['s5-first']),
 ('M4 takeover rows not renumbered',
  [(HM, 'img.headers[s] = make_header(key_of(h), live, ST_LIVE);', 'img.headers[s] = make_header(key_of(h), row_of(h), ST_LIVE);')], TAKEOVER),
 ('M5 PHASE 0 after a failed takeover',
  [(MM, '\t\tif (first) {\n\t\t\tif (mirror_seeded)', '\t\tif (true) {\n\t\t\tif (mirror_seeded)')], TAKEOVER),
 ('M6 OWNER not released on a failure',
  [(MM, '\t\tmap->release_claim();\n\t\treturn 0;\n\t};', '\t\treturn 0;\n\t};')], ['s5-fail']),
 ('M7 the delivery does not store SYNCS_COVERED',
  [(MM, 'map->status_word(DEVHASH_STATUS_SYNCS_COVERED),\n\t\t\t\tsyncs, __ATOMIC_RELAXED);', 'map->status_word(DEVHASH_STATUS_SYNCS_COVERED),\n\t\t\t\t0, __ATOMIC_RELAXED);')], ['s5-e2e']),
 ('M8 companion uniqueness: arrays only',
  [(CC, 'if (arrays != 1 || hashes != 1) {', 'if (arrays != 1) {')], ['s5-companion']),
 ('M9 companion uniqueness: hash maps only',
  [(CC, 'if (arrays != 1 || hashes != 1) {', 'if (hashes != 1) {')], ['s5-companion']),
 ('M10 one broken flag',
  [(MM, '\t\t\ts.hash_broken = true;\n\t\ts.hash_fn', '\t\t\ts.hash_broken = s.broken = true;\n\t\ts.hash_fn')], ['s5-isolation']),
 ('M11 count not copied',
  [(MM, '\tb[bpftime_devhash::ctl_offset(bpftime_devhash::CTL_COUNT) / 8] =\n\t\timg.count;', '\t(void)img.count;')], ['s5-first']),
 ('M12 companion shape not checked',
  [(CC, '\t\tif (!companion_shape(*map_at(manager, index))) {', '\t\tif (false) {')], ['s5-companion']),
 ('M13 no unseal after a failed first copy',
  [(MM, '\t\t\tmap->unseal(); // PHASE 0: CPU writes work again', '\t\t\t; // unseal removed')], ['s5-fail']),
 ('M14 mirror not reset after a failed first copy',
  [(MM, '\t\t\t\tmap->reset_mirror();', '\t\t\t\t;')], ['s5-fail']),
 ('M15 the gate ignores CUDA links',
  [(CC, '\t\tif (!cuda_link)\n\t\t\treturn;', '\t\t(void)cuda_link;')], ['s5-gate']),
 ('M16 the direct run not honoured',
  [(CC, 'if (!cuda_ctx || direct_run || manager == nullptr ||', 'if (!cuda_ctx || manager == nullptr ||')], ['s5-gate']),
]
def build():
    r = subprocess.run(['timeout', '1800', 'cmake', '--build', 'build-cuda', '--target', 'bpftime_runtime_tests', '-j6'],
                       cwd=W, capture_output=True, text=True)
    return r.returncode == 0, r.stdout[-3000:] + r.stderr[-3000:]
def run_children(children):
    name = 'bpftime-s5-mut-%d' % os.getpid()
    out = []
    caught = False
    for w in children:
        env = dict(os.environ, BPFTIME_DEVHASH_GPU_CHILD=w, SPDLOG_LEVEL='off', BPFTIME_DEVHASH_S5_SHM=name)
        if children is not TAKEOVER:
            env.pop('BPFTIME_DEVHASH_S5_SHM')
        r = subprocess.run(['timeout', '150', 'flock', '-o', LOCK, './build-cuda/runtime/unit-test/bpftime_runtime_tests',
                            '[.devhash-gpu-child]', '-o', '/dev/null'], cwd=W, env=env, capture_output=True, text=True)
        lines = [l for l in r.stdout.splitlines() if 'RESULT' in l or 'got' in l or 'false' in l or 'missing' in l or 'companion' in l][:6]
        out.append('  %s exit %d: %s' % (w, r.returncode, ' | '.join(lines)))
        if r.returncode != 0:
            caught = True
            break
    try:
        os.unlink('/dev/shm/' + name)
    except FileNotFoundError:
        pass
    return caught, out
log = open(OUT, 'w')
def say(s):
    log.write(s + '\n'); log.flush()
only = sys.argv[2:] 
for title, edits, children in M:
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
            caught, out = run_children(children)
            say('%s: %s' % (title, 'CAUGHT' if caught else 'MISSED'))
            for l in out:
                say(l)
    for f, text in saved.items():
        (W / f).write_text(text)
built, msg = build()
say('restored build: %s' % ('ok' if built else 'FAILED\n' + msg))
