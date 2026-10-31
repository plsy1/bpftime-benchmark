# notes：通读源码时的逐文件笔记

写正文之前，8 个读者分块逐文件读完官方源码（基线 `177a1dc465ff`）留下的笔记，每个事实都带 `路径:行号`。
比正文细，可以当源码索引用。

- `R1`–`R8`：各模块笔记（共享内存与 handler、map、两个入口、VM 与 helper、CPU 侧 attach、GPU、外围与构建、测试与示例）。
- `X-crosscheck.md`：跨模块问题的裁定。
- `00-outline.md`：写作前定的大纲。

这些是**原始笔记**，写作和核查阶段又修正过一些说法（例如 GPU 实际注册的 helper 数、per-CPU hash 的 flags 检查、
daemon 遇到 RINGBUF 的结局）。和正文或[附录 B](../B-known-issues.md) 冲突时，以正文和附录 B 为准。
