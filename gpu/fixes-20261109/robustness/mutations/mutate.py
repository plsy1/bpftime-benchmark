import sys
root='/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/'
m=sys.argv[1]
def sub(f, old, new):
    p=root+f; s=open(p).read(); assert old in s, (m, old[:50]); open(p,'w').write(s.replace(old,new,1))
U='attach/nv_attach_impl/nv_attach_utils.cpp'
if m=='sm-arch-setenv':
    sub(U,'''	detected = "sm_" + std::to_string(major * 10 + minor);''','''	detected = "sm_" + std::to_string(major * 10 + minor);
	setenv("BPFTIME_SM_ARCH", detected.c_str(), 1);''')
elif m=='sm-arch-sm61':
    sub(U,'''			(int)err);
		return {};''','''			(int)err);
		return "sm_61";''')
elif m=='setlocale-global':
    sub('attach/nv_attach_impl/ptx_compiler/ptx_compiler.cpp','''	return new nv_attach_impl_ptx_compiler;''','''	setlocale(LC_ALL, "");
	return new nv_attach_impl_ptx_compiler;''')
elif m=='keep-dirs-debug':
    sub('attach/nv_attach_impl/nv_attach_impl.cpp','''	const char *keep = std::getenv("BPFTIME_CUDA_KEEP_PTX");
	return keep != nullptr && keep[0] == '1';''','''	return spdlog::should_log(spdlog::level::debug);''')
elif m=='map-value-unchecked':
    sub('runtime/src/attach/bpf_attach_ctx_cuda.cpp','''	CUdeviceptr device = 0;
	if (cuMemHostGetDevicePointer(&device, const_cast<void *>(value), 0) ==
	    CUDA_SUCCESS)
		return reinterpret_cast<void *>(device);
	return nullptr;''','''	return const_cast<void *>(value);''')
elif m=='watcher-no-context':
    sub('runtime/src/attach/bpf_attach_ctx_cuda.cpp','''		    cuDevicePrimaryCtxRetain(&primary, device) == CUDA_SUCCESS)
			cuCtxSetCurrent(primary);''','''		    cuDevicePrimaryCtxRetain(&primary, device) == CUDA_SUCCESS)
			(void)0;''')
else:
    raise SystemExit('unknown')
