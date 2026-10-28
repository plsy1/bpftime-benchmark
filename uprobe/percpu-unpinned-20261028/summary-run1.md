| Map | Threads | Affinity | Backend | Runs | Calls | Lost counts | Lost % | Lost per run |
|---|---:|---|---|---:|---:|---:|---:|---|
| array | 6 | pinned | c98951a4 | 3 | 23,211,400 | 0 | 0.000 | 0, 0, 0 |
| array | 6 | pinned | kernel | 3 | 25,804,600 | 0 | 0.000 | 0, 0, 0 |
| array | 6 | unpinned | c98951a4 | 3 | 23,580,200 | 0 | 0.000 | 0, 0, 0 |
| array | 6 | unpinned | kernel | 3 | 25,327,100 | 0 | 0.000 | 0, 0, 0 |
| array | 12 | unpinned | c98951a4 | 3 | 23,177,300 | 0 | 0.000 | 0, 0, 0 |
| array | 12 | unpinned | kernel | 3 | 26,283,500 | 0 | 0.000 | 0, 0, 0 |
| percpu-array | 6 | pinned | c98951a4 | 3 | 23,402,700 | 0 | 0.000 | 0, 0, 0 |
| percpu-array | 6 | pinned | kernel | 3 | 25,936,100 | 0 | 0.000 | 0, 0, 0 |
| percpu-array | 6 | unpinned | c98951a4 | 3 | 23,953,700 | 18,791 | 0.078 | 9,925, 5,738, 3,128 |
| percpu-array | 6 | unpinned | kernel | 3 | 25,483,400 | 1,093 | 0.004 | 0, 1,093, 0 |
| percpu-array | 12 | unpinned | c98951a4 | 3 | 23,286,800 | 524,549 | 2.253 | 149,848, 220,803, 153,898 |
| percpu-array | 12 | unpinned | kernel | 3 | 26,372,000 | 2,963 | 0.011 | 970, 1,993, 0 |
| percpu-hash | 6 | pinned | c98951a4 | 3 | 22,224,600 | 0 | 0.000 | 0, 0, 0 |
| percpu-hash | 6 | pinned | kernel | 3 | 25,925,800 | 0 | 0.000 | 0, 0, 0 |
| percpu-hash | 6 | unpinned | c98951a4 | 3 | 21,163,800 | 12,567 | 0.059 | 3,518, 6,529, 2,520 |
| percpu-hash | 6 | unpinned | kernel | 3 | 25,611,600 | 1,077 | 0.004 | 0, 1,077, 0 |
| percpu-hash | 12 | unpinned | c98951a4 | 3 | 16,760,800 | 241,947 | 1.444 | 54,505, 92,991, 94,451 |
| percpu-hash | 12 | unpinned | kernel | 3 | 26,217,100 | 23,399 | 0.089 | 7,782, 9,752, 5,865 |

runs 54; runs with hits != calls or failed updates: 0; errors: 0
