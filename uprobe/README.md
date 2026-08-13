# Uprobe map investigation

- [综合调查报告](bpftime-uprobe-comprehensive-investigation-report-20260813.md)：**主入口**，统一说明 benchmark 语义修正、ARM64/x64 顶层结果、Jetson BPFtime–kernel 严格差额、L0–L3 分层和源码叶子归因。
- [12-operation investigation matrix](map-operation-investigation-matrix.md)：全部名义操作的语义、平台方向、调查阶段和下一步优先级。
- [Ordinary maps](ordinary/)：普通 array/hash 的 benchmark 语义、跨架构结果和路径分析。
- [Per-CPU maps](percpu/)：per-CPU array/hash 的跨架构结果和 Jetson 路径分析。
- [BPFtime–kernel attribution](attribution/)：严格 matched 顶层差距与生产 runtime 具体操作 A/B。
