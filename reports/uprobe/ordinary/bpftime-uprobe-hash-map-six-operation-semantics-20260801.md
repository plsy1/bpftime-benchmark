# `benchmark/uprobe` hash map 六种操作语义与 delete 修正（2026-08-01）

## 结论

对于 hash map，`lookup`、`update`、`delete` 不能只按三个 helper 名称理解。
key 是否存在会改变实际执行路径，因此完整的稳定操作语义应当有六种：

1. lookup-hit；
2. lookup-miss；
3. update-existing；
4. update-insert；
5. delete-hit；
6. delete-miss。

当前修改后的 `benchmark/uprobe` 实际覆盖其中三种：

| benchmark 项 | 修改后的准确语义 | 状态是否稳定 |
|---|---|---|
| `__bench_hash_map_update` | 近似 update-existing | 是，首轮 insert 可忽略 |
| `__bench_hash_map_lookup` | lookup-hit | 是 |
| `__bench_hash_map_delete` | delete-hit | 是，已修正 |

per-CPU hash map 的三个对应项目采用相同语义。lookup-miss、纯
update-insert、delete-miss 目前没有独立 benchmark 项。

## 六种语义

| 操作 | 测量前 key 状态 | helper/flag | 预期结果 | 测量后 key 状态 |
|---|---|---|---|---|
| lookup-hit | 存在 | `bpf_map_lookup_elem` | 返回 value 指针 | 存在 |
| lookup-miss | 不存在 | `bpf_map_lookup_elem` | 返回 `NULL` | 不存在 |
| update-existing | 存在 | `bpf_map_update_elem(..., BPF_EXIST)` | 覆盖 value | 存在 |
| update-insert | 不存在 | `bpf_map_update_elem(..., BPF_NOEXIST)` | 插入 key/value | 存在 |
| delete-hit | 存在 | `bpf_map_delete_elem` | 成功删除 | 不存在 |
| delete-miss | 不存在 | `bpf_map_delete_elem` | 返回失败 | 不存在 |

`BPF_ANY` 同时允许 insert 和 existing update，因此单看
`bpf_map_update_elem(..., BPF_ANY)` 无法判断测量的是哪一种路径，必须结合
map 前置状态解释。

## 当前 update 的实际语义

`uprobe.bpf.c` 中每次进入 `__bench_hash_map_update`，BPF 程序会对 key
`0..999` 执行 1000 次：

```c
bpf_map_update_elem(&hash_map, &key, &value, BPF_ANY);
```

victim 默认把该函数调用 100000 次。因此：

- 第一次函数调用：1000 次 update-insert；
- 后续 99999 次函数调用：99999000 次 update-existing；
- insert 只占全部 helper 调用的 `0.001%`。

所以当前结果可以视为 update-existing，首轮 insert 对平均值的影响可以忽略。
它不能用于说明 update-insert 的性能。

更准确的报告表述是：

> `__bench_hash_map_update` 使用 `BPF_ANY`。首轮插入 key，后续重复覆盖已有
> key，因此测量结果近似代表 update-existing 路径。

## 当前 lookup 的实际语义

lookup 在 update 之后执行。此时 key `0..999` 已经存在，并且 lookup 不改变
map 状态，因此全部 100000 次函数调用都是稳定的 lookup-hit。

每次函数调用包含 1000 次成功 lookup。当前 lookup 逻辑无需修改，只需在报告中
明确它是 lookup-hit，而不是笼统的 lookup 或 lookup-miss。

## 原 delete 的问题

原逻辑先让 update 填充 map，然后连续调用 `__bench_hash_map_delete` 100000 次。
每次 BPF 程序都尝试删除 key `0..999`：

```c
for (int i = 0; i < 1000; i++) {
    u32 key = i;
    bpf_map_delete_elem(&hash_map, &key);
}
```

因此原版实际是：

- 第一次调用：1000 次 delete-hit；
- 后续 99999 次调用：99999000 次 delete-miss；
- 成功删除只占 `0.001%`。

原 `__bench_hash_map_delete` 的平均值实际上代表空 map 上的 delete-miss，不能
解释成成功删除成本。

## 修改后的 delete-hit

现在为普通 hash 和 per-CPU hash 分别增加了 setup probe。每一个计时样本按以下
顺序执行：

```text
setup probe：重新写入 key 0..999        不计时
start_timer()
delete probe：删除 key 0..999           计时
end_timer()
```

对应代码：

- `benchmark/test.c`：`get_function_time_with_setup()` 在每个样本前调用 setup，
  只累计 delete 函数区间；
- `benchmark/uprobe/uprobe.bpf.c`：`DEFINE_HASH_DELETE_SETUP` 使用 `BPF_ANY`
  重新填充普通 hash 和 per-CPU hash。

这样每次进入 delete probe 时，1000 个 key 都已存在；计时区中的 1000 次
delete 均为 delete-hit。用于恢复状态的 1000 次 update 不包含在 delete 时间中。

## 输出单位

每个 uprobe benchmark 函数内部执行 1000 次 map helper。victim 输出的：

```text
Average time usage X ns
```

表示一次被测函数调用，即一批 1000 次 helper 的平均时间。换算单次 helper：

```text
单次 helper 平均成本约为 X / 1000 ns
```

修改后的 delete 计时为了在每轮之间恢复状态，使用每个样本单独开始/结束计时，
与其他非破坏性操作的一次性整段计时边界不同。一次 delete 样本包含 1000 个
helper，计时调用开销相对较小，但修复前后的 delete 绝对值仍不应直接当作同一
语义的数据比较：修复前是 miss，修复后是 hit。

## 尚未覆盖的三项

若需要完整六项，还应新增：

### lookup-miss

使用确定不存在的 key。还应明确是在空 map 上 miss，还是在指定装载率下 miss；
两者的 hash 探测长度可能明显不同。

### update-insert

每个样本前清空 map，在计时区内用 `BPF_NOEXIST` 插入 key `0..999`。完成一次
样本后必须在计时区外再次清空，不能连续重复调用后仍称为 insert。

### delete-miss

每个样本前确保目标 key 不存在，再在计时区内调用 delete。应把它单独命名为
delete-miss，不能与成功删除混合。

## 使用边界

- 当前 hash map 为 1024 entries，操作 key `0..999`，装载率接近 97.7%。hash
  hit、insert 和 populated-map miss 都会受到这一装载率影响。
- 修正后的 delete 状态保证针对默认单线程运行。多线程共享同一张 map 时，线程
  之间的 setup/delete 会互相竞争；若要测多线程，需要使用每线程独立 key 范围
  或独立 map。
- `BPF_MAP_TYPE_ARRAY` 和 `BPF_MAP_TYPE_PERCPU_ARRAY` 不支持删除。
  `__bench_array_map_delete` 与 `__bench_per_cpu_array_map_delete` 不能解释成
  delete-hit，它们属于无效操作/错误返回路径，不在本文的 hash delete 修正范围。

## 修改与验证状态

- 已修改：`benchmark/test.c`、`benchmark/uprobe/uprobe.bpf.c`；
- 已用仓库官方 Makefile 重新构建；
- 已完成 kernel eBPF 小迭代 attach/执行验证；
- 已完成 BPFtime 小迭代 attach/执行验证；
- 未运行完整多轮性能测试；
- 修改尚未提交。
