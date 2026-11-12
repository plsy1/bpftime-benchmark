import os, re, glob, json, statistics as st, datetime, sys
D='/home/jetson/src/bpftime-gpu-work/policy/logs/v2'
CELLS={}
for l in open('/home/jetson/src/bpftime-gpu-work/policy/rounds4.sh'):
    m=re.match(r'\s*"(c\d\d) (\w+) (\w+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d) (\d)"',l)
    if m: CELLS[m[1]]=dict(load=m[3],spin=int(m[4]),launches=int(m[5]),threads=int(m[6]),graph=int(m[9]))
SETS=['P','M100','AC','AC2']
R={}
def rd(p):
    return open(p).read() if os.path.exists(p) else None
for r in (1,2,3,4):
  for c,cd in CELLS.items():
    for s in SETS:
      b=f'{D}/r{r}/{c}_{s}'
      e=rd(b+'.env')
      if e is None: continue
      d={}
      d['vrc']=re.findall(r'victim \d+ rc=(\d+)',e); d['lrc']=re.findall(r'loader \d+ rc=(\d+)',e); d['ldrc']=re.findall(r'load rc=(\d+)',e)
      d['clk']=re.findall(r'cpu5=(\d+) cpu0=(\d+) gpu=(\d+)',e)
      d['temps']=[int(x) for m in re.findall(r'temps=([\d,]+)',e) for x in m.split(',') if x]
      d['envnote']=[l for l in e.splitlines() if 'not attached' in l or 'in use' in l or 'ABORT' in l]
      v=rd(b+'.1.victim')
      w=re.search(r'window_ns=(\d+) (\d+)',v); rw=re.search(r'realtime_ns=(\d+) (\d+)',v)
      pl=re.search(r'per_launch_us mean=([\d.]+) median=([\d.]+) p90=([\d.]+) max=([\d.]+)',v); cw=re.search(r'cpu_s=([\d.]+) wall_s=([\d.]+)',v)
      d['ws'],d['we']=int(w[1]),int(w[2]); d['rs'],d['re']=int(rw[1]),int(rw[2])
      d['mean']=float(pl[1]); d['med']=float(pl[2]); d['cpu']=float(cw[1]); d['wall']=float(cw[2]); d['cpuw']=d['cpu']/d['wall']
      ld=rd(b+'.1.loader')
      if ld:
        m=re.search(r'count=(\d+)',ld); d['count']=int(m[1]) if m else None
        line=[l for l in ld.splitlines() if l.startswith('hostspin ns=')]
        vals=[int(x) for x in line[0].split('=',1)[1].split(',') if x] if line else []
        d['nvals']=len(vals); d['lat']=[x/1000 for x in vals[1:]]
      sp=rd(b+'.stats')
      if sp:
        lines=[l for l in sp.splitlines() if l.strip()]
        d['nstats']=len(lines)
        d['st']={k:float(x) for k,x in re.findall(r'(\w+)=(-?[\d.]+)',lines[0])} if lines else {}
      lp=rd(b+'.load')
      if lp:
        pts=[(int(a),int(bb)) for a,bb in (l.split() for l in lp.splitlines() if len(l.split())==2) if d['ws']<=int(a)<=d['we']]
        d['rate']=(pts[-1][1]-pts[0][1])/((pts[-1][0]-pts[0][0])/1e9) if len(pts)>=2 else None
        d['npts']=len(pts)
      pw=rd(b+'.power')
      if pw is not None: d['power']=pw
      R[(r,c,s)]=d
