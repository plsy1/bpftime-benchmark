import sys; sys.path.insert(0, "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/v3chk")
from lib import *
for c in ('c05', 'c06', 'c25', 'c26', 'c27', 'c28'):
    print('==', c)
    for s in SETS[1:]:
        v = [cost(r, c, s) for r in R]
        print(f"  {s:5s} per-round " + " ".join(f"{x:6.2f}" for x in v) + f"  median {med(v):.2f}")
    fam = ['AC', 'AC2', 'AC3', 'AC4', 'AC5', 'ACn']
    for r in R:
        vals = {s: cost(r, c, s) for s in fam}
        lo = min(vals, key=vals.get)
        lo_ex5 = min((s for s in fam if s != 'AC5'), key=vals.get)
        print(f"  r{r}: lowest {lo} ({vals[lo]:.2f}); lowest excl AC5 {lo_ex5}; AC3-AC5 {vals['AC3']-vals['AC5']:+.2f}; M100 {cost(r,c,'M100'):.2f} AC3 {vals['AC3']:.2f}")
