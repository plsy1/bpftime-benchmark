# x86 NUC：多线程真实工具（bpftime 示例 malloc / funclatency），2026-11-04 03:10–03:55

Jetson 会话 bpftime-arm64环境-性能优化 执行，是 Jetson results `uprobe/realtools-20261104`（重写后 hash map 的真实工具验证）在 x86 上的对照。只用两个能在 NUC 上直接编的工具：bpftime 的 `example/malloc` 和 `example/tracing/funclatency`，在 dev 7ad5a152 的树（`~/src/bpftime-wt-r1-7ad5a152`）里用 clang-19 编译。负载是 Jetson 同一份 `workloads-mt/mt.c` 在 x86 上编的（线程 i 绑 CPU 5−i，即 P 核 0–2 的 6 个线程）。CPU 0–5 钉 3.5 GHz（turbo 开、C6 及更深关闭，`../x86-nuc-r1-miss-20261104/keypattern/pin-cpus.sh`），测前查残留、测后恢复。harness 是 Jetson `audit.py` / `measure_mt.py` 的副本，只改了路径（`harness/`）。

## 1. 两个工具，四条腿（`measure-mt/`，`summary.md`，3 轮 × 10 s，中位数）

| 工具 | 1 线程 | 2 | 4 | 6 |
|---|---:|---:|---:|---:|
| funclatency 示例：7ad5a152 / kernel | 5.21x | 5.73x | 5.14x | 4.75x |
| malloc 示例：7ad5a152 / kernel | 0.85x | 0.33x | 0.20x | 0.14x |

- **dev-a4ac0d31 这条腿在 x86 上无效**：什么都没追踪到（reported 0，吞吐等于不追踪）。它早于 49bc1fd9（x86 自旋锁协议修复，NUC 11/03 发现 0ffe368b 起 x86 上 map 操作卡死），所以不作比较。
- 计数：两个示例的程序都不是原子计数（funclatency `hist[slot] += 1`，源码里原子版被注释掉了；malloc 是 lookup、加一、`update(BPF_EXIST)`，用户态每秒读完就删 key），多线程下 kernel 和 bpftime 都对不上，是工具的问题。多线程正确性看 Jetson 上 libbpf-tools funclatency 和 bpftrace。

## 2. malloc 示例为什么塌（`profile/`，`perf record -e cpu_core/cycles/u`）

malloc 示例每次调用 `bpf_printk`。bpftime 把 printk 格式化后写到被追踪进程的 stdout 并 fflush：热点是 `_IO_fwrite`、`_IO_fflush`、`bpftime_trace_printk`、fmt，多线程时都排在 stdio 的 FILE 锁上（等锁的时间在内核里，`:u` 看不到）；hash map 只占 1–2%。kernel 的 bpf_printk 写每 CPU 的 trace 缓冲，所以能扩展。

## 3. 去掉 printk 的 malloc 示例（`ex-noprintk/`，`measure-mt-noprintk/`，`summary-noprintk.md`）

| 线程 | kernel | 7ad5a152 | 7ad5a152 / kernel |
|---:|---:|---:|---:|
| 1 | 1.92M | 5.24M | 2.7x |
| 2 | 2.75M | 7.41M | 2.7x |
| 4 | 4.91M | 6.32M | 1.3x |
| 6 | 4.51M | 4.11M | 0.91x |

所有线程更新同一个 key（每 pid 一个计数），bpftime 在 2 线程到顶后下降，kernel 在 4 线程到顶。6 线程的 profile（`profile-noprintk/`）：`fix_size_hash_map_impl::elem_update` 8.7%（同一把 stripe 锁、同一个缓存行），Frida 的 `_gum_function_context_begin_invocation` 7.4%（每个被 hook 函数一个计数，所有线程原子增减）；2 线程时 Frida 居首（10.5%），map 更新 3.3%。
