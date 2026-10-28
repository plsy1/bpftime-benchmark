
Live entries 1,000: per-op time per thread (us)
| Threads | none | kernel total | kernel map | kernel probe w/o map | 09421773 total | 09421773 map | 09421773 probe w/o map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.003 | 3.012 | 0.489 | 2.520 | 0.979 | 0.490 | 0.486 |
| 2 | 0.003 | 4.073 | 0.437 | 3.632 | 1.517 | 0.775 | 0.740 |
| 4 | 0.003 | 6.949 | 0.183 | 6.762 | 9.534 | 5.307 | 4.223 |
| 6 | 0.003 | 8.018 | 0.317 | 7.697 | 16.719 | 10.945 | 5.770 |

Live entries 100,000: per-op time per thread (us)
| Threads | none | kernel total | kernel map | kernel probe w/o map | 09421773 total | 09421773 map | 09421773 probe w/o map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.003 | 3.168 | 0.649 | 2.516 | - | - | 0.480 |
| 2 | 0.003 | 4.292 | 0.651 | 3.638 | 1.816 | 1.084 | 0.729 |
| 4 | 0.003 | 7.138 | 0.568 | 6.568 | 9.303 | 5.122 | 4.178 |
| 6 | 0.003 | 7.825 | 0.120 | 7.701 | 16.283 | 10.606 | 5.674 |

Live entries 900,000: per-op time per thread (us)
| Threads | none | kernel total | kernel map | kernel probe w/o map | 09421773 total | 09421773 map | 09421773 probe w/o map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.003 | 3.604 | 1.073 | 2.528 | - | - | 0.484 |
| 2 | 0.003 | 4.685 | 0.939 | 3.743 | 2.254 | 1.512 | 0.739 |
| 4 | 0.003 | - | - | 6.675 | - | - | 4.221 |
| 6 | 0.003 | - | - | 7.498 | 17.596 | 11.898 | 5.695 |

runs 180, failed checks 20
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 1, "live": 100000, "ops_per_sec": 911237.2, "total_ops": 7288300, "allocs": 7388300, "frees": 7288300, "workload_rc": 0, "seen_allocs": 7388300, "seen_frees": 7288299, "missed_frees": 1, "ok": false, "tool_tail": "[450871] Created uprobe/uretp
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 4, "live": 100000, "ops_per_sec": 551662.6, "total_ops": 4414500, "allocs": 4514500, "frees": 4414500, "workload_rc": 0, "seen_allocs": 4514500, "seen_frees": 4414499, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 4514500\nsum1 44
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 4, "live": 100000, "ops_per_sec": 430771.4, "total_ops": 3446600, "allocs": 3546600, "frees": 3446600, "workload_rc": 0, "seen_allocs": 3546600, "seen_frees": 3446599, "missed_frees": 1, "ok": false, "tool_tail": "[451169] Created uprobe/uretp
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 1, "live": 900000, "ops_per_sec": 768647.4, "total_ops": 6129100, "allocs": 7029100, "frees": 6129100, "workload_rc": 0, "seen_allocs": 7029100, "seen_frees": 6129099, "missed_frees": 1, "ok": false, "tool_tail": "[451910] Created uprobe/uretp
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 4, "live": 900000, "ops_per_sec": 548362.5, "total_ops": 4386800, "allocs": 5286800, "frees": 4386800, "workload_rc": 0, "seen_allocs": 5286800, "seen_frees": 4386799, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 5286800\nsum1 43
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 4, "live": 900000, "ops_per_sec": 396072.9, "total_ops": 3174200, "allocs": 4074200, "frees": 3174200, "workload_rc": 0, "seen_allocs": 4074200, "seen_frees": 3174199, "missed_frees": 1, "ok": false, "tool_tail": "[452203] Created uprobe/uretp
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 6, "live": 900000, "ops_per_sec": 721451.2, "total_ops": 5768700, "allocs": 6668700, "frees": 5768700, "workload_rc": 0, "seen_allocs": 6668700, "seen_frees": 5768699, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 6668700\nsum1 57
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 1, "live": 100000, "ops_per_sec": 912139.9, "total_ops": 7295900, "allocs": 7395900, "frees": 7295900, "workload_rc": 0, "seen_allocs": 7395900, "seen_frees": 7295899, "missed_frees": 1, "ok": false, "tool_tail": "[453960] Created uprobe/uretp
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 4, "live": 100000, "ops_per_sec": 578364.8, "total_ops": 4625500, "allocs": 4725500, "frees": 4625500, "workload_rc": 0, "seen_allocs": 4725500, "seen_frees": 4625499, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 4725500\nsum1 46
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 4, "live": 100000, "ops_per_sec": 430373.6, "total_ops": 3441300, "allocs": 3541300, "frees": 3441300, "workload_rc": 0, "seen_allocs": 3541300, "seen_frees": 3441299, "missed_frees": 1, "ok": false, "tool_tail": "[454248] Created uprobe/uretp
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 6, "live": 100000, "ops_per_sec": 744353.4, "total_ops": 5955800, "allocs": 6055796, "frees": 5955800, "workload_rc": 0, "seen_allocs": 6055796, "seen_frees": 5955799, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 6055796\nsum1 59
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 1, "live": 900000, "ops_per_sec": 768248.9, "total_ops": 6127500, "allocs": 7027500, "frees": 6127500, "workload_rc": 0, "seen_allocs": 7027500, "seen_frees": 6127499, "missed_frees": 1, "ok": false, "tool_tail": "[454999] Created uprobe/uretp
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 4, "live": 900000, "ops_per_sec": 535361.1, "total_ops": 4278300, "allocs": 5178300, "frees": 4278300, "workload_rc": 0, "seen_allocs": 5178300, "seen_frees": 4278299, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 5178300\nsum1 42
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 4, "live": 900000, "ops_per_sec": 394474.6, "total_ops": 3162000, "allocs": 4062000, "frees": 3162000, "workload_rc": 0, "seen_allocs": 4062000, "seen_frees": 3161999, "missed_frees": 1, "ok": false, "tool_tail": "[455288] Created uprobe/uretp
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 6, "live": 900000, "ops_per_sec": 730749.0, "total_ops": 5839300, "allocs": 6739300, "frees": 5839300, "workload_rc": 0, "seen_allocs": 6739300, "seen_frees": 5839299, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 6739300\nsum1 58
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 1, "live": 100000, "ops_per_sec": 911033.9, "total_ops": 7288600, "allocs": 7388600, "frees": 7288600, "workload_rc": 0, "seen_allocs": 7388600, "seen_frees": 7288599, "missed_frees": 1, "ok": false, "tool_tail": "[457019] Created uprobe/uretp
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 1, "live": 900000, "ops_per_sec": 768746.8, "total_ops": 6131700, "allocs": 7031700, "frees": 6131700, "workload_rc": 0, "seen_allocs": 7031700, "seen_frees": 6131699, "missed_frees": 1, "ok": false, "tool_tail": "[458053] Created uprobe/uretp
FAILED {"backend": "09421773", "obj": "track.bpf.o", "threads": 4, "live": 900000, "ops_per_sec": 394972.9, "total_ops": 3165200, "allocs": 4065200, "frees": 3165200, "workload_rc": 0, "seen_allocs": 4065200, "seen_frees": 3165199, "missed_frees": 1, "ok": false, "tool_tail": "[458336] Created uprobe/uretp
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 4, "live": 900000, "ops_per_sec": 532962.7, "total_ops": 4262400, "allocs": 5162400, "frees": 4262400, "workload_rc": 0, "seen_allocs": 5162400, "seen_frees": 4262399, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 5162400\nsum1 42
FAILED {"backend": "kernel", "obj": "track.bpf.o", "threads": 6, "live": 900000, "ops_per_sec": 737347.0, "total_ops": 5897600, "allocs": 6797600, "frees": 5897600, "workload_rc": 0, "seen_allocs": 6797600, "seen_frees": 5897599, "missed_frees": 1, "ok": false, "tool_tail": "attached\nsum0 6797600\nsum1 58
