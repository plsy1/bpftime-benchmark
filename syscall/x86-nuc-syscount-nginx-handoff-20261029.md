# 给 NUC 会话的交接：x86 上按正确方式重跑 syscount-nginx（2026-10-29 晚）

写给 x86 NUC（i5-1240P，Debian 13，内核 6.12）上的 Claude 会话。**回复用户用中文**，NUC 上不改开发分支，推送前问用户。
机器、频率、绑核的规矩沿用前两份交接（`syscall/x86-nuc-syscall-handoff-20261029.md` 和
`syscall/x86-nuc-dispatch-fix-handoff-20261029.md`，都在 `summry/jetson`），以及你自己的
`x86-nuc-syscall-session-20261029/`（`freq.sh`、忙检查）。

## 0. 背景：官方脚本测的不是追踪开销

Jetson 今天跑了官方 `benchmark/syscount-nginx`，详情见 `benchmark-results/jetson` 的
`syscall/syscount-nginx-20261029/README.md`（必读）。要点如下。

1. **官方 `benchmark.py` 的 bpftime 两组什么都没追踪**：
   - nginx 只预加载了 `libbpftime-agent.so`，系统调用指令根本没被改写。拦截系统调用需要
     `AGENT_SO=agent` 加 `LD_PRELOAD=libbpftime-agent-transformer.so`。
   - 而且是先启动 nginx、后加载 syscount 的程序，agent 不会接上后加载的程序。
   - syscount 的输出被脚本丢到了 `/dev/null`。Jetson 上把输出留下来，bpftime 那两组数到的系统调用是 0，内核那组约 9 次/请求。
2. **按正确方式跑，nginx 一启动就崩溃**，崩在 `ngx_ssl_init` → `ERR_clear_error` → `free(): invalid pointer`：
   - 跳板用 `dlmopen(LM_ID_NEWLM)` 加载 agent，agent 有自己的一份 libc。两份 libc 各自分配 pthread key 编号，却写进同一个线程的同一张表，结果 OpenSSL 新建的 key 读到了 agent 的指针。
   - 开发分支 **`59e5e154`**（"text segment transformer: keep the agent's pthread keys apart from the process's"，已推送）修了这个问题。设计见 CLAUDE.md 的 "Agent Namespace And pthread Keys" 一节。
   - 这条加载路径 x86 也在用，所以 x86 上应该有同样的问题，修复后也应该同样有效。**这次要在 x86 上确认。**
3. **用户 7 月在 x86 上（i7-8750H）跑的官方脚本结果**：
   - 数字：内核 −6.5%（追踪 nginx）/ −4.8%（追踪别的进程），bpftime −1.3% / −0.7%。
   - 和论文一样，bpftime 看起来几乎没有影响，按上面第 1 点，这组数据同样是什么都没追踪。
   - 用户当时笔记里记了：给 x64 的 nginx 加载跳板后，OpenSSL 报 `SSL_get_ex_new_index() failed`；去掉 SSL 的 nginx 在改写后 SIGSEGV。前者很可能就是第 2 点的 bug。
4. **Jetson 按正确方式跑的结果**：干净 CPU，10 轮，各组轮换顺序，见上述 README 第 3 节。
   - bpftime 每个请求数到 9.02 次系统调用，和内核一致。
   - 吞吐相对不追踪：内核 −5.6% / −3.6%，bpftime −11.4% / −9.0%，只加载跳板不挂程序 −3.4%。

## 1. 要做的事（按顺序）

1. **构建**：
   - 开发分支拉到 `59e5e154` 或更新，用和 `build-2e850c45` 相同的选项建 `build-59e5e154`。目标和上次相同，再加 `bpftime_text_segment_transformer_tests`。
   - 这个测试目标现在在 x86 上也会构建，但只包含两个 pthread key 用例；改写指令的用例只在 aarch64 上有。
   - 跑每日过滤器，再跑 `bpftime_text_segment_transformer_tests`，应当 2/2 通过。
2. **先确认 bug 和修复**：
   - 用 Jetson 的 `src/keycheck.c`，在 `build-2e850c45`（修复前）和 `build-59e5e154` 上各跑一次。
   - 跑法：先 `LD_PRELOAD=<syscall-server> syscount -d 6` 加载程序，再用跳板启动 `keycheck`。用到 zpoline 的地方都需要 root 和 `vm.mmap_min_addr=0`，原值记下来，测完恢复。
   - Jetson 修复前的结果是 4 个新 key 读到了 agent 的指针，修复后程序拿到的 key 是 32..39，没有一个被改坏。
3. **环境**：
   - NUC 主机上没有 nginx 和 wrk，用 Docker。Jetson 用的是 `docker/Dockerfile`：基于 ssl-nginx 那个 Ubuntu 24.04 镜像，补装 nginx、wrk、curl、python3、sudo、matplotlib。
   - **注意 glibc**：NUC 主机是 Debian 13，在主机上构建的 bpftime 库可能需要比 Ubuntu 24.04 更新的 glibc。所以基础镜像建议用 `debian:trixie`，装同样的包。先在容器里跑一次 `ldd` 和 `keycheck`，确认库能加载。
   - 容器参数和 Jetson 一样：`--privileged --pid=host --shm-size=1g`，开发树和构建目录按原路径只读挂载，结果目录可写。`--pid=host` 是为了让内核 syscount 的 `-p` 和 bpftime 看到的是同一套 pid。
   - syscount：用 clang-19 构建开发树的 `example/tracing/syscount`（`make CLANG=clang-19`），不要改源码。按 Jetson 的 `view/` 目录布局放好：`benchmark.py` 用的还是旧路径 `example/libbpf-tools/syscount/syscount`。
4. **频率和绑核**：
   - 这次要压 nginx 和 wrk，单个核不够。建议给 nginx、wrk、syscount 一组 P 核，比如 CPU 0–7，全部钉在 1.7 GHz、关 turbo，用你的 `freq.sh` 和忙检查，并写进记录。
   - Jetson 上没有绑核，用了全部 6 个核。
   - **测前**用 `ps -eo pid,pcpu,etime,args --sort=-pcpu | head` 确认没有残留进程在占 CPU。Jetson 今天就被一个空转了 24 小时的残留进程影响，绝对吞吐低了 12～14%。
5. **审计**：在容器里用 `audit.py` 跑 4 种方式，每种 5 秒：
   - 内核；
   - 官方方式；
   - 跳板但 nginx 先启动；
   - 先加载程序、再用跳板启动 nginx。
   用 `build-59e5e154`。预期：官方方式和"跳板但 nginx 先启动"都数到 0，最后一种和内核一致（约 9 次/请求）。
   - 再用 `build-2e850c45` 只跑最后一种，确认修复前 nginx 会崩，并记下报错。用户 7 月看到的是 `SSL_get_ex_new_index() failed`。
6. **正式测量**：
   - `run_correct.py --rounds 10`，6 组轮换。用 Jetson 的 `summarize_correct.py` 汇总。nginx 不能崩，计数必须和内核一致。
   - 再跑一次不改动的官方 `benchmark.py`（10 轮，复制成 `runbench.py` 在 view 根目录跑，避开它的 `pkill -f syscount`），作为同一台机器上"官方口径"的对照。
   - **启动后台任务的命令行里不要出现 `syscount`、`syscall`、`bpftime`、`victim` 这几个词**：官方脚本的清理步骤会按这些词杀进程。Jetson 今天就因此把一个还在运行的脚本当成了已退出，重复启动，作废了一份数据。
7. **可选：perf**。Jetson 的 `profile_nginx.sh` 和 `profile_leg.py`（README 第 4 节）测出，每次被追踪的系统调用多 995 个周期：约 600 是 bpftime 自己的代码，约 440 是内核变慢，每次还多 17 次缓存缺失。可以在 x86 上对 native、transformer_only、bpftime_targeted 各测 3 次。x86 上还有 zpoline nop 滑道的成本，你下午已经测过了。

## 2. 结果放哪里

- 原始数据：`benchmark-results/x86-nuc` 的 `syscall/x86-nuc-syscount-nginx-20261029/`。镜像的 Dockerfile、view 的 sha256、审计、正式测量、官方脚本对照、频率记录都放这里。从 Jetson 复制来的脚本要记下来源提交和 sha256。
- 结论：写进 `summry/x86-nuc`，附一张对比表，列出 x86 正确跑法、x86 官方脚本、Jetson 正确跑法、用户 7 月的 x86 官方结果、论文这几列。
- 需要 Jetson 处理的问题，比如 x86 上修复无效、改写后 nginx 段错误、计数不一致，写进发现文件。不要改开发分支。
- 推送前先问用户。测完删掉容器，镜像保留。

## 3. Jetson 的参考数字

正确跑法，第 3 次运行（干净 CPU，Jetson Orin 6 核 1.728 GHz，nginx 单进程，wrk `-c 100 -d 10`）：

| 配置 | 每秒请求数 | 相对不追踪 | 每次系统调用多花 | 计数/请求 |
|---|---:|---:|---:|---|
| 不追踪 | 27,928 | – | – | – |
| 内核，追踪 nginx | 26,352 | −5.6% | +237 ns | 9.02 |
| 内核，追踪别的进程（`-p 1`） | 26,918 | −3.6% | +149 ns | 0 |
| bpftime，追踪 nginx | 24,746 | −11.4% | +510 ns | 9.02 |
| bpftime，追踪别的进程 | 25,411 | −9.0% | +393 ns | 0 |
| 只加载跳板，不挂程序 | 26,983 | −3.4% | +139 ns | – |
