#!/usr/bin/env python3
"""Archive the exact tested source, checking the primary worktree matches it."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

out=Path(__file__).resolve().parent
source=Path('/home/jetson/src/bpftime-percpu-array-jit-20261020')
primary=Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate')
previous=out.parent/'percpu-array-jit-inline-20261020/source-sha256.json'
names=set(json.loads(previous.read_text())) | {
    'vm/compat/include/bpftime_array_cpu_query.hpp',
    'runtime/src/bpf_helper.cpp',
    'runtime/include/bpftime_helper_group.hpp',
    'runtime/src/bpf_map/userspace/per_cpu_array_map.hpp',
    'benchmark/uprobe/diagnostics/percpu_array_jit.bpf.c',
    'benchmark/uprobe/diagnostics/percpu_array_jit_loader.c',
}
hashes={}
for name in sorted(names):
    data=(source/name).read_bytes()
    assert data==(primary/name).read_bytes(),name
    dest=out/'source'/name
    dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source/name,dest)
    hashes[name]=hashlib.sha256(data).hexdigest()
(out/'source-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
patch=subprocess.run(['git','diff','HEAD','--',*sorted(names)],cwd=primary,
    check=True,capture_output=True,text=True).stdout
(out/'changes-vs-9ffbffb.diff').write_text(patch)
print(f'{len(hashes)} source files archived; primary and tested snapshot match.')
