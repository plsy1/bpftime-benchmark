import sys; sys.path.insert(0, "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/v3chk")
from lib import *
K = ('req', 'win0', 'win20', 'win80', 'win300', 'hot_iters')
for c in ('c05', 'c06', 'c25', 'c26', 'c27', 'c28'):
    print('==', c)
    for s in ('AC', 'AC2', 'AC3', 'AC4', 'AC5', 'ACn'):
        for r in R:
            print(f"  {s:4s} r{r} " + " ".join(f"{k}={stat(r,c,s,k)}" for k in K))
