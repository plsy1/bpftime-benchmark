#!/usr/bin/env python3
"""Single-case AB/BA; frozen runtime builds and original full-suite BPF/loader."""
import argparse
import csv
import datetime
import hashlib
import itertools
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import time

OUT=Path(__file__).resolve().parent
ROOT=Path('/home/jetson/src/bpftime-percpu-array-jit-20261020')
PREVIOUS=OUT.parent.parent/'percpu-array-jit-direct-cpu-20261020/artifacts'
CURRENT=OUT.parent/'artifacts'
ENGINES={'previous':PREVIOUS,'current':CURRENT,'direct-array-erase':OUT/'artifacts'}
RUNNER=OUT.parent/'hash-delete-recheck/runner'
PREFIX=['sudo','-n','taskset','-c','5','env']

def digest(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def stop(p):
    if p is None or p.poll() is not None:return
    subprocess.run(['sudo','-n','kill','-TERM','--',f'-{p.pid}'],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:p.wait(timeout=10)
    except subprocess.TimeoutExpired:
        subprocess.run(['sudo','-n','kill','-KILL','--',f'-{p.pid}'],check=True)
        p.wait(timeout=10)

def clocks():
    p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
    freq={k:int((p/f'scaling_{k}_freq').read_text()) for k in ('min','max','cur')}
    assert freq==dict(min=1728000,max=1728000,cur=1728000),freq
    temps={}
    for zone in Path('/sys/class/thermal').glob('thermal_zone*'):
        try:temps[(zone/'type').read_text().strip()]=int((zone/'temp').read_text())
        except OSError:pass
    return dict(frequency=freq,temperatures=temps)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--rounds',type=int,default=12)
    ap.add_argument('--iterations',type=int,default=10000)
    ap.add_argument('--warmup',type=int,default=1000)
    args=ap.parse_args()
    assert not (OUT/'environment.json').exists(),'Do not overwrite an earlier run'
    assert not Path('/dev/shm/bpftime_maps_shm').exists(),'Another runtime owns SHM'
    assert (OUT/'build-exit-status.txt').read_text().strip()=='0'
    paths=[RUNNER/'benchmark/test',ROOT/'benchmark/test.c',ROOT/'benchmark/uprobe/uprobe',
           ROOT/'benchmark/uprobe/.output/uprobe.bpf.o',CURRENT/'tools/bpftimetool/bpftimetool']
    for build in ENGINES.values():
        paths += [build/'runtime/agent/libbpftime-agent.so',build/'runtime/syscall-server/libbpftime-syscall-server.so']
    hashes={str(p):digest(p) for p in paths}
    meta=dict(status='running',started=datetime.datetime.now().astimezone().isoformat(),
        rounds=args.rounds,iterations=args.iterations,warmup=args.warmup,
        binary_sha256=hashes,clock_samples=[],
        design='Six balanced ABC orders repeated; only ordinary hash delete executes. Exact single-case victim from preceding recheck, original full-suite eBPF/loader. Third arm changes only bound-array erase call-site in independent build; current frozen server is shared by current and diagnostic arms. No kernel rerun.')
    base={k:v for k,v in os.environ.items() if not k.startswith('BPFTIME_') and k!='LD_PRELOAD'}
    selected=dict(BPFTIME_VM_NAME='llvm',BPFTIME_LOG_OUTPUT='console',SPDLOG_LEVEL='info')
    rows=[]
    (OUT/'environment.json').write_text(json.dumps(meta,indent=2)+'\n')
    try:
        orders=list(itertools.permutations(ENGINES))
        for n in range(1,args.rounds+1):
            for engine in orders[(n-1)%len(orders)]:
                assert not Path('/dev/shm/bpftime_maps_shm').exists()
                meta['clock_samples'].append(clocks())
                dest=OUT/f'{n:02d}-{engine}';dest.mkdir()
                build=ENGINES[engine]
                loader_env=dict(selected,LD_PRELOAD=str(build/'runtime/syscall-server/libbpftime-syscall-server.so'))
                victim_env=dict(selected,LD_PRELOAD=str(build/'runtime/agent/libbpftime-agent.so'))
                if engine=='previous':victim_env['BPFTIME_ARRAY_JIT_INLINE']='1'
                command=lambda env,path,*a:PREFIX+[f'{k}={v}' for k,v in env.items()]+[str(path),*map(str,a)]
                proc=target=None
                try:
                    with (dest/'loader.log').open('w') as log:
                        proc=subprocess.Popen(command(loader_env,ROOT/'benchmark/uprobe/uprobe'),cwd=RUNNER,env=base,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                        deadline=time.monotonic()+40
                        while 'Successfully started!' not in (dest/'loader.log').read_text():
                            assert proc.poll() is None and time.monotonic()<deadline,dest
                            time.sleep(.1)
                        with (dest/'victim.log').open('w') as victim_log:
                            target=subprocess.Popen(command(victim_env,RUNNER/'benchmark/test',args.iterations,args.warmup),cwd=RUNNER,env=base,stdout=victim_log,stderr=subprocess.STDOUT,start_new_session=True)
                            try:status=target.wait(timeout=120)
                            finally:stop(target)
                            assert status==0,(dest,status)
                        assert proc.poll() is None,dest
                    text=(dest/'victim.log').read_text()
                    assert 'shm_open_type 1' in text,dest
                    matches=re.findall(r'Benchmarking (__bench_\w+) in thread 1\nAverage time usage ([\d.]+) ns, iter (\d+) times',text)
                    assert len(matches)==1 and matches[0][0]=='__bench_hash_map_delete',(dest,matches)
                    case,ns,count=matches[0]
                    assert int(count)==args.iterations and float(ns)>0
                    rows.append(dict(round=n,engine=engine,raw_ns_per_invocation=float(ns),ns_per_operation=float(ns)/1000))
                    meta['clock_samples'].append(clocks())
                    print(f'Pair {n:02d}/{args.rounds} {engine}: {float(ns)/1000:.4f} ns/op',flush=True)
                    with (OUT/'raw.csv').open('w') as f:
                        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
                finally:
                    stop(target);stop(proc)
                    if proc is not None:
                        subprocess.run(['sudo','-n',str(CURRENT/'tools/bpftimetool/bpftimetool'),'remove'],cwd=RUNNER,env=base,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        assert hashes=={str(p):digest(p) for p in paths},'Artifact changed'
        summary={}
        for engine in ENGINES:
            values=[r['ns_per_operation'] for r in rows if r['engine']==engine]
            summary[engine]=dict(mean=statistics.mean(values),median=statistics.median(values),min=min(values),max=max(values),raw_values=values)
        pairs=[b/a for a,b in zip(summary['previous']['raw_values'],summary['current']['raw_values'])]
        summary['comparison']=dict(mean_change_percent=100*(summary['current']['mean']/summary['previous']['mean']-1),
            median_paired_change_percent=100*(statistics.median(pairs)-1),paired_ratios=pairs,
            current_slower_pairs=sum(x>1 for x in pairs))
        for reference in ('previous','current'):
            ratios=[b/a for a,b in zip(summary[reference]['raw_values'],summary['direct-array-erase']['raw_values'])]
            summary['direct_array_erase_vs_'+reference]=dict(
                mean_change_percent=100*(summary['direct-array-erase']['mean']/summary[reference]['mean']-1),
                median_paired_change_percent=100*(statistics.median(ratios)-1),
                paired_ratios=ratios,diagnostic_faster_pairs=sum(x<1 for x in ratios))
        (OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        print(json.dumps(summary,indent=2),flush=True)
        meta['status']='success'
    except BaseException as error:
        meta.update(status='failed',error=repr(error));raise
    finally:
        meta['finished']=datetime.datetime.now().astimezone().isoformat()
        (OUT/'environment.json').write_text(json.dumps(meta,indent=2)+'\n')

if __name__=='__main__':main()
