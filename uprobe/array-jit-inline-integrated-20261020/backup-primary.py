#!/usr/bin/env python3
from pathlib import Path
import hashlib
import json
import shutil

root=Path(__file__).resolve().parent
primary=Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate')
expected=json.loads((root/'original-source-sha256.json').read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert all(digest(Path(p))==h for p,h in expected.items()),'Original worktree changed; do not overwrite'
backup=root/'primary-before-integration'
backup.mkdir(exist_ok=False)
manifest={}
for name in json.loads((root/'source-sha256.json').read_text()):
    source=primary/name
    if source.exists():
        target=backup/name;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target);manifest[name]=digest(source)
    else:
        manifest[name]=None
(root/'primary-before-integration.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Preserved the original primary files and absence of new files before integration.')
