# 3. attach/：事件挂载层（Frida uprobe）

runtime 与所有 attach 实现之间唯一的契约：`ebpf_run_callback` 三参签名
`(memory, size, ret)`（`base_attach_impl.hpp:29`，110 行先读完）。

| 文件 | 行数 | 看什么 |
|---|---:|---|
| `frida_uprobe_attach_impl.hpp` | 111 | 两层实体：`frida_attach_entry`（一个 attach id 一条）与 `frida_internal_attach_entry`（一个函数地址一条，多对一） |
| `frida_internal_attach_entry.cpp` | 344 | **热路径核心**：`uprobe_listener_on_enter`(L294)/`on_leave`(L315)/override_handler |
| `frida_uprobe_attach_impl.cpp` | 331 | attach 建立：module 校验、entry 复用、UREPLACE 降级为 OVERRIDE+wrapper |
| `frida_register_conversion.cpp` | 110 | GumCpuContext↔pt_regs，aarch64 分支（你的平台） |
| `syscall_trace_attach_impl.cpp` | 201 | 对比读：同一契约、不同 ctx（依赖 text_segment_transformer 而非 Frida） |

坑：
- **`frida_uprobe_attach_impl.hpp:20-25` 的 UPROBE/URETPROBE 注释写反了**，以代码
  为准（on_enter 跑 UPROBE、on_leave 跑 URETPROBE）。
- BPF 拿到的 ctx 是栈上 pt_regs **副本**：从不回写，BPF 改寄存器不影响被 hook 函数；
  普通 uprobe 的 BPF 返回值也被丢弃。
- uprobe（`gum_interceptor_attach`，listener）与 override/replace
  （`gum_interceptor_replace`，换函数入口）是 Frida 两种互斥机制。
- `current_thread_gum_cpu_context` 只在 on_enter/override 设置——**uretprobe 里取栈
  会失败**。
- nv_attach_impl（CUDA）占了目录近半代码量，与 uprobe 无关，先跳过。

验证问题：sslsniff 的 `pt_regs *ctx` 在哪行构造、生命周期？同一地址挂 3 uprobe +
1 uretprobe 时三种实体各几个？`bpf_override_return` 的"不执行原函数"经过哪几步？

---
[← 返回目录](README.md)
