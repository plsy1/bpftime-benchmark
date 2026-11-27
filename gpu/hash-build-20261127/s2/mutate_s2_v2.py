#!/usr/bin/env python3
# S2 mutations: each must make a [gpu-devhash-cpu] test fail. Applies one
# mutation at a time to the source, rebuilds bpftime_runtime_tests, runs the
# tag under a timeout, restores the file. Results to mutations.txt.
import pathlib, subprocess, sys, time
REPO = pathlib.Path('/home/jetson/src/bpftime-proto-70f7')
OUT = pathlib.Path(sys.argv[1])
CPP = 'runtime/src/bpf_map/gpu/nv_gpu_device_hash_map.cpp'
MH = 'runtime/src/handler/map_handler.cpp'
HM = 'runtime/src/bpf_map/bpftime_hash_map.hpp'
M = [
 ('S2.1 value 16 accepted', MH,
  "if (max_entries == 0 || key_size != 4 || value_size != 8)\n\t\t\treturn EINVAL;\n\t\tif (max_entries > bpftime_devhash::MAX_ENTRIES_LIMIT)",
  "if (max_entries == 0 || key_size != 4 || value_size % 8 != 0)\n\t\t\treturn EINVAL;\n\t\tif (max_entries > bpftime_devhash::MAX_ENTRIES_LIMIT)"),
 ('S2.2 phase 0 routed to the mirror', CPP,
  "void *nv_gpu_device_hash_map_impl::elem_lookup(const void *key)\n{\n\tif (bpftime_devhash_reader_phase(&phase) != DEVHASH_PHASE_DEVICE)",
  "void *nv_gpu_device_hash_map_impl::elem_lookup(const void *key)\n{\n\tif (bpftime_devhash_reader_phase(&phase) == 99)"),
 ('S2.2 null next_key not checked', CPP,
  "\tif (next_key == nullptr) {\n\t\terrno = EINVAL; // as the CPU table answers\n\t\treturn -1;\n\t}\n\tif (bpftime_devhash_reader_phase(&phase) != DEVHASH_PHASE_DEVICE)\n\t\treturn table.map_get_next_key(key, next_key);",
  "\tif (bpftime_devhash_reader_phase(&phase) != DEVHASH_PHASE_DEVICE)\n\t\treturn table.map_get_next_key(key, next_key);"),
 ('S2.3 writer skips INFLIGHT', CPP,
  "\twriter_slot &w = my_writer_slot();\n\tconst uint64_t ph = bpftime_devhash_writer_enter(&w.count, &phase);\n\tcall_hook(devhash_hook_point::writer_after_phase_load, ph);\n\tif (ph != DEVHASH_PHASE_CPU) {\n\t\t__atomic_fetch_sub(&w.count, 1, __ATOMIC_RELEASE);\n\t\t// the device copy is the map from the seal on\n\t\terrno = EPERM;\n\t\treturn -1;\n\t}\n\tconst long r = table.elem_update(key, value, flags);\n\tconst int err = errno;\n\t__atomic_fetch_sub(&w.count, 1, __ATOMIC_RELEASE);",
  "\tcall_hook(devhash_hook_point::writer_after_inflight_inc, 0);\n\tconst uint64_t ph = __atomic_load_n(&phase, __ATOMIC_SEQ_CST);\n\tcall_hook(devhash_hook_point::writer_after_phase_load, ph);\n\tif (ph != DEVHASH_PHASE_CPU) {\n\t\t// the device copy is the map from the seal on\n\t\terrno = EPERM;\n\t\treturn -1;\n\t}\n\tconst long r = table.elem_update(key, value, flags);\n\tconst int err = errno;"),
 ('S2.4 no discount of dead writers', CPP,
  "void nv_gpu_device_hash_map_impl::discount_dead_writers()\n{\n",
  "void nv_gpu_device_hash_map_impl::discount_dead_writers()\n{\n\treturn;\n"),
 ('S2.4 SEAL_DEAD not stored', CPP,
  "\t\t\t__atomic_store_n(&seal_dead, 1, __ATOMIC_RELEASE);",
  "\t\t\t(void)seal_dead;"),
 ('S2.4 SEAL_DEAD unconditional', CPP,
  "\t\tif (bpftime_devhash_sealer_poll(&writers[0].count, n, stride) !=\n\t\t    0)\n\t\t\treturn false;\n\t\t__atomic_store_n(&seal_dead, 0, __ATOMIC_RELEASE);",
  "\t\treturn false;"),
 ('S2.4 claim kept after a failed seal', CPP,
  "\tif (!ok)\n\t\trelease_claim(self);\n\treturn ok;",
  "\treturn ok;"),
 ('S2.5 image built with next_key', CPP,
  "\ttable.for_each_in_use_unlocked([&](const void *k, const void *v) {",
  "\tauto scan = [&](auto &&fn) { uint32_t c, n; for (int r = table.map_get_next_key(nullptr, &n); r == 0; c = n, r = table.map_get_next_key(&c, &n)) fn(&n, table.elem_lookup(&n)); };\n\tscan([&](const void *k, const void *v) {"),
 ('S2.6 scan stops at max_entries', HM,
  "\t\tfor (size_t i = 0; i < _num_elements; i++) {\n\t\t\tuint8_t *e = elem_at(v, (uint32_t)i);\n\t\t\tif (!(load(word_of(e), std::memory_order_acquire) &",
  "\t\tfor (size_t i = 0; i < _max_elements; i++) {\n\t\t\tuint8_t *e = elem_at(v, (uint32_t)i);\n\t\t\tif (!(load(word_of(e), std::memory_order_acquire) &"),
 ('S2.7 no repair', CPP,
  "\t\tfor (uint64_t n = 0; n < slots && i != s; n++) {\n\t\t\tif (state_of(hdr[i]) == ST_EMPTY)\n\t\t\t\thdr[i] = OPAQUE_WORD;",
  "\t\tfor (uint64_t n = 0; n < slots && i != s; n++) {\n\t\t\tif (false)\n\t\t\t\thdr[i] = OPAQUE_WORD;"),
 ('S2.7 no pass B', CPP,
  "\t\tif (__atomic_load_n(mhdr() + s, __ATOMIC_RELAXED) == 0 &&\n\t\t    state_of(hdr[s]) != ST_EMPTY)",
  "\t\tif (false &&\n\t\t    state_of(hdr[s]) != ST_EMPTY)"),
 ('S2.7 values after headers', CPP,
  "\t// Pass A: values\n\tfor (uint64_t s = 0; s < slots; s++)\n\t\tif (state_of(hdr[s]) == ST_LIVE &&\n\t\t    __atomic_load_n(mval() + s, __ATOMIC_RELAXED) != val[s])\n\t\t\t__atomic_store_n(mval() + s, val[s], __ATOMIC_RELAXED);\n",
  "\tauto pass_a = [&] {\n\tfor (uint64_t s = 0; s < slots; s++)\n\t\tif (state_of(hdr[s]) == ST_LIVE &&\n\t\t    __atomic_load_n(mval() + s, __ATOMIC_RELAXED) != val[s])\n\t\t\t__atomic_store_n(mval() + s, val[s], __ATOMIC_RELAXED);\n\t};\n"),
 ('S2.7 values only when the header changes', CPP,
  "\t\tif (state_of(hdr[s]) == ST_LIVE &&\n\t\t    __atomic_load_n(mval() + s, __ATOMIC_RELAXED) != val[s])\n\t\t\t__atomic_store_n(mval() + s, val[s], __ATOMIC_RELAXED);\n\t// Pass B",
  "\t\tif (state_of(hdr[s]) == ST_LIVE &&\n\t\t    __atomic_load_n(mhdr() + s, __ATOMIC_RELAXED) != hdr[s])\n\t\t\t__atomic_store_n(mval() + s, val[s], __ATOMIC_RELAXED);\n\t// Pass B"),
 ('S2.7 the whole guard off', CPP, "\t\tif (keep) {", "\t\tif (false) {"),
 ('S2.7 key compared before state', CPP,
  "\t\tif (state_of(h) == ST_LIVE && key_of(h) == key)\n\t\t\treturn (int64_t)i;",
  "\t\tif (key_of(h) == key)\n\t\t\treturn state_of(h) == ST_LIVE ? (int64_t)i : -1;"),
 ('S2.8 refused returned as live', CPP,
  "\t\tconst uint64_t h = __atomic_load_n(mhdr() + i, __ATOMIC_ACQUIRE);\n\t\tif (state_of(h) == ST_LIVE) {\n\t\t\t*(uint32_t *)next_key = key_of(h);",
  "\t\tconst uint64_t h = __atomic_load_n(mhdr() + i, __ATOMIC_ACQUIRE);\n\t\tif (is_keyed(h)) {\n\t\t\t*(uint32_t *)next_key = key_of(h);"),
 ('S2.9 count = L', CPP,
  "\timg.count = live > max_entries - e ? live : max_entries - e;",
  "\timg.count = live; (void)e;"),
 ('S2.9 unsigned subtraction without min', CPP,
  "\tconst uint64_t e = empty < max_entries ? empty : max_entries;",
  "\tconst uint64_t e = empty;"),
 ('S2.9 rows not renumbered', CPP,
  "\t\t\timg.headers[s] = make_header(key_of(h), live, ST_LIVE);",
  "\t\t\timg.headers[s] = make_header(key_of(h), row_of(h), ST_LIVE);"),
 ('S2.10 cached copy treated as a takeover', CPP,
  "\t\tif (has_cached_copy)\n\t\t\treturn devhash_path::have_copy;",
  "\t\t(void)has_cached_copy;"),
 ('S2.10 EPERM taken as dead', CPP,
  "\tif (p->kill_errno(pid) == ESRCH)\n\t\treturn false;",
  "\tif (p->kill_errno(pid) != 0)\n\t\treturn false;"),
 ('S2.10 the leader only', CPP,
  "\tsnprintf(dir, sizeof(dir), \"/proc/%u/task\", pid);",
  "\tsnprintf(dir, sizeof(dir), \"/proc/%u/task/%u/../../task_leader_only\", pid, pid);\n\t{ char p2[64], b2[512]; const char *t2; snprintf(p2, sizeof(p2), \"/proc/%u/stat\", pid); if (read_stat_tail(p2, b2, sizeof(b2), &t2)) return (t2[0] == 'Z' || t2[0] == 'X') ? 0 : 1; return -1; }"),
 ('S2.10 start time ignored', CPP,
  "\t\tif (now != 0 && now != start)\n\t\t\treturn false; // the pid belongs to another process now",
  "\t\t(void)now;"),
 ('S2.11 PHASE-1 path skips the poll', CPP,
  "\treset_mirror();\n\t// seal() stores PHASE 1 again and polls: on a timeout PHASE 0 and\n\t// SEAL_DEAD, as for a first seal\n\treturn seal_and_build(out, bound_ns);",
  "\treset_mirror();\n\t(void)bound_ns;\n\treturn build_image(out);"),
 ('S2.13 phase-1 lookups sent to the mirror', CPP,
  "void *nv_gpu_device_hash_map_impl::elem_lookup(const void *key)\n{\n\tif (bpftime_devhash_reader_phase(&phase) != DEVHASH_PHASE_DEVICE)",
  "void *nv_gpu_device_hash_map_impl::elem_lookup(const void *key)\n{\n\tif (bpftime_devhash_reader_phase(&phase) == DEVHASH_PHASE_CPU)"),
 ('dispatch: update case removed', MH,
  "\tcase bpf_map_type::BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP: {\n\t\tauto impl = static_cast<nv_gpu_device_hash_map_impl *>(\n\t\t\tmap_impl_ptr.get());\n\t\treturn do_update(impl);\n\t}\n",
  ""),
 ('dispatch: lookup case removed', MH,
  "\tcase bpf_map_type::BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP: {\n\t\tauto impl = static_cast<nv_gpu_device_hash_map_impl *>(\n\t\t\tmap_impl_ptr.get());\n\t\treturn do_lookup(impl);\n\t}\n",
  ""),
]
# the values-after-headers mutant: pass A runs after pass C (hook stays before it)
VAH_EXTRA = ("\tuint64_t live = 0, refused = 0;\n\tfor (uint64_t s = 0; s < slots; s++) {\n\t\tlive += state_of(hdr[s]) == ST_LIVE;",
             "\tcall_hook(devhash_hook_point::delivery_before_pass_a, 0);\n\tpass_a();\n\tuint64_t live = 0, refused = 0;\n\tfor (uint64_t s = 0; s < slots; s++) {\n\t\tlive += state_of(hdr[s]) == ST_LIVE;")
VAH_HOOK = ("\tcall_hook(devhash_hook_point::delivery_before_pass_a, 0);\n\tauto pass_a", "\tauto pass_a")
log = OUT.open('w')
for name, f, old, new in M:
    p = REPO / f; orig = p.read_text()
    assert orig.count(old) == 1, (name, orig.count(old))
    s = orig.replace(old, new)
    if name == 'S2.7 values after headers':
        assert s.count(VAH_EXTRA[0]) == 1 and s.count(VAH_HOOK[0]) == 1
        s = s.replace(VAH_HOOK[0], VAH_HOOK[1]).replace(VAH_EXTRA[0], VAH_EXTRA[1])
    p.write_text(s)
    t0 = time.time()
    try:
        b = subprocess.run('make -j4 bpftime_runtime_tests 2>&1 | tail -3', shell=True, cwd=REPO/'build-cuda', capture_output=True, text=True, timeout=3000)
        if 'Error' in b.stdout:
            res = 'BUILD FAILED ' + b.stdout.strip().replace('\n', ' | ')
        else:
            r = subprocess.run('timeout -s KILL 400 ./runtime/unit-test/bpftime_runtime_tests "[gpu-devhash-cpu]" 2>&1 | grep -E "^test cases|All tests passed|FAILED|failed" | tail -4', shell=True, cwd=REPO/'build-cuda', capture_output=True, text=True, timeout=300)
            res = r.stdout.strip().replace('\n', ' | ') or ('no output (killed?) rc=%d' % r.returncode)
    finally:
        p.write_text(orig)
    log.write(f'{name}: {res}  [{time.time()-t0:.0f}s]\n'); log.flush()
subprocess.run('make -j4 bpftime_runtime_tests 2>&1 | tail -1', shell=True, cwd=REPO/'build-cuda')
log.write('restored and rebuilt\n')
