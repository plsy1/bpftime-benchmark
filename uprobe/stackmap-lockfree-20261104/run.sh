#!/bin/bash
# 11/04: stack trace map found without the lock (frozen build $1 = dev-efe08cbd) against bpf
# and dev-ff5515d8. 1. audit memleak, single thread (count vs bpf); 2. memleak 1/2/4/6
# threads, 3 rounds x 10 s, backends rotated; 3. perf profile of the 6-thread workload.
# Each stage bounded; cpu5 1728000 and no leftovers before.
set -u
B=$1
N=/home/jetson/src/results/uprobe/stackmap-lockfree-20261104
H=/home/jetson/src/realprog-20261026-btf/harness
BD=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds
PERF=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf
date; uname -r
check() {
  python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
  [ $? -eq 0 ] || exit 1
  pgrep -af 'keyloop|trackloop|countloop|churnloop|benchmark.py|callloop|mallocloop|nginx|wrk|memleak' | grep -v -e pgrep -e run.sh && { echo leftovers; exit 1; }
  ls -la /dev/shm | awk '$3=="root"' | grep -v -e nvsciipc_init -e ' \.$' -e ' \.\.$'
}
check
mkdir -p $N/audit $N/measure-mt $N/profile
cd $H && timeout 900 python3 audit.py --backends kernel,$B --tools memleak --secs 4 --out $N/audit/audit.jsonl > $N/audit/audit.log 2>&1
echo "audit exit $?"; date
check
cd $H && timeout 2400 python3 measure_mt.py --tools memleak --backends kernel,dev-ff5515d8,$B --out $N/measure-mt/memleak.jsonl > $N/measure-mt/memleak.log 2>&1
echo "memleak exit $?"; date
check
cd $N/profile && for b in dev-ff5515d8 $B; do
  sudo -n $BD/$b/bpftimetool remove >/dev/null 2>&1
  (sudo -n timeout 40 env LD_PRELOAD=$BD/$b/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024 SW_PRELOAD=$BD/$b/libbpftime-agent.so SW_CMD=/home/jetson/src/realprog-20261026-btf/workloads-mt/t6/mallocloop "SW_ARGS=10 64 1024" /home/jetson/src/realprog-20261026-btf/libbpf-tools/memleak -c /home/jetson/src/rp/sw 1 > memleak.$b.t6.tool.txt 2>&1 &)
  /bin/sleep 4; p=$(pgrep -x mallocloop | head -1)
  sudo -n timeout 20 $PERF record -q -e cycles:u -p $p -o memleak.$b.t6.perf.data -- /bin/sleep 4 2>/dev/null
  /bin/sleep 4; sudo -n pkill -9 -x memleak; sudo -n pkill -9 -x mallocloop; /bin/sleep 1
  sudo -n $BD/$b/bpftimetool remove >/dev/null 2>&1
  sudo -n chown jetson:jetson memleak.$b.t6.perf.data
  $PERF report -i memleak.$b.t6.perf.data --stdio --sort symbol -F overhead,sample,symbol 2>/dev/null | grep -v '^#' | grep -v '^$' | head -25 | cut -c1-160 > memleak.$b.t6.symbols.txt
done
echo "profile done"; date
check
sudo -n chown -R jetson:jetson $N
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
