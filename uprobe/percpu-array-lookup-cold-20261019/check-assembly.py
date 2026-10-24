#!/usr/bin/env python3
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent
BUILDS = Path('/tmp/bpftime-percpu-array-cold-20261019.YHrmOV')
SYMBOLS = {
    'ordinary': 'bpftime_array_map_lookup_elem_helper',
    'percpu': 'bpftime_percpu_array_map_lookup_elem_helper',
}
checks = {}
for version in ('baseline', 'cold-errors'):
    library = BUILDS/version/'runtime/agent/libbpftime-agent.so'
    checks[version] = {}
    for label,symbol in SYMBOLS.items():
        assembly = subprocess.check_output([
            'objdump','-d','-C','--no-show-raw-insn',
            '--disassemble='+symbol,str(library)
        ],text=True)
        (ROOT/f'{version}-{label}-lookup.asm').write_text(assembly)
        instructions = [line for line in assembly.splitlines()
                        if re.match(r'\s*[0-9a-f]+:\s',line)]
        assert instructions, (version,symbol)
        checks[version][label] = {
            'static_instruction_count':len(instructions),
            'stack_references':[line for line in instructions if '[sp' in line],
            'calls':[line for line in instructions if re.search(r':\s+blr?\s',line)],
            'errno_call':'__errno_location' in assembly,
        }
(ROOT/'assembly-check.json').write_text(json.dumps(checks,indent=2)+'\n')
print(json.dumps(checks,indent=2),flush=True)
pc = checks['cold-errors']['percpu']
assert not pc['errno_call'], 'Inline errno remains in specialized per-CPU helper'
assert len(pc['calls'])==1 and 'sched_getcpu' in pc['calls'][0], 'CPU query not exactly once in helper assembly'
ordinary = checks['cold-errors']['ordinary']
assert not ordinary['stack_references'] and not ordinary['calls'], 'Ordinary array frameless path regressed'
