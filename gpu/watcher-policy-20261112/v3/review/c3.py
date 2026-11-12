import sys; sys.path.insert(0, "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/v3chk")
from lib import *
n5 = 0
for c in ('c05', 'c06', 'c25', 'c26', 'c27', 'c28'):
    print('==', c)
    for s in ('AC', 'AC2', 'AC3', 'AC4', 'AC5', 'ACn', 'M100'):
        v = [dcpu(r, c, s) for r in R]
        print(f"  {s:5s} dcpu " + " ".join(f"{x:+.4f}" for x in v) + f" median {med(v):+.4f}")
    for s in ('AC2', 'AC3'):
        dd = [dcpu(r, c, s) - dcpu(r, c, 'AC') for r in R]
        print(f"  {s}-AC per round " + " ".join(f"{x:+.4f}" for x in dd) + f"  diff of medians {med([dcpu(r,c,s) for r in R]) - med([dcpu(r,c,'AC') for r in R]):+.4f}  median of diffs {med(dd):+.4f}")
    n5 += sum(cost(r, c, 'AC5') < cost(r, c, 'AC3') for r in R)
print('AC5 < AC3 cell-rounds', n5)
