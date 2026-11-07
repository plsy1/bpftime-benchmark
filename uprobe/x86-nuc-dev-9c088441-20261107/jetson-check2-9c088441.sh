#!/bin/bash
# x86 check of dev 9c088441, second part. The whole runtime suite in ONE process aborts on
# x86 with "double free or corruption" right after the LPM trie tests on both 9c088441 and
# d46ceb8f (first part), so use the NUC session's method instead (x86-nuc-dev-d5279469-20261103/
# tests): the daily filter in one process, then every runtime and Frida case in its own process
# (60 s each, run-each-case.py copied from there). First on d46ceb8f (the build directory holds
# it now), then on 9c088441; a failure is new only if d46ceb8f passes it.
set -u
D=~/src/jetson-check-9c088441
exec > >(tee -a ~/src/jetson-check-9c088441.log) 2>&1
echo "=== part 2"; date
cd $D || exit 1
mkdir -p $D/tests
sed -e "s|label, root = 'dev-d5279469', '/home/y/src/bpftime-wt-uprobe-177a1dc'|label, root = sys.argv[4], '$D'|" \
    ~/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-d5279469-20261103/tests/run-each-case.py > $D/tests/run-each-case.py
grep -q "sys.argv\[4\]" $D/tests/run-each-case.py || { echo "runner patch failed"; exit 1; }
FILTER='[array-lookup-cold],[percpu-array],[percpu-hash],[hash-fast4],[maps][hash],[bpftime_hash_map],test_arraymap_percpu*,Test basic operations of array map,Test basic operations of hash map,Test shm hash maps with sub process,Hash maps with colliding public names remain independent,[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops,[perf_event],[epoll],[uid_gid],[shm_release],[jit_load],[helpers],[stack_trace],Test stack trace map*,[cgroup_array],[trace_printk],[ratelimit],get_stackid returns errors as negative 64-bit values,[cpu-bounds],[percpu-layout],[map-create],[ringbuf],[pid_tgid],[fork-handlers]'
R=$D/build/runtime/unit-test/bpftime_runtime_tests
F=$D/build/attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests

run_all() { # $1 = label
    local L=$1
    git log --oneline -1
    timeout 1800 $R "$FILTER" > $D/filter.$L.log 2>&1
    echo "daily filter $L: rc $? $(grep -aE '^All tests|^test cases' $D/filter.$L.log | tail -1)"
    timeout 3600 python3 $D/tests/run-each-case.py $R runtime-each-$L 60 $L
    timeout 1800 python3 $D/tests/run-each-case.py $F frida-each-$L 60 $L
    date
}

git checkout -q --detach d46ceb8f4168d067d7c6aa579333a8fb29fc82d4 || exit 1
run_all base
git checkout -q --detach 9c0884414084af213448ab116f920917b9b9c8cb || exit 1
cmake --build $D/build --parallel 8 --target bpftime_runtime_tests bpftime_frida_uprobe_attach_tests > $D/build.new2.log 2>&1 || { grep -E "error" $D/build.new2.log | head -20; exit 1; }
run_all new
python3 - $D <<'PY'
import json, sys
from pathlib import Path
d = Path(sys.argv[1]) / 'tests'
for suite in ('runtime', 'frida'):
    res = {}
    for label in ('base', 'new'):
        p = d / f'{suite}-each-{label}' / 'results.json'
        res[label] = {r['case']: r for r in json.loads(p.read_text())} if p.exists() else {}
    fb = {c for c, r in res['base'].items() if not r['passed']}
    fn = {c for c, r in res['new'].items() if not r['passed']}
    print(f"{suite}: base {len(res['base'])} cases, {len(fb)} failed; new {len(res['new'])} cases, {len(fn)} failed")
    for c in sorted(fn - fb):
        print(f"  NEW FAILURE rc={res['new'][c]['rc']} {c}" + ("" if c in res['base'] else " (case only in new)"))
    for c in sorted(fb - fn):
        print(f"  fixed or flaky: {c}")
    slow = sorted(res['new'].values(), key=lambda r: -r['seconds'])[:3]
    print("  slowest new:", ", ".join(f"{r['case']} {r['seconds']} s" for r in slow))
PY
echo "/dev/shm after part 2:"; ls -l /dev/shm | tail -n +2
pgrep -u y -af 'bpftime|jetson-check' | grep -v pgrep
date
