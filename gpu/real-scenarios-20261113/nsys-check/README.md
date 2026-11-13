# Does Nsight Systems record kernels on this Orin? (2026-11-13)

Needed for step 0 of the measurement plan (census of an application's kernels without bpftime).

Board: Orin Nano, kernel 6.8.12-1021-tegra-btf, CUDA 13.2. No bpftime, no LD_PRELOAD.

## Result

Yes. `nsys` 2026.3.1 records every launch with its grid, block and GPU time.

Test program: `fin.cu` (10 launches of a 1M-element add, grid 4096 x 256), built with
`nvcc -O2 '-gencode=arch=compute_87,code=[sm_87,compute_87]'` (the fatbin then lists `fin.1.sm_87.ptx`).

```
/opt/nvidia/nsight-systems/2026.3.1/bin/nsys profile -t cuda -o fin ./fin
/opt/nvidia/nsight-systems/2026.3.1/bin/nsys stats -r cuda_gpu_kern_sum fin.nsys-rep
```

```
 Time (%)  Total Time (ns)  Instances  Avg (ns)  Med (ns)  Min (ns)  Max (ns)  StdDev (ns)       Name
    100.0          922,816         10  92,281.6  85,267.0    82,947   161,797     24,444.9  k(float *, int)
```

The exported SQLite table `CUPTI_ACTIVITY_KIND_KERNEL` holds grid (4096, 1, 1) and block (256, 1, 1) for all 10 launches.

## Notes

- A first try profiled bpftime's `example/gpu/cuda-counter/vec_add`, which loops forever. `timeout` killed it and nsys recorded no kernel data. That run says nothing about nsys; profile programs that exit on their own.
- `nsys status -e` on the BTF kernel: CPU profiling (process tree) OK, system-wide fails (paranoid level 2, no root). Not needed for kernel tracing.
- `/usr/local/cuda/bin/nsys` is 2025.6.3. It was only tried on the looping program, so use 2026.3.1.
- `ncu` (2026.1.1) is present and was not tried.
