# x86 跳板寄存器修复本身的成本（写循环，2026-10-29 夜，NUC）

按 `summry/jetson` 的 `syscall/x86-nuc-x86-hook-registers-handoff-3-20261029.md` 第 1 节第 2 步：在写循环上对比修复前后的跳板层。

- 修复前：`build-59e5e154`，跳板不保存寄存器。
- 修复后：`build-c23442ac`，恢复 6 个参数寄存器，XSAVE/XRSTOR 掩码 0xe7，跳过 red zone。

## 方法

- **脚本**：`measure-layers.sh`。用 `../../../x86-nuc-breakdown-20261029` 的 `run_layers.py` 和 `src/`（Jetson 的原样复制，写循环 100 万次 7 字节 write）。
- **设置**：和 10/29 下午的拆层相同。CPU 2 钉 1.7 GHz、关 turbo（`freq.sh`，C6 以上关闭），`vm.mmap_min_addr` 65536 → 0 → 65536（`mmap_min_addr.txt`）。
- **顺序**：按 A B A B 跑（`run1-59e5e154`、`run2-c23442ac`、`run3-59e5e154`、`run4-c23442ac`），每次 5 轮、9 层全跑。4 × 45 次全部有效。
- **perf**：最后对两个构建的 rewrite 层各做一次（`../../../x86-nuc-dispatch-fix-20261029/perf/profile.sh`，2000 万次 write）。`perf.data` 没提交，留在本机 `/home/y/src/bpftime-x86-runs/regs-fix-perf/`。
- **频率**：
  - 每次运行前、以及最后都做了忙检查（APERF/MPERF），全部 PASS，1709–1711 MHz（`*.freq-before.txt`、`freq-pinned.txt`、`freq-after.txt`）。
  - 每秒频率记录这次没有：参数顺序给错了，记录程序启动即退出（`freq-log.txt`），脚本已改正，没有重跑。
- **其他**：`plymouthd`（开机后一直占约 2.5% CPU）在测量期间临时绑到 CPU 8–15，测完改回（`plymouthd-affinity.txt`）。CPU 2 的设置测完已核对恢复。

## 结果（`compare.md`，周期按 1709 MHz 换算）

| 层（相对 none） | 59e5e154（修复前） | c23442ac（修复后） | 差 |
|---|---:|---:|---:|
| rewrite（只有跳板） | 65.0 ns（111 周期） | 216.1 ns（369 周期） | **+151.1 ns（+258 周期）** |
| agent-other（分发，无匹配程序） | 88.8 ns（152 周期） | 240.8 ns（411 周期） | +151.9 ns（+260 周期） |
| agent-exit（一个空程序） | 153.8 ns（263 周期） | 301.2 ns（515 周期） | +147.4 ns（+252 周期） |
| agent-count（两个计数程序） | 220.9 ns（378 周期） | 369.8 ns（632 周期） | +148.9 ns（+254 周期） |

两次同构建的运行之间相差不到 1.5 ns。修复对每次系统调用加了**约 255 个周期的固定成本**，和有没有程序无关。也就是说，只加载跳板时，x86 跳板从约 110 周期涨到约 370 周期。

## perf（`perf/`，rewrite 层，`hook-samples.py`）

| | 59e5e154 | c23442ac |
|---|---:|---:|
| 每次 write 的周期数 / 指令数（`perf stat`，含内核） | 562 / 1368 | 816 / 1396 |
| nop 滑道（第 0 页，地址 < 0x200）的样本 | 23.4%（≈131 周期） | 16.2%（≈132 周期） |
| `xsave (%rsp)` | – | **12.6%（≈103 周期）** |
| `xrstor (%rsp)` | – | **15.6%（≈127 周期）** |

- 指令只多了 28 条，周期多了 254 个，其中 XSAVE + XRSTOR 两条指令约占 230 个周期。nop 滑道修复前后都是约 131 个周期。
- 样本按"落在该指令上或之后"归属（`syscall_hooker_asm.dis.txt` 是修复后汇编的反汇编）。
- 内核样本因为以普通用户身份读取，显示为 `[unknown]`。
- i5-1240P 没有 AVX-512，掩码 0xe7 实际保存的是 x87、SSE、AVX 三部分。

## nginx 里 nop 滑道的加权估算（`nginx-nop-sled-estimate.md`）

- **估算方法**：
  - 写循环里 511 个 nop 约 131 个周期，即每个 nop 约 0.257 个周期。
  - 按 `../audit-e299cd3f.jsonl` 里 nginx 的调用分布加权：setsockopt ×2，close、recvfrom、openat、fstat、writev、sendfile、write 各 1 次。
  - 结果：平均每次调用走约 459 个 nop，**约 118 个周期**。
- **对照 nginx 里只加载跳板的开销**（`../correct/`）：每次调用多约 440 个周期（+257 ns），大致可以拆成：
  - XSAVE 修复本身约 255 个周期；
  - nop 滑道约 118 个周期；
  - 分发约 41 个周期（10/29 下午的写循环）。

  三项合计约 414，与 440 只差约 26 个周期。这是按写循环的单价估算的，没有在 nginx 里直接测。
