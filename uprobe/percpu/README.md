# Per-CPU map documents

主入口是 [Jetson path diagnosis](bpftime-uprobe-percpu-path-diagnosis-20260805.md)，包含 per-CPU array/hash lookup、update 和 hash delete-hit 的主要成本路径。

- [Per-CPU hash lookup leaf attribution](bpftime-uprobe-percpu-hash-lookup-leaf-attribution-20260813.md)：将完整 `impl.find()` 继续拆为 hash、equality、交互项和 Boost container 剩余路径。
