#!/usr/bin/env python3
# All-core CPU per event, paired GMR with 95% t-CI over (session prefix, round, rep).
# CPU-s = sum(busy over measured cores) * wall_s; events = req_nginx * ev_per_req (2 per request).
# cores: all = 0,1,3,4,5 (cpu2 = harness, not recorded); srv = 3,4,5 (nginx + sslsniff, no wrk).
# Usage: cpuevent.py rows... -- A:B ...
import json,math,sys,statistics
from scipy import stats
i=sys.argv.index('--'); files=sys.argv[1:i]; pairs=sys.argv[i+1:]
rows=[json.loads(l) for f in files for l in open(f)]
rows=[r for r in rows if r['valid'] and not (r.get('stock') and r['leg']!='B' and (r.get('sniff_cpu_s') or 1)<0.3)]
al={'K_wb32':'K+SSLSNIFF_PB_WAKEUP=32'}
def cpu(r,cores): return sum(r['busy'][c] for c in cores)*r['wall_s']
def val(r,cores): return cpu(r,cores)/(r['req_nginx']*2)*1e6   # CPU-us per event
def get(L,cores):
    L=al.get(L,L); return {(r['session_id'][:5],r['round'],r['rep']):val(r,cores) for r in rows if r['leg']==L}
for name,cores in (('all',['0','1','3','4','5']),('srv',['3','4','5'])):
    print(f'## cores={name}')
    legs=sorted(set(r['leg'] for r in rows))
    for L in legs:
        v=[val(r,cores) for r in rows if r['leg']==L]
        print(f'  median {L:34s} {statistics.median(v):7.2f} CPU-us/event n={len(v)}')
    for p in pairs:
        a,b=p.rsplit(':',1); A=get(a,cores); B=get(b,cores); k=[x for x in A if x in B]
        d=[math.log(A[x]/B[x]) for x in k]; n=len(d); m=sum(d)/n
        sd=(sum((x-m)**2 for x in d)/(n-1))**.5; t=stats.t.ppf(.975,n-1)*sd/n**.5
        dd=[A[x]-B[x] for x in k]; md=sum(dd)/n; sdd=(sum((x-md)**2 for x in dd)/(n-1))**.5; td=stats.t.ppf(.975,n-1)*sdd/n**.5
        print(f'  {a:30s} / {b:12s} GMR {math.exp(m):.4f} [{math.exp(m-t):.4f}, {math.exp(m+t):.4f}]  diff {md:+.2f} [{md-td:+.2f}, {md+td:+.2f}] us/event n={n}')
