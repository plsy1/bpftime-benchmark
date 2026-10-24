#!/usr/bin/env python3
from pathlib import Path
import difflib
import json
import re
import subprocess

root = Path(__file__).resolve().parent
paths = {
    'baseline': Path('/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors/runtime/agent/libbpftime-agent.so'),
    'candidate': Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate/build-uprobe-handler-fast-access-20261016/runtime/agent/libbpftime-agent.so'),
}
normalized = {}
for label, path in paths.items():
    output = subprocess.check_output(['objdump', '-d', '-C', '--no-show-raw-insn', '--disassemble=bpftime_array_map_lookup_elem_helper', str(path)], text=True)
    (root/f'{label}-lookup.asm').write_text(output)
    instructions = [m[1] for line in output.splitlines() if (m := re.match(r'\s*[0-9a-f]+:\s+(.*)', line))]
    assert len(instructions)>20
    normalized[label] = [re.sub(r'\b[0-9a-f]{6,16}(?= <)', 'ADDR', x) for x in instructions]
changed = normalized['baseline'] != normalized['candidate']
counts = {label: {op: sum(x.split()[0] == op for x in instructions)
                  for op in ('csetm', 'and', 'bl', 'blr')}
          for label, instructions in normalized.items()}
meaningful = (changed and counts['candidate']['csetm'] < counts['baseline']['csetm']
              and counts['candidate']['and'] < counts['baseline']['and']
              and counts['candidate']['bl'] == 0 and counts['candidate']['blr'] == 0
              and not any(re.search(r'\bsp\b', x) for x in normalized['candidate']))
(root/'assembly-check.json').write_text(json.dumps(dict(changed=changed, meaningful=meaningful, counts=counts, normalized=normalized),indent=2)+'\n')
(root/'assembly.diff').write_text('\n'.join(difflib.unified_diff(normalized['baseline'], normalized['candidate'], fromfile='baseline',tofile='candidate'))+'\n')
print('Production helper changed:',changed,'; mask dependency removed without stack/calls:',meaningful,flush=True)
print('Instruction counts:',counts,flush=True)
raise SystemExit(0 if meaningful else 42)
