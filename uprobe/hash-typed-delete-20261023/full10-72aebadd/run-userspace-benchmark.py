#!/usr/bin/env python3
"""Run benchmark/uprobe/benchmark.py with the kernel arm replaced by a skip
marker. Kernel BPF is unchanged by runtime commits; its numbers come from the
previous complete run (see compare-full.py). Run from full-runner/bpftime."""
import asyncio
import importlib.util
import sys

spec = importlib.util.spec_from_file_location('benchmark', 'benchmark/uprobe/benchmark.py')
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


async def skip_kernel(num_runs=10):
    bench.log_message('Kernel uprobe tests not run (unchanged; compared against the previous complete run)')
    return {'skipped': 'kernel arm not run'}

bench.run_kernel_uprobe_test = skip_kernel
sys.argv = ['benchmark.py', *sys.argv[1:]]
asyncio.run(bench.main())
