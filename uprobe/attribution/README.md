# BPFtime–kernel attribution

1. [Matched top-level gap](bpftime-uprobe-matched-kernel-gap-arm64-20260812.md)：统一确认 BPFtime 相对 kernel 的 wall、cycles 和 instructions 差距。
2. [Production-path attribution](bpftime-uprobe-production-path-attribution-arm64-20260813.md)：通过同一诊断 runtime 二进制中的生产路径 A/B 定位具体操作成本。
3. [Remaining four map-operation leaf attribution](bpftime-uprobe-remaining-map-leaf-attribution-arm64-20260813.md)：per-CPU hash update、per-CPU array lookup/update和ordinary array update的同binary叶子A/B。
完整调查状态见 uprobe 总表：[12-operation investigation matrix](../map-operation-investigation-matrix.md)。
