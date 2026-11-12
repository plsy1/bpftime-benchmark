# Per-round values per cell and set, both regimes, with residency medians (resid.py: whole victim
# lifetime, start-up included; emc_floor = bwmgr's requested floor, not the EMC rate). Run from the
# policy work directory.
import runpy, sys, os, io, contextlib, statistics as st
sys.path.insert(0, ".")
from resid import resid
SETS="P W D AC3 M100 AC2 AC4".split()
for k in ("on","off"):
    os.environ["CLOCKS"]=k; os.environ["SETS"]=" ".join(SETS); os.environ["ROUNDS_FILE"]="rounds7.sh"
    sys.argv=["summarize.py",f"logs/dvfs/{k}"]
    with contextlib.redirect_stdout(io.StringIO()):
        g=runpy.run_path("summarize.py")
    rows=g["rows"]; CELLS=g["CELLS"]
    print(f"===== {k}")
    for cn,c in CELLS.items():
        Pm={x["r"]:x for x in rows[(cn,"P")]}
        print(f"-- {cn}")
        for s in SETS:
            R=sorted(rows[(cn,s)],key=lambda x:x["r"])
            cpu=[x["cpu"]-Pm[x["r"]]["cpu"] for x in R]
            pw=[x["pw"][1] for x in R if x["pw"]]
            cv=[x["pw"][0] for x in R if x["pw"]]
            extra=""
            if c["threads"]>1: extra=" cost/call "+",".join(f"{(x['mean']-Pm[x['r']]['mean'])/c['threads']:.1f}" for x in R)
            if c["load"]!="off": extra+=" load "+",".join(f"{x['rate']/Pm[x['r']]['rate']:.3f}" for x in R)
            if c["probe"]=="host" and c["threads"]==1 and s!="P":
                extra+=" >1ms% "+",".join(f"{100*sum(v>1000 for v in x['lat'])/max(1,len(x['lat'])):.2f}" for x in R)
                extra+=" med "+",".join(f"{st.median(x['lat']):.1f}" for x in R if x['lat'])
            rs=[resid(f"logs/dvfs/{k}/r{x['r']}/{cn}_{s}") for x in R]
            m=lambda key: st.median(r[key] for r in rs)
            print(f"  {s:5} dCPU {','.join(f'{v:+.3f}' for v in cpu)} VIN {','.join(f'{v:.0f}' for v in pw)} CV {','.join(f'{v:.0f}' for v in cv)}{extra} | p0 {m('policy0_mhz'):.0f} p4 {m('policy4_mhz'):.0f} p0max {m('policy0_atmax'):.2f} p4max {m('policy4_atmax'):.2f} c7 {m('c7_share'):.2f} wfi {m('wfi_share'):.2f} c7/s {m('c7_per_s'):.0f} gpu {m('gpu_mhz'):.0f} emc_floor {m('emc_floor_mhz'):.0f}")
