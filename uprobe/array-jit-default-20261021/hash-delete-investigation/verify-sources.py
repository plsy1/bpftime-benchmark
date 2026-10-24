#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path

out=Path(__file__).resolve().parent
primary=Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate')
warm=Path('/home/jetson/src/bpftime-percpu-array-jit-20261020')
manifest=json.loads((out.parent/'source-sha256.json').read_text())
changed=[]
for name,digest in manifest.items():
    assert hashlib.sha256((primary/name).read_bytes()).hexdigest()==digest,('primary changed',name)
    if hashlib.sha256((warm/name).read_bytes()).hexdigest()!=digest:changed.append(name)
assert changed==['runtime/src/bpf_map/array_map_binding.hpp'],changed
assert (warm/changed[0]).read_bytes()==(out/'array_map_binding.diagnostic.hpp').read_bytes()
env=json.loads((out/'environment.json').read_text())
assert env['status']=='success'
for name,digest in env['binary_sha256'].items():
    with Path(name).open('rb') as stream:actual=hashlib.file_digest(stream,'sha256').hexdigest()
    assert actual==digest,name
print('Primary unchanged; warm differs only at archived array erase body; all measured artifacts immutable.')
