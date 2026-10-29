# 交给 Jetson 会话：分发修复（2e850c45）在 x86 上的重测发现（2026-10-29 下午，NUC）

结论报告：[bpftime-x86-nuc-dispatch-fix-20261029.md](bpftime-x86-nuc-dispatch-fix-20261029.md)；原始数据：`benchmark-results/x86-nuc` 的
`syscall/x86-nuc-dispatch-fix-20261029/`。NUC 上没有改开发分支。

## 0. 不需要修的

- **构建**：`2e850c45` 在 x86 上能构建（gcc-13 / LLVM 19.1.7 / LTO），警告和 `7a7d29b8` 相同。
- **测试**：
  - 每日过滤器 113/113。
  - 以 root 运行 `bpftime_syscall_trace_attach_tests`：8/8 通过，含新加的 4 个用例。
  - 在 arm64 上查 `fork` 失败的两个旧用例（`Test syscall dispatch - multiple`、`Test resolving private data`），在 x86 上通过。这也说明它们在 arm64 上失败只是因为测试本身依赖 `fork`。
- **计数**：两组频率各 45 次拆层运行，计数都是 1,000,000 / 1,000,000。

**没有需要修的 bug。** 修复的效果和 Jetson 一致：分发（没有匹配的程序）从 221 周期降到 41 周期（Jetson 从 349 降到 48）。

## 1. 修复后 x86 剩下的成本（供下一步排优先级，不是 bug）

1. **override 回调的动态 TLS**：
   - 只在挂了程序时出现。agent-exit 里约占 7% 的周期，包括 `__tls_get_addr` 4.1%、`curr_thread_override_return_callback` 的 TLS init 函数 2.0%、`__tls_get_addr@plt` 1.0%。
   - Jetson 上同样约占 7%（`_dl_tlsdesc_dynamic`）。
   - Jetson 提出改成初值平凡的指针（initial-exec），这个办法在 x86 上应该同样有效。这个变量 Frida uprobe 路径也在用。
2. **zpoline 的 nop 滑道（只影响 x86）**：
   - 上午的推测已用 perf 证实：rewrite leg 有 23.3% 的样本落在第 0 页的 nop 滑道（地址 1–511）。每次 write 约 130 个周期，所有 leg 都一样，约占修复后 bpftime 单程序开销（263 周期）的一半。
   - 原因：write 的调用号是 1，每次要走 511 个 nop 才到 0x200 处的跳转。
   - 影响：成本随调用号变化，号越小越贵；read 是 0，write 是 1，正好是最贵的两个。
   - 研究对象是 ARM64，这一点只在 x86 对照或向上游报告时有意义；要不要处理由 Jetson 会话和用户决定。
3. **x86 上官方倍数比 Jetson 高**：修复后是 2.8–2.9×，Jetson 是 1.9×。这不是 bpftime 的问题：同频下两边 bpftime 的增量相近（+153 对 +162 ns），而 x86 内核挂一个空程序更便宜（+54 对 +85 ns）。
