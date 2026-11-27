#!/usr/bin/env python3
# Mutations of the second fix of the writer slots and the claim (6a2850c0b;
# plan D8 holder hints, D11c) and the earlier ones aimed at the changed
# code: each must make a [gpu-devhash-cpu] test fail. A baseline run must
# pass first. One at a time: edit, rebuild bpftime_runtime_tests, run the
# tag under a timeout, restore from git.
# usage: mutate_s2_ofd2.py OUT [label-prefix ...]
import pathlib, re, subprocess, sys, time
REPO = pathlib.Path('/home/jetson/src/bpftime-proto-70f7')
BUILD = REPO / 'build-cuda'
OUT = pathlib.Path(sys.argv[1])
CPP = 'runtime/src/bpf_map/gpu/nv_gpu_device_hash_map.cpp'
RT = BUILD / 'runtime/unit-test/bpftime_runtime_tests'
M = [
 # the second fix
 ('hint ignored by the discount',
  '\t\tif (holder_alive(__atomic_load_n(&writers[i].holder,\n\t\t\t\t\t\t __ATOMIC_ACQUIRE),\n\t\t\t\t ino)) {',
  '\t\tif (false) {'),
 ('hint ignored by a scanning writer',
  '\t\t\tif (was != me && holder_alive(was, e->ino)) {', '\t\t\tif (false) {'),
 ('hint ignored by a claimant',
  '\t\t\tif (got && was != 0 && was != me &&\n\t\t\t    holder_alive(was, e->ino)) {', '\t\t\tif (false) {'),
 ('own slot cleared by its own rescan',
  '\t\t\t\twas == me ? 0 :\n', '\t\t\t\tfalse ? 0 :\n'),
 ('the file alone taken for the description',
  '\t\t    (probe == ~0ull || description_holds(e.fd, e.ino, probe)))', '\t\t    true)'),
 ('a description at fd 0-2 kept', '\t\tif (fd >= 0 && fd < 3) {', '\t\tif (false) {'),
 ('numbers closed without checking',
  '\tif (e.fd >= 0 && raw_fstat(e.fd, &st) && st.st_dev == e.dev &&\n\t    st.st_ino == e.ino)\n\t\traw_close(e.fd);',
  '\t(void)st;\n\tif (e.fd >= 0)\n\t\traw_close(e.fd);'),
 ('a vfork child records in the table',
  '\tif (table_pid.load(std::memory_order_relaxed) != raw_getpid())\n\t\treturn nullptr;', ''),
 # the earlier ones, on the changed code
 ('description without O_CLOEXEC', '\t\tfd = raw_open(path, O_RDWR | O_CLOEXEC);', '\t\tfd = raw_open(path, O_RDWR);'),
 ('no fork handler',
  '\tfor (uint32_t i = 0; i < LOCAL_MAPS; i++)\n\t\tif (local_maps[i].obj.load(std::memory_order_relaxed) != nullptr)\n\t\t\tclose_description(local_maps[i]);',
  ''),
 ('the leftover count of a gone holder not cleared',
  '\t\t\t\t    __atomic_exchange_n(&writers[i].count,\n\t\t\t\t\t\t\t\t0, __ATOMIC_SEQ_CST);',
  '\t\t\t\t    __atomic_load_n(&writers[i].count,\n\t\t\t\t\t\t\t\t__ATOMIC_SEQ_CST);'),
 ('the overflow decision not cached',
  '\t\te->retry_ns.store(monotonic_ns() + (held ? OVERFLOW_RETRY_NS :\n\t\t\t\t\t\t\t     NO_FILE_RETRY_NS),\n\t\t\t\t  std::memory_order_relaxed);',
  '\t\te->retry_ns.store(0, std::memory_order_relaxed);'),
 ('a claim that always succeeds',
  '\t\t\tgot = ofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE,\n\t\t\t\t       F_WRLCK) == 0;',
  '\t\t\tofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE, F_WRLCK);\n\t\t\tgot = true;'),
 ('no catch after the seal',
  '\t} catch (...) {\n\t\t// after a seal no CPU writer may wait on a copy that never comes\n\t\tif (get_phase() == DEVHASH_PHASE_SEALING)\n\t\t\tunseal();\n\t\trelease_claim();\n\t\tthrow;\n\t}',
  '\t} catch (...) {\n\t\tthrow;\n\t}'),
 ('no discount of dead writers', '\tif (probe_fd < 0)\n\t\treturn; // no file to probe: every count stands (alive)', '\treturn;'),
 ('no discount in the SEAL_DEAD branch',
  '\t\t// again\n\t\tdiscount_dead_writers(probe, (uint64_t)dev, (uint64_t)ino);\n\t\tif (bpftime_devhash_sealer_poll(counts, n, stride) != 0)',
  '\t\t// again\n\t\tif (bpftime_devhash_sealer_poll(counts, n, stride) != 0)'),
 ('no periodic discount',
  '\t\tif (polls % 4096 == 0) {\n\t\t\tdiscount_dead_writers(probe, (uint64_t)dev, (uint64_t)ino);',
  '\t\tif (polls % 4096 == 0) {'),
 ('the overflow slot left out of the sum',
  'bool nv_gpu_device_hash_map_impl::seal(uint64_t bound_ns)\n{\n\tconst uint64_t n = WRITER_SLOTS + 1, stride',
  'bool nv_gpu_device_hash_map_impl::seal(uint64_t bound_ns)\n{\n\tconst uint64_t n = WRITER_SLOTS, stride'),
 ('the destructor closes nothing',
  '\tif (e != nullptr &&\n\t    e->gen.load(std::memory_order_relaxed) == generation &&\n\t    table_pid.load(std::memory_order_relaxed) == raw_getpid())\n\t\tclose_description(*e);',
  '\t(void)e;'),
 ('a stale description kept at a new map\'s address',
  '\t\t\tclose_description(*e);\n\t\t\te->overflow_logged = false;\n\t\t\te->gen.store(gen, std::memory_order_release);',
  '\t\t\te->overflow_logged = false;\n\t\t\te->gen.store(gen, std::memory_order_release);'),
]


def sh(cmd, timeout, cwd=REPO):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def log(line):
    with OUT.open('a') as f:
        f.write(line + '\n')
    print(line, flush=True)


def run_tests():
    try:
        q = subprocess.run(['timeout', '-s', 'KILL', '900', str(RT), '[gpu-devhash-cpu]'],
                           cwd=BUILD, capture_output=True, text=True, timeout=960,
                           env={'SPDLOG_LEVEL': 'off', 'PATH': '/usr/bin:/bin'})
        return q.returncode, q.stdout + q.stderr
    except subprocess.TimeoutExpired:
        return -9, 'python timeout'


def summary(out):
    fails = [l.strip()[:130] for l in out.splitlines() if 'FAILED' in l][:3]
    summ = [l.strip() for l in out.splitlines() if 'test cases' in l or 'All tests passed' in l][-1:]
    return ' | '.join(fails + summ)


only = sys.argv[2:]
rc, out = sh(['cmake', '--build', str(BUILD), '--target', 'bpftime_runtime_tests', '-j4'], 3600)
rc, out = run_tests()
log(f'baseline: rc={rc} | {summary(out)}')
if rc != 0:
    sys.exit('baseline failed')
for label, old, new in M:
    if only and not any(label.startswith(o) for o in only):
        continue
    p = REPO / CPP
    s = p.read_text()
    if s.count(old) != 1:
        log(f'{label}: TEXT NOT FOUND ({s.count(old)})')
        continue
    p.write_text(s.replace(old, new))
    try:
        t0 = time.time()
        rc, out = sh(['cmake', '--build', str(BUILD), '--target', 'bpftime_runtime_tests', '-j4'], 3600)
        if rc != 0:
            errs = [l for l in out.splitlines() if 'error' in l][:4]
            log(f'{label}: BUILD FAILED | ' + ' | '.join(errs)[:700])
            continue
        rc, out = run_tests()
        # a crash or a kill is not a catch by a test
        verdict = 'CAUGHT' if rc in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) or (0 < rc < 128 and 'FAILED' in out) else ('MISSED' if rc == 0 else f'ABNORMAL({rc})')
        log(f'{label}: {verdict} rc={rc} [{time.time() - t0:.0f}s] | {summary(out)}')
        (OUT.parent / ('m2-' + re.sub(r'[^A-Za-z0-9]+', '-', label)[:60] + '.log')).write_text(out)
    finally:
        sh(['git', 'checkout', '--', CPP], 60)

rc, out = sh(['cmake', '--build', str(BUILD), '--target', 'bpftime_runtime_tests', '-j4'], 3600)
log(f'rebuilt: rc={rc}')
rc, out = sh(['git', 'status', '--short'], 60)
log('git status after restore: ' + (out.strip() or 'clean'))
