| Object | Threads | Backend | Runs | Reads | Writes | Torn | Torn per million reads |
|---|---:|---|---:|---:|---:|---:|---:|
| torn-noprealloc.bpf.o | 2 | 09421773 | 3 | 33,665,000 | 33,875,400 | 347 | 10.31 |
| torn-noprealloc.bpf.o | 2 | kernel | 3 | 9,233,900 | 7,407,000 | 2 | 0.22 |
| torn-noprealloc.bpf.o | 4 | 09421773 | 3 | 14,590,400 | 4,261,200 | 83 | 5.69 |
| torn-noprealloc.bpf.o | 4 | kernel | 3 | 12,599,300 | 4,274,400 | 9 | 0.71 |
| torn-noprealloc.bpf.o | 6 | 09421773 | 3 | 12,125,900 | 3,549,400 | 89 | 7.34 |
| torn-noprealloc.bpf.o | 6 | kernel | 3 | 18,969,000 | 3,783,500 | 49 | 2.58 |
| torn.bpf.o | 2 | 09421773 | 3 | 33,761,600 | 33,891,700 | 336 | 9.95 |
| torn.bpf.o | 2 | kernel | 3 | 8,989,700 | 8,209,000 | 1 | 0.11 |
| torn.bpf.o | 4 | 09421773 | 3 | 14,805,200 | 4,416,400 | 120 | 8.11 |
| torn.bpf.o | 4 | kernel | 3 | 12,283,000 | 4,684,100 | 7 | 0.57 |
| torn.bpf.o | 6 | 09421773 | 3 | 11,960,400 | 3,472,600 | 97 | 8.11 |
| torn.bpf.o | 6 | kernel | 3 | 18,248,700 | 3,772,600 | 51 | 2.79 |

runs 36, invalid 0
