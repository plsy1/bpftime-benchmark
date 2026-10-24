#!/usr/bin/env python3
"""Attribute top-frame PC samples, never call-chain-inclusive counts."""
from collections import Counter
from pathlib import Path
import csv
import json
import re
import sqlite3
import subprocess

root = Path(__file__).resolve().parent
agent = root/'artifacts/libbpftime-agent.so'
libc = Path('/usr/lib/aarch64-linux-gnu/libc.so.6')
helper_info = {'array_lookup': ('bpftime_array_map_lookup_elem_helper', 0x4e5844, 0xbc), 'percpu_array_lookup': ('bpftime_percpu_array_map_lookup_elem_helper', 0x4e5730, 0x114)}
assembly = {}
for case, (symbol, start, size) in helper_info.items():
    text = subprocess.check_output(['objdump', '-d', '-C', '--no-show-raw-insn', '--disassemble='+symbol, str(agent)], text=True)
    assert f'{start:016x} <{symbol}>' in text
    assembly[case] = {int(m[1], 16): m[2] for line in text.splitlines() if (m := re.match(r'\s*([0-9a-f]+):\s+(.*)', line))}
    assert len(assembly[case])*4 == size
    (root/f'{case}-assembly.txt').write_text(text)

def maps_for(path):
    rows=[]
    for line in path.read_text().splitlines():
        fields=line.split(maxsplit=5)
        if len(fields)==6:
            lo,hi=(int(x,16) for x in fields[0].split('-'))
            rows.append((lo,hi,int(fields[2],16),fields[1],fields[5]))
    return rows

def group(case, offset):
    # Descriptive instruction regions. Out-of-order execution and sampling skid
    # mean these are NOT exact cycle costs or independent optimization ceilings.
    if case=='array_lookup':
        if offset < 0x68: return 'manager_fd_type_resolution'
        if 0x6c <= offset < 0x84: return 'map_impl_offset_pointer'
        if offset == 0x68 or 0x84 <= offset < 0x90: return 'key_bounds'
        if 0x90 <= offset < 0xb4: return 'data_pointer_and_address'
        if offset == 0xb4: return 'entry_exit_registers'
        return 'error_or_fallback'
    if offset in (0,8,0x2c,0x30,0x8c,0x90,0xf0,0xf8): return 'entry_exit_registers'
    if offset in (4,0xc) or 0x10 <= offset < 0x1c or 0x34 <= offset < 0x8c: return 'manager_fd_type_resolution'
    if 0x94 <= offset < 0xac: return 'map_impl_offset_pointer'
    if offset == 0xac: return 'cpu_query_callsite'
    if 0xb0 <= offset < 0xc4: return 'key_bounds'
    if 0xc4 <= offset < 0xf0 or offset == 0xf4: return 'data_pointer_and_address'
    return 'error_or_fallback'

summary=[]
detail=[]
lines=['# Accepted-baseline production sampling', '', 'Top-frame event counts, filtered to the victim PID. Each capture is a five-second delayed window. Counts are descriptive; they are not exact time per instruction, and profiled wall times are not benchmark results.', '']
for directory in sorted((root/'profiles').iterdir()):
    case=directory.name.rsplit('-',1)[0]
    symbol,start,size=helper_info[case]
    pid=int((directory/'victim-pid.txt').read_text())
    maps=maps_for(directory/'proc-maps.txt')
    conn=sqlite3.connect(directory/'capture.sqlite')
    rows=conn.execute('''select e.start,e.cpu,c.originalIP,s.value,m.value
        from SAMPLING_CALLCHAINS c join COMPOSITE_EVENTS e on e.id=c.id
        left join StringIds s on s.id=c.symbol left join StringIds m on m.id=c.module
        where c.stackDepth=0 and (e.globalTid&16777215)=?''',(pid,)).fetchall()
    assert len(rows)>3000, (directory,len(rows))
    counts=Counter()
    pcs=Counter()
    cpu_pcs=Counter()
    symbols=Counter()
    cpus=Counter()
    for timestamp,cpu,ip,name,module in rows:
        cpus[cpu]+=1
        symbols[name]+=1
        mapping=next((m for m in maps if m[0]<=ip<m[1] and 'x' in m[3]),None)
        elf=None if mapping is None else ip-mapping[0]+mapping[2]
        if mapping and mapping[4].endswith('libbpftime-agent.so') and start<=elf<start+size:
            category=group(case,elf-start)
            pcs[elf]+=1
        elif mapping and mapping[4].endswith('libbpftime-agent.so') and 0x48a680<=elf<0x48a690:
            category='cpu_query_plt'
        elif mapping and mapping[4].endswith('libc.so.6') and 0xdc0e0<=elf<0xdc178:
            category='cpu_query_libc'
            cpu_pcs[hex(elf)]+=1
        elif module=='//anon': category='anonymous_JIT_or_trampoline'
        else: category='other_runtime_or_system'
        counts[category]+=1
    assert set(cpus)=={5}, cpus
    total=len(rows)
    item=dict(capture=directory.name, pid=pid, total_samples=total, period=int((directory/'cycle-period.txt').read_text()), time_span_ns=[min(x[0] for x in rows),max(x[0] for x in rows)], cpu_counts=dict(cpus), regions=dict(counts), region_percent={k:100*v/total for k,v in counts.items()}, top_symbols=symbols.most_common(12))
    summary.append(item)
    item['sched_getcpu_sampled_pcs'] = dict(cpu_pcs)
    lines += [f'## {directory.name}', '', f'{total} top-frame samples; CPU 5; sampling period {item["period"]} cycles.', '', '| Instruction region | Samples | Share of all target samples |','| --- | ---: | ---: |']
    for category,n in counts.most_common():
        lines.append(f'| {category} | {n} | {100*n/total:.2f}% |')
    lines += ['', 'Hottest helper PCs (ELF addresses; ARM sampling may skid):', '', '| PC | Helper offset | Samples | Instruction |','| --- | --- | ---: | --- |']
    for pc,n in pcs.most_common(12):
        lines.append(f'| `0x{pc:x}` | `+0x{pc-start:x}` | {n} | `{assembly[case][pc]}` |')
    lines.append('')
    for pc,n in sorted(pcs.items()):
        detail.append(dict(capture=directory.name,elf_pc=hex(pc),helper_offset=hex(pc-start),category=group(case,pc-start),samples=n,percent=100*n/total,instruction=assembly[case][pc]))
(root/'profiles-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
(root/'profiles-analysis.md').write_text('\n'.join(lines)+'\n')
with (root/'profile-instructions.csv').open('w') as f:
    writer=csv.DictWriter(f,fieldnames=list(detail[0]))
    writer.writeheader()
    writer.writerows(detail)
for x in summary:
    print(x['capture'],x['total_samples'],{k:round(v,2) for k,v in x['region_percent'].items()})
