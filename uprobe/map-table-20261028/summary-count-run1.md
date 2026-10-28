
NKEYS 1: map cost per call (us, kind minus control), and probe cost w/o map
| Threads | kernel hash | 09421773 hash | kernel percpu-hash | 09421773 percpu-hash | kernel percpu-array | 09421773 percpu-array | kernel array | 09421773 array | kernel probe w/o map | 09421773 probe w/o map |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.018 | 0.026 | 0.024 | 0.035 | -0.003 | 0.016 | 0.005 | 0.010 | 1.239 | 0.207 |
| 2 | 0.003 | 0.204 | -0.008 | 0.078 | -0.001 | 0.037 | -0.030 | 0.051 | 1.849 | 0.276 |
| 4 | -0.043 | 1.211 | 0.009 | 0.019 | -0.040 | 0.005 | 0.129 | -0.025 | 3.352 | 2.246 |
| 6 | 0.018 | 1.359 | -0.019 | 0.110 | -0.061 | 0.014 | -0.086 | 0.084 | 4.190 | 4.540 |

NKEYS 64: map cost per call (us, kind minus control), and probe cost w/o map
| Threads | kernel hash | 09421773 hash | kernel percpu-hash | 09421773 percpu-hash | kernel percpu-array | 09421773 percpu-array | kernel array | 09421773 array | kernel probe w/o map | 09421773 probe w/o map |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.008 | 0.028 | 0.021 | 0.037 | -0.014 | 0.016 | -0.002 | 0.010 | 1.246 | 0.208 |
| 2 | -0.008 | 0.207 | 0.015 | 0.071 | -0.006 | 0.037 | -0.003 | 0.030 | 1.851 | 0.273 |
| 4 | -0.083 | 0.490 | -0.180 | 0.036 | 0.012 | 0.019 | 0.007 | -0.017 | 3.463 | 2.242 |
| 6 | 0.104 | 1.037 | -0.002 | 0.146 | -0.027 | 0.103 | -0.043 | 0.053 | 4.118 | 4.553 |

runs 264, failed checks 6
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 2, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "hits": null, "failed": null, "map_sum": null, "map_entries": null, "ok": false, "tool_tail": "lobal shm initialized\n[2026-10-28 18:28:12][info][472368] b
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 4, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "hits": null, "failed": null, "map_sum": null, "map_entries": null, "ok": false, "tool_tail": "8:28:21][info][472476] Created uprobe/uretprobe perf event h
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 2, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "hits": null, "failed": null, "map_sum": null, "map_entries": null, "ok": false, "tool_tail": "lobal shm initialized\n[2026-10-28 18:38:54][info][477072] b
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 6, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "hits": null, "failed": null, "map_sum": null, "map_entries": null, "ok": false, "tool_tail": "8:39:18][info][477299] Created uprobe/uretprobe perf event h
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 4, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "hits": null, "failed": null, "map_sum": null, "map_entries": null, "ok": false, "tool_tail": "info][481896] Global shm constructed. shm_open_type 4 for bp
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 6, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "error": "TimeoutExpired(['sudo', '-n', 'env', 'SUM_MAP=counts', 'LD_PRELOAD=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/09421773/libbpft
