#!/usr/bin/env bash
set -u
S=/home/y/src/bpftime-x86-runs/paper-20261124/scripts/build-arm.sh
$S official /home/y/src/bpftime-paper/official-4708f409 off bpftime_text_segment_transformer bpftime-cli-cpp || echo FAIL official
$S dev /home/y/src/bpftime-paper/dev-9ccda7e8 off bpftime_text_segment_transformer bpftime-cli-cpp || echo FAIL dev
$S odiag /home/y/src/bpftime-paper/odiag-4708f409 off || echo FAIL odiag
$S l3port /home/y/src/bpftime-paper/l3port-4708f409 off || echo FAIL l3port
$S opc /home/y/src/bpftime-paper/official-4708f409 on || echo FAIL opc
$S dpc /home/y/src/bpftime-paper/dev-9ccda7e8 on || echo FAIL dpc
echo ALLDONE
