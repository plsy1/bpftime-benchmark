import sys; sys.path.insert(0, "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/v3chk")
from lib import *
def gt1(r, c, s):
    L = lat(r, c, s); return sum(v > 1_000_000 for v in L), len(L)
for c in ('c23', 'c24'):
    print('==', c)
    for s in SETS[1:]:
        v = [dcpu(r, c, s) for r in R]
        g = [gt1(r, c, s) for r in R]
        pooled = 100 * sum(a for a, _ in g) / sum(b for _, b in g)
        perrun = [100 * a / b for a, b in g]
        wins = [(stat(r, c, s, 'win0'), stat(r, c, s, 'req'), stat(r, c, s, 'hot_iters')) for r in R] if s != 'M100' else []
        print(f"  {s:5s} dcpu " + " ".join(f"{x:+.3f}" for x in v) + f" med {med(v):+.3f} | mean " + " ".join(f"{mean(r,c,s):.1f}" for r in R) + f" med {med([mean(r,c,s) for r in R]):.1f} | >1ms pooled {pooled:.2f}% perrun " + " ".join(f"{x:.2f}" for x in perrun) + f" n={g[0][1]} | win0/req/hot {wins}")
    print('  P mean', [mean(r, c, 'P') for r in R], 'P cpu', [round(cpu(r, c, 'P'), 3) for r in R])
