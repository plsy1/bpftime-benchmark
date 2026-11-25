import json,collections,statistics as st,math,sys
from scipy import stats
rows=[json.loads(l) for l in open(sys.argv[1])]
for r in rows:
  print(r['leg'],r['round'],r['rep'],r['order_idx'],r['warmup'],r['valid'],r['invalid_reason'],r['rps'],(r.get('cnt') or {}).get('lost'),r['env'],r['busy'].get('6'),r['stage'],r['build'])
V=[r for r in rows if r['valid'] and not r['warmup']]
d={(r['session_id'],r['round'],r['rep'],r['leg']):r['rps'] for r in V}
legs=sorted(set(r['leg'] for r in V))
for L in legs:
  v=[r['rps'] for r in V if r['leg']==L]
  print(L,len(v),'mean %.0f med %.0f cv %.2f%% min %.0f max %.0f'%(st.mean(v),st.median(v),100*st.stdev(v)/st.mean(v),min(v),max(v)))
def pair(A,Bl):
  keys=sorted(set(k[:3] for k in d))
  x=[];rat=[]
  for k in keys:
    if k+(A,) in d and k+(Bl,) in d:
      x.append((1/d[k+(A,)]-1/d[k+(Bl,)])*1e6/2); rat.append(d[k+(A,)]/d[k+(Bl,)])
  n=len(x);m=st.mean(x);se=st.stdev(x)/math.sqrt(n);t=stats.t.ppf(0.95,n-1)
  print(f'{A} vs {Bl}: n={n} mean={m:+.4f} 90%CI [{m-t*se:+.4f},{m+t*se:+.4f}] median={st.median(x):+.4f} ratio_med={st.median(rat):.4f}')
  print('   x:',' '.join('%+.3f'%v for v in x))
  return x
pair('Dn','Dn+BPFTIME_AOT_CACHE=0')
pair('Dnpc','Dnpc+BPFTIME_AOT_CACHE=0')
pair('Dn','B'); pair('Dn+BPFTIME_AOT_CACHE=0','B'); pair('Dnpc','B'); pair('Dnpc+BPFTIME_AOT_CACHE=0','B')
