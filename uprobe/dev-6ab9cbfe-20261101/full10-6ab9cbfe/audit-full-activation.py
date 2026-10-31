#!/usr/bin/env python3
"""Audit the exact full-suite artifacts with explicit INFO, outside timings."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time

root=Path(__file__).resolve().parent
runner=root/'full-runner/bpftime'
artifacts=root/'artifacts'
out=root/'full10/activation-audit'
out.mkdir(exist_ok=False)
assert not Path('/dev/shm/bpftime_maps_shm').exists(),'Do not overlap another run'
selected=dict(BPFTIME_VM_NAME='llvm',SPDLOG_LEVEL='info',BPFTIME_LOG_OUTPUT='console')
base={k:v for k,v in os.environ.items() if not k.startswith('BPFTIME_') and k!='LD_PRELOAD'}
prefix=['sudo','-n','taskset','-c','5','env']
server_env=dict(selected,LD_PRELOAD=str(artifacts/'runtime/syscall-server/libbpftime-syscall-server.so'))
victim_env=dict(selected,LD_PRELOAD=str(artifacts/'runtime/agent/libbpftime-agent.so'))
assert 'BPFTIME_ARRAY_JIT_INLINE' not in victim_env
def command(env,path,*args):
    return prefix+[f'{k}={v}' for k,v in env.items()]+[str(path),*map(str,args)]
def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
paths=[artifacts/'runtime/agent/libbpftime-agent.so',
       artifacts/'runtime/syscall-server/libbpftime-syscall-server.so',
       runner/'benchmark/test',runner/'benchmark/uprobe/uprobe',
       runner/'benchmark/uprobe/.output/uprobe.bpf.o']
hashes={str(p):digest(p) for p in paths}
meta=dict(status='running',iterations=1000,threads=1,binary_sha256=hashes,
          enable_switch='absent; default-on',
          note='Auxiliary activation audit only; excluded from the ten timing samples.')
(out/'environment.json').write_text(json.dumps(meta,indent=2)+'\n')
proc=None
def stop(p):
    if p is None or p.poll() is not None:return
    subprocess.run(['sudo','-n','kill','-TERM','--',f'-{p.pid}'],check=False,
                   stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:p.wait(timeout=15)
    except subprocess.TimeoutExpired:
        subprocess.run(['sudo','-n','kill','-KILL','--',f'-{p.pid}'],check=True)
        p.wait(timeout=10)
try:
    with (out/'loader.log').open('w') as log:
        proc=subprocess.Popen(command(server_env,runner/'benchmark/uprobe/uprobe'),
            cwd=runner,env=base,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        deadline=time.monotonic()+40
        while 'Successfully started!' not in (out/'loader.log').read_text():
            assert proc.poll() is None,'Audit loader exited'
            assert time.monotonic()<deadline,'Audit loader startup timed out'
            time.sleep(.1)
        with (out/'victim.log').open('w') as stream:
            target=subprocess.Popen(command(victim_env,runner/'benchmark/test',1,1000),
                cwd=runner,env=base,stdout=stream,stderr=subprocess.STDOUT,start_new_session=True)
            try:status=target.wait(timeout=60)
            finally:stop(target)
            assert status==0,status
        assert proc.poll() is None
    text=(out/'victim.log').read_text()
    # Kernel/libbpf program names may be truncated to BPF_OBJ_NAME_LEN - 1.
    assert re.search(r'\[array-jit-inline\] program=array_map_look\w* retained 1 maps',text)
    assert re.search(r'\[array-jit-inline\] program=per_cpu_array_\w* retained 1 maps',text)
    assert text.count('[array-jit-inline] inlined ') >= 2
    samples=re.findall(r'Benchmarking (__bench_\w+) in thread 1\nAverage time usage ([\d.]+) ns, iter 1000 times',text)
    assert len(samples)==17 and len(dict(samples))==17,len(samples)
    assert all(float(ns)>0 for _,ns in samples)
    assert hashes=={str(p):digest(p) for p in paths}
    meta.update(status='passed',observed_cases=17,
                inlined_sites_log_lines=text.count('[array-jit-inline] inlined '))
    print('Exact-artifact activation audit passed; 17 cases, explicit retained-map and inline logs.')
finally:
    stop(proc)
    subprocess.run(['sudo','-n',str(artifacts/'tools/bpftimetool/bpftimetool'),'remove'],
        cwd=runner,env=base,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    if meta['status']=='running':meta['status']='failed'
    (out/'environment.json').write_text(json.dumps(meta,indent=2)+'\n')
