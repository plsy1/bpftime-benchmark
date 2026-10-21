#!/usr/bin/env python3
"""Reuse the audited consuming workloads on the 177a1dc baseline.

Arms: same-run kernel; "previous" = frozen c0939bd artifacts from
array-jit-default-delete-fix-20261021 (8da867d541f8 base, default-on JIT, the
obsolete enable variable it sets is ignored); "inline" = this baseline's
frozen artifacts. All arms use the workload binaries built from this tree.
"""
import importlib.util
from pathlib import Path
import sys

OUT=Path(__file__).resolve().parent
ROOT=Path('/home/jetson/src/bpftime-baseline-177a1dc-20261021')
kind=sys.argv.pop(1)
assert kind in ('ordinary','percpu')
script=OUT.parent/'array-jit-default-20261021'/('run-ordinary.py' if kind=='ordinary' else 'run-matched.py')
spec=importlib.util.spec_from_file_location('consuming_workload',script)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.ROOT=ROOT
module.BUILD=OUT/'artifacts'
module.PREVIOUS=OUT.parent/'array-jit-default-delete-fix-20261021'/'artifacts'
module.DIAG=ROOT/'benchmark/uprobe/.output'/('array-jit-poc' if kind=='ordinary' else 'percpu-array-jit')
module.main()
