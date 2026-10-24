#!/usr/bin/env python3
"""Resume post-validation after the original script's INFO-only log gate."""
from pathlib import Path
import datetime
import hashlib
import json
import math
import shutil
import subprocess
import time

root=Path(__file__).resolve().parent
full=root/'full10'
deadline=time.monotonic()+3600
while True:
    status=root/'accepted-exit-status.txt';finished=root/'accepted-finished-at.txt';started=root/'accepted-started-at.txt'
    if status.exists() and finished.exists() and finished.stat().st_mtime>=started.stat().st_mtime:
        break
    assert time.monotonic()<deadline,'Timed out waiting for full benchmark'
    time.sleep(5)
try:
    assert 'Benchmark completed in ' in (full/'runner.log').read_text(),'Benchmark itself did not finish'
    result=json.loads((full/'benchmark-output.json').read_text())
    assert result['benchmark_info']['num_runs']==10
    for group,count in (('kernel_uprobe',17),('userspace_uprobe',17),('embed',1)):
        assert len(result[group])==count
        for name,item in result[group].items():
            assert len(item['raw_values'])==10,(group,name)
            assert all(math.isfinite(v) and v>0 for v in item['raw_values']),(group,name)
    # Keep the original exit/gate evidence rather than erasing the failed gate.
    for src,name in ((full/'exit-status.txt','exit-status-before-activation-audit.txt'),
                     (root/'accepted-exit-status.txt','accepted-exit-status-before-activation-audit.txt')):
        shutil.copy2(src,full/name)
    shutil.copy2(root/'run-full.sh',full/'run-full-before-activation-audit.sh')
    subprocess.run(['sha256sum','--check',str(full/'runtime-sha256.txt')],check=True)
    subprocess.run(['python3',str(root/'audit-full-activation.py')],check=True)
    with (full/'jetson-clocks-after.txt').open('w') as log:
        subprocess.run(['sudo','-n','jetson_clocks','--show'],stdout=log,stderr=subprocess.STDOUT,check=True)
    with (full/'ten-operation-comparison.md').open('w') as report:
        subprocess.run(['python3',str(root/'summarize-full.py')],stdout=report,check=True)
    source=Path('/home/jetson/src/bpftime-array-jit-integrated-20261020')
    primary=Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate')
    for name,expected in json.loads((root/'source-sha256.json').read_text()).items():
        assert hashlib.sha256((source/name).read_bytes()).hexdigest()==expected,name
        assert hashlib.sha256((primary/name).read_bytes()).hexdigest()==expected,name
    subprocess.run(['sha256sum','--check',str(full/'runtime-sha256.txt')],check=True)
    now=datetime.datetime.now().astimezone().isoformat()+'\n'
    (full/'final-validation.json').write_text(json.dumps(dict(status='passed',
        kernel_cases=17,userspace_cases=17,embed_cases=1,samples_per_case=10,
        activation_audit='separate exact-artifact INFO audit; not included in timings',
        original_gate_exit=(full/'exit-status-before-activation-audit.txt').read_text().strip(),
        benchmark_duration_seconds=result['benchmark_info']['total_duration_seconds']),indent=2)+'\n')
    (full/'exit-status.txt').write_text('0\n')
    (full/'finished-at.txt').write_text(now)
    (root/'accepted-exit-status.txt').write_text('0\n')
    (root/'accepted-finished-at.txt').write_text(now)
    (full/'post-validation-exit-status.txt').write_text('0\n')
    print('Full benchmark data, activation audit, source identity and immutable artifacts all passed.')
except BaseException:
    (full/'post-validation-exit-status.txt').write_text('1\n')
    raise
