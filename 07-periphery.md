# 6. 外围：daemon / verifier / tools / 单测

| 部件 | 关键文件 | 一句话 |
|---|---|---|
| daemon | `daemon/kernel/bpf_tracer.bpf.c`(732L)、`daemon/user/{bpf_tracer,handle_bpf_event,bpftime_driver}.cpp` | 内核 eBPF tracer 监控目标进程的 bpf()/perf_event_open()，用 `bpf_probe_write_user` 改写 uprobe 路径实现"透明劫持内核 eBPF 到用户态"；`relocate_bpf_prog_insns` 做 map fd→shm id 重定位 |
| verifier | `bpftime-verifier/src/{bpftime-verifier,platform-impl}.cpp` | PREVAIL 薄封装；**配置全是 thread_local**，跨线程校验前必须重新 set_*；只支持 uprobe/tracepoint + hash/array/ringbuf |
| tools | `tools/cli/main.cpp`(1275L) vs `tools/bpftimetool/main.cpp`(380L) | **cli 是注入器**（fork+LD_PRELOAD 或 Frida 注入），**bpftimetool 是 shm 直操作器**（不注入）——你天天用的 `remove` 就是它调 `bpftime_remove_global_shm` |
| 单测 | `runtime/unit-test/CMakeLists.txt` | Catch2 v3；`bpftime_runtime_tests` 可按 tag 过滤（如 `"[software_perf_event]"`）；attach 类测试要先 `make -C runtime/test/bpf` |

坑：daemon 有**两层 eBPF**（自己的内核 tracer vs 被劫持的目标程序），别混淆；
`daemon/test/test_daemon.cpp` 的 TEST_CASE 是空的，真测试在 test_daemon_driver.cpp。

---

---
[← 返回目录](README.md)
