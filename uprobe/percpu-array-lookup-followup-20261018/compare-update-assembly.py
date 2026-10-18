#!/usr/bin/env python3
"""Compare frozen update paths while ignoring relocation-only differences."""
import difflib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent
SNAPSHOTS = Path('/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb')
OUTPUT = ROOT / 'update-assembly'
OUTPUT.mkdir(exist_ok=True)
SYMBOLS = {
    'update-leaf': 'bpftime::per_cpu_array_map_impl::elem_update_base(void const*, void const*, unsigned long)',
    'update-helper': 'bpftime_percpu_array_map_update_elem_helper',
}
summary = {}
for label, name in SYMBOLS.items():
    versions = {}
    for version in ('baseline', 'inline-leaf'):
        library = SNAPSHOTS / version / 'runtime/agent/libbpftime-agent.so'
        symbols = subprocess.check_output(['nm', '-S', '-C', str(library)], text=True)
        match = next(re.match(r'([0-9a-f]+)\s+([0-9a-f]+)\s+\w\s+(.*)', line)
                     for line in symbols.splitlines() if line.endswith(' ' + name))
        start, size = (int(match.group(i), 16) for i in (1, 2))
        assembly = subprocess.check_output([
            'objdump', '-d', '-C', '--no-show-raw-insn',
            f'--start-address={start}', f'--stop-address={start+size}', str(library)
        ], text=True)
        (OUTPUT / f'{label}-{version}.asm').write_text(assembly)
        normalized = []
        for line in assembly.splitlines():
            instruction = re.match(r'\s*([0-9a-f]+):\s+(.*)', line)
            if not instruction:
                continue
            address = int(instruction.group(1), 16)
            body = instruction.group(2).split('//')[0].strip()
            # objdump prints direct branch destinations with a symbolic annotation.
            # Internal targets become offsets within this function. External targets
            # retain their symbol names, not their address in either ELF image.
            target = re.search(r'\b([0-9a-f]+) <(.+)>', body)
            if target:
                dest = int(target.group(1), 16)
                destination = (f'self+0x{dest-start:x}' if start <= dest < start+size
                               else '<' + target.group(2) + '>')
                body = body[:target.start()] + destination + body[target.end():]
            normalized.append(f'{address-start:03x}: {body}')
        (OUTPUT / f'{label}-{version}.normalized.txt').write_text('\n'.join(normalized)+'\n')
        versions[version] = {'address': hex(start), 'size': size,
                             'address_mod_64': start % 64,
                             'instructions': normalized}
    baseline = versions['baseline']['instructions']
    candidate = versions['inline-leaf']['instructions']
    same = baseline == candidate
    diff = ''.join(difflib.unified_diff([x+'\n' for x in baseline],
                                      [x+'\n' for x in candidate],
                                      fromfile='baseline', tofile='inline-leaf'))
    (OUTPUT / f'{label}.diff').write_text(diff)
    summary[label] = {'normalized_instructions_identical': same,
                      'versions': {key: {k:v for k,v in value.items() if k != 'instructions'}
                                   for key,value in versions.items()}}
(OUTPUT / 'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print(json.dumps(summary, indent=2))
