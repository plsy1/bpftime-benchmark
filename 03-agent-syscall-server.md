# 2. agent 与 syscall-server（两个 .so 的入口薄层）

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `runtime/syscall-server/syscall_server_main.cpp` | 276 | 全读：syscall() 分流表 |
| `runtime/syscall-server/syscall_context.cpp` | 1097 | `handle_sysbpf`(L373)、`handle_perfevent`(L697)、`handle_mmap64`(L840) |
| `runtime/syscall-server/syscall_server_utils.cpp` | 228 | `start_up`；uprobe perf type 的 mock |
| `runtime/agent/agent.cpp` | 1030 | 只看主线：shm 打开→register_attach_impl→init_attach_ctx；IPC/refresh 可略 |
| 两个 `CMakeLists.txt` | — | 确认 ".so = 薄入口 + 整个 runtime 静态库" |

坑：
- **两个 .so 各链一份完整 runtime 静态库**——同一段 helper/map 代码在两个进程各有
  一份，真正共享的只有 shm 数据。（这就是为什么你的消融只需换 agent .so。）
- agent 有三种进入方式（LD_PRELOAD / Frida 注入 / text-transformer），且有一套防
  重入 IPC（abstract unix socket `bpftime-agent-<pid>`）——读主流程时容易被绕晕，先跳过。
- `init_original_functions` 里 `unsetenv("LD_PRELOAD")`：**子进程不继承注入**——
  调试 fork/exec 场景必记。
- 每个 `handle_*` 都有 `run_with_kernel`（BPFTIME_RUN_WITH_KERNEL）分支——读主线时
  一律先忽略。
- 返回给 libbpf 的假 fd 会用 `open_fake_fd()` dup 占位，让号码在进程 fd 表里也被占住。

验证问题：loader `mmap(perf fd)` 拿到的内存来自哪里？agent 为何要重试 60 次开 shm？
SIGUSR1 的 detach 为什么要经过 pipe 而不在信号处理函数里直接做？

---
[← 返回目录](README.md)
