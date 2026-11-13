import sys
P = sys.argv[1]
s = open(P).read()
def rep(a, b):
    global s
    assert s.count(a) == 1, (s.count(a), a[:80])
    s = s.replace(a, b)
rep("""If the P40 run used a copy of bpftime's example, it made a CPU ARRAY lookup and a printk per thread, which would be the only published real-application cost of direction-2 operations.""",
"""If the P40 run used a copy of bpftime's example, it made a printk per thread (and, on bpftime versions where an ordinary ARRAY lookup was still a host call, one more per thread). That would be the only published real-application cost of direction-2 operations.""")
s = s.rstrip('\n') + """

## Files in this folder

- `README.md`: this report.
- `workflow-output.json`: raw workflow output (two readers, two verifiers, synthesis). `scripts/survey-workflow.js`: the workflow.
- `listings/`: `cuobjdump --list-ptx` per vendor library on this board, and `summary.txt` from `scripts/vendor_summary.py`.
- `nsys-check/`: the 11-13 check that Nsight Systems records kernels here (`README.md`, `fin.cu`).
- `scripts/patch_readme.py`, `scripts/patch_readme2.py`: the edits that applied the late verifier corrections to the synthesizer's text.
"""
open(P, 'w').write(s)
print('saved')
