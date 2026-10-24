#!/usr/bin/env python3
"""Build diagnostic copies with this accepted build's generated toolchain flags."""
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parent
build = Path('/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate/build-uprobe-handler-fast-access-20261016/benchmark')
(root/'layers/bin').mkdir(exist_ok=True)
(root/'layers/obj').mkdir(exist_ok=True)
for target, source in [('array-map-path-layers', 'array_map_path_layers.cpp'), ('per-cpu-array-path-layers', 'per_cpu_array_path_layers.cpp')]:
    generated = build/f'CMakeFiles/{target}.dir'
    flags = {}
    for line in (generated/'flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    obj = root/'layers/obj'/f'{target}.o'
    compile_cmd = ['/usr/bin/g++-13', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'], '-c', str(root/'layers'/source), '-o', str(obj)]
    print(shlex.join(compile_cmd), flush=True)
    subprocess.run(compile_cmd, cwd=build, check=True)
    link = shlex.split((generated/'link.txt').read_text())
    found = 0
    for i, arg in enumerate(link):
        if arg.endswith('.cpp.o'):
            link[i] = str(obj)
            found += 1
    assert found == 1, found
    link[link.index('-o')+1] = str(root/'layers/bin'/target)
    print(shlex.join(link), flush=True)
    subprocess.run(link, cwd=build, check=True)
