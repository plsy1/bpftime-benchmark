# Run only the gate's smoke step against a frozen build laid out like a build directory.
import sys
from pathlib import Path
sys.path.insert(0, '/home/jetson/src/bpftime-jetson-daily/tools/jetson-upstream')
import tracker
config, repo = tracker.load_config(tracker.DEFAULT_CONFIG)
try:
    tracker.smoke_real_tools(config, Path('/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate'), Path(sys.argv[1]))
    print('SMOKE PASSED')
except tracker.TrackerError as e:
    print('SMOKE FAILED:', e)
