#!/usr/bin/env python3
"""Freeze exact measured sources, including all parent-owned overlays."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

out=Path(__file__).resolve().parent
source=Path('/home/jetson/src/bpftime-percpu-array-jit-20261020')
primary=Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate')
previous=out.parent/'percpu-array-jit-direct-cpu-20261020/source-sha256.json'
names=set(json.loads(previous.read_text())) | {
    'vm/compat/llvm-vm/array-map-descriptor.patch',
    'vm/compat/llvm-vm/compat_llvm.hpp',
    'vm/compat/include/bpftime_vm_compat.hpp',
    'vm/vm-core/include/ebpf-vm.h',
    'vm/vm-core/src/ebpf-vm.cpp',
    'runtime/include/bpf_attach_ctx.hpp',
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
status=subprocess.run(['git','status','--short'],cwd=primary/'vm/llvm-jit',
    check=True,capture_output=True,text=True)
assert not status.stdout,'Upstream submodule must remain clean'
(out/'submodule-sha.txt').write_text(subprocess.check_output(
    ['git','rev-parse','HEAD'],cwd=primary/'vm/llvm-jit',text=True))
print(f'{len(hashes)} source files archived; primary and measured sources match.')
