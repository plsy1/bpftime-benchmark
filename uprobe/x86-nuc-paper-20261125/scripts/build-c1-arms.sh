#!/usr/bin/env bash
# Eval plan B1-B5 (x86): new arms for C1. Sequential; each build takes the measurement lock.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; P=/home/y/src/bpftime-paper
b() { echo "== $(date +%T) $*"; "$R/scripts/build-arm.sh" "$@" && echo "OK $1" || echo "FAIL $1 ($?)"; }
b oll3 $P/oll3-70f7bc1f off
b ol730x $P/ol730x-70f7bc1f off
# wait for the submodule fetch of the last two worktrees
until ! pgrep -f 'git.*(oll3x730|dn730)' >/dev/null && [ -d $P/dn730-c71a9987/vm/llvm-jit/src ]; do sleep 20; done
b oll3x730 $P/oll3x730-70f7bc1f off
b dn730 $P/dn730-c71a9987 off
PAPER_LAYOUT_FLAGS=-falign-functions=64 b dnr $P/dnew-c71a9987 off
PAPER_LAYOUT_FLAGS=-falign-functions=64 b oll3x730r $P/oll3x730-70f7bc1f off
b oll3pc $P/oll3-70f7bc1f on
b oll3x730pc $P/oll3x730-70f7bc1f on
echo "BUILDS-DONE $(date +%T)"
