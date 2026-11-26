#!/usr/bin/env bash
# J4 builds (ATC week 1): Dpcr = 9ccda7e8 checks ON rebuilt in a fresh build dir (same options as Dpc);
# DnR = c71a9987 bench + -falign-functions=64 (layout control, PREREG-2 §1). One at a time.
set -u
R=$HOME/src/paper-runs-20261124; P=$HOME/src/bpftime-paper
chmod u+w $R/builds
echo "=== $(date +%T) Dpcr"; BDIR_TAG=rebuild-20261125 $R/scripts/build-arm2.sh Dpcr $P/dev-9ccda7e8 checks; echo "Dpcr rc=$?"
echo "=== $(date +%T) DnR"; BDIR_TAG=align64-20261125 EXTRA_CMAKE="-DCMAKE_C_FLAGS=-falign-functions=64 -DCMAKE_CXX_FLAGS=-falign-functions=64" $R/scripts/build-arm2.sh DnR $P/dev-c71a9987 bench bpftime_text_segment_transformer bpftime-cli-cpp; echo "DnR rc=$?"
chmod a-w $R/builds
echo "=== $(date +%T) done"
