import pickle,statistics as st,sys
cells,data,issues=pickle.load(open("data.pkl","rb"))
SETS=["P","W","M100","A","AC","AB","ABC"]
R=[1,2,3,4]
def cpu(rec): return sum(v['cpu'] for v in rec['v'])/max(v['wall'] for v in rec['v'])
def q(v,p): return v[min(len(v)-1,int(len(v)*p))]
KS=["req","wfe_waits","slow_sleeps","defer_sleeps","hot_iters","parks","park_timeouts","unseen_latch","unseen_hits","gate_checks","gate_open_checks","gate_opens","latch_trips","cap_forced","guard_skips","win0","win20","win80","win300"]
only=sys.argv[1:] 
for c,cd in cells.items():
    if only and c not in only: continue
    print(f"\n=== {c} {cd}")
    for s in SETS:
        recs=[data[(r,c,s)] for r in R]
        P=[data[(r,c,"P")] for r in R]
        d=[cpu(a)-cpu(b) for a,b in zip(recs,P)]
        line=f"{s:4} cpu {st.median(cpu(a) for a in recs):.3f} dCPU med {st.median(d):+.4f} per-round [{', '.join(f'{x:+.3f}' for x in d)}] plmean {st.median(st.mean(v['mean'] for v in a['v']) for a in recs):.1f} plmed {st.median(st.mean(v['med'] for v in a['v']) for a in recs):.1f}"
        if cd['load']!="off":
            rat=[a['rate']/b['rate'] for a,b in zip(recs,P)]
            line+=f" load {st.median(rat):.3f} [{min(rat):.3f}-{max(rat):.3f}]"
        else:
            pw=[a['pw'] for a in recs]
            line+=" pw "+(f"{st.median(p[0] for p in pw if p):.0f}/{st.median(p[1] for p in pw if p):.0f} n={[p[3] if p else 0 for p in pw]} " if any(pw) else "NONE ")+ f"win {[round(a['pwwin'][2],2) for a in recs]}"
        if cd['T']==32 and s!="P":
            per=[(st.mean(v['mean'] for v in a['v'])-st.mean(v['mean'] for v in b['v']))/32 for a,b in zip(recs,P)]
            line+=f" costpercall {st.median(per):.2f} [{', '.join(f'{x:.1f}' for x in per)}]"
        print(line)
        if s!="P" and cd['T']==1 and cd['probe']=="host":
            L=sorted(x/1000 for a in recs for ld in a['ld'] for x in ld['ns'][1:])
            Lr=[sorted(x/1000 for ld in a['ld'] for x in ld['ns'][1:]) for a in recs]
            gt=100*sum(x>1000 for x in L)/len(L)
            print(f"      lat n={len(L)} mean {st.mean(L):.1f} med {st.median(L):.1f} p90 {q(L,.9):.1f} p99 {q(L,.99):.1f} max {L[-1]:.1f} >1ms {gt:.2f}%  per-round med {[round(st.median(x),1) for x in Lr]} >1ms {[round(100*sum(y>1000 for y in x)/len(x),2) for x in Lr]} p99 {[round(q(x,.99)) for x in Lr]}")
        if recs[0]['st']:
            sums=[{k:sum(dd.get(k,0) for dd in a['st']) for k in KS} for a in recs]
            print("      st "+" ".join(f"{k}={st.median(x[k] for x in sums):.0f}{[int(x[k]) for x in sums] if k in ('gate_opens','latch_trips','unseen_latch','parks','park_timeouts','unseen_hits') else ''}" for k in KS)+f" pols={[a['stpol'] for a in recs][0]} nlines={[len(a['st']) for a in recs]}")
