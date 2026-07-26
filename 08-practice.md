# 第三部分：整体节奏、验证实验与已知不一致

**节奏**（按依赖序）：
第 1 天主线 13 步 → 子系统 1（shm，半天）→ 子系统 2（入口层，半天）→
子系统 3（attach，1 天）→ 子系统 4（vm，1 天）→ 子系统 5（maps/helpers，半天，
大半你已熟）→ 子系统 6（按需）。

**边读边做的实验**（利用现有环境）：
1. 读 shm 章时：跑一次 benchmark 不 remove，`bpftimetool export` dump 出 handler
   表对照 handler_variant 逐槽认类型。
2. 读 JIT 章时：`bpftime_prog_load` 后 dump JIT 符号表，确认
   `_bpf_helper_ext_0025` 的绑定地址就是 `bpf_perf_event_output`。
3. 读 attach 章时：给 `uprobe_listener_on_enter/on_leave` 各加一个计数器重编 agent，
   跑 16B 短测核对 2.001 事件/请求的另一半（enter 侧计数）。
4. 读 probe_read 时：确认 `ENABLE_PROBE_READ_CHECK` 在你的构建里是否打开
   （`grep -r PROBE_READ_CHECK build-alignment-test/CMakeCache.txt`），
   若开着，做一个关掉它的变体消融——直接回答 ③段 +3.7pp 的来源。

**已知的代码-注释不一致清单**（阅读时以代码为准）：
- `frida_uprobe_attach_impl.hpp:20-25`：UPROBE/URETPROBE 注释写反。
- `llvmbpf_vm::exec` 自称 "interpreter mode"，实为惰性 JIT。
- `per_cpu_hash_map` 注释称"sharable lock 保证线程安全"，实际 `should_lock=false`。

---
[← 返回目录](README.md)
