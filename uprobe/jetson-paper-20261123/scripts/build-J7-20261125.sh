#!/usr/bin/env bash
# J7 builds (ATC week 1, NUC patches handoff-J7-20261125, sha256 e1a440a4...). One at a time, each with transformer + CLI
# like Ol/Olpc. Worktrees made by git worktree add --no-checkout + copy of the base tree + git am (logs/worktrees-J7-20261125.log).
set -u
R=$HOME/src/paper-runs-20261124; P=$HOME/src/bpftime-paper
chmod u+w $R/builds
for spec in "OlL3 oll3-70f7bc1f bench" "Ol730x ol730x-70f7bc1f bench" "OlL3x730 oll3x730-70f7bc1f bench" "Dn730 dn730-c71a9987 bench" "OlL3x730pc oll3x730-70f7bc1f checks" "OlL3pc oll3-70f7bc1f checks"; do
  set -- $spec
  echo "=== $(date +%T) $1"
  $R/scripts/build-arm2.sh $1 $P/$2 $3 bpftime_text_segment_transformer bpftime-cli-cpp; echo "$1 rc=$?"
done
chmod a-w $R/builds
echo "=== $(date +%T) done"
