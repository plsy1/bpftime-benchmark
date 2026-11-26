#!/usr/bin/env bash
# Paper campaign, Jetson: session guard (plan 2.0.1).
# Usage: check_env.sh start <logdir> <expected-minutes>
#        check_env.sh end   <logdir>
# Exit non-zero on any failed check. Writes its evidence into <logdir>.
set -euo pipefail
MODE=${1:?start|end}; LOG=${2:?logdir}; MIN=${3:-0}
R=$HOME/src/paper-runs-20261124
mkdir -p "$LOG"
fail() { echo "GUARD FAIL ($MODE): $*" | tee -a "$LOG/guard-$MODE.txt" >&2; exit 1; }
note() { echo "$*" >>"$LOG/guard-$MODE.txt"; }
note "$(date --iso-8601=seconds) uptime_s=$(cut -d' ' -f1 /proc/uptime)"
# 11-25: read the load before this guard's own sha256 pass (hashing the builds raised it past 0.3:
# J1 refused 3 times at 0.37-0.75 right after waitload passed at < 0.15; likely also E18a 11-24 16:25).
read -r L1_PRE _ </proc/loadavg

# clocks: both clusters pinned at 1728000 (min = max = cur)
for c in 0 5; do
  f=/sys/devices/system/cpu/cpu$c/cpufreq
  v="$(cat $f/scaling_min_freq) $(cat $f/scaling_max_freq) $(cat $f/scaling_cur_freq)"
  note "cpu$c min/max/cur: $v"
  [ "$v" = "1728000 1728000 1728000" ] || fail "cpu$c clocks $v"
done
sudo -n jetson_clocks --show >"$LOG/jetson_clocks-$MODE.txt" 2>&1 || fail "jetson_clocks --show"

# leftover measured processes, by executable path (not argv), excluding this script's ancestry
python3 - "$LOG/guard-$MODE.txt" <<'PY' || fail "leftover processes"
import os, sys
anc, p = set(), os.getpid()
while p > 1:
    anc.add(p)
    p = int(open(f'/proc/{p}/stat').read().rsplit(')', 1)[1].split()[1])
bad = []
for pid in os.listdir('/proc'):
    if not pid.isdigit() or int(pid) in anc: continue
    try: exe = os.readlink(f'/proc/{pid}/exe')
    except OSError: continue
    b = os.path.basename(exe)
    if b in ('nginx', 'wrk', 'test', 'uprobe', 'memleak', 'bpftrace', 'java', 'llama-cli', 'llama-bench', 'llama-server') \
       or b.startswith(('sslsniff', 'bpftime', 'run_meas')):
        bad.append((pid, exe))
    try: cmd = open(f'/proc/{pid}/cmdline', 'rb').read().replace(b'\0', b' ').decode(errors='replace')
    except OSError: continue
    if b.startswith('python') and ('run_meas' in cmd or 'llama' in cmd):
        bad.append((pid, cmd[:120]))
open(sys.argv[1], 'a').write(f'leftovers: {bad}\n')
sys.exit(1 if bad else 0)
PY

# bpftime shm segments
left=$(find /dev/shm -mindepth 1 -name 'bpftime*' 2>/dev/null)
[ -z "$left" ] || fail "bpftime shm left: $left"

# frozen artifacts unchanged
for d in "$R"/builds/*/ "$R/sslsniff/" "$R/bench/"; do
  (cd "$d" && sha256sum --check --quiet sha256.txt) || fail "sha256 mismatch in $d"
done
note "sha256 ok"

# thermal
for z in /sys/class/thermal/thermal_zone*; do note "thermal $(cat $z/type)=$(cat $z/temp 2>/dev/null)"; done

if [ "$MODE" = start ]; then
  l1=$L1_PRE
  read -r l1_after _ </proc/loadavg
  note "loadavg1=$l1 (before sha256; after sha256 $l1_after)"
  awk -v l="$l1" 'BEGIN{exit !(l < 0.3)}' || fail "load $l1 >= 0.3"
  ! sudo -n docker ps -a --format '{{.Names}}' | grep -qx paper-jetson || fail "container paper-jetson exists"
  # timers that must not fire within the session window +-20 min
  now=$(date +%s); lo=$((now - 1200)); hi=$((now + MIN * 60 + 1200))
  systemctl list-timers --all --no-pager >"$LOG/timers-start.txt" 2>&1 || true
  for t in apt-daily.timer apt-daily-upgrade.timer man-db.timer fstrim.timer e2scrub_all.timer; do
    for prop in NextElapseUSecRealtime LastTriggerUSec; do
      v=$(systemctl show "$t" -p $prop --value 2>/dev/null || true)
      [ -n "$v" ] && [ "$v" != n/a ] || continue
      ts=$(date -d "$v" +%s 2>/dev/null || echo 0)
      if [ "$ts" -ge "$lo" ] && [ "$ts" -le "$hi" ]; then fail "$t $prop=$v inside window"; fi
    done
  done
  note "timers ok for ${MIN} min"
  # other Claude sessions' load is recorded, not refused
  ps -eo pid,etime,rss,pcpu,comm --sort=-pcpu | head -15 >"$LOG/top-start.txt"
fi
free -m >"$LOG/free-$MODE.txt"
note "guard $MODE ok"
echo "guard $MODE ok"
