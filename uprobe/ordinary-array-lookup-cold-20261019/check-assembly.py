#!/usr/bin/env python3
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent
BUILDS = Path('/tmp/bpftime-array-lookup-cold-20261019.8N6hqp')
checks = {}
for version in ('baseline', 'cold-errors'):
    library = BUILDS / version / 'runtime/agent/libbpftime-agent.so'
    asm = subprocess.check_output([
        'objdump', '-d', '-C', '--no-show-raw-insn',
        '--disassemble=bpftime_array_map_lookup_elem_helper', str(library)
    ], text=True)
    (ROOT / f'{version}-lookup.asm').write_text(asm)
    instructions = [line for line in asm.splitlines()
                    if re.match(r'\s*[0-9a-f]+:\s', line)]
    assert instructions, f'Missing production helper symbol in {version}'
    checks[version] = {
        'instruction_count': len(instructions),
        'stack_references': [line for line in instructions if '[sp' in line],
        'calls': [line for line in instructions if re.search(r':\s+blr?\s', line)],
        'errno_call': '__errno_location' in asm,
    }
(ROOT / 'assembly-check.json').write_text(json.dumps(checks, indent=2)+'\n')
print(json.dumps(checks, indent=2), flush=True)
candidate = checks['cold-errors']
assert not candidate['stack_references'], 'Normal helper still uses a stack frame'
assert not candidate['calls'], 'Normal helper still contains calls'
assert not candidate['errno_call'], 'Normal helper still handles errno inline'
