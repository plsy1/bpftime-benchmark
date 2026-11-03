#!/usr/bin/env python3
"""measure_mt.py with the malloc example rebuilt without its bpf_printk (ex-noprintk/): separates
the map's multi-thread cost from the printk path's (bpftime prints to the traced process's stdout
under the stdio lock; the kernel to per-CPU trace buffers)."""
import audit, measure_mt
audit.EX = audit.ROOT / 'ex-noprintk'
measure_mt.main()
