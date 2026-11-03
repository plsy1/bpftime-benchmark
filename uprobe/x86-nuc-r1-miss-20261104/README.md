# x86 NUC：hash map 更新期间漏查（审查 R1），bpf 对照与 bpftime 修复（2026-11-04 凌晨）

由 Jetson 会话 bpftime-arm64环境-性能优化 通过 ssh 在 NUC 上执行（用户要求：直接在 NUC 上测 bpf，修 bpftime，再在 NUC 上测 bpftime）。机器同 `../x86-nuc-dev-d5279469-20261103`：i5-1240P，Debian 13，内核 6.12.113，glibc 2.41。开发分支提交 `7ad5a152` 在 NUC 上以独立 worktree `~/src/bpftime-wt-r1-7ad5a152`（d5279469 + `git am`，tree 与 Jetson 上相同：2bceadf7）构建，没有动 NUC 会话的 worktree 和构建目录。

## 问题

两个同桶的 key，两个线程不停更新、三个线程不停查找。d5279469 的更新先把新元素挂到链头、再摘掉旧元素（和 bpf 的 `htab_map_update_elem` 一样：`hlist_nulls_add_head_rcu` 后 `hlist_nulls_del_rcu`）；站在旧元素前驱上的读者两个都看不到，读到本桶终止符，漏查一个一直存在的 key。

## 1. bpf 自己的 hash map（`bpf/`）

- `r1.bpf.c` / `r1.c`：`BPF_MAP_TYPE_HASH`，64 项，`BPF_F_ZERO_SEED`（需要 root），4 字节 key，24 字节 value `{key, x, x}`；两个 key 按 jhash2（种子 0）`& 63` 选在同一个桶（27 号），用 next_key 顺序核对过桶号。XDP 程序由 `BPF_PROG_TEST_RUN` 驱动，`bpf_loop` 每次运行做 1000 次操作（不用 bpf_loop 时测试运行本身的开销把频率压到每秒 200 万次更新，0 次漏查，`bpf-run1.log`）。
- 读写频率：每秒 2200–5200 万次查找、500–900 万次更新，和 bpftime 压力测试同量级。
- 漏查：每轮 2 s，有的轮 0，有的轮很多。不绑核（`bpf-run2/3.log`）：15 轮中 3 轮漏查 0.095–0.22%。绑核（`placements-with-bpftime-d5279469.log`，u0,u1,r0,r1,r2）：

| 布局 | bpf 每轮漏查率（5 轮） | bpftime d5279469 |
|---|---|---|
| A 全在 P 核 0,2,4,6,1 | 0, 0, 0.029%, 0.11%, 0.18% | 0.001–0.065%，每轮都有 |
| B 全在 E 核 8–12 | 0, 0, 0, 0.54%, 0.63% | 0–4 次 |
| C 写者 E 核、读者 P 核 | 0, 0, 0, 0, 0.19% | 0–7 次 |
| D 写者 P 核、读者 E 核 | 0, 0, 0, 0.032%, 0.46% | 0.13–0.31%，每轮都有 |

- bpf 通过指针读还会读到拼接值（`extra_elems` 重用的窗口），每轮 0–23,025 次；bpftime 的 `lookup_read` 拷贝一直是 0。

## 2. bpftime 的修复（`bpftime/`，`r1bt.cpp` 同样的负载，同样绑核）

- `7f35edec`（新元素放进旧元素在链中的位置，next 用 acquire，每步重查状态字）：每轮 0–25 次漏查。去掉每步重查（`norecheck`）差不多，说明剩下的不是那条路径。
- 原因：读者读到链头 E 之后、读 E 的状态字之前，E 先被退役、又被重用到同一条链更靠后的位置；读者读到的是重用后的状态字，所以状态字检查都通过，沿 E 的新 next 走，跳过了前面的元素。
- `7ad5a152`：沿 next 走之前再读一次来路链接（链头或前一个元素的 next，acquire），必须还指向 E，状态字也不变，否则从头重来。**4 种布局 × 5 轮，约 35 亿次查找，漏查 0、拼接 0**（`placements-linkcheck.log`）。

## 3. 测试（`tests/`）

- 标签 `[bpftime_hash_map],[maps],[map-create],[percpu-hash],[hash-seed]`：62 个用例 60 过、2 跳过（要 root）。
- 4 个并发压力测试各 20 次，80 次全过；"hash map lookups of keys updated at once never miss" 现在要求漏查为 0，20 次都是 0。

## 4. 官方 uprobe 十轮（`full10/`，沿用 NUC 会话的脚本，只改了腿和目录）

7ad5a152 和 NUC 会话冻结的 d5279469 同场，CPU 2 开 turbo 钉 4.4 GHz，C6 及更深关闭；1192 个忙碌采样都在 4390–4425 MHz；两条腿都通过样本校验，测后频率设置已恢复，没有残留进程。7ad5a152 腿的第 3 个样本在所有 map 行都是离群值（hash lookup 10.86 对 4.72 ns），所以看中位数：

| 用例 | 7ad5a152 | d5279469 | 变化（中位数） |
|---|---:|---:|---:|
| hash update | 10.92 | 11.78 | −10.1% |
| hash lookup | 5.32（中位 4.72） | 4.94（中位 4.87） | −3.2% |
| hash delete | 15.04 | 14.61 | −0.5% |
| per-CPU hash update / lookup / delete | | | +0.6% / +2.2% / +1.0% |

array 的代码没有变，array delete 中位数也差 +5%，是布局带来的差别。完整表格：`full10/run/comparison.md`。

## 5. 键模式矩阵（`keypattern/`，11/04 02:50–03:20）

NUC 会话 11/03 的 run.sh / run_keys.py 复制过来，腿换成 dev-d5279469 和 dev-7ad5a152（构建同第 4 节），CPU 0–5 钉 3.5 GHz（turbo 开、C6 及更深关闭），测前测后检查、测后恢复。204 次运行全部 ok、计数精确（`summary.md`）。

- 1 个线程：map 开销 7ad5a152 / d5279469 = 0.96–1.03x，原地替换和链接复查在 x86 上看不出代价。
- 6 个线程：0.91–1.27x。这一列是总耗时减去对照的差，两条腿的总耗时只差 ±4%，差值的波动被放大，看不出方向。
