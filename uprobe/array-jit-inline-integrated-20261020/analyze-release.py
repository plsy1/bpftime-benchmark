#!/usr/bin/env python3
from pathlib import Path
import json
import re
import subprocess

root=Path(__file__).resolve().parent
data=json.loads((root/'ab6-release/summary.json').read_text())
env=json.loads((root/'ab6-release/environment.json').read_text())
assert env['status']=='success'
assert len(env['frequency_samples'])==60
assert all(v==dict(min=1728000,max=1728000,cur=1728000) for v in env['frequency_samples'])
lines=['# Ordinary-array JIT integration result', '',
    'New workload: runtime-dependent keys, 1,000 elements per invocation, checksum/hit/miss counts stored in a result map. Raw elapsed time includes the probe, key generation, lookup/read and checksum work. It is not a pure isolated helper cost and is NOT comparable to the old unused-result ten-operation table.', '',
    'All five engines use the same BPF object, loader and victim. `frozen` is the pending C++ offset-pointer candidate; `poc` is the unchanged prior restricted-lifetime prototype; `off` and `inline` use the same integrated library with specialization disabled/enabled. No changes to the formal ten-item results.', '',
    '| Case | Kernel ns/element | Frozen C++ | Prior POC | Integrated off | Integrated inline | Inline vs off | Inline/kernel | Paired wins / 6 |',
    '| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |']
for row in data:
    wins=sum(c<b for b,c in zip(row['off']['raw_values'],row['inline']['raw_values']))
    label=row['case'] + (' (unused-result simplification; not a gain)' if row['not_a_gain_metric'] else '')
    lines.append(f'| {label} | {row["kernel"]["median"]:.4f} | {row["frozen"]["median"]:.4f} | {row["poc"]["median"]:.4f} | {row["off"]["median"]:.4f} | {row["inline"]["median"]:.4f} | {row["inline_vs_off_percent"]:+.2f}% | {row["inline_vs_kernel"]:.3f}x | {wins} |')
checks=list((root/'ab6-release').glob('*/checksum-check.json'))
assert len(checks)==30
assert all(json.loads(p.read_text())['status']=='passed' for p in checks)
lines += ['', '30 process-run checksum/count validations and 60 frequency checks passed; artifact hashes remained unchanged. The unused case is specifically excluded from optimization-gain claims.', '']

asm_summary=[]
for engine in ('off','inline'):
    directory=root/'smoke-release'/f'{engine}-01'
    log=(directory/'victim.log').read_text()
    for m in re.finditer(r'\[array-jit-poc\] dump=(\S+) address=([0-9a-f]+) readable_window_bytes=(\d+)',log):
        path=Path(m[1]);address=int(m[2],16);length=int(m[3])
        assert path.stat().st_size==length
        asm=subprocess.check_output(['objdump','-D','-b','binary','-m','aarch64',f'--adjust-vma={address}',str(path)],text=True)
        (directory/'jit'/f'{path.stem}.asm').write_text(asm)
        instructions=[]
        for line in asm.splitlines():
            item=re.match(r'\s*[0-9a-f]+:\s+[0-9a-f]+\s+(.*)',line)
            if item:
                instructions.append(item[1])
                if item[1].split()[0]=='ret':break
        asm_summary.append(dict(engine=engine,program=path.stem,address=hex(address),readable_window_bytes=length,
            instructions_to_first_ret=instructions,call_instructions_to_first_ret=sum(x.split()[0] in ('bl','blr') for x in instructions)))
(root/'jit-window-analysis.json').write_text(json.dumps(asm_summary,indent=2)+'\n')
lines += ['## Actual JIT code evidence', '',
    'Code dumps start at the actual JIT entry and are bounded by its readable executable mapping. They are windows, not exact symbol sizes: padding or other functions may follow. Counts below stop at the first return and are descriptive, not full CFG cost models.', '',
    '| Engine | Program | Instructions through first return | Call sites in that prefix |',
    '| --- | --- | ---: | ---: |']
for row in asm_summary:
    lines.append(f'| {row["engine"]} | {row["program"]} | {len(row["instructions_to_first_ret"])} | {row["call_instructions_to_first_ret"]} |')
lines += ['', 'Scope: LLVM JIT, ordinary-array lookup-only programs, opt-in integration. Program-owned references retain original backing and mapping across FD close/reuse/reset/unlink/recreate. Misses resolve the same pinned identity. Signal callbacks and AOT replacement are explicitly rejected for bound programs. See lifecycle tests and README for remaining restrictions.', '']
(root/'RESULT.md').write_text('\n'.join(lines))
print('\n'.join(lines))
