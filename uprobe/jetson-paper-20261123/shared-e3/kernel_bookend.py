#!/usr/bin/env python3
"""Kernel-only group of benchmark.py (plan E3 bookend). Run from a runner's bpftime dir.
Usage: kernel_bookend.py <iters> <out.json>"""
import asyncio, importlib.util, json, sys
spec = importlib.util.spec_from_file_location('bench', 'benchmark/uprobe/benchmark.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
k = asyncio.run(m.run_kernel_uprobe_test(int(sys.argv[1])))
json.dump({'kernel_uprobe': k}, open(sys.argv[2], 'w'), indent=1)
assert len(k) == 17, len(k)
