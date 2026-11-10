# x86 上重测系统调用分发修复（2026-10-29 下午，NUC）

按 `summry/jetson` 的 `syscall/x86-nuc-dispatch-fix-handoff-20261029.md` 做：开发分支 `2e850c45`（"syscall trace: pass system calls
without programs straight through"）。对照组是同一台机器上 10/29 上午的 `7a7d29b8`（[上午的报告](bpftime-x86-nuc-syscall-20261029.md)）。
方法、脚本和频率设置都没变，只换了构建目录。原始数据在 `benchmark-results/x86-nuc` 的 `syscall/x86-nuc-dispatch-fix-20261029/`。
交给 Jetson 的事项见[发现文件](bpftime-x86-nuc-dispatch-fix-findings-20261029.md)。

## 1. 结论

1. **修复在 x86 上正确**：
   - 构建通过，警告和上午相同；每日过滤器 113/113。
   - 以 root 跑 `bpftime_syscall_trace_attach_tests`，**8/8 通过**，包括新加的 4 个用例。Jetson 上因 arm64 没有 `fork` 而失败的两个旧用例，在 x86 上也通过了。
   - 两组频率各 45 次拆层运行全部有效，计数准确。
2. **分发层（没有匹配的程序）从约 220 周期降到约 40 周期**：
   - 1.7 GHz：129.6 → 24.2 ns；4.4 GHz：51.0 → 8.9 ns。降幅 82%。
   - 修复后和 Jetson 的 48 周期基本相同。上午看到的"ARM64 比 x86 多约 130 周期"，几乎全在这次去掉的那部分里（两次 `malloc`/`free` 和动态 TLS），现在两种架构的分发成本一样了。
3. **挂一个空程序时，bpftime 的总开销（agent-exit − none）从 386 周期降到 263 周期**，降了 32%：
   - 1.7 GHz：+225.7 → +153.9 ns；4.4 GHz：+86.6 → +59.8 ns。
   - Jetson 修复后是 252 周期，两边几乎持平。
4. **官方 benchmark/syscall**：
   - bpftime 开销 / kernel 开销：1.7 GHz 从 3.9× 降到 **2.8×**，4.4 GHz 从 4.2× 降到 **2.9×**；Jetson 从 3.4× 降到 1.9×；官方仓库公布的 x86 结果是 2.3×。
   - x86 的倍数比 Jetson 高，不是因为 bpftime 更慢：同频下两边 bpftime 的增量差不多（+153 对 +162 ns），而 x86 内核挂一个空程序便宜得多（+54 对 +85 ns）。
5. **修复后 x86 剩下的两个大头**（perf，1.7 GHz）：
   - **zpoline 的 nop 滑道**：每次 write 约 130 个周期。所有 leg 都一样，约占修复后 bpftime 单程序开销的一半。上午的推测得到证实：write 的调用号是 1，每次要走 511 个 nop。
   - **override 回调的动态 TLS**：只在挂了程序时出现，约占 7% 的周期（`__tls_get_addr`、TLS init 函数），和 Jetson 上看到的相同。

## 2. 主要数字

### 拆层（中位 ns/write，括号里是周期数）

| | x86 1.7 GHz 修复前 | 修复后 | x86 4.4 GHz 修复前 | 修复后 | Jetson 修复前 | 修复后 |
|---|---:|---:|---:|---:|---:|---:|
| 跳板（rewrite − none） | 65.8（112） | 64.5（110） | 24.5（107） | 25.1（109） | 37.5（65） | 37.5（65） |
| 分发（agent-other − passthrough） | 129.6（221） | **24.2（41）** | 51.0（222） | **8.9（39）** | 202.1（349） | **27.8（48）** |
| 一个空程序（agent-exit − agent-other） | 29.5（50） | 65.3（112） | 11.1（48） | 25.5（111） | 42.2（73） | 79.7（138） |
| bpftime 总计（agent-exit − none） | 225.7（386） | **153.9（263）** | 86.6（377） | **59.8（260）** | 282.2（488） | **146.1（252）** |
| kernel 一个空程序（kernel-exit − none） | 52.0 | 51.6 | 22.0 | 21.5 | 84.1 | 85.7 |

"一个空程序"这一步变大了，原因和 Jetson 相同：设置 override 回调现在只在有程序时做，这部分成本从分发那一步挪了过来。

### 官方 benchmark/syscall（10 轮，均值）

| | x86 1.7 GHz 修复前 → 后 | x86 4.4 GHz 修复前 → 后 | Jetson 修复前 → 后 |
|---|---|---|---|
| kernel 增量 | +57.8 → +53.8 ns | +20.9 → +21.0 ns | +84.8 → +84.6 ns |
| bpftime 增量 | +224.2 → **+152.7 ns** | +87.4 → **+60.9 ns** | +286.6 → +162.4 ns |
| 倍数 | 3.9× → **2.8×** | 4.2× → **2.9×** | 3.4× → 1.9× |

### perf（1.7 GHz，2000 万次 write，每次 write 的周期数 / 指令数）

| | 修复前 | 修复后 |
|---|---:|---:|
| rewrite（只有跳板） | – | 563 / 1367 |
| agent-other | 794 / 2106 | **608 / 1481** |
| agent-exit | 837 / 2265 | 721 / 1806 |

- 修复后，分发比跳板多 45 个周期、114 条指令。Jetson 是 51 个周期、138 条指令。
- 修复前热点里的 `malloc`/`cfree`、`std::function`、`__tls_get_addr` 和 TLS init，在 agent-other 里全部消失。

## 3. 设置

- 机器和工具链：i5-1240P，内核 6.12.113，gcc-13 / LLVM 19.1.7 / LTO，构建目录 `build-2e850c45`。
- 频率：
  - 1.7 GHz（关 turbo）：实测 1709 MHz。
  - 4.4 GHz（开 turbo）：实测中位 4346 MHz。
- 绑核：拆层绑 CPU 2；官方脚本给 CPU 2 和 4。
- `vm.mmap_min_addr`：原值 65536，测量时设成 0，测完恢复。
- 两组测量和 perf 结束后都核对过，频率、空闲状态和 `vm.mmap_min_addr` 都已恢复。上午 1.7 GHz 那组 CPU 4 上限没恢复对的问题，这次脚本修正后没有再出现。
