#!/usr/bin/env python3
"""Preserve the integration as a parent-only patch against the C++ candidate."""
from pathlib import Path
import difflib
import hashlib
import json
import shutil

root=Path(__file__).resolve().parent
original=Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate')
source=Path('/home/jetson/src/bpftime-array-jit-integrated-20261020')
files=['CLAUDE.md', 'vm/compat/include/bpftime_vm_compat.hpp',
    'vm/compat/llvm-vm/compat_llvm.hpp', 'vm/compat/llvm-vm/CMakeLists.txt',
    'vm/compat/llvm-vm/array-map-inline.patch',
    'vm/vm-core/include/ebpf-vm.h','vm/vm-core/src/ebpf-vm.cpp',
    'runtime/include/bpftime_prog.hpp','runtime/src/bpftime_prog.cpp',
    'runtime/src/attach/bpf_attach_ctx.cpp','runtime/src/bpftime_shm_internal.hpp',
    'runtime/src/bpftime_shm_internal.cpp','runtime/src/handler/handler_manager.cpp','runtime/src/handler/map_handler.hpp',
    'runtime/src/bpf_map/userspace/array_map.hpp','runtime/src/bpf_map/array_map_binding.hpp',
    'runtime/src/bpf_map/array_map_binding.cpp','runtime/CMakeLists.txt',
    'runtime/unit-test/CMakeLists.txt','runtime/unit-test/maps/test_array_jit_poc.cpp',
    'runtime/unit-test/maps/test_array_jit_lifetime.cpp','benchmark/uprobe/benchmark.py',
    'benchmark/uprobe/diagnostics/array_jit_poc.bpf.c',
    'benchmark/uprobe/diagnostics/array_jit_poc_loader.c',
    'benchmark/uprobe/diagnostics/array_jit_poc_victim.c']
baseline_files=['runtime/src/bpf_map/offset_ptr_access.hpp',
    'runtime/unit-test/maps/test_per_cpu_array.cpp']
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
patch=[];hashes={};original_hashes={}
for name in files+baseline_files:
    new=source/name;old=original/name;target=root/'source'/name
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(new,target)
    hashes[name]=digest(new)
    if old.exists():original_hashes[str(old)]=digest(old)
    if name in baseline_files:
        assert new.read_bytes()==old.read_bytes(),name
        continue
    before=old.read_text().splitlines(keepends=True) if old.exists() else []
    patch.extend(difflib.unified_diff(before,new.read_text().splitlines(keepends=True),
        fromfile='a/'+name if old.exists() else '/dev/null',tofile='b/'+name))
upstream='vm/llvm-jit/src/llvm_jit_context.cpp'
assert digest(source/upstream)==digest(original/upstream),'Submodule must stay unmodified'
original_hashes[str(original/upstream)]=digest(original/upstream)
(root/'integration-vs-cpp-candidate.diff').write_text(''.join(patch))
(root/'source-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
(root/'original-source-sha256.json').write_text(json.dumps(original_hashes,indent=2)+'\n')
print('Archived',len(hashes),'source files; upstream submodule is unchanged.',flush=True)
