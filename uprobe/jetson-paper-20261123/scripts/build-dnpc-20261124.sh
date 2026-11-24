#!/usr/bin/env bash
set -u
R=$HOME/src/paper-runs-20261124; P=$HOME/src/bpftime-paper
chmod u+w $R/builds
$R/scripts/build-arm.sh Dnpc $P/dev-c71a9987 checks bpftime_text_segment_transformer bpftime-cli-cpp; echo "Dnpc rc=$?"
chmod a-w $R/builds
