#!/usr/bin/env bash
# Run a command under the campaign measurement lock: flock -n, retry every 60 s up to 20 tries
# when the lock is held (flock exit 75), and never hold the lock more than 5 minutes (280 s + 10 s kill-after = 290 s).
LOCK=/home/y/src/bpftime-x86-runs/paper-20261124/.measure.lock
for try in $(seq 1 20); do
  flock -n -E 75 "$LOCK" timeout --signal=INT --kill-after=10 280 "$@"
  rc=$?
  [ $rc -ne 75 ] && exit $rc
  echo "locked.sh: lock held (try $try/20), waiting 60 s" >&2
  sleep 60
done
echo "locked.sh: gave up after 20 tries" >&2
exit 75
