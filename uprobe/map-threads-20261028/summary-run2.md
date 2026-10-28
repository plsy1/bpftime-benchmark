Per-call time per thread (us), median of runs; none = no tracing
| Threads | none | kernel control | 09421773 control | official control | kernel churn-k4-v8 | 09421773 churn-k4-v8 | official churn-k4-v8 | kernel churn-k8-v112 | 09421773 churn-k8-v112 | official churn-k8-v112 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.011 | 1.847 | 0.451 | 0.445 | 2.045 | 0.567 | 0.652 | 2.074 | 0.604 | 0.667 |
| 2 | 0.011 | 2.634 | 0.547 | 0.539 | 3.011 | 0.966 | 0.938 | 3.049 | 1.009 | 0.952 |
| 4 | 0.011 | 4.824 | 2.715 | 2.749 | 5.041 | 9.006 | 8.805 | 4.953 | 8.947 | 9.251 |
| 6 | 0.012 | 6.360 | 4.785 | 4.828 | 6.683 | 15.505 | 15.997 | 6.654 | 15.958 | 16.018 |

Map cost per call (us, churn - control: insert + lookup + delete)
| Threads | kernel churn-k4-v8 | 09421773 churn-k4-v8 | official churn-k4-v8 | kernel churn-k8-v112 | 09421773 churn-k8-v112 | official churn-k8-v112 |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.199 | 0.115 | 0.207 | 0.228 | 0.152 | 0.222 |
| 2 | 0.377 | 0.420 | 0.399 | 0.415 | 0.463 | 0.413 |
| 4 | 0.216 | 6.290 | 6.057 | 0.129 | 6.232 | 6.502 |
| 6 | 0.324 | 10.719 | 11.169 | 0.294 | 11.173 | 11.190 |

Probe cost per call without maps (us, control - none)
| Threads | kernel | 09421773 | official |
|---:|---:|---:|---:|
| 1 | 1.835 | 0.440 | 0.434 |
| 2 | 2.623 | 0.536 | 0.528 |
| 4 | 4.813 | 2.704 | 2.738 |
| 6 | 6.348 | 4.774 | 4.816 |

runs 120, failed checks 0
