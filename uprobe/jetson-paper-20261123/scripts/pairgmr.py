#!/usr/bin/env python3
# Paired GMR (A/B) with 95% t-CI over (round, rep) pairs. Usage: pairgmr.py rows... -- A:B ...
import json,math,sys
from scipy import stats
i=sys.argv.index('--'); files=sys.argv[1:i]; pairs=sys.argv[i+1:]
rows=[json.loads(l) for f in files for l in open(f)]
rows=[r for r in rows if r['valid'] and not (r.get('stock') and r['leg']!='B' and (r.get('sniff_cpu_s') or 1)<0.3)]
def get(L): return {(r['session_id'][:5],r['round'],r['rep']):r['rps'] for r in rows if r['leg']==L}
al={'K_wb32':'K+SSLSNIFF_PB_WAKEUP=32','EXACT':'BPFTIME_DIAG_EXACT=1'}
for p in pairs:
    a,b=p.split(':'); A=get(a); B=get(b); k=[x for x in A if x in B]
    d=[math.log(A[x]/B[x]) for x in k]; n=len(d); m=sum(d)/n
    sd=(sum((x-m)**2 for x in d)/(n-1))**.5; t=stats.t.ppf(.975,n-1)*sd/n**.5
    print(f'{a:32s} / {b:24s} {math.exp(m):.4f} [{math.exp(m-t):.4f}, {math.exp(m+t):.4f}] n={n}')
