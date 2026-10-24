#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import re
import subprocess

OUT=Path(__file__).resolve().parent
base=OUT.parent
builds={
    'previous':base/'percpu-array-jit-direct-cpu-20261020/artifacts',
    'current':base/'array-jit-default-20261021/artifacts',
}
if (OUT/'artifacts/runtime/agent/libbpftime-agent.so').exists():
    builds['direct-array-erase']=OUT/'artifacts'
symbols=('bpftime_map_delete_elem_helper',
         '_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0',
         '_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm')
report={}
for label,build in builds.items():
    library=build/'runtime/agent/libbpftime-agent.so'
    nm=subprocess.check_output(['nm','-anS',str(library)],text=True)
    entries={}
    for line in nm.splitlines():
        fields=line.split()
        if len(fields)>=4 and fields[3] in symbols:
            entries[fields[3]]=dict(address=int(fields[0],16),bytes=int(fields[1],16))
    for symbol,entry in entries.items():
        asm=subprocess.check_output(['objdump','-d','--no-show-raw-insn',f'--disassemble={symbol}',str(library)],text=True)
        (OUT/f'{label}-{symbol}.asm').write_text(asm)
        instructions=[]
        for line in asm.splitlines():
            match=re.match(r'\s*([0-9a-f]+):\s+(.*)',line)
            if match:
                instructions.append(dict(offset=int(match[1],16)-entry['address'],asm=match[2]))
        entry['instruction_count']=len(instructions)
        entry['generic_delete_branches']=[x for x in instructions if
            re.search(r'<_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb\.constprop\.0>$',x['asm'])
            and re.match(r'b(?:l)?\s',x['asm'])]
    with library.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
    report[label]=dict(library=str(library),sha256=digest,symbols=entries)
(OUT/'native-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
for label,item in report.items():
    print(label,{symbol:dict(bytes=x['bytes'],instructions=x['instruction_count'],generic_dispatch_branches=len(x['generic_delete_branches'])) for symbol,x in item['symbols'].items()})
