# x64 fixed-frequency value-size experiment

在此前运行 `kernel-map-runtime-x64-20260803-fixedfreq` 的同一台 x64 主机上，
按下面口径复现本目录的 Jetson 实验。不要使用 GitHub Actions 数据。

## 代码和结果分支

```bash
cd /home/y1/src/bpftime-offical-no-btf
git fetch benchmark
git switch codex/official-no-btf
git pull --ff-only benchmark codex/official-no-btf
git checkout 176eb291ead95eb5f8a56280deae626fac46eaa9

cd /home/y1/src/benchmark-results
git fetch origin
git switch benchmark-results/jetson
git pull --ff-only origin benchmark-results/jetson
```

开始前记录代码 branch、commit 和 dirty status。不得清理或覆盖用户已有修改；如果
工作树不干净，先判断是否影响这四个新增诊断文件。

## 构建

使用与 Jetson 相同的 BPF 编译器 Clang 15：

```bash
make -C /home/y1/src/bpftime-offical-no-btf/benchmark/uprobe \
  kernel-array-update-sizes-diagnostic \
  CLANG=/usr/lib/llvm-15/bin/clang
```

产物：

```text
benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes
benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes-victim
```

## 正式运行条件

沿用 `uprobe/kernel-map-runtime-x64-20260803-fixedfreq/` 的配置：

- CPU5；关闭其 SMT sibling CPU11。
- `intel_pstate` active mode。
- 所有 governor 设为 `performance`。
- `no_turbo=1`，`min_perf_pct=max_perf_pct=100`。
- 用 `turbostat` 验证正式测试期间 `Bzy_MHz=2200`。
- loader 和 victim 都以 root 运行并固定 CPU5。
- `kernel.bpf_stats_enabled=1` 仅在采集期间开启，结束恢复原值。
- 结束时恢复 CPU11、governor、turbo 和 pstate 百分比，并确认 BPF link 为零。

运行参数必须保持：

```text
value sizes: 8, 16, 32, 64, 128, 256 bytes
iterations: 20000
rounds: 5
warm-up: 1000
helper loops per BPF invocation: 1000
control/real order: odd rounds control-first, even rounds real-first
```

## wall-time

锁频完成后执行：

```bash
ROOT=/home/y1/src/bpftime-offical-no-btf
LOADER=$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes
VICTIM=$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes-victim

sudo sysctl -w kernel.bpf_stats_enabled=1
sudo turbostat --quiet --cpu 5 --out wall-turbostat.txt \
  --show CPU,Avg_MHz,Busy%,Bzy_MHz,TSC_MHz,CoreTmp,PkgTmp,PkgWatt -- \
  taskset -c 5 "$LOADER" "$VICTIM" 20000 5 5 1000 \
  >raw-wall.csv 2>raw-wall.stderr.txt
```

## PMU profile

使用此前已经验证的支持 `bpftool prog profile` 的二进制：

```text
/home/y1/src/.toolchains/bpftool-profile-v2/bpftool
```

分别采集：

```text
cycles
instructions
l1d_loads
llc_misses
```

必须采用 **pairwise** 方式：一次 loader run 只 profile 同一 value size 的
`kaus_<size>_ctrl` 与 `kaus_<size>_real`，不能同时挂 12 个 profiler，否则会发生
PMU multiplex。每个 size/metric 的操作为：

1. 创建尚不存在的 gate 路径。
2. 启动 loader：

   ```bash
   sudo taskset -c 5 env \
     KAUS_START_GATE=/tmp/kaus-<metric>-<size>.gate \
     KAUS_ONLY_SIZE=<size> \
     "$LOADER" "$VICTIM" 20000 5 5 1000
   ```

3. 找到新加载的 `kaus_<size>_ctrl` 和 `kaus_<size>_real` program ID。
4. 对两个 ID 分别启动：

   ```bash
   sudo "$BPFTOOL" -j prog profile id <id> duration 10 <metric>
   ```

5. 两个 profiler 均已启动后等待 1 秒，再创建 gate 文件放行 loader。
6. 保存 profile JSON、stderr、loader raw CSV、xlated dump、JIT dump 和
   `turbostat` 输出。
7. 每个 JSON 必须满足：

   ```text
   run_cnt == 101000
   enabled == running
   ```

每项净值计算：

```text
wall ns/helper
  = (real avg_ns/invocation - control avg_ns/invocation) / 1000

net counter/helper
  = (real counter/run_cnt - control counter/run_cnt) / 1000
```

## 结果目录和交付

创建：

```text
benchmark-results/uprobe/kernel-array-update-sizes-x64-20260803-fixedfreq/
```

至少保存：

- `README.md`
- `environment.txt`
- `raw-wall/`
- 四个 `raw-profile-<metric>-pairwise/`
- `wall-summary.csv`
- `profile-summary.csv`
- `combined-summary.csv`
- 精确执行脚本和汇总脚本
- 与 Jetson 的 `comparison-arm64.csv`

最终逐尺寸报告：

- x64 与 ARM64 的 wall ns/helper、cycles/helper、instructions/helper；
- `x64 - ARM64` 的绝对差值；
- 差值在 8–32B 是否近似固定；
- 64–256B 是否出现不同的复制斜率；
- 固定包装成本还是 memcpy 长度成本更能解释原始 8B 差距。

完成校验后提交并推送到 `benchmark-results/jetson`，报告提交哈希。不要覆盖本目录
的 Jetson 原始结果。
