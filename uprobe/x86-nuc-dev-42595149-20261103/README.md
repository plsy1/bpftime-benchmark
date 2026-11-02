# x86 NUC：dev 42595149 复测（2026-11-03 凌晨）

a4ac0d31 在 x86 上所有 bpftime 自旋锁都会卡死（`../x86-nuc-dev-a4ac0d31-20261102/`）。Jetson 会话修复后推送了 `dev/uprobe-official-177a1dc465ff`，现在的头是 `42595149`，按 Jetson 的请求在 x86 上复测。按 NUC 的规矩只测不改，开发分支没有改动。

- 新增的三个提交（`a4ac0d31..42595149`）：
  - `7d9690c3`：aarch64 的 syscall 改写让 hook 跑在自己的栈上（修 Go 程序崩溃）。x86 上新加的 `ensure_hook_stack()` 是空函数，其余改动都在 `__aarch64__` 分支里。
  - `49bc1fd9`：**x86 自旋锁修复**。只在 `__GLIBC__ && __aarch64__` 下用内联锁，其他平台调回 `pthread_spin_lock/unlock`。
  - `42595149`：hash map 的元素计数改成一个共享的原子变量，不再用每个 stripe（分段锁的一段）各自的配额。
- 机器：`debian-ichi-nuc`，i5-1240P，Debian 13，内核 `6.12.113+deb13-amd64`，glibc 2.41。
- 时间：01:30 构建，01:30–01:32 测试，01:32–01:34 反复跑新标签，01:34–01:39 uprobe 两轮对照。

## 0. 结论

- **修复有效，x86 上不再卡死。** runtime 单测 234 个用例，每个限时 60 秒：227 通过、7 失败，这 7 个就是 6ab9cbfe 上原有的那 7 个（要 root 或没开统计选项），sudo 下需要 root 的 4 个全过。a4ac0d31 是 165 通过、69 失败。
- **请求的标签全部通过**：`[spin-lock]`、`[bpftime_hash_map]`（16 个用例）、`[percpu-hash]`（15 个）、`[value-access]`、`[percpu-locks]`、`[home-stripe]`，以及新加的 `[bpftime_hash_map][capacity]`（元素计数在反复增删下不出错的测试）。
- **反复跑也稳定**：`[percpu-locks]` 50 次，`[value-access]`、`[home-stripe]`、`[spin-lock]`、`[capacity]` 各 20 次，再在所有 CPU 被 stress-ng 占满时跑 `[percpu-locks]` 20 次，共 150 次全过。Jetson 担心的 3 个时序用例在 x86 上没有出问题。
- **其他套件**：每日过滤器 140 个用例 139 过 1 跳过（要 root）；Frida 19/19；syscall trace 普通用户 6/9（同以前，读不了 `/sys/kernel/tracing`），sudo 9/9；x86 syscall 改写连跑 3 次 5/5。
- **uprobe 两轮对照**（只看有没有离谱的数，两次采样不够下性能结论）：
  - 探针、helper、数组 lookup、embed 都和冻结的 6ab9cbfe 持平（±2.3%）。
  - per-CPU hash 快了 21–30%，普通 hash update 快了 16%，和这几个提交的改动方向一致。
  - 普通 hash delete 慢了 14%（13.6 → 15.6 ns），普通 array update 慢了 11%（3.34 → 3.71 ns），per-CPU array update 慢了 4.5%。每组两次采样很接近，不像噪声，但每次操作只多 0.2–2 ns。按约定不做 x86 优化，只记录。

## 1. 构建（`build/`）

- 脚本是 a4ac0d31 那次的，只改了输出目录。构建目录 `/home/y/src/bpftime-wt-uprobe-177a1dc/build-42595149`。
- `source-state.txt`：`42595149`，`vm/llvm-jit` `87bac39a`，没有改动；`cmake-options.txt` 和上次一致；退出码 0；警告 10 条，和上次相同。
- 开发工作树是从 a4ac0d31 快进过来的，快进前没有本地改动。

## 2. 测试

见 `tests/README.md`。

## 3. uprobe 两轮对照（`uprobe-sanity/`）

- **方法**：官方 `benchmark/uprobe/benchmark.py --iter 2`，同一场里先跑 42595149，再跑 11/02 冻结的 6ab9cbfe，每条腿都带自己的 kernel 组。
  - 脚本 `run-sanity.sh` 由 6ab9cbfe 的 `full10/run-full.sh` 改来，只改了腿、轮数和超时（每次 `benchmark.py` 900 秒，超时放在 LD_PRELOAD 外面）；`session.sh` 负责测前检查残留进程，并把 plymouthd 临时绑到 E 核 12–14，测完改回 0–15。
  - CPU 2 开 turbo、钉 4.4 GHz，C6 及更深的空闲状态关闭，同 11/02。
  - 工作负载程序 `benchmark/test`、`benchmark/uprobe/uprobe` 和 11/02 相同（`a4ac0d31..42595149` 没有改 `benchmark/`），测前测后都核对了产物哈希（`artifact-sha256.txt`）。
  - 冻结的 42595149 产物在本机 `/home/y/src/bpftime-x86-runs/dev-42595149-20261103/uprobe-sanity/`，不提交。
- **频率**：E 核 15 上每秒采样，CPU 2 忙碌 > 90% 的 243 个采样都在 4,398–4,424 MHz。每条腿前后的忙碌检查都在 4.37–4.40 GHz。测完 CPU 2 的设置已恢复（`freq-restored.txt` 和测前只差瞬时频率）。
- **检查**：两条腿都通过了样本校验（17 + 17 + 1 个用例，每个 2 个有限正数）。测前测后都没有残留进程（`leftover-processes.txt`、`leftover-after.txt` 为空）。`session-exit.txt` 记的是 `run-sanity.sh` 的退出码 0；外层脚本最后退出码是 1，那只是最后一次 `pgrep` 没找到任何进程。
- **结果**：`sanity-4.4ghz-turbo/comparison.md`（`summarize.py`），摘要：

| 操作（ns） | 42595149 | 6ab9cbfe（同场） | 变化 | 42595149/kernel | 6ab9cbfe/kernel |
|---|---:|---:|---:|---:|---:|
| 普通 array lookup（不用返回值） | 0.45 | 0.45 | +0.1% | 0.25× | 0.25× |
| 普通 array update | 3.71 | 3.34 | +10.9% | 0.50× | 0.45× |
| 普通 hash lookup | 5.28 | 5.40 | −2.2% | 0.51× | 0.51× |
| 普通 hash update | 10.02 | 11.98 | −16.4% | 0.29× | 0.34× |
| 普通 hash delete | 15.59 | 13.63 | +14.3% | 0.46× | 0.39× |
| per-CPU array lookup | 1.28 | 1.28 | 0 | 0.70× | 0.70× |
| per-CPU array update | 4.94 | 4.73 | +4.5% | 0.66× | 0.64× |
| per-CPU hash lookup | 7.20 | 10.24 | −29.7% | 0.63× | 0.95× |
| per-CPU hash update | 11.68 | 16.14 | −27.7% | 0.45× | 0.64× |
| per-CPU hash delete | 25.17 | 31.69 | −20.6% | 0.22× | 0.28× |
| uprobe | 125.98 | 123.16 | +2.3% | 0.11× | 0.11× |
| uretprobe | 186.34 | 184.86 | +0.8% | 0.14× | 0.14× |
| uprobe + uretprobe | 217.76 | 212.98 | +2.2% | 0.16× | 0.16× |
| probe read | 1507.34 | 1506.66 | 0 | 0.08× | 0.08× |
| probe write | 1280.61 | 1279.73 | +0.1% | 0.07× | 0.06× |
| embed | 62.83 | 63.19 | −0.6% | | |

- 两条腿的 kernel 组大多互相差 2% 以内；例外是普通 hash lookup 2.9%、per-CPU hash lookup 5.9%、probe read 3.9%、probe write 8.9%（kernel 的 read/write 本身波动大）。上面几个 bpftime 的变化都比各自 kernel 组的差别大，或方向相反。
- 6ab9cbfe 这次的数和 11/02 十轮几乎一样（array update 3.34 对 3.36，hash delete 13.63 对 13.71），说明机器状态和那时一致。
- 两次采样：42595149 的 array update 是 3.78、3.63，6ab9cbfe 是 3.34、3.34；hash delete 是 15.56、15.61 对 13.59、13.67。

## 4. 清理

- `/dev/shm`：测试新留下的 3 个文件（共 0.14 GB，都属于用户 y）已删，列表在 `tests/dev-shm-removed.txt`；现在只剩测前就有的 5 个（`tests/dev-shm-before.txt`）。
- 没有残留进程；stress-ng 随脚本结束。没有起容器。`vm.mmap_min_addr` 恢复为 65536。
