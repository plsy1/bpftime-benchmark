#!/usr/bin/env bash
# Builds for the Orin combined round on the new base (official 70f7bc1f).
set -u
R=$HOME/src/paper-runs-20261124; P=$HOME/src/bpftime-paper
chmod u+w $R/builds
for spec in "Ol official-70f7bc1f bench X" "Olpc official-70f7bc1f checks X" "Dn dev-c71a9987 bench X" "Dxn dexact-c71a9987 checks -"; do
  set -- $spec
  echo "=== $(date +%T) $1"
  if [ "$4" = X ]; then $R/scripts/build-arm.sh $1 $P/$2 $3 bpftime_text_segment_transformer bpftime-cli-cpp; else $R/scripts/build-arm.sh $1 $P/$2 $3; fi; echo "$1 rc=$?"
done
chmod a-w $R/builds
