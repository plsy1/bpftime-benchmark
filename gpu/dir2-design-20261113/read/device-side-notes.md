Device-side facts for direction 2 (draft kept for durability; final answer goes through StructuredOutput).
Worktree: /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate at 52126f33.

Key citations
- host_call: attach/nv_attach_impl/trampoline/default_trampoline.cu:129-174
- CommSharedMem: trampoline/gpu_helper_protocol.h:69-84; offsets request_id +12, map_id +16 (trampoline_ptx.h:362-363), req +24 (protocol.h:104), map_update.value +1048600 (trampoline_ptx.h:848), resp +2097184 (trampoline_ptx.h:420); flags +2097176 and time_sum +2097192 computed
- watcher serve: runtime/src/attach/bpf_attach_ctx_cuda.cpp:124-187, 303-330
- map_info: default_trampoline.cu:84-101; create_map_basic_info bpf_attach_ctx_cuda.cpp:505-554; bpf_attach_ctx.cpp:643-644
- map fd: ptxpass_core/src/core.cpp:226-273 (no lddw helpers) + llvmbpf compiler.cpp:1111-1127 (submodule; overlay leaves src==1 alone)
- map type remap on integrated GPU: bpftime_shm.hpp:138-154, syscall_context.cpp:566-577 (3823cac5)
- 1b3d9c99: bpf_attach_ctx_cuda.cpp:469-503, 526-528; default_trampoline.cu:265-275, 336-353, 372-375
- aa6eb973: nv_attach_utils.cpp:155-171; default_trampoline.cu:124-127, 410-422; nv_attach_impl.cpp:2378-2382; nv_attach_fatbin_record.cpp:293-295
- compile target: trampoline Makefile:3,10-12,21,25-26; passes "sm_61"; nv_attach_utils.cpp:31-67; nv_attach_fatbin_record.cpp:103-160; nv_attach_impl.cpp:2245-2301
- ringbuf: default_trampoline.cu:424-485; nv_gpu_ringbuf_map.cpp:11-104; poll path syscall_context.cpp:1293-1299, bpftime_shm_internal.cpp:1620-1639
- segment registration: bpftime_shm_internal.cpp:1407-1475; size bpftime_config.hpp:81, bpftime_config.cpp:119-120
