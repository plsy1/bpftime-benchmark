# x86 上的 syscount-nginx（2026-10-29 晚，NUC）

按 `summry/jetson` 的 `syscall/x86-nuc-syscount-nginx-handoff-20261029.md` 做，开发分支 `59e5e154`。
- 原始数据在 `benchmark-results/x86-nuc` 的 `syscall/x86-nuc-syscount-nginx-20261029/`，其 README 有全部细节。
- 交给 Jetson 的问题见[发现文件](bpftime-x86-nuc-syscount-nginx-findings-20261029.md)。

## 1. 结论

1. **pthread key 冲突在 x86 上也存在，`59e5e154` 修好了。**
   - keycheck：修复前，跳板加 syscount 程序时 4 个新 key 读到 agent 的指针；修复后进程拿到 key 32..39，0 个被改坏。和 Jetson 完全一致。
   - nginx：修复前启动就报 `SSL_get_ex_new_index() failed` 和 `munmap_chunk(): invalid pointer`，就是用户 7 月在 x86 上记下的报错。
   - 测试：`bpftime_text_segment_transformer_tests` 2/2，每日过滤器 113/113，`bpftime_syscall_trace_attach_tests` 8/8。
2. **但 x86 上 nginx 仍然起不来：x86 跳板有第二个 bug，需要 Jetson 修。**
   - 现象：修复后，先加载程序再用跳板启动 nginx，nginx 在 `__libc_sigaction` 里段错误（r8 = 0x28）。
   - 原因：x86 的 zpoline 跳板调用 C 代码后，没有把 rdi、rsi、rdx、r10、r8、r9 和 xmm 寄存器恢复回来。而内核的系统调用只改 rax、rcx、r11，glibc 依赖这一点。
   - 验证：`regcheck` 证实挂了程序时这些寄存器全被改掉。官方 177a1dc 也一样，所以是上游 bug。开发分支从 `2e850c45`（直通分发）起，不挂程序时也会改掉 rdx。
   - 用户 7 月记的"去掉 SSL 的 nginx 改写后 SIGSEGV"，很可能也是这个 bug。
   - ARM64 不受影响：那边的跳板保存了全部寄存器。
3. **当时 x86 上无法按正确方式测量**：`run_correct.py` 和 perf 都没跑。
   - **后续（10/29 夜）**：Jetson 的 `0a602133` + `e299cd3f` 修好了跳板，修复后正确跑法已补测，见第 4 节和[验证发现](bpftime-x86-nuc-x86-hook-registers-findings-2-20261029.md)。
   - 补测结果：nginx 不再崩溃，bpftime 计数 = 内核。吞吐相对不追踪：bpftime −18.7% / −17.1%，内核 −5.3% / −3.6%，只加载跳板 −11.9%。
4. **审计结果和 Jetson 一致**：
   - 内核每个请求数到 9.015 次系统调用（Jetson 是 9.02）。
   - 官方方式和"跳板但 nginx 先启动"都数到 0，也就是官方脚本的 bpftime 两组什么都没追踪。
5. **官方脚本原样在 x86 上的结果**：CPU 0–7 钉 1.7 GHz、关 turbo，10 轮。内核 −4.3% / −2.9%，bpftime −1.1% / −1.6%。bpftime 看起来几乎没有开销，原因同上：什么都没追踪。

## 2. 对比表（吞吐相对不追踪）

| | x86 正确跑法（`e299cd3f`，10 轮） | x86 官方脚本（本次） | Jetson 正确跑法（run 3） | Jetson 官方脚本 | 用户 7 月 x86 官方脚本（i7-8750H） | 论文（Fig. 10，x86） |
|---|---|---:|---:|---:|---:|---:|
| 内核，追踪 nginx | −5.31% | −4.27% | −5.6% | −6.4% | −6.5% | −10.3% |
| 内核，追踪别的进程 | −3.63% | −2.92% | −3.6% | −3.7% | −4.8% | −9.6% |
| bpftime，追踪 nginx | **−18.68%** | −1.07%（什么都没追踪） | **−11.4%** | −2.2%（什么都没追踪） | −1.3%（什么都没追踪） | −3.36% |
| bpftime，追踪别的进程 | −17.11% | −1.57%（什么都没追踪） | −9.0% | −6.0%（什么都没追踪） | −0.7%（什么都没追踪） | 没有影响 |
| 只加载跳板，不挂程序 | −11.92% | – | −3.4% | – | – | – |
| 每个请求数到的系统调用 | 内核 9.015 / bpftime 9.015 | 内核未输出 | 9.02 / bpftime 9.02 | – | – | – |

- **绝对吞吐**：
  - x86 不追踪时，官方脚本那次 53,884 次/秒，正确跑法那次 58,371 次/秒。两次都是 1.7 GHz、4 个 P 核，但脚本、顺序、access.log 的增长都不同，中间还重启过，所以不直接比较。
  - Jetson 正确跑法是 27,928 次/秒。
- **官方脚本都在误导**：三份官方脚本的结果都显示"bpftime 比内核影响小"，但那是因为 bpftime 两组什么都没追踪。真正追踪了的两组都是 bpftime 比内核慢：x86 −18.7% / −17.1%，Jetson −11.4% / −9.0%。
- **每次 nginx 系统调用多花的时间**（每请求多花的时间 ÷ 9.015 次）：
  - x86（1.7 GHz）：内核 +107 ns，bpftime +437 ns，只加载跳板 +257 ns；
  - Jetson（1.728 GHz）：内核 +237 ns，bpftime +510 ns，只加载跳板 +139 ns。
  - x86 上只加载跳板的成本是 Jetson 的近两倍。可能是 zpoline 的 nop 滑道，加上新修复在每次调用上做的 XSAVE/XRSTOR，未单独测（见验证发现第 2 节）。

## 3. 设置

- 主机：i5-1240P，Debian 13，内核 6.12.113；bpftime 用 gcc-13 / LLVM 19.1.7 / LTO 构建（`build-59e5e154`）。
- 容器：基于 debian:trixie（glibc 与主机相同），nginx 1.26.3，wrk 4.1.0。参数 `--privileged --pid=host --shm-size=1g --cpuset-cpus 0-7`，并挂上主机的 tracefs。
- syscount：用 clang-19 编出来，没改源码。
- 测量时：官方脚本跑在 CPU 0–7（4 个 P 核的全部线程），钉 1.7 GHz、关 turbo，前后忙检查都通过。
- `vm.mmap_min_addr`：只在 keycheck、regcheck 和审计期间设为 0，之后都恢复成 65536。
- 恢复过程：结束时 `pin-cpus.sh` 恢复失败（SMT 兄弟两个都钉时它的状态文件里有重复项），已按记录手动恢复并核对，脚本也已修正。
- 收尾：容器已删除，镜像保留，Docker 已重新停掉。

## 4. 后续：x86 跳板寄存器修复后的正确跑法（10/29 夜，开发分支 `e299cd3f`）

原始数据：`syscall/x86-nuc-syscount-nginx-20261029/regs-fix/`。

**正确性**
- `e299cd3f` 在 x86 上用 LTO 能构建。每日过滤器 113/113，`bpftime_syscall_trace_attach_tests` 8/8。
- 新的 x86 跳板测试因为测试没注册 capstone 而必然失败（2/3，修复前后一样）。这一点交给 Jetson，见验证发现第 1 节。
- regcheck：不挂程序、挂 syscount 程序都是 0 个寄存器改变。打上变异补丁（修复前的跳板）后，又和 `59e5e154` 一样被改坏。
- 审计：先加载程序再用跳板启动 nginx，nginx 正常；bpftime 每个请求数到 9.016 次，内核 9.015 次。

**正式测量**：`run_correct.py --rounds 10`，6 组轮换，CPU 0–7 钉 1.7 GHz、关 turbo，60 次全部有效，nginx 一次都没退出。

| 配置 | 每秒请求数 | 相对不追踪 | 每次 nginx 系统调用多花 | 每个请求数到的调用 |
|---|---:|---:|---:|---|
| 不追踪 | 58,371 | – | – | – |
| 内核，追踪 nginx | 55,273 | −5.31% | +107 ns | 9.015 |
| 内核，追踪别的进程 | 56,251 | −3.63% | +72 ns | 0 |
| bpftime，追踪 nginx | 47,466 | **−18.68%** | +437 ns | 9.015（= 内核） |
| bpftime，追踪别的进程 | 48,383 | −17.11% | +392 ns | 0 |
| 只加载跳板，不挂程序 | 51,410 | −11.92% | +257 ns | – |
