#!/usr/bin/env bash
# E12 (and E14) prerequisites: the bpftime examples the audit runs, rebuilt CO-RE from the official
# 4708f409 tree with clang-15. The 10-26 tree (bpftime-official-177a1dc-noaffinity-20261023) is gone.
# Recipe reconstructed from ~/src/realprog-20261026-btf/build-examples.log (the 10-26 script itself was
# not found): each example is copied to a sibling "<name>-core" directory inside the official tree's
# example/ (same depth, so the Makefiles' ../../third_party paths hold; same names audit.py expects)
# and built with `make CLANG=clang-15` (`clang` is not on PATH here). Each copy builds its own libbpf
# and bootstrap bpftool from third_party (a few minutes per example: the build is serial, -j2).
# The "-core" directories are untracked additions to the official worktree, like the campaign's
# sslsniff-paper-* builds; tracked files are not touched (checked at the end).
# Not built (not needed): opensnoop-core / syscount-core (the audit uses libbpf-tools' opensnoop;
# syscount is not one of the 18). tracing/bpftrace/*.bt are scripts, used from the tree as is.
# Unchanged, reused read-only: ~/src/realprog-20261026-btf/{libbpf-tools,workloads,harness/bash_driver.py}
# and ~/src/rp/sw (memleak's static wrapper); bpftrace 0.20.2 and OpenJDK 17 from the system.
set -euo pipefail
R=$HOME/src/paper-runs-20261124
T=$HOME/src/bpftime-paper/official-4708f409
EX=$T/example
LOGD=$R/logs/E12-examples
mkdir -p "$R/logs"; mkdir "$LOGD"
[ "$(git -C "$T" rev-parse HEAD)" = "$(sed -n 's/^commit: //p' "$R/builds/O/source-state.txt")" ]
git -C "$T" status --short --untracked-files=no >"$LOGD/tracked-before.txt"
for d in malloc tracing/funclatency minimal/usdt_minimal tracing/bashreadline tracing/sslsniff \
         bpf_features/get_stack_id_example; do
    c=$EX/$d-core
    [ ! -e "$c" ] || { echo "$c exists; refusing to overwrite"; exit 1; }
    cp -r "$EX/$d" "$c"
    echo "=== $d-core" >>"$LOGD/build.log"
    nice make -C "$c" CLANG=/usr/bin/clang-15 -j2 >>"$LOGD/build.log" 2>&1
    echo "rc=$?" >>"$LOGD/build.log"
done
# The binaries audit.py runs
B=(malloc-core/malloc malloc-core/victim tracing/funclatency-core/funclatency
   minimal/usdt_minimal-core/usdt_minimal minimal/usdt_minimal-core/victim
   tracing/bashreadline-core/readline tracing/sslsniff-core/sslsniff
   bpf_features/get_stack_id_example-core/get_stack_id_example bpf_features/get_stack_id_example-core/victim
   tracing/bpftrace/bashreadline.bt tracing/bpftrace/gethostlatency.bt)
(cd "$EX" && sha256sum "${B[@]}") >"$LOGD/examples-sha256.txt"
# CO-RE relocations kept (as 10-26: .BTF.ext present)
for o in "$EX"/*-core/.output/*.bpf.o "$EX"/*/*-core/.output/*.bpf.o; do
    printf '%s ' "$o"; llvm-objdump-15 -h "$o" | grep -c 'BTF.ext' || true
done >"$LOGD/btf-ext.txt"
git -C "$T" status --short --untracked-files=no >"$LOGD/tracked-after.txt"
cmp -s "$LOGD/tracked-before.txt" "$LOGD/tracked-after.txt"
# The rest of the audit's inputs, recorded (not copied: they are the 10-26 binaries the history used)
(cd "$HOME/src/realprog-20261026-btf" && sha256sum libbpf-tools/{funclatency,gethostlatency,memleak,javagc,opensnoop,bashreadline} \
    workloads/{callloop,mallocloop,openloop,resolveloop,sslloop,staticwrap,GcLoop.class} harness/bash_driver.py) >"$LOGD/realprog-inputs-sha256.txt"
sha256sum "$HOME/src/rp/sw" >>"$LOGD/realprog-inputs-sha256.txt"
{ bpftrace --version; java -version 2>&1 | head -1; } >"$LOGD/tools-versions.txt"
cp "$LOGD/examples-sha256.txt" "$LOGD/realprog-inputs-sha256.txt" "$R/prep/E12/" 2>/dev/null || true
cat "$LOGD/examples-sha256.txt"
echo "E12 examples built"
