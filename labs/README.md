# labs：动手验证用的小程序

第 2、3 章的部分"动手验证"需要一个不依赖 libbpf 的最小 loader。这里是它和两个配套程序：

| 文件 | 作用 |
|---|---|
| `loader.c` | 直接用 `bpf(2)` / `perf_event_open(2)` 建一个 ARRAY map、手写一段计数字节码、挂到 `victim` 的 `target_func` 上，每秒打印一次计数。环境变量 `DELAY`（建 map 后等几秒再建 prog/link）、`SECS`（运行秒数）。 |
| `victim.c` | 每 100ms 调一次 `target_func` 的目标进程。**必须 `-O0` 编译**。 |
| `poke.c` | 只调一次 `close(-1)`：用来演示"任何被拦截的调用都会让 syscall-server 启动"。 |
| `build.sh` | 编译以上三个程序。 |
| `ch02-shm.sh` | 第 2 章：段文件、假 fd、锁文件、导出 handler 表、第二个 syscall-server 清表。 |
| `ch03-refresh.sh` | 第 3 章：agent 先启动时只扫一次表；IPC `refresh` 之后才开始计数。 |

```bash
./build.sh
BPFTIME_BUILD=/path/to/bpftime/build ./ch02-shm.sh
BPFTIME_BUILD=/path/to/bpftime/build ./ch03-refresh.sh
```

脚本都用随机的 `BPFTIME_GLOBAL_SHM_NAME`，结束时删掉段和锁文件，不会碰默认的 `bpftime_maps_shm`。
需要的构建产物：`libbpftime-syscall-server.so`、`libbpftime-agent.so`、`bpftimetool`。
