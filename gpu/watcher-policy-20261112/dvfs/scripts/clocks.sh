#!/usr/bin/env bash
# Clock regimes for the watcher-policy rounds: clocks.sh on|off|check-on|check-off|snapshot FILE
#   on:  `jetson_clocks` (CPU/GPU/EMC minimum = maximum, CPU idle states disabled, GPU rail gating
#        off) plus the GPU devfreq governor `performance`: the state every earlier round ran in.
#   off: the values written below, taken from the board's boot scripts where they set them
#        (/etc/systemd/nvpower.sh: CPU floor 729600 kHz, GPU governor nvhost_podgov and its
#        parameters) and otherwise the lowest available frequency (GPU 306 MHz, EMC 204 MHz),
#        CPU idle states enabled. GPU rail gating stays off in both: this board refuses it
#        ("Railgating is not supported" from nvgpu on writing 1). Not proven to
#        equal a freshly booted board (that needs a reboot); the README lists them as written.
#   check-*: exit 1 unless every value of that regime is in place.
#   snapshot FILE: dump every value either regime touches.
set -u
C=/sys/devices/system/cpu
G=/sys/class/devfreq/17000000.gpu
E=/sys/class/devfreq/bwmgr
RG=/sys/devices/platform/gpu.0/railgate_enable
w() { echo "$2" | sudo -n tee "$1" >/dev/null || { echo "clocks.sh: cannot write $2 to $1" >&2; return 1; }; }
cpus() { ls -d $C/cpu[0-9]* ; }

snapshot() {
  {
    date '+%F %T'
    sudo -n jetson_clocks --show 2>&1
    for c in $(cpus); do
      echo "$c gov=$(cat $c/cpufreq/scaling_governor) min=$(cat $c/cpufreq/scaling_min_freq) max=$(cat $c/cpufreq/scaling_max_freq) cur=$(cat $c/cpufreq/scaling_cur_freq) idle_disable=$(cat $c/cpuidle/state*/disable | paste -sd,)"
    done
    echo "schedutil rate_limit_us=$(cat $C/cpufreq/schedutil/rate_limit_us 2>/dev/null)"
    for d in /sys/class/devfreq/*; do echo "$d gov=$(cat $d/governor) min=$(cat $d/min_freq) max=$(cat $d/max_freq) cur=$(cat $d/cur_freq)"; done
    for f in polling_interval timer nvhost_podgov/load_max nvhost_podgov/load_target nvhost_podgov/load_margin nvhost_podgov/up_freq_margin nvhost_podgov/down_freq_margin; do
      [ -e $G/$f ] && echo "gpu $f=$(cat $G/$f)"
    done
    echo "railgate_enable=$(cat $RG) railgate_delay=$(cat ${RG%/*}/railgate_delay) is_railgated=$(cat ${RG%/*}/is_railgated) power_control=$(cat ${RG%/*}/power/control)"
  } > "$1"
}

set_off() {
  for c in $(cpus); do w $c/cpufreq/scaling_min_freq 729600 || return 1; done
  for s in $C/cpu[0-9]*/cpuidle/state[0-9]*/disable; do w $s 0 || return 1; done
  # GPU powered (jetson_clocks left rail gating off) while its governor changes, as nvpower.sh
  w $G/governor nvhost_podgov || return 1
  w $G/polling_interval 10; w $G/timer delayed
  w $G/nvhost_podgov/load_max 900; w $G/nvhost_podgov/load_target 400; w $G/nvhost_podgov/load_margin 100
  w $G/nvhost_podgov/up_freq_margin 2; w $G/nvhost_podgov/down_freq_margin 1
  w $G/min_freq 306000000 || return 1
  w $E/min_freq 204000000 || return 1
}

set_on() {
  sudo -n jetson_clocks || return 1
  w $G/governor performance || return 1
}

check_on() {
  local bad=0
  for c in $(cpus); do [ "$(cat $c/cpufreq/scaling_min_freq)" = 1728000 ] && [ "$(cat $c/cpufreq/scaling_max_freq)" = 1728000 ] || bad=1; done
  for s in $C/cpu[0-9]*/cpuidle/state[0-9]*/disable; do [ "$(cat $s)" = 1 ] || bad=1; done
  [ "$(cat $G/governor)" = performance ] && [ "$(cat $G/min_freq)" = 1020000000 ] && [ "$(cat $G/max_freq)" = 1020000000 ] || bad=1
  [ "$(cat $E/min_freq)" = 3199000000 ] && [ "$(cat $RG)" = 0 ] || bad=1
  [ "$(cat $C/cpu5/cpufreq/scaling_cur_freq)" = 1728000 ] && [ "$(cat $G/cur_freq)" = 1020000000 ] || bad=1
  return $bad
}

check_off() {
  local bad=0
  for c in $(cpus); do [ "$(cat $c/cpufreq/scaling_min_freq)" = 729600 ] && [ "$(cat $c/cpufreq/scaling_max_freq)" = 1728000 ] || bad=1; done
  for s in $C/cpu[0-9]*/cpuidle/state[0-9]*/disable; do [ "$(cat $s)" = 0 ] || bad=1; done
  [ "$(cat $G/governor)" = nvhost_podgov ] && [ "$(cat $G/min_freq)" = 306000000 ] && [ "$(cat $G/max_freq)" = 1020000000 ] || bad=1
  [ "$(cat $E/min_freq)" = 204000000 ] && [ "$(cat $RG)" = 0 ] || bad=1
  return $bad
}

case ${1:-} in
  on) set_on && sleep 3 && check_on;;
  off) set_off && sleep 3 && check_off;;
  check-on) check_on;;
  check-off) check_off;;
  snapshot) snapshot "$2";;
  *) echo "usage: clocks.sh on|off|check-on|check-off|snapshot FILE" >&2; exit 2;;
esac
