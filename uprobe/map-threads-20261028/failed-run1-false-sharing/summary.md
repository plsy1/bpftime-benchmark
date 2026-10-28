Per-call time per thread (us), median of runs; none = no tracing
| Threads | none | kernel control | 09421773 control | official control | kernel churn-k4-v8 | 09421773 churn-k4-v8 | official churn-k4-v8 | kernel churn-k8-v112 | 09421773 churn-k8-v112 | official churn-k8-v112 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.011 | 1.825 | 0.452 | 0.460 | 2.045 | 0.568 | 0.676 | 2.059 | 0.604 | 0.665 |
| 2 | 0.011 | 2.732 | 0.577 | 0.571 | 3.069 | 0.986 | 0.964 | 3.010 | 1.023 | 0.976 |
| 4 | 0.015 | 5.032 | 4.106 | 4.121 | 5.334 | 8.788 | 8.969 | 5.324 | 9.022 | 8.923 |
| 6 | 0.020 | 6.488 | 5.955 | 5.890 | 6.652 | 15.457 | 15.728 | 6.673 | 15.489 | 16.010 |

Map cost per call (us, churn - control: insert + lookup + delete)
| Threads | kernel churn-k4-v8 | 09421773 churn-k4-v8 | official churn-k4-v8 | kernel churn-k8-v112 | 09421773 churn-k8-v112 | official churn-k8-v112 |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.221 | 0.116 | 0.216 | 0.234 | 0.152 | 0.205 |
| 2 | 0.337 | 0.409 | 0.393 | 0.278 | 0.446 | 0.405 |
| 4 | 0.302 | 4.682 | 4.848 | 0.291 | 4.916 | 4.802 |
| 6 | 0.164 | 9.502 | 9.839 | 0.185 | 9.534 | 10.120 |

Probe cost per call without maps (us, control - none)
| Threads | kernel | 09421773 | official |
|---:|---:|---:|---:|
| 1 | 1.813 | 0.441 | 0.449 |
| 2 | 2.720 | 0.566 | 0.560 |
| 4 | 5.017 | 4.091 | 4.106 |
| 6 | 6.467 | 5.934 | 5.869 |

runs 120, failed checks 0
