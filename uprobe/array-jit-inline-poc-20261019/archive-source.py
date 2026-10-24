#!/usr/bin/env python3
from pathlib import Path
import difflib
import hashlib
import json
import shutil

root=Path(__file__).resolve().parent
original=Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate')
poc=Path('/home/jetson/src/bpftime-array-jit-poc-20261019')
files=['vm/compat/include/bpftime_vm_compat.hpp','vm/compat/llvm-vm/compat_llvm.hpp',
    'vm/vm-core/include/ebpf-vm.h','vm/vm-core/src/ebpf-vm.cpp',
    'vm/llvm-jit/src/llvm_jit_context.cpp','runtime/include/bpftime_prog.hpp',
    'runtime/src/bpftime_prog.cpp','runtime/src/attach/bpf_attach_ctx.cpp',
    'runtime/src/bpf_map/userspace/array_map.hpp','runtime/unit-test/CMakeLists.txt',
    'runtime/unit-test/maps/test_array_jit_poc.cpp',
    'benchmark/uprobe/diagnostics/array_jit_poc.bpf.c',
    'benchmark/uprobe/diagnostics/array_jit_poc_loader.c',
    'benchmark/uprobe/diagnostics/array_jit_poc_victim.c']
patch=[]; hashes={}; original_hashes={}
for name in files:
    new=poc/name; old=original/name
    target=root/'source'/name;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(new,target)
    hashes[name]=hashlib.sha256(new.read_bytes()).hexdigest()
    before=old.read_text().splitlines(keepends=True) if old.exists() else []
    after=new.read_text().splitlines(keepends=True)
    patch.extend(difflib.unified_diff(before,after,fromfile='a/'+name if old.exists() else '/dev/null',tofile='b/'+name))
    if old.exists():original_hashes[str(old)]=hashlib.sha256(old.read_bytes()).hexdigest()
(root/'poc-vs-cpp-candidate.diff').write_text(''.join(patch))
(root/'source-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
(root/'original-source-sha256.json').write_text(json.dumps(original_hashes,indent=2)+'\n')
print('Archived',len(files),'POC source files; original worktree is only read.')
