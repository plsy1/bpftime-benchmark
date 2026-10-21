#!/usr/bin/env python3
"""Global-data validation (globals workload): previous = 5466f0d baseline, inline = bdd9835.
Same BPF object, live maps, runtime keys and observable results on all engines."""
import argparse
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import time

ROOT=Path('/home/jetson/src/bpftime-global-data-bdd9835-20261021')
RESULTS=Path(__file__).resolve().parent
PREVIOUS=Path('/home/jetson/src/results/uprobe/baseline-177a1dc-jit-llvm16-20261021/artifacts')
BUILD=Path('/home/jetson/src/results/uprobe/array-jit-global-data-20261021/formal/artifacts')
POC=Path('/home/jetson/src/bpftime-array-jit-poc-20261019/build-poc')
DIAG=ROOT/'benchmark/uprobe/.output/percpu-array-jit-globals'
CASES=('control_hit','hit','control_mixed','mixed','unused')

def checksum(seed,mask):
    state=seed; total=hits=misses=0
    for _ in range(1000):
        state=(state*1664525+1013904223)&0xffffffff
        key=(state>>16)&mask
        if key<1024: total+=key*17+5; hits+=1
        else: total+=0x9e3779b9; misses+=1
    return total,hits,misses

def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def freq():
    p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
    values={k:int((p/f'scaling_{k}_freq').read_text()) for k in ('min','max','cur')}
    assert values==dict(min=1728000,max=1728000,cur=1728000),values
    return values

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--runs',type=int,default=6)
    ap.add_argument('--iterations',type=int,default=50000)
    ap.add_argument('--dump-jit',action='store_true')
    ap.add_argument('--engines',nargs='+',choices=('kernel','previous','poc','off','inline'),
        default=['kernel','previous','inline'])
    args=ap.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    assert not Path('/dev/shm/bpftime_maps_shm').exists()
    artifacts=[DIAG/name for name in ('loader','victim','percpu_array_jit.bpf.o')]
    builds=([PREVIOUS] if 'previous' in args.engines else []) + ([BUILD] if any(x in args.engines for x in ('off','inline')) else [])
    if 'poc' in args.engines:builds.append(POC)
    artifacts += [build/name for build in builds for name in
        ('runtime/agent/libbpftime-agent.so','runtime/syscall-server/libbpftime-syscall-server.so')]
    hashes={str(p):digest(p) for p in artifacts}
    meta=dict(status='running',started=datetime.datetime.now().astimezone().isoformat(),
        runs=args.runs,iterations=args.iterations,binary_sha256=hashes,
        cpu=5,frequency_samples=[],workload='runtime LCG keys; value checksum; live maps; 1000 elements/invocation')
    (out/'environment.json').write_text(json.dumps(meta,indent=2)+'\n')
    rows=[]
    base_env={k:v for k,v in os.environ.items() if not k.startswith('BPFTIME_') and k!='LD_PRELOAD'}
    base_env.update(BPFTIME_VM_NAME='llvm',BPFTIME_LOG_OUTPUT='console',SPDLOG_LEVEL='info')
    prefix=['sudo','-n','taskset','-c','5']
    def command(env,path,*rest):
        return prefix+['env',*[f'{k}={v}' for k,v in env.items()
            if k in ('BPFTIME_VM_NAME','BPFTIME_LOG_OUTPUT','SPDLOG_LEVEL','LD_PRELOAD','BPFTIME_ARRAY_JIT_POC','BPFTIME_ARRAY_JIT_INLINE','BPFTIME_ARRAY_JIT_POC_DUMP_DIR')],str(path),*map(str,rest)]
    def cleanup(proc):
        if proc.poll() is None:
            subprocess.run(['sudo','-n','kill','-TERM','--',f'-{proc.pid}'],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            try:proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                subprocess.run(['sudo','-n','kill','-KILL','--',f'-{proc.pid}'],check=False)
                proc.wait(timeout=10)
    def measure(engine,round_no):
        meta['frequency_samples'].append(freq())
        dest=out/f'{engine}-{round_no:02d}';dest.mkdir()
        loader_env=dict(base_env);victim_env=dict(base_env)
        if engine!='kernel':
            build=PREVIOUS if engine=='previous' else POC if engine=='poc' else BUILD
            loader_env['LD_PRELOAD']=str(build/'runtime/syscall-server/libbpftime-syscall-server.so')
            victim_env['LD_PRELOAD']=str(build/'runtime/agent/libbpftime-agent.so')
            victim_env['BPFTIME_ARRAY_JIT_POC']='1' if engine=='poc' else '0'
            assert engine in ('kernel','previous','inline'),'Default-on experiment has no off arm'
            if args.dump_jit and engine in ('previous','off','inline'):
                (dest/'jit').mkdir()
                victim_env['BPFTIME_ARRAY_JIT_POC_DUMP_DIR']=str(dest/'jit')
        proc=None
        try:
            with (dest/'loader.log').open('w') as log:
                proc=subprocess.Popen(command(loader_env,DIAG/'loader'),cwd=ROOT,env=base_env,
                    stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                deadline=time.monotonic()+40
                while 'Successfully started!' not in (dest/'loader.log').read_text():
                    if proc.poll() is not None or time.monotonic()>deadline:
                        raise RuntimeError(f'Loader startup failed: {dest}')
                    time.sleep(.1)
                time.sleep(1)
                with (dest/'victim.log').open('w') as target_log:
                    target=subprocess.Popen(command(victim_env,DIAG/'victim',args.iterations,round_no%5),
                        cwd=ROOT,env=base_env,stdout=target_log,stderr=subprocess.STDOUT,start_new_session=True)
                    try:status=target.wait(timeout=180)
                    finally:cleanup(target)
                    assert status==0,(dest,status)
                assert proc.poll() is None,f'Loader exited during target: {dest}'
                cleanup(proc)
            text=(dest/'victim.log').read_text()
            if engine!='kernel':assert 'shm_open_type 1' in text,dest
            if engine=='poc':assert '[array-jit-poc] inlined ' in text,dest
            if engine in ('previous','inline'):assert '[array-jit-inline] inlined ' in text,dest
            if engine in ('previous','inline'):assert '[array-jit-inline] program=' in text,dest
            # The baseline must leave every global-data program unbound.
            if engine=='previous':assert 'global data maps' not in text,dest
            if engine=='inline':assert 'program=hit retained 2 maps and 2 global data maps' in text,dest  # .rodata + .bss
            if engine=='off':
                assert '[array-jit-poc] inlined ' not in text,dest
                assert '[array-jit-inline] inlined ' not in text,dest
            times={}
            for line in text.splitlines():
                fields=line.split(',')
                if fields[0] in CASES and len(fields)==5:
                    assert fields[0] not in times
                    assert int(fields[1])==args.iterations
                    times[fields[0]]=(float(fields[3]),int(fields[4]))
            assert set(times)==set(CASES),times
            expected_seed=(0x12345678+(args.iterations-1)*0x9e3779b9)&0xffffffff
            assert all(seed==expected_seed for _,seed in times.values())
            observed={}
            for line in (dest/'loader.log').read_text().splitlines():
                m=re.fullmatch(r'RESULT slot=(\d+) seed=(\d+) checksum=(\d+) hits=(\d+) misses=(\d+)',line)
                if m:
                    slot,seed,total,hits,misses=map(int,m.groups())
                    observed[slot]=(seed,total,hits,misses)
            assert set(observed)=={0,1,2,3},(dest,observed)
            cpu_rows=re.findall(r'RESULT_CPU slot=(\d+) cpu=(\d+)',(dest/'loader.log').read_text())
            assert sorted(cpu_rows)==[(str(i),'5') for i in range(4)],(dest,cpu_rows)
            for slot in range(4):
                assert observed[slot]==(expected_seed,*checksum(expected_seed,1023 if slot<2 else 2047)),(dest,slot,observed[slot])
            (dest/'checksum-check.json').write_text(json.dumps(dict(status='passed',observed=observed),indent=2)+'\n')
            for case,(ns,seed) in times.items():
                assert ns>0
                rows.append(dict(engine=engine,round=round_no,case=case,raw_ns_per_element=ns,last_seed=seed))
            meta['frequency_samples'].append(freq())
            print(f'Completed {engine} round {round_no}/{args.runs}; all four checksums/counts passed',flush=True)
        finally:
            if proc is not None:cleanup(proc)
            if engine!='kernel':
                subprocess.run(['sudo','-n',str(BUILD/'tools/bpftimetool/bpftimetool'),'remove'],
                    cwd=ROOT,env=base_env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
    try:
        for i in range(args.runs):
            order=['kernel','previous','poc','off','inline'] if i%2==0 else ['inline','off','poc','previous','kernel']
            for engine in order:
                if engine in args.engines:measure(engine,i+1)
        assert hashes=={str(p):digest(p) for p in artifacts}
        with (out/'raw.csv').open('w') as stream:
            w=csv.DictWriter(stream,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
        summary=[]
        for case in CASES:
            item=dict(case=case,not_a_gain_metric=(case=='unused'))
            for engine in args.engines:
                values=[r['raw_ns_per_element'] for r in rows if r['case']==case and r['engine']==engine]
                item[engine]=dict(median=statistics.median(values),mean=statistics.mean(values),raw_values=values)
            if 'inline' in item and 'previous' in item:
                item['inline_vs_previous_percent']=100*(item['inline']['median']/item['previous']['median']-1)
                item['paired_wins_vs_previous']=sum(a<b for a,b in zip(item['inline']['raw_values'],item['previous']['raw_values']))
            if 'inline' in item and 'off' in item:
                item['inline_vs_off_percent']=100*(item['inline']['median']/item['off']['median']-1)
            if 'inline' in item and 'kernel' in item:
                item['inline_vs_kernel']=item['inline']['median']/item['kernel']['median']
            summary.append(item)
        (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        print(json.dumps(summary,indent=2),flush=True)
        meta['status']='success'
    except BaseException as error:
        meta.update(status='failed',error=repr(error));raise
    finally:
        meta['finished']=datetime.datetime.now().astimezone().isoformat()
        (out/'environment.json').write_text(json.dumps(meta,indent=2)+'\n')

if __name__=='__main__':main()
