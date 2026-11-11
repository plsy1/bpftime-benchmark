import os, re, sys, statistics as st, datetime, json, pickle
D="/home/jetson/src/bpftime-gpu-work/policy/logs"
SETS=["P","W","M100","A","AC","AB","ABC"]
cells={}
for l in open("/home/jetson/src/bpftime-gpu-work/policy/rounds.sh"):
    m=re.match(r'\s*"(c\d\d) (.*?)"',l)
    if m:
        f=m[2].split(); cells[m[1]]=dict(probe=f[0],load=f[1],spin=int(f[2]),L=int(f[3]),T=int(f[4]),J=int(f[5]),gap=int(f[6]),graph=int(f[7]),np=int(f[8]))
def rd(p):
    try: return open(p).read()
    except FileNotFoundError: return None
data={}
issues=[]
for r in [1,2,3,4]:
    for c,cd in cells.items():
        for s in SETS:
            b=f"{D}/r{r}/{c}_{s}"
            env=rd(b+".env")
            if env is None: issues.append(f"missing env {b}"); continue
            rec=dict(env=env)
            vs=[]
            for i in range(1,cd['np']+1):
                t=rd(f"{b}.{i}.victim") or ""
                g=lambda pat: re.search(pat,t)
                w=g(r"window_ns=(\d+) (\d+)"); rw=g(r"realtime_ns=(\d+) (\d+)"); cw=g(r"cpu_s=([\d.]+) wall_s=([\d.]+)")
                pl=g(r"per_launch_us mean=([\d.]+) median=([\d.]+) p90=([\d.]+) max=([\d.]+)")
                ln=g(r"launches=(\d+) threads=(\d+)")
                if not (w and rw and cw and pl): issues.append(f"bad victim {b}.{i}"); vs.append(None); continue
                vs.append(dict(s=int(w[1]),e=int(w[2]),rs=int(rw[1]),re=int(rw[2]),cpu=float(cw[1]),wall=float(cw[2]),
                    mean=float(pl[1]),med=float(pl[2]),p90=float(pl[3]),max=float(pl[4]),L=int(ln[1]),T=int(ln[2]),
                    graph=g(r"graph=(\d)")[1]))
                if int(ln[1])!=cd['L'] or int(ln[2])!=cd['T']: issues.append(f"victim params mismatch {b}.{i}")
                err=rd(f"{b}.{i}.victim.err") or ""
                if re.search(r"(?i)error|fail|warn",err): issues.append(f"victim.err has err/warn {b}.{i}: "+"; ".join(x for x in err.splitlines() if re.search(r'(?i)error|fail|warn',x))[:200])
            rec['v']=vs
            # loaders
            lds=[]
            if s!="P":
                for i in range(1,cd['np']+1):
                    t=rd(f"{b}.{i}.loader") or ""
                    m=re.search(r"count=(\d+)",t)
                    ns=[int(x) for x in t.split("ns=")[1].split()[0].split(",") if x] if "ns=" in t else []
                    lds.append(dict(count=int(m[1]) if m else -1, ns=ns))
                    le=rd(f"{b}.{i}.loader.err") or ""
                    bad=[x for x in le.splitlines() if re.search(r"(?i)error|fail|warn",x)]
                    if bad: issues.append(f"loader.err {b}.{i}: "+" | ".join(bad)[:300])
            rec['ld']=lds
            # env checks
            rcs=re.findall(r"victim (\d+) rc=(\d+)",env); lrc=re.findall(r"loader (\d+) rc=(\d+)",env); ldrc=re.findall(r"load rc=(\d+)",env)
            clk=re.findall(r"cpu5=(\d+) cpu0=(\d+) gpu=(\d+)",env)
            temps=[int(x) for m in re.findall(r"temps=([\d,]+)",env) for x in m.split(",") if x]
            rec['tmax']=max(temps)/1000 if temps else None
            ok=True; why=[]
            if len(rcs)!=cd['np'] or any(x[1]!="0" for x in rcs): ok=False; why.append("victim rc")
            if s!="P" and (len(lrc)!=cd['np'] or any(x[1]!="0" for x in lrc)): ok=False; why.append(f"loader rc {lrc}")
            if cd['load']!="off" and ldrc!=["0"]: ok=False; why.append(f"load rc {ldrc}")
            if len(clk)!=2 or any(x!=("1728000","1728000","1020000000") for x in clk): ok=False; why.append(f"clk {clk}")
            if "not attached" in env or "in use" in env or "ABORT" in env: ok=False; why.append("note")
            if any(v is None for v in vs): ok=False; why.append("victim parse")
            for ld in lds:
                exp=cd['L']+1+cd['graph'] if cd['T']==1 else None
                if cd['probe']=="host" and cd['T']==1 and ld['count']!=exp: ok=False; why.append(f"count {ld['count']} exp {exp}")
                if cd['probe']=="host" and cd['T']==1 and len(ld['ns'])!=ld['count']: why.append(f"nslen {len(ld['ns'])} vs count {ld['count']}")
                if cd['probe']=="none" and ld['count'] not in (0,-1): why.append(f"none-probe count {ld['count']}")
                if cd['T']==32: why.append(f"burst count {ld['count']} nslen {len(ld['ns'])}")
            rec['ok']=ok; rec['why']=why
            # stats
            stt=rd(b+".stats")
            rec['st']=[{k:float(v) for k,v in re.findall(r"(\w+)=(-?[\d.]+)",l) if k not in("pid","policy")} for l in stt.splitlines()] if stt else []
            rec['stpol']=re.findall(r"policy=(\w+)",stt) if stt else []
            # load
            lt=rd(b+".load")
            if lt is not None and all(vs):
                pts=[tuple(map(int,l.split())) for l in lt.splitlines() if len(l.split())==2]
                s0=min(v['s'] for v in vs); e0=max(v['e'] for v in vs)
                ins=[p for p in pts if s0<=p[0]<=e0]
                rec['rate']=(ins[-1][1]-ins[0][1])/((ins[-1][0]-ins[0][0])/1e9) if len(ins)>=2 else None
                rec['loadcover']=(pts[0][0]<=s0, pts[-1][0]>=e0, len(ins))
                # max gap between samples in window
                rec['loadmaxgap']=max((ins[k+1][0]-ins[k][0] for k in range(len(ins)-1)),default=0)/1e6
            pw=rd(b+".power")
            if pw is not None and all(vs):
                rs=max(v['rs'] for v in vs); re_=min(v['re'] for v in vs)
                lo=-(-rs//10**9); hi=re_//10**9 - 1
                cv=[];vin=[];secs=set();allsecs=[]
                tmaxp=0
                for l in pw.splitlines():
                    m=re.match(r"(\d\d-\d\d-\d{4} \d\d:\d\d:\d\d)",l)
                    if not m: continue
                    ts=int(datetime.datetime.strptime(m[1],"%m-%d-%Y %H:%M:%S").timestamp())
                    allsecs.append(ts)
                    for tt in re.findall(r"@([\d.]+)C",l): tmaxp=max(tmaxp,float(tt))
                    if lo<=ts<=hi:
                        a=re.search(r"VDD_CPU_GPU_CV (\d+)mW",l); bb=re.search(r"VDD_IN (\d+)mW",l)
                        cv.append(int(a[1])); vin.append(int(bb[1])); secs.add(ts)
                rec['pw']=(st.mean(cv),st.mean(vin),len(cv),len(secs)) if cv else None
                rec['pwwin']=(rs/1e9,re_/1e9,(re_-rs)/1e9, min(allsecs) if allsecs else None, max(allsecs) if allsecs else None)
                rec['tmaxp']=tmaxp
            data[(r,c,s)]=rec
pickle.dump((cells,data,issues),open(sys.argv[1],"wb"))
print("records",len(data),"ok",sum(v['ok'] for v in data.values()))
from collections import Counter
print("issues",len(issues)); 
for x in issues[:40]: print(" ",x)
whys=Counter(w.split(" ")[0]+" "+w.split(" ")[1] if w.startswith("burst") else w for v in data.values() for w in v['why'])
for k,v in whys.most_common(40): print(" why",k,v)
print("tmax env",max(v['tmax'] for v in data.values()),"tmax power", max(v.get('tmaxp',0) for v in data.values()))
