# ARM64 per-CPU top-level paired baseline

本目录复核已有的 ARM64 正式 `benchmark/uprobe` 5 轮 raw，不新增实验。

数据来源：

```text
../uprobe-top-arm64-20260803/raw/
```

每个同一 victim run 中同时包含普通 map 和 per-CPU map，因此使用同一个
`__bench_uprobe` 做 empty subtraction：

```text
net ns/helper = (map case ns/invocation - empty uprobe ns/invocation) / 1000
per-CPU extra = per-CPU net - ordinary net
```

`paired-extra.csv` 使用同一环境、同一 run 的配对差值，避免把不同日期或不同进程
的中位数直接相减。

## 语义边界

- `array_lookup`：lookup-hit；
- `array_update`：existing update（array 初始 entry 已存在）；
- `hash_lookup`：lookup-hit；
- `hash_update`：`BPF_ANY`，第一次调用可能 insert，之后主要是 existing；
- per-CPU array delete 不支持，不在本基线中；
- per-CPU hash delete 已在另一个修复后单轮目录中单独验证，不混入本 5 轮数据。

## 结果用途

这是后续 Jetson per-CPU array/hash 路径 A/B 的顶层基线。它只说明 per-CPU 相对
ordinary map 的新增量，不单独证明某一个源码函数的因果成本。下一步应在同一
harness 中测量 CPU 获取、slot 地址计算、key/value 容器访问和公共 handler/shm
层。

运行：

```bash
python3 parse.py
```
