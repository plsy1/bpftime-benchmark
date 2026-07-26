# 1. runtime 核心：共享内存与 handler 注册表（"内核替身"）

一切的中心是 shm 里一个定长（默认 6144 槽）的
`boost vector<handler_variant>`——**下标即假 fd，variant 类型即 fd 种类**。

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `runtime/src/handler/handler_manager.hpp` | 133 | handler_variant 的 7 个成员、5 个 shm 命名对象常量（先读，全读） |
| `runtime/src/bpftime_shm_internal.cpp` | 1159 | **L640 构造函数是全子系统最密的 100 行**：三种 shm_open_type 分支 = server 建 / agent 开的协作 |
| `runtime/src/bpftime_shm_internal.hpp` | 306 | `class bpftime_shm` 当 API 目录读；`is_*_fd` 系列是 fd 判型层 |
| `runtime/src/bpftime_shm.cpp` | 714 | 纯 C ABI 转发层，扫一遍即可 |
| `handler/{prog,link,map}_handler.hpp` | ~420 | "shm 只放数据、VM 在各进程本地"的分界 |
| `runtime/src/bpftime_config.cpp` | 172 | BPFTIME_* 环境变量 → agent_config |

坑：
- shm 里只有可跨进程的数据（boost 容器 + offset_ptr）；JIT 代码、`bpftime_prog`
  都是各进程从指令字节私有重建的——**改 shm 里的 prog 不影响已实例化的 agent**。
- `shm_holder` 是 union 包裹 + placement new，为绕过静态析构顺序；
  `bpftime_destroy_global_shm` 只析构本进程对象，**删系统 shm 的是
  `bpftime_remove_global_shm`**（即 `bpftimetool remove`——你的清理协议的原理）。
- server 用 `SHM_CREATE_OR_OPEN` 而非删了重建，是为了让已注入的 agent 保持映射；
  配套 epoch seqlock（奇数=server 正在 reset）。**这正是你踩过的"shm 残留"双峰
  假象的结构性根源。**
- `clear_id_at` 会级联删除引用该 perf/prog 的 link（O(n) 线性扫全表）。
- handler 注册表本身**没有全局锁**：假定写方基本只有 server。

验证问题：sslsniff 传给 helper 的 map fd 如何从整数变成 shm 缓冲区指针？server
重启会话后 agent 为何不用重新 mmap？agent 退出时 `__destruct_shm` 为何要检查
`SHM_OPEN_ONLY`？

---
[← 返回目录](README.md)
