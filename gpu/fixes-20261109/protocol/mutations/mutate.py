import sys
root='/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/'
m=sys.argv[1]
def sub(path, old, new):
    p=root+path; s=open(p).read()
    assert old in s, (m, old[:60])
    open(p,'w').write(s.replace(old,new,1))
T='attach/nv_attach_impl/trampoline/default_trampoline.cu'
if m=='none':
    pass
elif m=='payload-before-lock':
    # the request written by every lane before the lock, as before the fix
    sub(T,'''	HelperCallResponse resp = {};

	for (unsigned int turn = 0; turn < 32; turn++) {''','''	HelperCallResponse resp = {};
	fill(comm->req);

	for (unsigned int turn = 0; turn < 32; turn++) {''')
    sub(T,'''			comm->map_id = map_id;
			fill(comm->req);''','''			comm->map_id = map_id;''')
elif m=='no-module-lock':
    sub(T,'''			while (atomicCAS(&__bpftime_module_comm_lock, 0, 1) !=
			       0) {
			}''','')
    sub(T,'''			atomicExch(&__bpftime_module_comm_lock, 0);''','')
elif m=='no-thread-bounds':
    sub(T,'''	if (tid >= info.max_thread_count || idx >= (uint64_t)info.max_entries) {
		warn_outside_map(map, info, tid, idx);
		return nullptr;
	}''','')
elif m=='no-ringbuf-size-check':
    sub(T,'''	if (data_size > (uint64_t)map_info.value_size) {
		atomicAdd(&header->dropped, 1u);
		return 3;
	}''','')
elif m=='no-drop-count':
    s=open(root+T).read()
    s=s.replace('atomicAdd(&header->dropped, 1u);','')
    open(root+T,'w').write(s)
elif m=='watcher-sleeps':
    sub('runtime/src/attach/bpf_attach_ctx_cuda.cpp','''					last_request = clock::now();
					idle_sleep = cuda_watcher_min_sleep;
					continue;''','''					std::this_thread::sleep_for(
						std::chrono::milliseconds(1));
					continue;''')
    sub('runtime/src/attach/bpf_attach_ctx_cuda.cpp','''			if (clock::now() - last_request <
			    cuda_watcher_busy_window) {''','''			if (false) {''')
elif m=='drain-one-per-poll':
    sub('runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp','''		for (; head != tail; head++) {''','''		for (int once = 0; head != tail && once < 1; head++, once++) {''')
else:
    raise SystemExit('unknown '+m)
