#!/usr/bin/env bash
# Pin several CPUs at fixed frequencies (turbo on, min = max, powersave governor,
# idle states C6 and deeper disabled on each CPU and its SMT sibling), or restore.
# Same method as ../x86-nuc-full10-20261026/freq.sh, extended to several CPUs.
# Usage: pin-cpus.sh pin <state-file> <cpu>:<khz>... | restore <state-file> | show <cpu>...
set -euo pipefail
P=/sys/devices/system/cpu
w() { echo "$2" | sudo -n tee "$1" >/dev/null; }
# thread_siblings_list is "2-3", "2,3" or "8"; print every thread of the core.
threads() { tr ',-' '\n\n' <$P/cpu$1/topology/thread_siblings_list; }
show() {
    echo "no_turbo=$(cat $P/intel_pstate/no_turbo)"
    for c in "$@"; do
        for f in scaling_governor energy_performance_preference scaling_min_freq scaling_max_freq; do
            echo "cpu$c.$f=$(cat $P/cpu$c/cpufreq/$f)"
        done
        for t in $(threads "$c"); do
            for d in $P/cpu$t/cpuidle/state*; do echo "cpu$t.idle.${d##*/}.disable=$(cat $d/disable)"; done
        done
    done
}
case $1 in
pin)
    state=$2; shift 2
    test ! -e "$state"
    cpus=(); for a in "$@"; do cpus+=("${a%%:*}"); done
    { echo "cpus=${cpus[*]}"; show "${cpus[@]}"; } >"$state"
    for a in "$@"; do
        c=${a%%:*} khz=${a##*:}
        if [ "$khz" -ge "$(cat $P/cpu$c/cpufreq/scaling_min_freq)" ]; then
            w $P/cpu$c/cpufreq/scaling_max_freq "$khz"; w $P/cpu$c/cpufreq/scaling_min_freq "$khz"
        else
            w $P/cpu$c/cpufreq/scaling_min_freq "$khz"; w $P/cpu$c/cpufreq/scaling_max_freq "$khz"
        fi
        test "$(cat $P/cpu$c/cpufreq/scaling_min_freq)" = "$khz"
        test "$(cat $P/cpu$c/cpufreq/scaling_max_freq)" = "$khz"
        for t in $(threads "$c"); do
            for d in $P/cpu$t/cpuidle/state*; do [ "${d##*state}" -ge 2 ] && w $d/disable 1; done
        done
    done
    ;;
restore)
    state=$2
    get() { sed -n "s/^$1=//p" "$state"; }
    read -ra cpus <<<"$(get cpus)"
    for c in "${cpus[@]}"; do
        for t in $(threads "$c"); do
            for d in $P/cpu$t/cpuidle/state*; do w $d/disable "$(get "cpu$t.idle.${d##*/}.disable")"; done
        done
        w $P/cpu$c/cpufreq/scaling_min_freq "$(get cpu$c.scaling_min_freq)"
        w $P/cpu$c/cpufreq/scaling_max_freq "$(get cpu$c.scaling_max_freq)"
        w $P/cpu$c/cpufreq/scaling_governor "$(get cpu$c.scaling_governor)"
        w $P/cpu$c/cpufreq/energy_performance_preference "$(get cpu$c.energy_performance_preference)"
    done
    w $P/intel_pstate/no_turbo "$(get no_turbo)"
    ;;
show) shift; show "$@" ;;
esac
