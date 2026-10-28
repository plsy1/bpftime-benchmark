
NKEYS 1: map cost per call (us, kind minus control), and probe cost w/o map
| Threads | kernel hash | 09421773 hash | c98951a4 hash | kernel percpu-hash | 09421773 percpu-hash | c98951a4 percpu-hash | kernel percpu-array | 09421773 percpu-array | c98951a4 percpu-array | kernel array | 09421773 array | c98951a4 array | kernel probe w/o map | 09421773 probe w/o map | c98951a4 probe w/o map |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | - | - | - | 0.023 | 0.035 | 0.044 | - | - | - | - | - | - | 1.240 | 0.208 | 0.206 |
| 2 | - | - | - | -0.007 | 0.078 | 0.187 | - | - | - | - | - | - | 1.847 | 0.276 | 0.276 |
| 4 | - | - | - | 0.192 | 0.024 | 0.389 | - | - | - | - | - | - | 3.311 | 2.249 | 2.296 |
| 6 | - | - | - | -0.065 | 0.003 | 0.268 | - | - | - | - | - | - | 4.111 | 4.588 | 4.619 |

NKEYS 64: map cost per call (us, kind minus control), and probe cost w/o map
| Threads | kernel hash | 09421773 hash | c98951a4 hash | kernel percpu-hash | 09421773 percpu-hash | c98951a4 percpu-hash | kernel percpu-array | 09421773 percpu-array | c98951a4 percpu-array | kernel array | 09421773 array | c98951a4 array | kernel probe w/o map | 09421773 probe w/o map | c98951a4 probe w/o map |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | - | - | - | 0.021 | 0.037 | 0.048 | - | - | - | - | - | - | 1.243 | 0.208 | 0.206 |
| 2 | - | - | - | -0.007 | 0.072 | 0.177 | - | - | - | - | - | - | 1.845 | 0.275 | 0.279 |
| 4 | - | - | - | 0.024 | - | 0.152 | - | - | - | - | - | - | 3.465 | 2.250 | 2.306 |
| 6 | - | - | - | -0.041 | - | 0.563 | - | - | - | - | - | - | 4.135 | 4.568 | 4.655 |

runs 168, failed checks 8
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 4, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "hits": null, "failed": null, "map_sum": null, "map_entries": null, "ok": false, "tool_tail": "026-10-28 22:00:19][info][523893] Global shm constructed. sh
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 6, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "error": "TimeoutExpired(['sudo', '-n', 'env', 'SUM_MAP=counts', 'LD_PRELOAD=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/09421773/libbpft
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 2, "nkeys": 1, "ops_per_sec": 5658186.2, "total_ops": 33962100, "workload_rc": 0, "hits": 33962100, "failed": 0, "map_sum": 33962099, "map_entries": 1, "ok": false, "tool_tail": "\n[2026-10-28 22:03:19][info][525167] Global shm ini
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 2, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "hits": null, "failed": null, "map_sum": null, "map_entries": null, "ok": false, "tool_tail": "lobal shm initialized\n[2026-10-28 22:06:46][info][526687] b
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 4, "nkeys": 64, "ops_per_sec": null, "total_ops": null, "workload_rc": -11, "hits": null, "failed": null, "map_sum": null, "map_entries": null, "ok": false, "tool_tail": "lobal shm initialized\n[2026-10-28 22:07:10][info][526888] b
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 6, "nkeys": 64, "error": "TimeoutExpired(['sudo', '-n', 'env', 'SUM_MAP=counts', 'LD_PRELOAD=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/09421773/libbpftime-agent.so', 'BPFTIME_LOG_OUTPUT=console', '/home/jetson/s
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 4, "nkeys": 64, "error": "TimeoutExpired(['sudo', '-n', 'env', 'SUM_MAP=counts', 'LD_PRELOAD=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/09421773/libbpftime-agent.so', 'BPFTIME_LOG_OUTPUT=console', '/home/jetson/s
FAILED {"backend": "09421773", "obj": "count-percpu-hash.bpf.o", "threads": 6, "nkeys": 64, "ops_per_sec": 1295953.5, "total_ops": 7780100, "workload_rc": 0, "error": "TimeoutExpired(['sudo', '-n', 'env', 'SUM_MAP=counts', 'LD_PRELOAD=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/09421773/l
