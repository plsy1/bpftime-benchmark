#!/usr/bin/env python3
"""Paired full-workload short screen: 1cb20807 versus f26cc335, to check the full10 uprobe+uretprobe +10.6% and probe write +6.4%.

Copy of array-jit-global-data-20261021/formal/hash-update-screen/run-screen.py with the builds, workload root and printed cases changed."""
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
ROOT=OUT.parent/'full-runner/bpftime'
PREVIOUS=Path('/home/jetson/src/results/uprobe/hash-probe-bound-20261022/artifacts')
CURRENT=OUT.parent/'artifacts'
ENGINES={'previous-on':(PREVIOUS,None),'current-on':(CURRENT,None)}
PREFIX=['sudo','-n','taskset','-c','5','env']
PATTERN=r'Benchmarking (__bench_\w+) in thread 1\nAverage time usage ([\d.]+) ns, iter (\d+) times'

def digest(p):
    with p.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()

def stop(proc):
    if proc is None or proc.poll() is not None: return
    subprocess.run(['sudo','-n','kill','-TERM','--',f'-{proc.pid}'],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try: proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        subprocess.run(['sudo','-n','kill','-KILL','--',f'-{proc.pid}'],check=True)
        proc.wait(timeout=10)

def clock_sample():
    p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
    freq={k:int((p/f'scaling_{k}_freq').read_text()) for k in ('min','max','cur')}
    assert freq==dict(min=1728000,max=1728000,cur=1728000),freq
    temps={}
    for p in Path('/sys/class/thermal').glob('thermal_zone*'):
        try: temps[(p/'type').read_text().strip()]=int((p/'temp').read_text())
        except OSError: pass
    return dict(frequency=freq,temperature_millicelsius=temps)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--rounds',type=int,default=12)
    ap.add_argument('--iterations',type=int,default=10000)
    args=ap.parse_args()
    assert not (OUT/'environment.json').exists(),'Use a new directory for a new run'
    assert not Path('/dev/shm/bpftime_maps_shm').exists(),'Another runtime owns shared memory'
    subprocess.run(['sudo','-n','true'],check=True)
    paths=[ROOT/'benchmark/test',ROOT/'benchmark/uprobe/uprobe',ROOT/'benchmark/uprobe/.output/uprobe.bpf.o',CURRENT/'tools/bpftimetool/bpftimetool']
    for build in (PREVIOUS,CURRENT):
        paths.extend([build/'runtime/agent/libbpftime-agent.so',build/'runtime/syscall-server/libbpftime-syscall-server.so'])
    hashes={str(p):digest(p) for p in paths}
    orders=list(itertools.permutations(ENGINES))
    metadata=dict(status='running',started=datetime.datetime.now().astimezone().isoformat(),rounds=args.rounds,iterations=args.iterations,binary_sha256=hashes,
        design='Alternating AB/BA; 1cb20807 versus f26cc335, both default-on; original full-suite object/victim, fresh maps/process per arm; raw ns/op, no control subtraction. Historical unused-return array lookup is not a gain metric. No kernel run.',clock_samples=[])
    (OUT/'environment.json').write_text(json.dumps(metadata,indent=2)+'\n')
    base={k:v for k,v in os.environ.items() if not k.startswith('BPFTIME_') and k!='LD_PRELOAD'}
    selected=dict(BPFTIME_VM_NAME='llvm',BPFTIME_LOG_OUTPUT='console',SPDLOG_LEVEL='info')
    rows=[]
    try:
        for round_no in range(1,args.rounds+1):
            for engine in orders[(round_no-1)%len(orders)]:
                build,enabled=ENGINES[engine]
                dest=OUT/f'{round_no:02d}-{engine}';dest.mkdir()
                assert not Path('/dev/shm/bpftime_maps_shm').exists()
                metadata['clock_samples'].append(clock_sample())
                loader_env=dict(selected,LD_PRELOAD=str(build/'runtime/syscall-server/libbpftime-syscall-server.so'))
                victim_env=dict(selected,LD_PRELOAD=str(build/'runtime/agent/libbpftime-agent.so'))
                if enabled is not None: victim_env['BPFTIME_ARRAY_JIT_INLINE']=enabled
                command=lambda env,path,*rest:PREFIX+[f'{k}={v}' for k,v in env.items()]+[str(path),*map(str,rest)]
                proc=target=None
                try:
                    with (dest/'loader.log').open('w') as log:
                        proc=subprocess.Popen(command(loader_env,ROOT/'benchmark/uprobe/uprobe'),cwd=ROOT,env=base,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                        deadline=time.monotonic()+40
                        while 'Successfully started!' not in (dest/'loader.log').read_text():
                            assert proc.poll() is None and time.monotonic()<deadline,dest
                            time.sleep(.1)
                        with (dest/'victim.log').open('w') as log2:
                            target=subprocess.Popen(command(victim_env,ROOT/'benchmark/test',1,args.iterations),cwd=ROOT,env=base,stdout=log2,stderr=subprocess.STDOUT,start_new_session=True)
                            status=target.wait(timeout=150)
                            assert status==0,(dest,status)
                        assert proc.poll() is None,dest
                    text=(dest/'victim.log').read_text()
                    assert 'shm_open_type 1' in text,dest
                    inlined=text.count('[array-jit-inline] inlined ')
                    assert inlined>=2,(dest,inlined)
                    samples=re.findall(PATTERN,text)
                    assert len(samples)==17 and len({s[0] for s in samples})==17,(dest,len(samples))
                    for case,ns,count in samples:
                        assert int(count)==args.iterations and float(ns)>0
                        rows.append(dict(round=round_no,engine=engine,case=case,raw_ns_per_invocation=float(ns),raw_ns_per_operation=float(ns)/1000 if '_map_' in case else None))
                    metadata['clock_samples'].append(clock_sample())
                    got={x['case']:x['raw_ns_per_invocation'] for x in rows if x['round']==round_no and x['engine']==engine}
                    print(f"Round {round_no}/{args.rounds} {engine}: probe write {got['__bench_write']:.1f}, uprobe+uretprobe {got['__bench_uprobe_uretprobe']:.1f} ns/inv; 17 cases complete",flush=True)
                    with (OUT/'raw.csv').open('w') as f:
                        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
                finally:
                    stop(target);stop(proc)
                    if proc is not None:
                        subprocess.run(['sudo','-n',str(CURRENT/'tools/bpftimetool/bpftimetool'),'remove'],cwd=ROOT,env=base,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        assert hashes=={str(p):digest(p) for p in paths},'Artifact changed'
        report=[]
        for case in sorted({x['case'] for x in rows}):
            item=dict(case=case,unit='ns/op' if '_map_' in case else 'ns/invocation',not_a_gain_metric=('array_map_lookup' in case))
            for engine in ENGINES:
                values=[x['raw_ns_per_operation'] if '_map_' in case else x['raw_ns_per_invocation'] for x in rows if x['engine']==engine and x['case']==case]
                item[engine]=dict(mean=statistics.mean(values),median=statistics.median(values),min=min(values),max=max(values),raw_values=values)
            for engine in ('current-on',):
                paired=[item[engine]['raw_values'][i]/item['previous-on']['raw_values'][i] for i in range(args.rounds)]
                item[engine+'_vs_previous']=dict(ratio_of_means=item[engine]['mean']/item['previous-on']['mean'],median_paired_ratio=statistics.median(paired),paired_ratios=paired)
            report.append(item)
        (OUT/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
        metadata['status']='success'
    except BaseException as error:
        metadata.update(status='failed',error=repr(error));raise
    finally:
        metadata['finished']=datetime.datetime.now().astimezone().isoformat()
        (OUT/'environment.json').write_text(json.dumps(metadata,indent=2)+'\n')

if __name__=='__main__': main()
