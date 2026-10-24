#!/usr/bin/env python3
"""Check actual JIT code windows; counts are not latency or full CFG models."""
import json
from pathlib import Path
import re
import subprocess

root=Path(__file__).resolve().parent
report=[]
for case in ('hit','mixed'):
    row=dict(case=case)
    for engine in ('previous','inline'):
        path=root/'smoke'/f'{engine}-01/jit'/f'{case}.bin'
        dump=subprocess.run(['objdump','-D','-b','binary','-m','aarch64','--no-show-raw-insn',str(path)],check=True,capture_output=True,text=True).stdout
        (root/f'jit-{engine}-{case}.txt').write_text(dump)
        instructions=[]
        for line in dump.splitlines():
            m=re.match(r'\s*[0-9a-f]+:\s+(.*)',line)
            if m:
                instructions.append(m[1].split('//')[0].strip())
                if instructions[-1]=='ret':break
        expected=[f'mov x{i}, xzr' for i in range(5)]
        normalized=[re.sub(r'\s+',' ',x) for x in instructions]
        sequences=sum(normalized[i:i+5]==expected for i in range(len(normalized)-4))
        row[engine]=dict(instructions_before_first_ret=len(instructions),five_zero_argument_sequences=sequences)
    assert row['previous']['five_zero_argument_sequences']>=1,row
    assert row['inline']['five_zero_argument_sequences']==0,row
    report.append(row)
(root/'jit-abi-check.json').write_text(json.dumps(dict(status='passed',cases=report,note='Real executable windows; first-ret counts are not whole-function sizes or timing attribution.'),indent=2)+'\n')
print('Real JIT check passed: five zero-argument moves eliminated in hit and mixed workloads.')
