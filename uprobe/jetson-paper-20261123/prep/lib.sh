# Shared pieces of the E10-E14 run scripts (sourced, not executed). Modelled on scripts/run-e3.sh.
# The caller sets: SID (session id), MIN (guard minutes), KILL_NAMES (process names to kill on
# exit), ARMS_USED (frozen arms whose bpftimetool removes the shm on exit).
# Provides: R, F (output dir, created here; refuses an existing one), LOG, start_session, step,
# budget N (sets B = min(N, seconds left before the session deadline); exits 97 when < 60 s are left).
# The deadline is start + MIN minutes - 120 s (room for the end guard), so the whole session stays inside
# the window check_env.sh vetted. No outer timeout around the script: a timeout --kill-after would
# SIGKILL past the EXIT trap and skip finish().
R=$HOME/src/paper-runs-20261124
P=$R/prep
F=$R/raw/$SID
mkdir "$F"            # refuses to overwrite: fails if the directory exists (set -e)
LOG=$F/log; mkdir "$LOG"
CLAUDE_PIDS=()
export PYTHONDONTWRITEBYTECODE=1
# Plan 1.5: these stay unset in every leg.
unset BPFTIME_VM_NAME BPFTIME_DISABLE_JIT BPFTIME_SYSCALL_LAZY BPFTIME_HASH_ZERO_SEED \
      BPFTIME_MAP_PRODUCTION_AB BPFTIME_ARRAY_JIT_INLINE BPFTIME_AOT_CACHE LD_PRELOAD

finish()
{
    status=$?
    trap - EXIT
    # An outer timeout cannot reach the sudo'd children started in their own sessions, and Python's
    # finally does not run on SIGTERM: kill by name, then remove the bpftime shm with each arm's tool.
    for n in ${KILL_NAMES:-}; do sudo -n pkill -9 -x "$n" >>"$LOG/cleanup.log" 2>&1 || true; done
    sleep 0.5
    for a in ${ARMS_USED:-D}; do sudo -n "$R/builds/$a/bpftimetool" remove >>"$LOG/cleanup.log" 2>&1 || true; done
    for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 0-5 "$p" >/dev/null 2>&1 || true; done
    sudo -n chown -R "$(id -u):$(id -g)" "$F" 2>/dev/null || true
    "$R/harness/check_env.sh" end "$LOG" >>"$LOG/check-end.txt" 2>&1 || { echo "end guard FAILED" >>"$LOG/warnings.txt"; status=98; }
    printf '%s\n' "$status" >"$LOG/exit-status.txt"
    date --iso-8601=seconds >"$LOG/finished-at.txt"
    exit "$status"
}

start_session()   # guard, record scripts, move this Claude session's process tree to cpu2
{
    trap finish EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    date --iso-8601=seconds >"$LOG/started-at.txt"
    DEADLINE=$(( $(date +%s) + MIN * 60 - 120 ))
    "$R/harness/check_env.sh" start "$LOG" "$MIN"
    { uname -r; cat /proc/cmdline; } >"$LOG/kernel.txt"
    (cd "$P" && sha256sum lib.sh ./*-run.sh ./*-build.sh $(find E1? -type f -name '*.py' -o -type f -name '*.sh' | sort)) >"$LOG/prep-sha256.txt" 2>/dev/null || true
    for a in ${ARMS_USED:-}; do cat "$R/builds/$a/source-state.txt" | head -2 >>"$LOG/arms.txt"; done
    local p=$$ c
    while [ "$p" -gt 1 ]; do
        c=$(cat /proc/$p/comm 2>/dev/null || true)
        case $c in claude|node|2.1.*|ccd-cli*) CLAUDE_PIDS+=("$p");; esac
        p=$(awk '{print $4}' /proc/$p/stat)
    done
    for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 2 "$p" >>"$LOG/claude-taskset.txt" 2>&1 || true; done
    taskset -pc 2 $$ >/dev/null
}

budget()   # N: B = min(N, time left before DEADLINE); refuse the step when under 60 s are left
{
    local left=$(( DEADLINE - $(date +%s) ))
    [ "$left" -ge 60 ] || { echo "$(date +%T) no time left for the next step (deadline)" >>"$LOG/steps.txt"; exit 97; }
    B=$(( $1 < left ? $1 : left ))
}

thermal() { for z in /sys/class/thermal/thermal_zone*; do printf "%s=%s " "$(cat $z/type)" "$(cat $z/temp 2>/dev/null)"; done; echo; }
step() { echo "$(date +%T) $* | $(thermal)" >>"$LOG/steps.txt"; }

check_workloads()  # dir: verify the frozen workload copies (written by the build script)
{
    (cd "$1" && sha256sum --check --quiet sha256.txt) || { echo "workload sha256 mismatch in $1"; exit 1; }
}

assert_clean()     # nothing of ours left between steps
{
    test -z "$(find /dev/shm -mindepth 1 -name 'bpftime*' 2>/dev/null)" || { echo "bpftime shm left"; exit 1; }
}
