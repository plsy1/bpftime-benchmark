import json,sys,statistics as st,collections
def load(p):
    return [json.loads(l) for l in open(p) if l.strip()]
for p in sys.argv[1:]:
    rows=load(p); d=collections.defaultdict(list); err=collections.Counter()
    for r in rows:
        k=(r['tool'],r.get('threads',1),r['backend'])
        if 'error' in r or 'ops_per_sec' not in r: err[k]+=1; continue
        d[k].append(r['ops_per_sec'])
    keys=sorted({(t,n) for t,n,b in list(d)+list(err)})
    bes=['none','kernel','dev-a4ac0d31','dev-7ad5a152']
    print(f'## {p}\n| tool | thr | none | bpf | a4ac | 7ad5 | a4ac/bpf | 7ad5/bpf | 7ad5/a4ac | errors |\n|---|--:|--:|--:|--:|--:|--:|--:|--:|---|')
    for t,n in keys:
        m={b:(st.median(d[(t,n,b)]) if d[(t,n,b)] else None) for b in bes}
        f=lambda x:'-' if x is None else (f'{x/1e6:.2f}M' if x>=1e6 else f'{x/1e3:.1f}k')
        r=lambda a,b:'-' if not a or not b else f'{a/b:.2f}'
        e=' '.join(f'{b}:{err[(t,n,b)]}' for b in bes if err[(t,n,b)])
        print(f"| {t} | {n} | "+' | '.join(f(m[b]) for b in bes)+f" | {r(m['dev-a4ac0d31'],m['kernel'])} | {r(m['dev-7ad5a152'],m['kernel'])} | {r(m['dev-7ad5a152'],m['dev-a4ac0d31'])} | {e} |")
