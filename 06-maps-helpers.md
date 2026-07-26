# 5. bpf_map/ 与 bpf_helper.cpp（map 实现与 helper 注册）

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `bpf_map/map_common_def.hpp` | 103 | 先读：`bytes_vec`、`ensure_on_current_cpu`（快照，便宜）vs `ensure_on_certain_cpu`（真迁移，昂贵） |
| `userspace/per_cpu_array_map.cpp` + `per_cpu_hash_map.cpp` | ~360 | per-CPU 用户态模拟的两种范式；`elem_*` vs `elem_*_userspace` 双接口 |
| `bpf_helper.cpp` | 1400 | 注册骨架 L1095-1369（helper_group 三大组）；你的锚点 `bpf_perf_event_output` L494 |
| `userspace/perf_event_array_map.cpp` | 83 | key=cpu → value=perf handler fd |
| `shared/perf_event_array_kernel_user.cpp` | 525 | 内核共享路径：user_ringbuffer + 动态生成的内核"搬运"程序 |
| `bpftime_probe_read`（bpf_helper.cpp L126-299） | — | SIGSEGV+改写 ucontext PC 的容错——**③段成本的来源候选** |

坑（多条与你的 benchmark 结论直接相关）：
- **per-CPU map 并不真正 per-CPU 隔离**：只是 getcpu 快照选 slot，迁移后两线程可写
  同一 slot；"正确性"依赖内核关抢占假设在用户态本就不成立——这正是删绑核序列的
  正确性论证背景。
- `elem_*`（helper 视角，单 CPU 切片）与 `elem_*_userspace`（syscall 视角，一次全部
  ncpu 份）语义不同，混淆会误读 value 大小。
- `per_cpu_hash_map::elem_delete` 只清零本 cpu 切片不删条目，**且清零范围疑似有
  bug**（fill 的是 `[begin, begin+cpu*value_size)`）；per_cpu_array 完全不支持 delete。
- `bpftime_tail_call` 每次调用都重新 new prog + 重注册 helper + 重 load（可能重
  JIT）——与内核 O(1) 跳转成本完全不同，若哪天 benchmark 带 tail call 要先看这里。
- `ENABLE_PROBE_READ_CHECK` 打开时 probe_read 每次 1-2 个 sigaction syscall——
  **这是 ③段 +3.7pp 的头号嫌疑，值得做下一轮消融**。
- `bpf_probe_read_str` 只是 strncpy，不返回内核语义的长度，已知偏差。

验证问题：`call 25` 从 VM 到 perf_event_handler 经过哪几层、map 类型在哪步分流
userspace/kernel-shared？关掉 `enable_shm_maps_helper_group` sslsniff 会死在哪个
helper？probe_read 的 SIGSEGV 恢复在 aarch64 改写哪个寄存器？

---
[← 返回目录](README.md)
