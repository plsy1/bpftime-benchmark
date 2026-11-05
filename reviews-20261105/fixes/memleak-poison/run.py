# memleak on mallocloop (tool started first, workload run with the agent, as
# uprobe/memleak-fix-20261104/detach-manual), 5 s, per build: does the agent
# disable memleak's tracepoint programs for libbpf's CO-RE poison (call
# 195896080)? The agent logs to the workload's output.
import re, sys
from pathlib import Path
sys.path.insert(0, '/home/jetson/src/realprog-20261026-btf/harness')
import audit
HERE = Path(__file__).resolve().parent
for backend in sys.argv[1].split(','):
    audit.cleanup(backend)
    rc, wout, tout = audit.traced([str(audit.LT / 'memleak')],
                                  [str(audit.WL / 'mallocloop'), '5', '64', '1024'],
                                  backend)
    audit.cleanup(backend)
    (HERE / f'{backend}.workload.txt').write_text(wout)
    (HERE / f'{backend}.tool.txt').write_text(tout)
    poison = wout.count('_bpf_helper_ext_195896080')
    disabled = sorted(set(re.findall(r'JIT compilation of (\S+) failed', wout)))
    print(f'{backend}: workload rc {rc}, "_bpf_helper_ext_195896080" {poison}, '
          f'disabled programs {disabled}')
