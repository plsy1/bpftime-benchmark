#!/usr/bin/env bash
# I2 (x86): map churn on 1/2/4/8 threads; P cores 0,2,4,6 then SMT siblings 1,3,5,7 (T=8 uses both threads of each P core); 3.5 GHz.
set -euo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; C=$R/churn; H=$R/harness
SID=${1:?sid}; F=$R/raw/$SID; mkdir "$F"
PINS=(0:3500000 2:3500000 4:3500000 6:3500000 1:3500000 3:3500000 5:3500000 7:3500000)
finish() { s=$?; trap - EXIT; [ -e "$F/pin-state.txt" ] && "$H/pin-cpus.sh" restore "$F/pin-state.txt" || true; sudo -n chown -R "$(id -u):$(id -g)" "$F" 2>/dev/null || true; echo "$s" > "$F/exit-status.txt"; exit $s; }
trap finish EXIT
awk '{ if ($1 >= 0.3) { print "load too high: " $1; exit 1 } }' /proc/loadavg
(cd "$C/src" && sha256sum --check --quiet sha256.txt)
for a in official l3port olatest ol730x dnew dn730f; do (cd "$R/builds/$a" && sha256sum --check --quiet sha256.txt); done
"$H/pin-cpus.sh" pin "$F/pin-state.txt" "${PINS[@]}"
sudo -n modprobe msr
for t in $(seq 15); do sudo -n python3 "$H/busy-check.py" 2112 "${PINS[@]}" >> "$F/check-before.txt" && break; test "$t" -lt 15; done
taskset -c 12-14 python3 "$C/run_churn.py" --backends none,kernel,official,l3port,olatest,ol730x,dnew,dn730f --objs churn-k4-v8.bpf.o,control.bpf.o --threads 1,2,4,8 --reps 3 --cpus 0,2,4,6,1,3,5,7 --out "$F/churn.jsonl" > "$F/console.log" 2>&1
sudo -n python3 "$H/busy-check.py" 2112 "${PINS[@]}" > "$F/check-after.txt" || echo FAIL >> "$F/check-after.txt"
