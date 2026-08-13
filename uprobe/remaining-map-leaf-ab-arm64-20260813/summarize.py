#!/usr/bin/env python3
import csv, re, statistics
from pathlib import Path

ROOT=Path(__file__).resolve().parent
CONFIG={
 "percpu_hash_update": {"control":"update_control","modes":["base","percpu_hash_update_raw_key_copy","percpu_hash_update_raw_value_copy","percpu_hash_update_cached_hash","percpu_hash_update_fixed4_equal","percpu_hash_update_cached_hash_fixed4_equal","percpu_hash_update_no_copy","percpu_hash_update_no_find"]},
 "percpu_array_update": {"control":"update_control","modes":["base","percpu_array_direct","percpu_array_fixed_cpu","percpu_array_update_address_only","percpu_array_update_fixed_cpu_address_only","percpu_array_update_no_address","percpu_array_update_fixed_cpu_no_address","percpu_array_update_no_body"]},
 "percpu_array_lookup": {"control":"lookup_control","modes":["base","percpu_array_direct","percpu_array_fixed_cpu","percpu_array_lookup_no_address","percpu_array_lookup_fixed_cpu_no_address","percpu_array_lookup_no_body"]},
 "array_update": {"control":"update_control","modes":["base","cache_control","cached_handler","direct_map","array_update_address_only","array_update_no_address","array_update_no_body"]},
}
RX=re.compile(r"^(\w+),[^,]*,[^,]*,[^,]*,([0-9.]+)$",re.M)
def stat(v): return statistics.mean(v),statistics.median(v),statistics.stdev(v),min(v),max(v)

for op,cfg in CONFIG.items():
 out=ROOT/op
 wall={m:[] for m in cfg["modes"]}; raw=[]
 for mode in cfg["modes"]:
  for run,p in enumerate(sorted((out/'raw/wall').glob(f'{mode}-run??.txt')),1):
   vals={k:float(v) for k,v in RX.findall(p.read_text(errors='replace'))}
   net=(vals[op]-vals[cfg['control']])/1000
   wall[mode].append(net); raw.append((mode,run,f'{net:.9f}'))
 with (out/'wall-raw.csv').open('w',newline='') as f:
  w=csv.writer(f); w.writerow(('mode','run','net_ns_per_helper')); w.writerows(raw)
 with (out/'wall-summary.csv').open('w',newline='') as f:
  w=csv.writer(f); w.writerow(('mode','mean','median','sample_sd','min','max'))
  for mode in cfg['modes']:
   assert len(wall[mode])==5,(op,mode,len(wall[mode])); w.writerow((mode,*(f'{x:.9f}' for x in stat(wall[mode]))))
 pmu={(m,k):[] for m in cfg['modes'] for k in ('cycles','instructions')}; raw=[]
 for mode in cfg['modes']:
  for run in range(1,4):
   cases={}
   for case in (cfg['control'],op):
    metrics={}
    p=out/f'raw/pmu/{mode}-{case}-run{run:02d}.perf.csv'
    for line in p.read_text().splitlines():
     row=line.split(',')
     if len(row)>=3 and row[0].replace('.','',1).isdigit() and row[2] in ('cycles','instructions'):
      if len(row)>=5 and row[4] and float(row[4])<99.9: raise RuntimeError(f'multiplexed {p}')
      metrics[row[2]]=float(row[0])
    assert set(metrics)=={'cycles','instructions'},p; cases[case]=metrics
   for metric in ('cycles','instructions'):
    net=(cases[op][metric]-cases[cfg['control']][metric])/(20000*1000)
    pmu[(mode,metric)].append(net); raw.append((mode,run,metric,f'{net:.9f}'))
 with (out/'pmu-raw.csv').open('w',newline='') as f:
  w=csv.writer(f); w.writerow(('mode','run','metric','net_per_helper')); w.writerows(raw)
 with (out/'pmu-summary.csv').open('w',newline='') as f:
  w=csv.writer(f); w.writerow(('mode','metric','mean','median','sample_sd','min','max'))
  for mode in cfg['modes']:
   for metric in ('cycles','instructions'): w.writerow((mode,metric,*(f'{x:.9f}' for x in stat(pmu[(mode,metric)]))))

def means(op,metric):
 p=ROOT/op/('wall-summary.csv' if metric=='ns/helper' else 'pmu-summary.csv')
 rows=list(csv.DictReader(p.open()))
 if metric=='ns/helper': return {r['mode']:float(r['mean']) for r in rows}
 name=metric.split('/')[0]; return {r['mode']:float(r['mean']) for r in rows if r['metric']==name}

rows=[]
def add(op,name,metric,value,boundary=None):
 share='' if boundary is None else f'{100*value/boundary:.6f}'
 rows.append((op,name,metric,f'{value:.9f}',share))

for metric in ('ns/helper','cycles/helper','instructions/helper'):
 m=means('percpu_hash_update',metric)
 find=m['percpu_hash_update_no_copy']-m['percpu_hash_update_no_find']
 dest=m['base']-m['percpu_hash_update_no_copy']
 h=m['percpu_hash_update_fixed4_equal']-m['percpu_hash_update_cached_hash_fixed4_equal']
 eq=m['percpu_hash_update_cached_hash']-m['percpu_hash_update_cached_hash_fixed4_equal']
 inter=(m['base']-m['percpu_hash_update_cached_hash'])-h
 rem=find-h-eq-inter
 for name,val,b in [('key_assign_over_memcpy',m['base']-m['percpu_hash_update_raw_key_copy'],None),('value_assign_over_memcpy',m['base']-m['percpu_hash_update_raw_value_copy'],None),('destination_value_copy',dest,None),('complete_find',find,None),('hash_leaf',h,find),('equality_leaf',eq,find),('hash_equality_interaction',inter,find),('container_remainder',rem,find)]: add('percpu_hash_update',name,metric,val,b)

for op,path in [('percpu_array_update',['base','percpu_array_direct','percpu_array_update_address_only','percpu_array_update_fixed_cpu_address_only','percpu_array_update_fixed_cpu_no_address','percpu_array_update_no_body']),('percpu_array_lookup',['base','percpu_array_direct','percpu_array_fixed_cpu','percpu_array_lookup_fixed_cpu_no_address','percpu_array_lookup_no_body']),('array_update',['base','array_update_address_only','array_update_no_address','array_update_no_body'])]:
 for metric in ('ns/helper','cycles/helper','instructions/helper'):
  m=means(op,metric); boundary=m[path[0]]-m[path[-1]]
  add(op,'complete_concrete_body',metric,boundary,None)
  labels=['std_function_wrapper','value_copy','cpu_selection','address_calculation','checks_remainder'] if op=='percpu_array_update' else (['std_function_wrapper','cpu_selection','address_calculation','checks_remainder'] if op=='percpu_array_lookup' else ['value_copy','address_calculation','checks_remainder'])
  for label,a,b in zip(labels,path,path[1:]): add(op,label,metric,m[a]-m[b],boundary)

for metric in ('ns/helper','cycles/helper','instructions/helper'):
 m=means('array_update',metric)
 add('array_update','shm_fd_variant_lookup',metric,m['cache_control']-m['cached_handler'])
 add('array_update','generic_handler_dispatch',metric,m['cached_handler']-m['direct_map'])

with (ROOT/'leaf-attribution.csv').open('w',newline='') as f:
 w=csv.writer(f); w.writerow(('operation','component','metric','mean_effect','share_of_body_or_find_percent')); w.writerows(rows)
print((ROOT/'leaf-attribution.csv').read_text(),end='')
