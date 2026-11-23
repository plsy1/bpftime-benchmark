# x86 pinbench under load and the syscall cost model (plan 3.5 item 6)

Session PB-loaded-20261123-try1: a 60 s B leg (policy M; nginx {2,6}, wrk {8-11}) loads the machine; 10 s in, pinbench runs on CPU 4 (3.5 GHz), 10 runs x 200,000 iterations. Idle figures: E2 session (10 runs x 1e6).

| case | starting mask | idle ns | loaded ns | loaded/idle |
|---|---|---:|---:|---:|
| pin sequence (getcpu, getaffinity, setaffinity cur, setaffinity orig) | single {4} | 801.1 | 795.2 | 0.99 |
| pin sequence | pair {4,6} | 1612.1 | 1615.4 | 1.00 |
| pin sequence | P cores {0,2,4,6} | 1604.4 | 1618.5 | 1.01 |
| pin sequence | all 0-15 | 1522.0 | 3034.5 | 1.99 |
| sched_getcpu | - | 1.1 | 1.1 | 1.00 |
| getppid | - | 86.7 | 86.7 | 1.00 |
| getpid | - | 87.1 | 87.3 | 1.00 |
| gettid | - | 83.7 | 84.0 | 1.00 |
| getuid + getgid | - | 164.9 | 164.9 | 1.00 |
| readlinkat /proc/self/exe | - | 695.8 | 698.4 | 1.00 |

Load changes nothing on x86 except the all-CPUs mask, where restoring the mask lets the thread move to busy cores.

Cost model: predicted extra time per request = calls per request (E7) x loaded unit cost; measured = E6 knock-out steps.

| re-entry | calls/req (E7) | predicted us/req | measured us/req (E6) | measured / predicted |
|---|---|---:|---:|---:|
| pinning (policy M mask {2,6}) | 2 pin sequences | 3.231 | 3.542 | 1.10 |
| uid/gid | 6 getuid + 6 getgid | 0.989 | 1.091 | 1.10 |
| pid/tid | 2 getpid + 2 gettid | 0.343 | 0.413 | 1.20 |

Indirect-cost coefficient on x86: 1.10-1.20 (the syscalls' direct cost explains 83-91% of each step; the rest is cache/pipeline disturbance).

## Container vs host control (2026-11-24)

Question from the Jetson session: is the in-situ pin cost on the Orin (about 2.3x the idle price) caused by nginx running inside the container's cpuset cgroup while pinbench ran on the host? x86 check: the same pinbench, same CPU 4, host vs a throwaway container of the paper image (--pid=host --privileged), ABAB x 3 rounds, 5 runs x 1e6 iterations each; governor not pinned, so only ratios are meaningful. Raw: raw/PB-container-20261124/rows.jsonl.

| case | container / host |
|---|---:|
| pin sequence, single mask | 0.98 |
| pin sequence, pair mask | 0.90 |
| pin sequence, all CPUs | 0.90 |
| getpid / gettid / getuid+getgid | 1.00 / 1.03 / 1.00 |

On x86 (6.12) the container does not make pinning slower (the pair mask is 10% faster inside, consistently across all 15 runs). This does not rule the cgroup out on the Orin (6.8); the Jetson should run the same in-container check.
