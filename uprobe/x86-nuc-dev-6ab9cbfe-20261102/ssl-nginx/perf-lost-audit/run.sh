#!/usr/bin/env bash
# x86 NUC, 11/02: perf event loss re-audit asked for by the Jetson re-audit
# (benchmark-results/jetson efad8076, uprobe/perf-lost-audit-20261102): more rounds, with the
# kernel leg in the same audit. Legs kernel, official, a7d54d7e, dev-6ab9cbfe; the leg order
# rotates every round. Each round is one `run_fair_stderr.py --audit` (../audit-repeat/, keeps
# sslsniff's stderr; wrk -c 10 -d 3 as the audit). 16 B: 16 rounds; 256 KB: 6 rounds.
# Same container image, frozen builds, pinning and frequencies as ../run.sh; no timing claims.
# sslsniff stdout captures stay in $RAW/captures/perf-lost-audit (not committed); the json and
# stderr files are copied to out/ at the end.
set -euo pipefail
R=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-6ab9cbfe-20261102/ssl-nginx
A=$R/perf-lost-audit
RAW=/home/y/src/bpftime-x86-runs/dev-6ab9cbfe-20261102/ssl-nginx
B=$RAW/builds
CAP=$RAW/captures/perf-lost-audit
IMAGE=bpftime-ssl-nginx:x86-20261026
NAME=ssl-x86-6ab9-lost
PINS=(2:3500000 4:3500000 8:2500000 9:2500000)
TSC=2112
LEGS=(kernel official a7d54d7e dev-6ab9cbfe)
finish()
{
    status=$?
    trap - EXIT
    sudo -n docker rm -f "$NAME" >>"$A/cleanup.log" 2>&1 || true
    if [ -e "$A/pin-state.txt" ]; then "$R/pin-cpus.sh" restore "$A/pin-state.txt" || status=99; fi
    "$R/pin-cpus.sh" show 2 4 8 9 >"$A/pin-restored.txt" 2>&1 || true
    printf '%s\n' "$status" >"$A/exit-status.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$A/started-at.txt"
mkdir "$CAP"
sha256sum --check --quiet "$RAW/artifact-sha256.txt"
! sudo -n docker ps -a --format '{{.Names}}' | grep -qx "$NAME"
"$R/pin-cpus.sh" pin "$A/pin-state.txt" "${PINS[@]}"
for try in $(seq 15); do
    sudo -n python3 "$R/busy-check.py" "$TSC" "${PINS[@]}" >>"$A/check-before.txt" && break
    test "$try" -lt 15
done
sudo -n docker run -d --name "$NAME" --privileged --pid=host --shm-size=1g -v /sys:/sys \
    -v /lib/modules:/lib/modules:ro -v "$R":/bench -v "$B":/builds:ro -v "$RAW/raw/sslsniff":/sslsniff:ro \
    -v "$CAP":/cap "$IMAGE" sleep infinity >"$A/container-id.txt"
audit()  # <set name> <payload> <round>
{
    local k=$(( ($3 - 1) % ${#LEGS[@]} ))
    local order=("${LEGS[@]:k}" "${LEGS[@]:0:k}")
    local legs; legs=$(IFS=,; echo "${order[*]}")
    echo "$1 round $3 order $legs $(date --iso-8601=seconds)" >>"$A/rounds.txt"
    sudo -n timeout 900 docker exec -e NGINX_CPUS=2 -e SNIFF_CPUS=4 -e WRK_CPUS=8,9 "$NAME" \
        taskset -c 10 python3 /bench/audit-repeat/run_fair_stderr.py --payload "$2" --legs "$legs" \
        --audit --out "/cap/$1-r$3.json" >"$A/$1-r$3.log" 2>&1
}
for i in $(seq 16); do audit a16 16 "$i"; done
for i in $(seq 6); do audit a256k 262144 "$i"; done
mkdir "$A/out"
cp "$CAP"/*.json "$CAP"/*.stderr "$A/out/"
date --iso-8601=seconds >"$A/finished-at.txt"
