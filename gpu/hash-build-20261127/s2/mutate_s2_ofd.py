#!/usr/bin/env python3
# Mutations of the S2 rework on OFD locks (plan section 10; S2.10, S2.14 and
# the S2.3/S2.4/S2.11 cases whose code changed): each must make a
# [gpu-devhash-cpu] test fail. One at a time: edit, rebuild
# bpftime_runtime_tests, run the tag under a timeout, restore from git.
# usage: mutate_s2_ofd.py OUT [label-prefix ...]
import pathlib, re, subprocess, sys, time
REPO = pathlib.Path('/home/jetson/src/bpftime-proto-70f7')
BUILD = REPO / 'build-cuda'
OUT = pathlib.Path(sys.argv[1])
CPP = 'runtime/src/bpf_map/gpu/nv_gpu_device_hash_map.cpp'
RT = BUILD / 'runtime/unit-test/bpftime_runtime_tests'
M = [
 ('S2.14 description without O_CLOEXEC',
  '\t\tfd = open(path, O_RDWR | O_CLOEXEC);', '\t\tfd = open(path, O_RDWR);'),
 ('S2.14 no fork handler',
  '\tfor (uint32_t i = 0; i < LOCAL_MAPS; i++) {\n\t\tif (local_maps[i].fd >= 0)\n\t\t\tclose(local_maps[i].fd);\n\t\tforget_locks(local_maps[i]);\n\t}\n\tlocal_mutex.unlock();',
  '\tlocal_mutex.unlock();'),
 ('S2.14 leftover count not cleared',
  '\t\t\tconst uint64_t left = __atomic_exchange_n(\n\t\t\t\t&writers[i].count, 0, __ATOMIC_SEQ_CST);',
  '\t\t\tconst uint64_t left = 0;'),
 ('S2.14 the sealer probes through the writers\' own description',
  '\tconst int probe = open_segment_file(this, dev, ino);',
  '\tconst int probe = [&] {\n\t\tstd::lock_guard<std::mutex> g(local_mutex);\n\t\tlocal_map *e = local_entry(this, generation);\n\t\t(void)dev; (void)ino;\n\t\treturn e != nullptr && ensure_description(*e, this) ? dup(e->fd) : -1;\n\t}();'),
 ('S2.14 the overflow decision not cached',
  '\t\te->retry_ns.store(monotonic_ns() + OVERFLOW_RETRY_NS,\n\t\t\t\t  std::memory_order_relaxed);',
  '\t\te->retry_ns.store(0, std::memory_order_relaxed);'),
 ('S2.14 a claim that always succeeds',
  '\t\t\tgot = ofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE,\n\t\t\t\t       F_WRLCK) == 0;',
  '\t\t\tofd_lock(e->fd, lock_base() + OWNER_LOCK_BYTE, F_WRLCK);\n\t\t\tgot = true;'),
 ('S2.14 no fstat check',
  '\treturn ensure_description(*e, this) && e->owner_locked;',
  '\treturn e->owner_locked;'),
 ('S2.14 no catch after the seal',
  '\t} catch (...) {\n\t\t// after a seal no CPU writer may wait on a copy that never comes\n\t\tif (get_phase() == DEVHASH_PHASE_SEALING)\n\t\t\tunseal();\n\t\trelease_claim();\n\t\tthrow;\n\t}',
  '\t} catch (...) {\n\t\tthrow;\n\t}'),
 ('S2.10 a cached copy treated as a takeover',
  '\tif (has_cached_copy)\n\t\treturn devhash_path::have_copy;\n\tswitch (phase) {',
  '\t(void)has_cached_copy;\n\tswitch (phase) {'),
 ('S2.3 writer skips its count',
  '\tuint64_t *w = my_writer_count();\n\tconst uint64_t ph = bpftime_devhash_writer_enter(w, &phase);\n\tcall_hook(devhash_hook_point::writer_after_phase_load, ph);\n\tif (ph != DEVHASH_PHASE_CPU) {\n\t\t__atomic_fetch_sub(w, 1, __ATOMIC_RELEASE);\n\t\t// the device copy',
  '\tuint64_t dummy = 1, *w = &dummy;\n\tconst uint64_t ph = bpftime_devhash_writer_enter(w, &phase);\n\tcall_hook(devhash_hook_point::writer_after_phase_load, ph);\n\tif (ph != DEVHASH_PHASE_CPU) {\n\t\t__atomic_fetch_sub(w, 1, __ATOMIC_RELEASE);\n\t\t// the device copy'),
 ('S2.4 no discount of dead writers',
  '\tif (probe_fd < 0)\n\t\treturn; // no file to probe: every count stands (alive)',
  '\treturn;'),
 ('S2.4 SEAL_DEAD not stored',
  '\t\t\t__atomic_store_n(&seal_dead, 1, __ATOMIC_RELEASE);', '\t\t\t(void)seal_dead;'),
 ('S2.4 SEAL_DEAD unconditional',
  '\t\tif (bpftime_devhash_sealer_poll(counts, n, stride) != 0)\n\t\t\treturn false;\n\t\t__atomic_store_n(&seal_dead, 0, __ATOMIC_RELEASE);',
  '\t\treturn false;'),
 ('S2.4 claim kept after a failed seal',
  '\tif (!ok)\n\t\trelease_claim();\n\treturn ok;', '\treturn ok;'),
 ('S2.11 PHASE-1 path skips the poll',
  '\treset_mirror();\n\t// seal() stores PHASE 1 again and polls',
  '\treset_mirror();\n\t__atomic_store_n(&phase, DEVHASH_PHASE_SEALING, __ATOMIC_SEQ_CST);\n\treturn build_image(out);\n\t// seal() stores PHASE 1 again and polls'),
]


def sh(cmd, timeout, cwd=REPO):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def log(line):
    with OUT.open('a') as f:
        f.write(line + '\n')
    print(line, flush=True)


only = sys.argv[2:]
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
        try:
            q = subprocess.run(['timeout', '-s', 'KILL', '900', str(RT), '[gpu-devhash-cpu]'],
                               cwd=BUILD, capture_output=True, text=True, timeout=960,
                               env={'SPDLOG_LEVEL': 'off', 'PATH': '/usr/bin:/bin'})
            rc, out = q.returncode, q.stdout + q.stderr
        except subprocess.TimeoutExpired:
            rc, out = -9, 'python timeout'
        fails = [l.strip()[:160] for l in out.splitlines() if 'FAILED' in l][:3]
        summ = [l.strip() for l in out.splitlines() if 'test cases' in l or 'All tests passed' in l][-1:]
        log(f"{label}: {'CAUGHT' if rc != 0 else 'MISSED'} rc={rc} [{time.time() - t0:.0f}s] | "
            + ' | '.join(fails + summ))
        (OUT.parent / ('m-' + re.sub(r'[^A-Za-z0-9]+', '-', label)[:60] + '.log')).write_text(out)
    finally:
        sh(['git', 'checkout', '--', CPP], 60)

rc, out = sh(['cmake', '--build', str(BUILD), '--target', 'bpftime_runtime_tests', '-j4'], 3600)
log(f'rebuilt: rc={rc}')
rc, out = sh(['git', 'status', '--short'], 60)
log('git status after restore: ' + (out.strip() or 'clean'))
