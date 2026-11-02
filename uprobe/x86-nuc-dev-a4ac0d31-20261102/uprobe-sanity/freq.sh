#!/usr/bin/env bash
# Pin one P-core thread for measurement (turbo off, min = max, idle states
# C6 and deeper disabled on it and its SMT sibling), or restore it.
# Without the idle-state step the core came out of C6/C8/C10 at 1.4 GHz and
# took ~6 s to reach the requested 1.7 GHz (HWP request was min = max = 22).
# Usage: freq.sh pin <cpu> <khz> <state-file> [turbo-off|turbo-on]
#        freq.sh restore <cpu> <state-file> | freq.sh show <cpu>
# turbo-on keeps turbo enabled and pins min = max at a turbo frequency.
set -euo pipefail
P=/sys/devices/system/cpu
# thread_siblings_list is "2-3" or "2,3"; print the other thread.
sibling() { tr ',-' '\n\n' <$P/cpu$1/topology/thread_siblings_list | grep -vx "$1"; }
show() {
    local c=$1 s
    echo "no_turbo=$(cat $P/intel_pstate/no_turbo)"
    for s in $c $(sibling "$c"); do
        for d in $P/cpu$s/cpuidle/state*; do
            echo "cpu$s.idle.${d##*/}.$(cat $d/name).disable=$(cat $d/disable)"
        done
    done
    for f in scaling_governor energy_performance_preference scaling_min_freq scaling_max_freq scaling_cur_freq; do
        echo "cpu$c.$f=$(cat $P/cpu$c/cpufreq/$f)"
    done
}
case $1 in
pin)
    c=$2 khz=$3 state=$4 mode=${5:-turbo-off}
    test ! -e "$state"
    show "$c" >"$state"
    case $mode in
    turbo-off) nt=1 ;;
    turbo-on) nt=0 ;;
    *) echo "bad mode $mode" >&2; exit 2 ;;
    esac
    echo "$nt" | sudo -n tee $P/intel_pstate/no_turbo >/dev/null
    # Keep the powersave governor: with turbo off, switching to performance
    # put cpu2 at 1.4 GHz instead of 1.7 (seen 10/26 on this i5-1240P, 6.12).
    echo "$khz" | sudo -n tee $P/cpu$c/cpufreq/scaling_max_freq >/dev/null
    echo "$khz" | sudo -n tee $P/cpu$c/cpufreq/scaling_min_freq >/dev/null
    for s in $c $(sibling "$c"); do
        for d in $P/cpu$s/cpuidle/state*; do
            [ "${d##*state}" -ge 2 ] && echo 1 | sudo -n tee $d/disable >/dev/null
        done
    done
    test "$(cat $P/cpu$c/cpufreq/scaling_min_freq)" = "$khz"
    test "$(cat $P/cpu$c/cpufreq/scaling_max_freq)" = "$khz"
    test "$(cat $P/intel_pstate/no_turbo)" = "$nt"
    ;;
restore)
    c=$2 state=$3
    get() { sed -n "s/^$1=//p" "$state"; }
    for s in $c $(sibling "$c"); do
        for d in $P/cpu$s/cpuidle/state*; do
            echo "$(get "cpu$s.idle.${d##*/}.$(cat $d/name).disable")" | sudo -n tee $d/disable >/dev/null
        done
    done
    echo "$(get cpu$c.scaling_governor)" | sudo -n tee $P/cpu$c/cpufreq/scaling_governor >/dev/null
    echo "$(get cpu$c.energy_performance_preference)" | sudo -n tee $P/cpu$c/cpufreq/energy_performance_preference >/dev/null
    echo "$(get cpu$c.scaling_min_freq)" | sudo -n tee $P/cpu$c/cpufreq/scaling_min_freq >/dev/null
    echo "$(get no_turbo)" | sudo -n tee $P/intel_pstate/no_turbo >/dev/null
    echo "$(get cpu$c.scaling_max_freq)" | sudo -n tee $P/cpu$c/cpufreq/scaling_max_freq >/dev/null
    ;;
show) show "$2" ;;
esac
