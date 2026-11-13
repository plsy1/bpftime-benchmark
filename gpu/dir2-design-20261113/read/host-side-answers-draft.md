Scratch draft (host side / cross-process view), dev 52126f33. Final answer goes via StructuredOutput.

Q1 serve_cuda_request: bpf_attach_ctx_cuda.cpp:126-187; lookup/update/delete -> bpftime_map_*_elem with from_syscall=true (bpftime_shm.cpp:88-144); gpu_address_of_map_value :559-574; segment registered bpftime_shm_internal.cpp:1407-1474.
Per-CPU via host call: userspace path (map_handler.cpp:171-183, 487-495, 815-826); thread_local buffer map_common_def.hpp:208-214 -> NULL (inferred). Update layout mismatch (trampoline copies value_size, default_trampoline.cu:354-363; host expects stride*ncpu, per_cpu_*:userspace) (inferred).
1501: Jetson -> HASH (syscall_context.cpp:568-575, bpftime_shm.hpp:137-155, 3823cac5); IPC: value_buffer staging copy (nv_gpu_shared_array_map.cpp:70-128).
Q2 hash: replace_locked bpftime_hash_map.hpp:529-538; update :715-745; delete :755-781; header :100-103, :523-524; CLAUDE.md:853-860. No hash pin (only arrays: bpf_attach_ctx_cuda.cpp:469-503; CLAUDE.md:242-243).
Q3 loader: syscall_context.cpp:600-679, default :852-854, mmap :1036-1048; 1527 drain nv_gpu_ringbuf_map.cpp:56-104 via syscall_server_main.cpp:610-624. Channels: agent.cpp IPC socket :227-418, SIGUSR1 :583-594/:1215, epoch_seq bpf_attach_ctx.cpp:201-283 + auto-refresh agent.cpp:740-785.
Q4: CPU helpers from_syscall=false bpf_helper.cpp:401-419; launchlate.bpf.c:46-111; launchlate.c:159-175,261,296.
Q5: hooks nv_attach_impl.cpp:436-534; agent.cpp:841-857; note() probed_launch_activity.cpp:16-47; any_running :49-72; event_query_every_us=1000 cuda_watcher_policy.hpp:28; detach wait nv_attach_impl.cpp:159-170 filled only by record_patched_launch :1521-1525 (late path, frida_setup.cpp:193/242/570); substitution listeners return early frida_setup.cpp:262-268.
