#!/usr/bin/env bash
set -euo pipefail

CODE_ROOT=${CODE_ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${1:-source.patch}
FILES=(
  benchmark/uprobe/Makefile
  benchmark/uprobe/diagnostics/helper_map_closure.bpf.c
  benchmark/uprobe/diagnostics/helper_map_closure.c
  benchmark/uprobe/diagnostics/helper_map_closure_victim.c
  benchmark/uprobe/diagnostics/helper_hash_closure.bpf.c
  benchmark/uprobe/diagnostics/helper_hash_closure.c
  benchmark/uprobe/diagnostics/helper_hash_closure_victim.c
)

cd "$CODE_ROOT"
: >"$OUT"
for file in "${FILES[@]}"; do
  if git ls-files --error-unmatch "$file" >/dev/null 2>&1; then
    git diff -- "$file" >>"$OUT"
  else
    git diff --no-index -- /dev/null "$file" >>"$OUT" || test $? -eq 1
  fi
done
