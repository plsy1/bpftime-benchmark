#!/usr/bin/env python3
"""Reuse the audited consuming workloads with immutable formal artifacts."""
import importlib.util
from pathlib import Path
import sys

OUT=Path(__file__).resolve().parent
kind=sys.argv.pop(1)
assert kind in ('ordinary','percpu')
script=OUT.parent/'array-jit-default-20261021'/('run-ordinary.py' if kind=='ordinary' else 'run-matched.py')
spec=importlib.util.spec_from_file_location('consuming_workload',script)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.BUILD=OUT/'artifacts'
module.main()
