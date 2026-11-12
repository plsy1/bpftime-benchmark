exec(open('/tmp/claude-2002/v2chk/parse.py').read())
med=st.median
def dcpu(c,s): return [R[(r,c,s)]['cpuw']-R[(r,c,'P')]['cpuw'] for r in (1,2,3,4)]
def loadr(c,s): return [R[(r,c,s)]['rate']/R[(r,c,'P')]['rate'] for r in (1,2,3,4)]
def pooled(c,s): return sorted(x for r in (1,2,3,4) for x in R[(r,c,s)]['lat'])
def gt1(c,s): L=pooled(c,s); return 100*sum(x>1000 for x in L)/len(L)
def gt1r(c,s): return [100*sum(x>1000 for x in R[(r,c,s)]['lat'])/len(R[(r,c,s)]['lat']) for r in (1,2,3,4)]
def cnt(c,s,k): return [R[(r,c,s)]['st'].get(k) for r in (1,2,3,4)]
def means(c,s): return [R[(r,c,s)]['mean'] for r in (1,2,3,4)]
def cpc(c,s): T=CELLS[c]['threads']; return [(R[(r,c,s)]['mean']-R[(r,c,'P')]['mean'])/T for r in (1,2,3,4)]
def latmeds(c,s): return [st.median(R[(r,c,s)]['lat']) for r in (1,2,3,4)]
def f(x): return [round(v,3) if v is not None else None for v in x]
