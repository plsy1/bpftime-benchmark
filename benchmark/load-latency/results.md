# BPFtime Part 3: Load Latency Results

*Generated on 2026-06-10 08:59:30*

## Environment
- **OS:** Linux 7.0.0-14-generic
- **Python:** 3.14.4

## Performance Results

| Load Method | Latency (ms) | Description |
| :--- | :--- | :--- |
| **bpftime start** (LD_PRELOAD launch) | 6.34 ms | Measure time inside agent from init to successful attach |
| **bpftime attach** (Frida injection) | 60.25 ms | Measure wall-clock time of the attach process injection |

## Conclusion
- **LD_PRELOAD launch** (`bpftime start`) is extremely fast because it occurs directly during process initialization.
- **Frida dynamic injection** (`bpftime attach`) takes slightly longer (involving process attachment, thread creation, and remote injection) but allows attaching to already running processes without restarting them.