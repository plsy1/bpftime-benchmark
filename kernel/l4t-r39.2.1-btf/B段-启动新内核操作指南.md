# B 段：启动并验证新内核（6.8.12-1021-tegra-btf）

A 段已完成（2026-10-24）：新内核作为**非默认**启动项 `btf` 装好了，原内核 `primary` 仍是默认，什么都没坏。
B 段要真正用新内核启动一次。新内核如果起不来，或者起来了但 WiFi 不通，远程就连不上了，所以**这一步必须本人在 Jetson 旁边做**。

## 0. 准备

- Jetson 接上**显示器和 USB 键盘**（启动菜单只在屏幕/串口上显示，远程看不到）。
- 有线网线可选：WiFi 万一出问题，可以用有线登录。
- 确认没有测量在跑：`pgrep -a -x "wrk|nginx|sslsniff"` 没有输出。

## 1. 重启并选新内核

```bash
sudo reboot
```

屏幕上出现 **L4T boot options** 菜单（只停 **3 秒**，看到就马上按方向键）：

```
primary kernel
6.8.12-1021-tegra-btf (BTF, syscall tracepoints, kprobes)
```

- 用 ↓ 选第二项，按 Enter。
- 没来得及按也没关系：会自动进 `primary`（原内核），再重启一次就行。

## 2. 进系统后一键检查

登录后（远程或本地都行）运行：

```bash
sudo /home/jetson/src/l4t-r39.2.1/check-btf-kernel.sh
```

**期望结果：最后一行 `14 OK, 0 FAIL`。** 各项含义：

| 检查项 | 说明 |
|---|---|
| running 6.8.12-1021-tegra-btf | 确实进了新内核 |
| BTF / module BTF | `/sys/kernel/btf/vmlinux` 存在，CO-RE 程序能用 |
| syscall tracepoints | `syscalls:sys_enter_*` 存在（数量应有几百个） |
| kprobe event | 能创建 kprobe（脚本会建一个再删掉） |
| function tracer | fentry/fexit 所需的 ftrace 可用 |
| root on NVMe | 根分区正常挂载（说明 initrd 里的 NVMe 驱动没问题） |
| WiFi / Tailscale | 远程连接用的网络正常 |
| nvgpu / nvidia | GPU 和显示驱动加载成功 |
| r8168 | 有线网卡驱动可用 |
| perf PMU | perf 计数可用 |

脚本还会列出内核错误日志。下面这些**原内核开机时也有**，可以忽略（完整清单在 `dmesg-errors-stock.txt`）：

- `optee ... arm_ffa ... bus was not initialized`
- `camera-diag: Failed to find ISP device node` 及几行 camera-diagnostics
- `tegra210-i2s ... I2S4 TX event failed`

需要注意的是：`module load failures` 应该为 0；以及上面清单以外的新错误。

## 3. 结果处理

**全部通过 → 改成默认启动：**

```bash
sudo sed -i 's/^DEFAULT primary$/DEFAULT btf/' /boot/extlinux/extlinux.conf
grep ^DEFAULT /boot/extlinux/extlinux.conf     # 应输出 DEFAULT btf
```

改完可以再重启一次，确认不按键也会进新内核（`uname -r` 为 `6.8.12-1021-tegra-btf`）。原内核仍保留在菜单的第一项。

**有失败项 → 回到原内核，不用修任何东西：**

```bash
sudo reboot        # 菜单里不按键，默认进 primary
```

把 `check-btf-kernel.sh` 的输出和 `sudo dmesg > ~/dmesg-btf.txt` 留下，回头分析。

## 4. 起不来时的退路

| 情况 | 做法 |
|---|---|
| 新内核卡住、黑屏、重启循环 | 断电重启，在菜单里选 `primary kernel`（或不按键等它进默认项） |
| 已经改成 `DEFAULT btf` 后才出问题 | 启动菜单里选 `primary kernel`，进系统后执行 `sudo cp /boot/extlinux/extlinux.conf.before-btf /boot/extlinux/extlinux.conf` |
| 菜单也进不去（极少见） | 把 NVMe SSD 拆下接到别的 Linux 机器，把根分区里的 `/boot/extlinux/extlinux.conf.before-btf` 覆盖回 `extlinux.conf` |

## 5. 切换之后要做的事

- 告诉我已经切到新内核，我会：
  - 重新测 kernel 侧的对照数据（uprobe 微基准、ssl-nginx 的 kernel 组），因为打开 kprobe/ftrace 后 kernel 侧数字可能变化；
  - 先执行 `sudo jetson_clocks` 锁频再测（重启后频率不会自动锁定）。
- 注意：以后 `apt upgrade` 如果更新了 NVIDIA 的 L4T 内核包，可能会重写 `extlinux.conf`，`btf` 启动项会消失、默认变回原内核。届时让我重新加回即可（内核文件本身还在）。

## 相关文件

| 文件 | 用途 |
|---|---|
| `/boot/Image-btf`、`/boot/initrd-btf` | 新内核和它的 initrd |
| `/lib/modules/6.8.12-1021-tegra-btf/` | 新内核的模块（含 NVIDIA 树外驱动） |
| `/boot/extlinux/extlinux.conf.before-btf` | 改动前的启动配置备份 |
| `/home/jetson/src/l4t-r39.2.1/check-btf-kernel.sh` | 第 2 步的检查脚本 |
| `/home/jetson/src/l4t-r39.2.1/dmesg-errors-stock.txt` | 原内核开机时的错误日志，用于对照 |
| `/home/jetson/src/l4t-r39.2.1/README-btf-kernel.md` | A 段的技术细节（英文） |
