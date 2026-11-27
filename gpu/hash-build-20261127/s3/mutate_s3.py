#!/usr/bin/env python3
# S3 mutations (plan PLAN-stage1.md, S3.2-S3.11): each must make the named
# test fail. One mutation at a time: edit the source (or, for the text checks
# of S3.11, the generated PTX header), regenerate what the edit feeds, rebuild
# the test binary, run the test case under the GPU lock and a timeout, then
# restore the files from git. Results to the file given as argv[1].
import pathlib, re, subprocess, sys, time
REPO = pathlib.Path('/home/jetson/src/bpftime-proto-70f7')
BUILD = REPO / 'build-cuda'
OUT = pathlib.Path(sys.argv[1])
LOCK = '/tmp/claude-2002/-home-jetson-src/548cc3e6-4b28-4a87-b830-6e00c1ee829e/scratchpad/gpu.lock'
RT = BUILD / 'runtime/unit-test/bpftime_runtime_tests'
NV = BUILD / 'attach/nv_attach_impl/test/bpftime_nv_attach_tests'
DH = 'attach/nv_attach_impl/devhash/devhash.cu'
DHPTX = 'attach/nv_attach_impl/devhash_ptx.h'
TR = 'attach/nv_attach_impl/trampoline/default_trampoline.cu'
MAPCPP = 'runtime/src/bpf_map/gpu/nv_gpu_device_hash_map.cpp'
GENERATED = ['attach/nv_attach_impl/devhash_ptx.h',
             'attach/nv_attach_impl/devhash_test_ptx.h',
             'attach/nv_attach_impl/trampoline_ptx.h']

T_IMAGES = "Device hash map on the GPU: run d on S2's images"
T_FULL = 'Device hash map on the GPU: walks end on a full directory'
T_OPAQUE = 'Device hash map on the GPU: opaque slots are walked past'
T_ABI = 'Device hash map on the GPU: results as the routing pass calls them'
T_TRAMP = 'The trampoline answers 1533 calls without a host call'


def fp_stale(text):
    # the fingerprint literal of __bpftime_devhash_layout_fp, made stale
    m = re.search(r'(__bpftime_devhash_layout_fp\(\)\s*\{[^}]*mov\.u64\s+%rd\d+,\s*)(-?\d+)(;)', text)
    assert m, 'fingerprint not found'
    return text[:m.start(2)] + str(int(m.group(2)) ^ 0x10) + text[m.end(2):]


# (label, edits [(file, old, new, count)], regen, target, binary, test, timeout)
# count: 1 = the old text must occur exactly once; 'first' = replace the first
# of several; a callable = apply it to the whole text
M = [
 ('S3.2 build_image leaves count 0',
  [(MAPCPP, '\tout.count = n;\n\tif (!ok)', '\tout.count = 0;\n\t(void)n;\n\tif (!ok)', 1)],
  None, 'bpftime_runtime_tests', RT, T_IMAGES, 1800),
 ('S3.3 takeover_image does not renumber rows',
  [(MAPCPP, '\t\t\timg.headers[s] = make_header(key_of(h), live, ST_LIVE);',
    '\t\t\timg.headers[s] = make_header(key_of(h), row_of(h), ST_LIVE);', 1)],
  None, 'bpftime_runtime_tests', RT, T_IMAGES, 1800),
 ('S3.6 no S bound on the walks',
  [(DH, '\tfor (u64 n = 0; n < m.l.slots; n++) {\n\t\tconst u64 h = ld_relaxed(m.g + slot_offset(m.l, i));',
    '\tfor (u64 n = 0;; n++) {\n\t\tconst u64 h = ld_relaxed(m.g + slot_offset(m.l, i));', 1),
   (DH, '\tconst u64 cnt = ctl(m, CTL_COUNT);\n\tfor (u64 n = 0; n < m.l.slots;) {',
    '\tconst u64 cnt = ctl(m, CTL_COUNT);\n\tfor (u64 n = 0;;) {', 1)],
  'devhash', 'bpftime_runtime_tests', RT, T_FULL, 1800),
 ('S3.7 unbounded full-directory wait',
  [(DH, '\t\tif (total >= BPFTIME_DEVHASH_WAIT_BOUND) {\n\t\t\tcount(m, CNT_UNSTABLE_E2BIG);',
    '\t\tif (false) {\n\t\t\tcount(m, CNT_UNSTABLE_E2BIG);', 1)],
  'devhash', 'bpftime_runtime_tests', RT, T_FULL, 1800),
 ('S3.8 key compared before state',
  [(DH, '\t\tif (holds_key(h, k)) {\n\t\t\tif (state_of(h) != ST_LIVE)',
    '\t\tif (key_of(h) == k) {\n\t\t\tif (state_of(h) != ST_LIVE)', 1),
   (DH, '\t\t\tif (holds_key(h, key)) {\n\t\t\t\tif (state_of(h) == ST_LIVE)\n\t\t\t\t\treturn eexist(m, OC_EEXIST_LIVE);',
    '\t\t\tif (key_of(h) == key) {\n\t\t\t\tif (state_of(h) == ST_LIVE)\n\t\t\t\t\treturn eexist(m, OC_EEXIST_LIVE);', 1)],
  'devhash', 'bpftime_runtime_tests', RT, T_OPAQUE, 900),
 ('S3.9 bad flags not counted',
  [(DH, '\t\tcount(m, CNT_BAD_FLAGS);\n', '', 1)],
  'devhash', 'bpftime_runtime_tests', RT, T_ABI, 900),
 ('S3.9 an int return (declared int)',
  [(DH, 'extern "C" __device__ __noinline__ u64\n__bpftime_devhash_insert(',
    'extern "C" __device__ __noinline__ int\n__bpftime_devhash_insert(', 1)],
  'devhash', 'bpftime_runtime_tests', RT, T_ABI, 900),
 ('S3.9 an int return (32-bit result, zero-extended)',
  [(DH, '\t\treturn R_EINVAL;\n\t}\n\tconst u64 value = *(const u64 *)value_ptr;\n\treturn insert_v2(m, (u32)key, value);',
    '\t\treturn (u64)(u32)R_EINVAL;\n\t}\n\tconst u64 value = *(const u64 *)value_ptr;\n\treturn (u64)(u32)insert_v2(m, (u32)key, value);', 1)],
  'devhash', 'bpftime_runtime_tests', RT, T_ABI, 900),
 ('S3.10 trampoline lookup branch removed',
  [(TR, '\tconst auto &map_info = ::map_info[map];\n\tif (map_info.map_type == BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP)\n\t\treturn 0;\n\tif (map_info.map_type == BPF_MAP_TYPE_GPU_THREAD_LOCAL_ARRAY_MAP) {\n\t\tauto real_key = *(uint32_t *)(uintptr_t)key;\n\t\treturn (uint64_t)(uintptr_t)thread_local_offset(',
    '\tconst auto &map_info = ::map_info[map];\n\tif (map_info.map_type == BPF_MAP_TYPE_GPU_THREAD_LOCAL_ARRAY_MAP) {\n\t\tauto real_key = *(uint32_t *)(uintptr_t)key;\n\t\treturn (uint64_t)(uintptr_t)thread_local_offset(', 1)],
  'trampoline', 'bpftime_runtime_tests', RT, T_TRAMP, 900),
 ('S3.11 .release changed to .relaxed',
  [(DH, 'asm volatile("st.release.gpu.global.b64 [%0], %1;"',
    'asm volatile("st.relaxed.gpu.global.b64 [%0], %1;"', 1)],
  'devhash', 'bpftime_nv_attach_tests', NV, '[devhash-layout]', 600),
 ('S3.11 one walk load made ld.global',
  [(DHPTX, 'ld.relaxed.gpu.global.b64', 'ld.global.b64', 'first')],
  None, 'bpftime_nv_attach_tests', NV, '[devhash-layout]', 600),
 ('S3.11 a stale fingerprint',
  [(DHPTX, None, fp_stale, 'fn')],
  None, 'bpftime_nv_attach_tests', NV, '[devhash-layout]', 600),
 ('S3.11 a .relaxed load in the trampoline 1533 branch',
  [(TR, '\tconst auto &map_info = ::map_info[map];\n\tif (map_info.map_type == BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP)\n\t\treturn 0;\n\tif (map_info.map_type == BPF_MAP_TYPE_GPU_THREAD_LOCAL_ARRAY_MAP) {',
    '\tconst auto &map_info = ::map_info[map];\n\tif (map_info.map_type == BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP) {\n\t\tuint64_t v;\n\t\tasm volatile("ld.relaxed.gpu.global.u64 %0, [%1];" : "=l"(v) : "l"(map_info.extra_buffer) : "memory");\n\t\t(void)v;\n\t\treturn 0;\n\t}\n\tif (map_info.map_type == BPF_MAP_TYPE_GPU_THREAD_LOCAL_ARRAY_MAP) {', 1)],
  'trampoline', 'bpftime_nv_attach_tests', NV, '[devhash-layout]', 600),
]


def sh(cmd, timeout, cwd=REPO):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def regen(kind):
    if kind == 'devhash':
        return sh(['make', '-C', 'attach/nv_attach_impl/devhash'], 900)
    if kind == 'trampoline':
        return sh(['make', '-C', 'attach/nv_attach_impl/trampoline', 'CLANG=clang++-16', '../trampoline_ptx.h'], 900)
    return 0, ''


def build(target):
    return sh(['cmake', '--build', str(BUILD), '--target', target, '-j4'], 3600)


def restore(files):
    sh(['git', 'checkout', '--'] + sorted(set(files)) + GENERATED, 120)


def summary(out):
    keep = []
    for l in out.splitlines():
        if ('FAILED' in l or 'test cases' in l or 'All tests passed' in l
                or re.search(r'child \S+ status', l) or 'JIT log' in l and 'error' in l
                or 'with violations' in l or l.startswith('violations by kind')
                or 'served' in l):
            keep.append(l.strip()[:220])
    return ' | '.join(keep[:14])


def log(line):
    with OUT.open('a') as f:
        f.write(line + '\n')
    print(line, flush=True)


only = set(sys.argv[2:])
for label, edits, kind, target, binary, test, tmo in M:
    if only and not any(label.startswith(o) for o in only):
        continue
    files = []
    try:
        for f, old, new, n in edits:
            p = REPO / f
            s = p.read_text()
            if n == 'fn':
                s2 = new(s)
            elif n == 'first':
                assert s.count(old) >= 1, (label, f, 'not found')
                s2 = s.replace(old, new, 1)
            else:
                assert s.count(old) == 1, (label, f, s.count(old))
                s2 = s.replace(old, new)
            p.write_text(s2)
            files.append(f)
        t0 = time.time()
        rc, out = regen(kind)
        if rc != 0:
            log(f'{label}: REGEN FAILED (counts as caught only if intended) | {out[-600:]!r}')
            continue
        rc, out = build(target)
        if rc != 0:
            errs = [l for l in out.splitlines() if 'error' in l][:5]
            log(f'{label}: BUILD FAILED | ' + ' | '.join(errs)[:900])
            continue
        cmd = ['timeout', '-s', 'KILL', str(tmo), 'flock', LOCK, 'env', 'SPDLOG_LEVEL=off',
               str(binary), test]
        try:
            p = subprocess.run(cmd, cwd=BUILD, capture_output=True, text=True, timeout=tmo + 60)
            rc, out = p.returncode, p.stdout + p.stderr
        except subprocess.TimeoutExpired:
            rc, out = -9, 'python timeout'
        verdict = 'CAUGHT' if rc != 0 else 'MISSED'
        log(f'{label}: {verdict} rc={rc} [{time.time() - t0:.0f}s] | {summary(out)}')
        (OUT.parent / ('m-' + re.sub(r'[^A-Za-z0-9]+', '-', label)[:60] + '.log')).write_text(out)
    finally:
        restore(files)

# back to the committed tree, rebuilt
restore([])
for t in ('bpftime_runtime_tests', 'bpftime_nv_attach_tests'):
    rc, out = build(t)
    log(f'rebuilt {t}: rc={rc}')
rc, out = sh(['git', 'status', '--short'], 60)
log('git status after restore: ' + (out.strip() or 'clean'))
