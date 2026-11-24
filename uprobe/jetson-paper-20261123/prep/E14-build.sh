#!/usr/bin/env bash
# E14 prerequisites: none of its own. measure.py reuses E12's audit copy (prep/E12/audit.py) and the
# official example/*-core builds made by prep/E12-build.sh. This only checks they are in place.
set -euo pipefail
R=$HOME/src/paper-runs-20261124
EXL=$R/logs/E12-examples
for f in "$EXL/examples-sha256.txt" "$EXL/realprog-inputs-sha256.txt" "$R/prep/E12/audit.py" "$R/prep/E14/measure.py"; do
    [ -f "$f" ] || { echo "missing $f: run prep/E12-build.sh first"; exit 1; }
done
(cd "$HOME/src/bpftime-paper/official-4708f409/example" && sha256sum --check --quiet "$EXL/examples-sha256.txt")
echo "E14 prerequisites in place"
