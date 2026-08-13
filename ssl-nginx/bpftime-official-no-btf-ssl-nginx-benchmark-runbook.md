# bpftime official-no-btf `ssl-nginx` benchmark 操作手册（runbook）

Jetson 上 `bpftime-offical-no-btf` 的 ssl-nginx benchmark 全部操作方式的维护文档：
标准 docker 流程、两套消融变体的用法、一键批量脚本、归档与分析口径。
分析结论见姊妹文档
`bpftime-official-no-btf-ssl-nginx-path-ablation-20260726.md`。

## 1. 环境与关键路径

| 项 | 值 |
|---|---|
| 源码树 | `/home/jetson/src/bpftime-offical-no-btf`（分支 codex/official-no-btf，含 8 字节对齐修复） |
| 构建目录 | `build-alignment-test/`（RelWithDebInfo；产出 agent / syscall-server `.so` 与 bpftimetool） |
| Docker 镜像 | `bpftime:official-no-btf`（benchmark 脚本与 nginx/wrk 已内置；`.so` 和 sslsniff 由宿主机 mount 覆盖） |
| BPF 程序 | `example/tracing/sslsniff/`（`example/sslsniff` 是指向它的符号链接，容器内同构） |
| runtime 变体（④ 段内部消融） | `v4-ablation/`（no-affinity / no-mapops / no-ringwrite / noop-helper / original） |
| BPF 程序变体（V1–V4） | `v1-v4-ablation/`（v1-empty-probe / v2-no-copy-no-output / v3-no-output / v4-full） |
| 结果归档根 | `~/src/benchmark-results/` |
| 一轮完整 run | `draw_figture.py`：7 payload × 3 模式 × 10 次 wrk（100 连接 10s），约 50 分钟 |

时区注意：容器内为 UTC，宿主机 JST（+9）。结果文件名里的时间戳是 UTC。

## 2. 标准单轮流程（手动）

### 2.1 创建容器

```bash
IMAGE=bpftime:official-no-btf
NAME=bpftime-official-no-btf
SRC=/home/jetson/src/bpftime-offical-no-btf

sudo docker rm -f "$NAME" 2>/dev/null || true

chmod a+rx \
  "$SRC/build-alignment-test/runtime/agent/libbpftime-agent.so" \
  "$SRC/build-alignment-test/runtime/syscall-server/libbpftime-syscall-server.so" \
  "$SRC/build-alignment-test/tools/bpftimetool/bpftimetool"

sudo docker run -d \
  --name "$NAME" \
  --privileged \
  --pid=host \
  -v /sys:/sys \
  -v /lib/modules:/lib/modules:ro \
  -v "$SRC/example/tracing/sslsniff:/bpftime/example/tracing/sslsniff" \
  -v "$SRC/build-alignment-test/runtime/agent/libbpftime-agent.so:/bpftime/build/runtime/agent/libbpftime-agent.so:ro" \
  -v "$SRC/build-alignment-test/runtime/syscall-server/libbpftime-syscall-server.so:/bpftime/build/runtime/syscall-server/libbpftime-syscall-server.so:ro" \
  -v "$SRC/build-alignment-test/tools/bpftimetool/bpftimetool:/bpftime/build/tools/bpftimetool/bpftimetool:ro" \
  "$IMAGE" sleep infinity
```

### 2.2 执行（tmux 后台）

```bash
cd "$SRC"
sudo build-alignment-test/tools/bpftimetool/bpftimetool remove
LOG="$HOME/src/benchmark-results/bpftime-official-no-btf-ssl-$(date +%Y%m%d_%H%M%S).log"
tmux new-session -d -s bpftime-official-no-btf \
  "sudo docker exec -u 0 \"$NAME\" bash -lc \
  'cd /bpftime && umask 022 && \
   chmod a+rx /bpftime /bpftime/benchmark /bpftime/benchmark/ssl-nginx && \
   chmod a+r /bpftime/benchmark/ssl-nginx/index.html && \
   build/tools/bpftimetool/bpftimetool remove && \
   PYTHONUNBUFFERED=1 python3 benchmark/ssl-nginx/draw_figture.py' \
  2>&1 | tee \"$LOG\""
```

### 2.3 结果归档

```bash
DEST=~/src/benchmark-results/<实验名>/runXX   # 先 mkdir -p
sudo docker exec "$NAME" bash -lc '
  cd /bpftime/benchmark/ssl-nginx &&
  find . -maxdepth 1 -type f \
    \( -name "*.txt" -o -name "*.json" -o -name "*.png" \) \
    -print0 | tar --null -T - -cf -
' | sudo tar -C "$DEST" -xf -
mv "$LOG" "$DEST/"
```

### 2.4 清理

```bash
sudo docker stop "$NAME" && sudo docker rm "$NAME"
```

## 3. 变体运行

两套变体都是**预编译产物 + 换 mount**，镜像与 benchmark 脚本不动。可自由组合
（例如 v4-full BPF 程序 × no-affinity runtime）。

### 3.1 runtime 变体（④ 段内部消融，`v4-ablation/`）

换 2.1 中 agent / syscall-server 两行 mount：

```bash
V=no-affinity   # no-mapops / no-ringwrite / noop-helper / original
  -v "$SRC/v4-ablation/$V/lib/libbpftime-agent.so:/bpftime/build/runtime/agent/libbpftime-agent.so:ro" \
  -v "$SRC/v4-ablation/$V/lib/libbpftime-syscall-server.so:/bpftime/build/runtime/syscall-server/libbpftime-syscall-server.so:ro" \
```

- helper 改动只在 agent `.so` 里；syscall-server 五份字节一致，换不换均可。
- 重新构建：`v4-ablation/build-variant.sh <variant>`（脚本自带 pristine 校验与还原）。
- 变体含义与解读注意（no-ringwrite/noop 下消费者收不到事件等）见 `v4-ablation/README.md`。

### 3.2 BPF 程序变体（V1–V4，`v1-v4-ablation/`）

换 2.1 中 sslsniff 目录那行 mount（变体目录顶层即 `sslsniff` 二进制）：

```bash
V=v1-empty-probe   # v2-no-copy-no-output / v3-no-output / v4-full
  -v "$SRC/v1-v4-ablation/$V:/bpftime/example/tracing/sslsniff" \
```

- V2/V3 来自当时的 git 提交原件（`d28ca42` / `73e9b78`）；**V1 是按定义重构的**，
  严格续跑前建议 16B smoke 对照 `empty-probe/run01` ≈14.8k RPS。
- 重新构建：`v1-v4-ablation/build-variant.sh <variant>`。

## 4. 一键批量跑 v4-ablation 四个变体

```bash
sudo -v && tmux new-session -d -s v4-ablation \
  "sudo /home/jetson/src/bpftime-offical-no-btf/v4-ablation/run-all-variants.sh \
   2>&1 | tee -a /home/jetson/src/benchmark-results/v4-ablation/batch-\$(date +%Y%m%d_%H%M%S).log"
```

`run-all-variants.sh` 特性：

- 顺序跑 no-affinity → no-mapops → no-ringwrite → noop-helper（可传参指定子集/顺序）；
- 每变体自动：建容器 → 宿主机+容器 `bpftimetool remove` → **清掉镜像残留的旧结果文件**
  （历史归档里混入的 `benchmark_results_20260711_081448.json` 即此来源）→ 跑完整 run →
  归档到 `benchmark-results/v4-ablation/<variant>/runXX/`（run 号自动递增，日志随行）→ 删容器；
- 单变体默认 90 分钟超时（`PER_RUN_TIMEOUT` 秒可调），失败/超时不阻塞后续，结束打印汇总；
- 四个变体总计约 3.5 小时。进度：`tmux attach -t v4-ablation`。

## 4.5 单 payload 短测与事件审计（快速验证用）

完整 run 太久时用短测：单 payload、benchmark.py 一次（3 模式 × 10 wrk，约 6.5 分钟）。

```bash
# 单变体单 payload 短测（root）：
sudo v4-ablation/short-test.sh <variant> [payload_bytes]   # 默认 16
# 归档到 benchmark-results/v4-ablation/short/<variant>/<size>b-runXX/
# （含 benchmark.log、benchmark_results_*.json、env-snapshot.txt）

# 事件守恒审计（bpftime 腿单次 wrk，对账事件数/消费者CPU/worker迁移）：
sudo v4-ablation/event-audit.sh <variant> [payload_bytes]
```

短测方法学要点（2026-07-27 批次的实践）：判定阈值跑前预注册
（`benchmark-results/v4-ablation/short/PREREGISTRATION.md`）；批次首尾各跑一次
original 做 bookend（漂移 >2pp 全批降级）；主统计用中位数、MAD>3 标离群；
承载核心结论的变体跑 ≥2 轮；吞吐提升必须过事件守恒门槛（≈2 事件/请求，零丢失）
才承认。governor 保持 schedutil（与全部历史数据同条件），不要中途 jetson_clocks。

## 5. 归档与分析口径

- 目录约定：`benchmark-results/<实验名>/runXX/`，一个 runXX = 一次完整 `draw_figture.py`
  （7 个 `benchmark_results_*.json` + `size_benchmark_*.{json,txt}` + `absolute_performance.png` + 日志）。
- 现有实验名：`empty-probe`、`no-copy_no-output`、`no-output`、`full-unaligned`（异常对照）、
  `full-aligned`、`v4-ablation/<variant>`；旧诊断在 `old/`（已封存，不再引用）。
- 统计口径：`impact = (Baseline − Mode) / Baseline × 100%`，一律用**同轮 baseline**；
  版本间比较用 impact 差值（pp）。相对值 `(BPFtime/Kernel − 1) × 100%`。
- 手动归档时注意剔除时间戳明显不属于本轮的文件（批量脚本已自动清理）。

## 6. 必守事项（历史教训）

1. **每轮 benchmark 前必须 `bpftimetool remove`**（宿主机与容器内都做）：共享内存残留
   曾造成 13–14k RPS 的虚高双峰假象（见 unaligned-sequence 诊断结论）。
2. 变体产物一律走 `build-variant.sh`，不要手改 `runtime/src/bpf_helper.cpp` 或
   `sslsniff.bpf.c` 后直接编译——脚本会做 pristine 校验、还原和 sha256 溯源。
3. worktree 常态必须保持 full/pristine（两个 build 脚本的最后一次构建应为
   `original` / `v4-full`，确保 `build-alignment-test/` 与 sslsniff 二进制是 full 行为）。
4. tmux 里跑 sudo 前先 `sudo -v` 缓存凭据，避免后台会话卡在密码提示。
5. 结果没归档前不要 `docker rm` 容器（结果生成在容器文件系统内）。

## 7. 相关文档

- 分析结论（维护中）：`bpftime-official-no-btf-ssl-nginx-path-ablation-20260726.md`
- 消融数据说明：`~/src/benchmark-results/README.md`
- runtime 变体详情：`bpftime-offical-no-btf/v4-ablation/README.md`
- BPF 程序变体详情：`bpftime-offical-no-btf/v1-v4-ablation/README.md`
- 历史根因报告：`bpftime-software-perf-record-alignment-fix-20260722.md`、
  `bpftime-latest-jetson-ssl-nginx-bridge-path-analysis-20260721.md`

## 更新记录

- 2026-07-27：首版。收录标准 docker 流程、v4-ablation 与 v1-v4-ablation 变体用法、
  一键批量脚本、归档与分析口径、必守事项。
- 2026-07-27：新增 4.5 节：`short-test.sh` 单 payload 短测、`event-audit.sh`
  事件守恒审计，及短测方法学要点（预注册/bookend/中位数/守恒门槛）。
