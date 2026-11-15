# C4: run-20261115-235928-full vs run-20261116-002008-full

## Cost

- 798 items; B/A median 1.000 (range 0.89-1.31); 36 moved by more than 10%

| item | A ns | B ns | B/A |
|---|---:|---:|---:|
| full|l8|sysonly|G48|dev | 13.33 | 11.88 | 0.89 |
| full|w32|ar_aa|G1|dev | 20.21 | 22.32 | 1.10 |
| full|w32|ar_ar|G32|dev | 14.14 | 15.75 | 1.11 |
| full|w32|ar_ar|G384|dev | 10.95 | 12.09 | 1.10 |
| full|w32|ar_ar|G48|dev | 14.15 | 15.96 | 1.13 |
| full|w32|ar_sc|G16|reg | 343.52 | 390.08 | 1.14 |
| full|w32|ar_sc|G1|dev | 20.16 | 22.31 | 1.11 |
| full|w32|ar_sc|G2|reg | 97.90 | 107.78 | 1.10 |
| full|w32|nofence|G16|dev | 6.72 | 8.75 | 1.30 |
| full|w32|nofence|G1|dev | 7.08 | 7.92 | 1.12 |
| full|w32|nofence|G2|dev | 6.76 | 7.80 | 1.15 |
| full|w32|nofence|G32|dev | 6.73 | 8.70 | 1.29 |
| full|w32|nofence|G384|dev | 6.71 | 8.82 | 1.31 |
| full|w32|nofence|G48|dev | 6.74 | 8.81 | 1.31 |
| full|w32|nofence|G4|dev | 6.68 | 8.01 | 1.20 |
| full|w32|nofence|G8|dev | 6.77 | 8.42 | 1.24 |
| full|w32|nofence|G96|dev | 6.76 | 8.79 | 1.30 |
| full|w32|rec|G0|- | 7.14 | 8.47 | 1.19 |
| full|w32|rel_ar|G384|dev | 10.79 | 11.99 | 1.11 |
| full|w32|rel_ar|G96|dev | 13.20 | 15.11 | 1.15 |
| full|w32|rel_sc|G1|dev | 19.89 | 22.31 | 1.12 |
| full|w32|rel_sc|G4|dev | 19.27 | 21.69 | 1.13 |
| full|w32|rf_argpu|G0|- | 6.79 | 8.54 | 1.26 |
| full|w32|rf_arsys|G0|- | 10.90 | 12.49 | 1.15 |
| full|w32|rf_scgpu|G0|- | 6.78 | 8.53 | 1.26 |
| full|w32|rf_scsys|G0|- | 10.91 | 12.49 | 1.15 |
| full|w32|rmw_ar|G1|dev | 18.10 | 20.42 | 1.13 |
| full|w32|rmw_ar|G2|dev | 15.24 | 17.40 | 1.14 |
| full|w32|rmw_ar|G384|dev | 10.92 | 12.13 | 1.11 |
| full|w32|rmw_ar|G4|dev | 13.80 | 15.71 | 1.14 |
| full|w32|sysonly|G16|dev | 16.36 | 18.56 | 1.13 |
| full|w32|sysonly|G32|dev | 13.93 | 15.61 | 1.12 |
| full|w32|sysonly|G48|dev | 13.60 | 15.81 | 1.16 |
| full|w32|sysonly|G8|dev | 16.34 | 18.90 | 1.16 |
| full|w32|sysonly|G96|dev | 11.13 | 14.31 | 1.29 |
| full|w32|tf_sc|G1|dev | 20.31 | 22.40 | 1.10 |

- rounds: A 10/10, B 10/10

## Cost decisions

| shape | layout | protocol | A: G*, ns, % | B: G*, ns, % |
|---|---|---|---|---|
| anchor | w32 | tf_sc | 384, 3.79, 11.1 | 384, 3.80, 11.2 |
| anchor | w32 | ar_sc | 384, 3.80, 11.2 | 384, 3.80, 11.2 |
| anchor | w32 | rel_sc | 384, 3.69, 10.8 | 384, 3.68, 10.8 |
| anchor | w32 | ar_ar | 384, 3.79, 11.1 | 384, 3.80, 11.2 |
| anchor | w32 | rel_ar | 384, 3.67, 10.8 | 384, 3.68, 10.8 |
| anchor | w32 | ar_aa | 384, 3.80, 11.2 | 1536, 3.50, 10.3 |
| anchor | w32 | rmw_ar | 384, 3.79, 11.2 | 1536, 3.46, 10.2 |
| anchor | l8 | tf_sc | 384, 3.40, 10.0 | 384, 3.37, 9.9 |
| anchor | l8 | ar_sc | 384, 3.43, 10.1 | 384, 3.37, 9.9 |
| anchor | l8 | rel_sc | 384, 3.37, 9.9 | 384, 3.31, 9.7 |
| anchor | l8 | ar_ar | 384, 3.39, 10.0 | 384, 3.32, 9.8 |
| anchor | l8 | rel_ar | 384, 3.34, 9.8 | 384, 3.26, 9.6 |
| anchor | l8 | ar_aa | 384, 3.41, 10.0 | 384, 3.36, 9.9 |
| anchor | l8 | rmw_ar | 384, 3.37, 9.9 | 384, 3.30, 9.7 |
| anchor | l8 | ar32_sc | None, 21.59, 63.5 | None, 21.57, 63.5 |
| anchor | l8 | ar32_ar | None, 18.43, 54.2 | None, 18.74, 55.1 |
| anchor | l8 | ar32_aa | None, 21.57, 63.5 | None, 21.63, 63.6 |
| large | w32 | tf_sc | None, 3.00, 8.8 | None, 2.99, 8.8 |
| large | w32 | ar_sc | None, 2.98, 8.8 | None, 2.98, 8.8 |
| large | w32 | rel_sc | None, 2.89, 8.5 | None, 2.88, 8.5 |
| large | w32 | ar_ar | None, 3.00, 8.8 | None, 2.98, 8.8 |
| large | w32 | rel_ar | None, 2.88, 8.5 | None, 2.87, 8.5 |
| large | w32 | ar_aa | None, 2.99, 8.8 | None, 2.99, 8.8 |
| large | w32 | rmw_ar | None, 2.95, 8.7 | None, 2.95, 8.7 |
| large | l8 | tf_sc | 384, 2.97, 8.7 | 384, 2.95, 8.7 |
| large | l8 | ar_sc | 384, 2.99, 8.8 | 384, 2.96, 8.7 |
| large | l8 | rel_sc | 384, 2.93, 8.6 | 384, 2.90, 8.5 |
| large | l8 | ar_ar | 384, 2.98, 8.8 | 384, 2.94, 8.6 |
| large | l8 | rel_ar | 384, 2.92, 8.6 | 384, 2.88, 8.5 |
| large | l8 | ar_aa | 384, 2.98, 8.8 | 384, 2.94, 8.7 |
| large | l8 | rmw_ar | 384, 2.94, 8.7 | 384, 2.90, 8.5 |
| large | l8 | ar32_sc | None, 20.88, 61.4 | None, 20.93, 61.6 |
| large | l8 | ar32_ar | None, 19.74, 58.1 | None, 19.82, 58.3 |
| large | l8 | ar32_aa | None, 20.85, 61.3 | None, 20.88, 61.4 |
| full | w32 | tf_sc | 96, 13.30, 39.1 | 32, 16.77, 49.3 |
| full | w32 | ar_sc | 96, 13.26, 39.0 | 32, 16.73, 49.2 |
| full | w32 | rel_sc | 96, 13.67, 40.2 | 32, 16.13, 47.5 |
| full | w32 | ar_ar | 4, 14.21, 41.8 | 2, 15.34, 45.1 |
| full | w32 | rel_ar | 2, 13.73, 40.4 | 2, 14.92, 43.9 |
| full | w32 | ar_aa | 96, 13.25, 39.0 | 32, 16.76, 49.3 |
| full | w32 | rmw_ar | 4, 13.80, 40.6 | 4, 15.71, 46.2 |
| full | l8 | tf_sc | 32, 12.04, 35.4 | 32, 12.03, 35.4 |
| full | l8 | ar_sc | 32, 12.05, 35.4 | 32, 12.03, 35.4 |
| full | l8 | rel_sc | 8, 12.05, 35.5 | 8, 12.03, 35.4 |
| full | l8 | ar_ar | 32, 12.20, 35.9 | 32, 12.23, 36.0 |
| full | l8 | rel_ar | 32, 12.58, 37.0 | 32, 12.48, 36.7 |
| full | l8 | ar_aa | 32, 12.06, 35.5 | 32, 12.02, 35.4 |
| full | l8 | rmw_ar | 32, 12.27, 36.1 | 32, 11.95, 35.2 |
| full | l8 | ar32_sc | None, 27.42, 80.7 | None, 27.68, 81.4 |
| full | l8 | ar32_ar | None, 21.66, 63.7 | None, 21.61, 63.6 |
| full | l8 | ar32_aa | None, 27.39, 80.6 | None, 27.57, 81.1 |

## Correctness verdicts

- any protocol violation: A False, B False
- calls without a result: A none, B none

- l8 G=1 ar_aa: same kind
- l8 G=1 ar_sc: same kind
- l8 G=1 sysonly: DIFFERS (A: 0 of 79461888 without the contributors' gpu-scope release although no fence at all would have failed about 346 times: on | B: 0 of 81067008 without the contributors' gpu-scope release although no fence at all would have failed about 1.27e+03 time)
- l8 G=1 nosys: same kind
- l8 G=1 none: same kind
- l8 G=8 ar_aa: same kind
- l8 G=8 ar_sc: same kind
- l8 G=8 sysonly: DIFFERS (A: 0 of 10200576 without the contributors' gpu-scope release although no fence at all would have failed about 1.84e+05 time | B: 0 of 10324416 without the contributors' gpu-scope release although no fence at all would have failed about 2.03e+05 time)
- l8 G=8 nosys: DIFFERS (A: 0 of 10288512 without the completer's system-scope fence although no fence at all would have failed about 1.85e+05 times | B: 0 of 11016576 without the completer's system-scope fence although no fence at all would have failed about 2.16e+05 times)
- l8 G=8 none: same kind
- l8 G=48 ar_aa: same kind
- l8 G=48 rmw_ar: same kind
- l8 G=48 ar32_aa: same kind
- l8 G=48 ar_sc: same kind
- l8 G=48 tf_sc: same kind
- l8 G=48 rel_sc: same kind
- l8 G=48 ar_ar: same kind
- l8 G=48 rel_ar: same kind
- l8 G=48 ar32_sc: same kind
- l8 G=48 ar32_ar: same kind
- l8 G=48 sysonly: DIFFERS (A: 0 of 905376 without the contributors' gpu-scope release although no fence at all would have failed about 3.07e+05 times: | B: 0 of 874016 without the contributors' gpu-scope release although no fence at all would have failed about 3.08e+05 times:)
- l8 G=48 nosys: DIFFERS (A: 0 of 802272 without the completer's system-scope fence although no fence at all would have failed about 2.72e+05 times:  | B: 0 of 800928 without the completer's system-scope fence although no fence at all would have failed about 2.82e+05 times: )
- l8 G=48 none: same kind
- l8 G=384 ar_aa: same kind
- l8 G=384 ar_sc: same kind
- l8 G=384 sysonly: DIFFERS (A: 0 of 26036 without the contributors' gpu-scope release although no fence at all would have failed about 6.52e+03 times:  | B: 0 of 25956 without the contributors' gpu-scope release although no fence at all would have failed about 6.45e+03 times: )
- l8 G=384 nosys: DIFFERS (A: 0 of 20352 without the completer's system-scope fence although no fence at all would have failed about 5.1e+03 times: on | B: 0 of 20616 without the completer's system-scope fence although no fence at all would have failed about 5.12e+03 times: o)
- l8 G=384 none: same kind
- w32 G=1 ar_aa: same kind
- w32 G=1 ar_sc: same kind
- w32 G=1 sysonly: DIFFERS (A: 0 of 118679040 without the contributors' gpu-scope release although no fence at all would have failed about 14.1 times:  | B: 0 of 118702080 without the contributors' gpu-scope release although no fence at all would have failed about 3.67 times: )
- w32 G=1 nosys: DIFFERS (A: 0 of 157687296 without the completer's system-scope fence although no fence at all would have failed about 18.7 times: o | B: 0 of 156446208 without the completer's system-scope fence although no fence at all would have failed about 4.84 times: o)
- w32 G=1 none: same kind
- w32 G=8 ar_aa: same kind
- w32 G=8 ar_sc: same kind
- w32 G=8 sysonly: DIFFERS (A: 0 of 10534848 without the contributors' gpu-scope release although no fence at all would have failed about 7.43e+05 time | B: 0 of 9762816 without the contributors' gpu-scope release although no fence at all would have failed about 5.89e+05 times)
- w32 G=8 nosys: DIFFERS (A: 0 of 11086464 without the completer's system-scope fence although no fence at all would have failed about 7.82e+05 times | B: 0 of 9053184 without the completer's system-scope fence although no fence at all would have failed about 5.46e+05 times:)
- w32 G=8 none: same kind
- w32 G=48 ar_aa: same kind
- w32 G=48 rmw_ar: same kind
- w32 G=48 ar_sc: same kind
- w32 G=48 tf_sc: same kind
- w32 G=48 rel_sc: same kind
- w32 G=48 ar_ar: same kind
- w32 G=48 rel_ar: same kind
- w32 G=48 sysonly: DIFFERS (A: 0 of 912160 without the contributors' gpu-scope release although no fence at all would have failed about 4.02e+05 times: | B: 0 of 914272 without the contributors' gpu-scope release although no fence at all would have failed about 4.24e+05 times:)
- w32 G=48 nosys: DIFFERS (A: 0 of 799168 without the completer's system-scope fence although no fence at all would have failed about 3.52e+05 times:  | B: 0 of 798176 without the completer's system-scope fence although no fence at all would have failed about 3.7e+05 times: o)
- w32 G=48 none: same kind
- w32 G=384 ar_aa: same kind
- w32 G=384 ar_sc: same kind
- w32 G=384 sysonly: DIFFERS (A: 0 of 26972 without the contributors' gpu-scope release although no fence at all would have failed about 8.51e+03 times:  | B: 0 of 26616 without the contributors' gpu-scope release although no fence at all would have failed about 1.31e+04 times: )
- w32 G=384 nosys: DIFFERS (A: 0 of 22172 without the completer's system-scope fence although no fence at all would have failed about 6.99e+03 times: o | B: 0 of 23500 without the completer's system-scope fence although no fence at all would have failed about 1.15e+04 times: o)
- w32 G=384 none: same kind

