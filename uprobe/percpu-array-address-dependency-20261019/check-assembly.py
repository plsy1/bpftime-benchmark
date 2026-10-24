#!/usr/bin/env python3
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parent
BUILDS=Path('/tmp/bpftime-percpu-array-address-20261019.S1mHZP')
checks={}
for version in ('baseline','address-first'):
    checks[version]={}
    lib=BUILDS/version/'runtime/agent/libbpftime-agent.so'
    for label,symbol in [('ordinary','bpftime_array_map_lookup_elem_helper'),
                         ('percpu','bpftime_percpu_array_map_lookup_elem_helper')]:
        asm=subprocess.check_output(['objdump','-d','-C','--no-show-raw-insn',
                                     '--disassemble='+symbol,str(lib)],text=True)
        (ROOT/f'{version}-{label}-lookup.asm').write_text(asm)
        lines=[line for line in asm.splitlines() if re.match(r'\s*[0-9a-f]+:\s',line)]
        assert lines,(version,symbol)
        calls=[line for line in lines if re.search(r':\s+blr?\s',line)]
        checks[version][label]={
            'static_instruction_count':len(lines),
            'stack_references':[line for line in lines if '[sp' in line],
            'calls':calls,
            'cpu_call_sites':[line for line in calls if 'sched_getcpu' in line],
        }
(ROOT/'assembly-check.json').write_text(json.dumps(checks,indent=2)+'\n')
print(json.dumps(checks,indent=2),flush=True)
assert checks['address-first']['percpu']['cpu_call_sites'],'CPU query disappeared'
ordinary=checks['address-first']['ordinary']
assert not ordinary['stack_references'] and not ordinary['calls'],'Ordinary array frameless path regressed'
