#!/usr/bin/env python3
"""J8/J13 extra analysis: paired p-values (Holm), per-round diffs (leg x round),
drop-first-rep sensitivity, H1 per-event CPU (schedstat, per-core busy), irq/event, unreported events, perf sc."""
import json, math, statistics as st, sys, collections
from math import lgamma, exp, log

def betacf(a,b,x):
    qab,qap,qam=a+b,a+1,a-1; c=1.0; d=1-qab*x/qap; d=1/(d if abs(d)>1e-30 else 1e-30); h=d
    for m in range(1,300):
        m2=2*m; aa=m*(b-m)*x/((qam+m2)*(a+m2)); d=1+aa*d; d=1/(d if abs(d)>1e-30 else 1e-30); c=1+aa/c; c=c if abs(c)>1e-30 else 1e-30; h*=d*c
        aa=-(a+m)*(qab+m)*x/((a+m2)*(qap+m2)); d=1+aa*d; d=1/(d if abs(d)>1e-30 else 1e-30); c=1+aa/c; c=c if abs(c)>1e-30 else 1e-30; de=d*c; h*=de
        if abs(de-1)<1e-12: break
    return h
def ibeta(a,b,x):
    if x<=0: return 0.0
    if x>=1: return 1.0
    bt=exp(lgamma(a+b)-lgamma(a)-lgamma(b)+a*log(x)+b*log(1-x))
    return bt*betacf(a,b,x)/a if x<(a+1)/(a+b+2) else 1-bt*betacf(b,a,1-x)/b
def p2(t,df): return ibeta(df/2,0.5,df/(df+t*t))

path, exp_ = sys.argv[1], sys.argv[2]
rows=[json.loads(l) for l in open(path)]
rows=[r for r in rows if r['exp']==exp_ and r['valid'] and not r.get('warmup')]
meas=[r for r in rows if r['round'] in (0,1)]
by=collections.defaultdict(dict)
for r in meas: by[(r['round'],r['rep'])][r['leg']]=r
def x(r): return 1e6/r['rps']/2
def pairs(a,b,f,keep=lambda k:True):
    return [f(v[a])-f(v[b]) for k,v in sorted(by.items()) if a in v and b in v and keep(k)]
def lpairs(a,b,keep=lambda k:True):
    return [math.log(v[a]['rps']/v[b]['rps']) for k,v in sorted(by.items()) if a in v and b in v and keep(k)]
def tstat(d):
    n=len(d); m=st.fmean(d); s=st.stdev(d); t=m/(s/math.sqrt(n)); return m,t,p2(t,n-1),n

W='+SSLSNIFF_PB_WAKEUP=32'; N0='+BPFTIME_CONSUMER_NAP_US=0'
legs=sorted({r['leg'] for r in meas})
if exp_=='J13':
    fam=[('M1','Dnpc','K'+W),('M2','Dxn+BPFTIME_DIAG_EXACT=1','K'+W),('M3','Dnpc'+N0+W,'K'+W),('M4','Dnpc'+N0,'K'),('M5','Dn','OlL3x730')]
else:
    fam=[('M5','Dn','OlL3x730'),('M6','Dn730','OlL3')]
print(f'== {exp_}: superiority family (paired log-ratio t-test, two-sided; Holm over the tests listed here)')
res=[]
for name,a,b in fam:
    m,t,p,n=tstat(lpairs(a,b)); res.append((p,name,a,b,m,n))
k=len(res); srt=sorted(res); adj={}; run=0
for i,(p,name,*_) in enumerate(srt):
    run=max(run,min(1,(k-i)*p)); adj[name]=run
for p,name,a,b,m,n in res:
    print(f'{name} {a} / {b}: GMR {math.exp(m):.4f} p={p:.2e} Holm-adj={adj[name]:.2e} n={n}')
print()
print('== leg x round: per-round paired diffs (us/event), and drop-first-rep sensitivity')
for name,a,b in fam:
    out=[]
    for rd in (0,1):
        d=pairs(a,b,x,lambda k:k[0]==rd); m=st.fmean(d); h=2.093*st.stdev(d)/math.sqrt(len(d)); out.append(f'r{rd} {m:+.3f} [{m-h:+.3f},{m+h:+.3f}]')
    d0=pairs(a,b,x,lambda k:k[0]==0); d1=pairs(a,b,x,lambda k:k[0]==1)
    # Welch t for round difference
    s=math.sqrt(st.variance(d0)/len(d0)+st.variance(d1)/len(d1)); t=(st.fmean(d0)-st.fmean(d1))/s
    first={0:min(k[1] for k in by if k[0]==0),1:min(k[1] for k in by if k[0]==1)}
    dd=pairs(a,b,x,lambda k:k[1]!=first[k[0]]); m=st.fmean(dd); h=2.0*st.stdev(dd)/math.sqrt(len(dd))
    print(f'{name} x({a})-x({b}): {"; ".join(out)}; round diff p={p2(t,38):.3f}; drop-first n={len(dd)} {m:+.3f} [{m-h:+.3f},{m+h:+.3f}]')
print()
print('== H1 per event (medians over n reps): sniff CPU (schedstat), nginx CPU, core-3 busy CPU, all-core busy CPU (cores 0,1,3,4,5), irq on core 3, migrations, unreported events')
print('| leg | n | sniff us/ev | nginx us/ev | core3 us/ev | all-core us/ev | core3 irq/ev | nginx migr | sniff Pss MB | unreported ev (sum) |')
print('|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
for L in legs:
    rs=[r for r in meas if r['leg']==L]
    ev=[2*r['req_nginx'] for r in rs]
    def med(f): 
        v=[f(r,e) for r,e in zip(rs,ev)]; v=[a for a in v if a is not None]; return st.median(v) if v else float('nan')
    sn=med(lambda r,e: r['sniff_cpu_ns']/1e3/e if r.get('sniff_cpu_ns') else None)
    ng=med(lambda r,e: r['nginx_cpu_ns']/1e3/e)
    c3=med(lambda r,e: r['busy']['3']*r['wall_s']*1e6/e)
    ca=med(lambda r,e: sum(r['busy'][c] for c in ('0','1','3','4','5'))*r['wall_s']*1e6/e)
    iq=med(lambda r,e: r['irq_delta']['3']/e)
    mg=med(lambda r,e: r['nginx_migrations'])
    ps=med(lambda r,e: r['sniff_smaps_kb']['Pss']/1024 if r.get('sniff_smaps_kb') else None)
    un=sum((r['cnt']['write_ev']-r['cnt']['read_ev']) for r in rs if r.get('cnt'))
    print(f'| {L} | {len(rs)} | {sn:.2f} | {ng:.2f} | {c3:.2f} | {ca:.2f} | {iq:.3f} | {mg:.0f} | {ps:.1f} | {un} |')
print()
pr=[r for r in rows if r['round']==2]
if pr:
    print('== round 2 perf sc: syscalls per request on the worker (mean of reps)')
    agg=collections.defaultdict(lambda: collections.defaultdict(list))
    for r in pr:
        for k,v in (r.get('perf_per_req') or {}).items(): agg[r['leg']][k].append(v)
    for L,d in agg.items():
        tot=st.fmean(d['raw_syscalls:sys_enter']) if 'raw_syscalls:sys_enter' in d else float('nan')
        top=sorted(((st.fmean(v),k) for k,v in d.items() if k!='raw_syscalls:sys_enter' and st.fmean(v)>=0.005),reverse=True)
        print(f'{L}: all syscalls {tot:.2f}; of which '+', '.join(f'{k.split(":")[-1]} {m:.2f}' for m,k in top))
