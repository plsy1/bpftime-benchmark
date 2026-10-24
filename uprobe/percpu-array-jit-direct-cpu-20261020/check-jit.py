#!/usr/bin/env python3
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
                instructions.append(re.sub(r'\s+',' ',m[1].split('//')[0].strip()))
                if instructions[-1]=='ret':break
        expected=[f'mov x{i}, xzr' for i in range(5)]
        sequences=sum(instructions[i:i+5]==expected for i in range(len(instructions)-4))
        error_checks=sum(bool(re.match(r'cmn w\d+, #0x1$',x)) for x in instructions)
        row[engine]=dict(instructions_before_first_ret=len(instructions),five_zero_argument_sequences=sequences,raw_i32_failure_checks=error_checks)
    assert row['previous']['five_zero_argument_sequences']>=1,row
    assert row['inline']['five_zero_argument_sequences']==0,row
    assert row['inline']['raw_i32_failure_checks']>=1,row
    report.append(row)
(root/'jit-check.json').write_text(json.dumps(dict(status='passed',cases=report,note='Real executable windows. First-ret counts are not whole-function lengths or latency models.'),indent=2)+'\n')
print('Actual JIT uses a zero-argument raw i32 CPU call with explicit -1 handling.')
