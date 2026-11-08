; Hand-written stand-in for what llvmbpf's generate_ptx(is_gpu=true,
; main_with_arguments=false, func_name="__probe_func__<kernel>") produces:
; a void function with no parameters, the GPU-mode eBPF stack
; (alloca [STACK_SIZE*MAX_LOCAL_FUNC_DEPTH+10 x i64] = [2058 x i64]),
; helpers declared as external i64(i64,i64,i64,i64,i64), stack keys passed
; to helpers as integers (so the stack escapes, as in real map-using probes).
; Program: v = map_lookup(3, &key0); if (v) __sync_fetch_and_add(v, 1);
;          w = map_lookup(3, &key1); if (w) *w = bpf_get_globaltimer();
target datalayout = "e-i64:64-i128:128-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

declare i64 @_bpf_helper_ext_0001(i64, i64, i64, i64, i64)
declare i64 @_bpf_helper_ext_0502(i64, i64, i64, i64, i64)

define void @__probe_func___Z9vectorAddPKfS0_Pf() {
setupBlock:
  %stackBegin = alloca [2058 x i64], align 8
  %r10 = getelementptr [2058 x i64], ptr %stackBegin, i64 0, i64 2048
  %k0 = getelementptr i8, ptr %r10, i64 -8
  store i32 0, ptr %k0, align 4
  %k0i = ptrtoint ptr %k0 to i64
  %v0 = call i64 @_bpf_helper_ext_0001(i64 3, i64 %k0i, i64 0, i64 0, i64 0)
  %isnull0 = icmp eq i64 %v0, 0
  br i1 %isnull0, label %skip0, label %inc0
inc0:
  %p0 = inttoptr i64 %v0 to ptr
  %old = atomicrmw add ptr %p0, i64 1 seq_cst
  br label %skip0
skip0:
  %k1 = getelementptr i8, ptr %r10, i64 -16
  store i32 1, ptr %k1, align 4
  %k1i = ptrtoint ptr %k1 to i64
  %v1 = call i64 @_bpf_helper_ext_0001(i64 3, i64 %k1i, i64 0, i64 0, i64 0)
  %isnull1 = icmp eq i64 %v1, 0
  br i1 %isnull1, label %done, label %ts
ts:
  %t = call i64 @_bpf_helper_ext_0502(i64 0, i64 0, i64 0, i64 0, i64 0)
  %p1 = inttoptr i64 %v1 to ptr
  store i64 %t, ptr %p1, align 8
  br label %done
done:
  ret void
}
