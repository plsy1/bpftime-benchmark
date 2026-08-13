# Meeting - 20260805 Speaker Notes

> Speak only the English part. The Chinese text is for reference.

## Opening / 开场

**English:** This week, I corrected the map benchmark semantics, compared ARM64 with x64, and performed layered tests of BPFtime's map-helper execution path on Jetson.

**中文参考：** 这周我修正了 map benchmark 语义，对比了 ARM64 和 x64，并在 Jetson 上对 BPFtime 的 map-helper 执行路径进行了分层测试。

## Corrected Benchmark Semantics / 修正 Benchmark 语义

**English:** I first checked what each benchmark case actually measures. Lookup can be either a hit or a miss, update can either overwrite an existing key or insert a new key, and delete can either remove an existing key or miss. In this benchmark, both array and hash lookup are stable lookup hits. Array update always overwrites a preallocated slot. Hash update uses `BPF_ANY`; it inserts the keys only during the first invocation, so almost all timed operations are steady-state updates of existing keys. The problem was hash delete. Its original setup created the keys only once. The first invocation deleted them successfully, but almost every later invocation measured delete misses. I fixed this by restoring the keys outside the timed region before every sample, so the corrected case consistently measures delete hits. Array delete is not a valid comparison because BPF array maps do not support deletion, so I excluded it. The same correction was applied to ordinary and per-CPU hash delete.

**中文参考：** 我首先确认了每个 benchmark case 实际测量的语义。Lookup 可以是 hit 或 miss，update 可以是覆盖已有 key 或插入新 key，delete 可以是成功删除已有 key 或 miss。在这个 benchmark 中，array 和 hash lookup 都是稳定的 lookup-hit。Array update 总是覆盖预分配的 slot。Hash update 使用 `BPF_ANY`，只在第一次调用时插入 key，因此几乎所有计时操作都是稳态 update-existing。真正的问题在 hash delete：原始 setup 只创建一次 key，第一次调用能成功删除，后续几乎全部测到 delete-miss。我在每个计时样本前、计时区域外恢复 key，使修正后的 case 稳定测量 delete-hit。BPF array map 不支持 delete，因此 array delete 不是有效性能对比项，我将它排除。这个修正同时应用于普通和 per-CPU hash delete。

## Platform Results / 平台结果

**English:** For ordinary maps, BPFtime wins hash update and hash delete on both platforms. Array update is the only ARM64/x64 winner reversal. Both kernels use the same C source, but ARM64 inlines `bpf_obj_memcpy()` into `array_map_update_elem()` and reaches `__memcpy` directly, while x64 retains the out-of-line path through `bpf_obj_memcpy()` and `memcpy_orig()`. For per-CPU maps, kernel BPF wins every valid operation on both platforms.

**中文参考：** 普通 map 中，BPFtime 在两个平台的 hash update 和 hash delete 上都更快。Array update 是唯一的 ARM64/x64 胜负翻转项。两端内核使用同一份 C 源码，但 ARM64 把 `bpf_obj_memcpy()` 内联进 `array_map_update_elem()` 并直接进入 `__memcpy`，x64 则保留了经过 `bpf_obj_memcpy()` 和 `memcpy_orig()` 的非内联路径。Per-CPU map 的所有有效操作在两个平台上都是 kernel BPF 更快。

## Runtime Layers / Runtime 层级

**English:** I use four diagnostic layers to describe the BPFtime map-helper execution path. L0 is the concrete map implementation. L1 is the generic map handler. L2 is shared-memory fd and type dispatch. L3 is the complete helper entry called by the JIT-compiled BPF program.

**中文参考：** 我使用四个诊断层描述 BPFtime map-helper 的执行路径。L0 是具体 map 实现，L1 是 generic map handler，L2 是 shared-memory fd 和类型分发，L3 是 JIT 编译后的 BPF 程序调用的完整 helper 入口。

## Major Cost Locations / 主要成本位置

**English:** For ordinary array lookup and update, the concrete L0 array operation is small. Most of the measured BPFtime internal cost appears in the L1 generic handler and the L2 shared-memory dispatch path.

**中文参考：** 对于普通 array lookup 和 update，具体的 L0 array 操作成本很小。测得的 BPFtime 内部成本主要出现在 L1 generic handler 和 L2 shared-memory 分发路径。

**English:** For ordinary hash lookup, the cost is split between the L0 hash implementation and the outer runtime path. L0 includes generic key comparison and probing, while the outer path adds BPFtime's userspace lock and logging.

**中文参考：** 对于普通 hash lookup，成本分布在 L0 hash 实现和外层 runtime 路径中。L0 包含通用 key 比较和 probing，外层路径增加了 BPFtime 的 userspace 锁和日志操作。

**English:** For per-CPU array operations, the internal cost starts in L0 with userspace CPU selection and function dispatch, and continues through the common L1 and L2 runtime layers.

**中文参考：** 对于 per-CPU array 操作，内部成本从 L0 的 userspace CPU 选择和函数分发开始，并继续经过公共的 L1 和 L2 runtime 层。

**English:** For per-CPU hash lookup and update, the dominant cost is already inside L0. It comes from BPFtime's Boost shared-memory hash and vector representation. The outer L1 and L2 layers add more overhead, but they are not the main part.

**中文参考：** 对于 per-CPU hash lookup 和 update，主要成本已经出现在 L0，来自 BPFtime 的 Boost shared-memory hash 和 vector 表示。外层 L1 和 L2 还会增加成本，但不是主要部分。

**English:** For per-CPU hash delete, the dominant L0 cost is synchronous vector and node destruction and shared-memory reclamation before the helper returns.

**中文参考：** 对于 per-CPU hash delete，主要的 L0 成本是 helper 返回前同步执行 vector/node 析构和共享内存回收。

## Conclusion / 结论

**English:** In summary, the corrected benchmark now provides stable and comparable semantics. Ordinary map behavior is operation-dependent, while every valid per-CPU map operation is slower in BPFtime on both platforms. The layered tests also identify where time is spent inside BPFtime's map-helper execution path. A strict end-to-end attribution of the BPFtime-kernel gap can be presented separately.

**中文参考：** 总结来说，修正后的 benchmark 现在具有稳定且可比的语义。普通 map 的表现取决于具体操作，而 BPFtime 的所有有效 per-CPU map 操作在两个平台上都慢于 kernel BPF。分层测试也定位了时间主要花在 BPFtime map-helper 执行路径的哪些位置。BPFtime 与 kernel 顶层差距的严格端到端归因可以另外汇报。
