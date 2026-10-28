Per-call time per thread (us), median of runs; none = no tracing
| Threads | none | kernel control | 09421773 control | kernel churn-k4-v8 | 09421773 churn-k4-v8 |
|---:|---:|---:|---:|---:|---:|
| 2 | 0.011 | 2.697 | 0.547 | 2.996 | 0.958 |
| 4 | 0.011 | 3.149 | 0.922 | 3.358 | 3.600 |

Map cost per call (us, churn - control: insert + lookup + delete)
| Threads | kernel churn-k4-v8 | 09421773 churn-k4-v8 |
|---:|---:|---:|
| 2 | 0.299 | 0.411 |
| 4 | 0.209 | 2.678 |

Probe cost per call without maps (us, control - none)
| Threads | kernel | 09421773 |
|---:|---:|---:|
| 2 | 2.686 | 0.536 |
| 4 | 3.138 | 0.911 |

runs 30, failed checks 0
