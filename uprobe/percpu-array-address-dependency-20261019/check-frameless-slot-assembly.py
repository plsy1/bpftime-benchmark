#!/usr/bin/env python3
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parent
BUILDS=Path('/tmp/bpftime-percpu-array-address-20261019.S1mHZP')
checks={}
for version in ('baseline','frameless-slot'):
    checks[version]={}
    lib=BUILDS/version/'runtime/agent/libbpftime-agent.so'
    for label,symbol in [('ordinary','bpftime_array_map_lookup_elem_helper'),
                         ('percpu','bpftime_percpu_array_map_lookup_elem_helper')]:
        asm=subprocess.check_output(['objdump','-d','-C','--no-show-raw-insn',
                                     '--disassemble='+symbol,str(lib)],text=True)
        (ROOT/f'{version}-{label}-lookup.asm').write_text(asm)
        lines=[line for line in asm.splitlines() if re.match(r'\s*[0-9a-f]+:\s',line)]
        assert lines,(version,symbol)
        checks[version][label]={
            'static_instruction_count':len(lines),
            'stack_references':[line for line in lines if '[sp' in line],
            'calls':[line for line in lines if re.search(r':\s+blr?\s',line)],
            'slot_tail_branch':any(re.search(r':\s+b\s+.*<.*lookup_cpu_slot',line) for line in lines),
        }
lib=BUILDS/'frameless-slot/runtime/agent/libbpftime-agent.so'
symbols=subprocess.check_output(['nm','-S','-C',str(lib)],text=True)
match=next(re.match(r'([0-9a-f]+)\s+([0-9a-f]+)\s+\w\s+(.*)',line)
           for line in symbols.splitlines() if 'bpftime::per_cpu_array_map_impl::lookup_cpu_slot(' in line)
assert match,'Missing non-inlined CPU-slot helper'
start,size=(int(match.group(i),16) for i in (1,2))
asm=subprocess.check_output(['objdump','-d','-C','--no-show-raw-insn',
                             f'--start-address={start}',f'--stop-address={start+size}',str(lib)],text=True)
(ROOT/'frameless-slot-cpu-slot.asm').write_text(asm)
lines=[line for line in asm.splitlines() if re.match(r'\s*[0-9a-f]+:\s',line)]
calls=[line for line in lines if re.search(r':\s+blr?\s',line)]
saved=set()
for line in lines:
    if '[sp' in line:
        saved.update(int(n) for n in re.findall(r'\bx(\d+)\b',line) if 19<=int(n)<=28)
after=lines[next(i for i,line in enumerate(lines) if 'sched_getcpu' in line)+1:]
leaf={'static_instruction_count':len(lines),'calls':calls,
      'callee_saved_registers':sorted(saved),'after_cpu_query':after}
checks['frameless-slot']['cpu-slot']=leaf
(ROOT/'frameless-slot-assembly-check.json').write_text(json.dumps(checks,indent=2)+'\n')
print(json.dumps(checks,indent=2),flush=True)
assert len(calls)==1 and 'sched_getcpu' in calls[0],'CPU-slot leaf does not query CPU once'
assert len(saved)<=2,'CPU-slot leaf still spills extra registers'
assert not any(re.search(r':\s+ldr\s',line) for line in after),'Metadata loads remain after CPU query'
assert checks['frameless-slot']['percpu']['slot_tail_branch'],'Extra normal call/return rather than tail branch'
outer=checks['frameless-slot']['percpu']
assert not outer['stack_references'] and not outer['calls'],'Outer adapter still uses a stack frame/calls'
ordinary=checks['frameless-slot']['ordinary']
assert not ordinary['stack_references'] and not ordinary['calls'],'Ordinary array frameless path regressed'
