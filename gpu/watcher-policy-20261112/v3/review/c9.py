import sys; sys.path.insert(0, "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/v3chk")
from lib import *
def rate(r, c, s):
    x = runs[(r, c, s)]; s0, e0 = x['win_ns']
    p = [q for q in x['loadpts'] if s0 <= q[0] <= e0]
    return (p[-1][1] - p[0][1]) / ((p[-1][0] - p[0][0]) / 1e9)
def q(v, p): v = sorted(v); return v[min(len(v) - 1, int(len(v) * p))]
for c in ('c01', 'c02', 'c15', 'c21'):
    print('==', c, cells[c])
    for s in SETS:
        v = [dcpu(r, c, s) for r in R]
        line = f"  {s:5s} dcpu " + " ".join(f"{x:+.3f}" for x in v) + f" med {med(v):+.3f} | mean " + " ".join(f"{mean(r,c,s):.1f}" for r in R) + f" med {med([mean(r,c,s) for r in R]):.1f}"
        if cells[c]['load'] != 'off':
            rr = [rate(r, c, s) / rate(r, c, 'P') for r in R]
            line += " | load/P " + " ".join(f"{x:.3f}" for x in rr) + f" med {med(rr):.3f}"
        if s != 'P':
            L = [v for r in R for v in lat(r, c, s)]
            line += f" | lat mean {st.mean(L)/1000:.1f} med {med(L)/1000:.1f} p99 {q(L,.99)/1000:.1f} >1ms {100*sum(v>1e6 for v in L)/len(L):.2f}% perrun " + " ".join(f"{st.mean(lat(r,c,s))/1000:.1f}" for r in R)
        print(line)
