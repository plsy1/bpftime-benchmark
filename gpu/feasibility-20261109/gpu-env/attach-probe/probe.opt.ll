; ModuleID = 'probe.ll'
source_filename = "probe.ll"
target datalayout = "e-i64:64-i128:128-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

declare i64 @_bpf_helper_ext_0001(i64, i64, i64, i64, i64) local_unnamed_addr

declare i64 @_bpf_helper_ext_0502(i64, i64, i64, i64, i64) local_unnamed_addr

define void @__probe_func___Z9vectorAddPKfS0_Pf() local_unnamed_addr {
setupBlock:
  %stackBegin = alloca [2058 x i64], align 8
  %k0 = getelementptr inbounds [2058 x i64], ptr %stackBegin, i64 0, i64 2047
  store i32 0, ptr %k0, align 8
  %k0i = ptrtoint ptr %k0 to i64
  %v0 = call i64 @_bpf_helper_ext_0001(i64 3, i64 %k0i, i64 0, i64 0, i64 0)
  %isnull0 = icmp eq i64 %v0, 0
  br i1 %isnull0, label %skip0, label %inc0

inc0:                                             ; preds = %setupBlock
  %p0 = inttoptr i64 %v0 to ptr
  %old = atomicrmw add ptr %p0, i64 1 seq_cst, align 8
  br label %skip0

skip0:                                            ; preds = %inc0, %setupBlock
  %k1 = getelementptr inbounds [2058 x i64], ptr %stackBegin, i64 0, i64 2046
  store i32 1, ptr %k1, align 8
  %k1i = ptrtoint ptr %k1 to i64
  %v1 = call i64 @_bpf_helper_ext_0001(i64 3, i64 %k1i, i64 0, i64 0, i64 0)
  %isnull1 = icmp eq i64 %v1, 0
  br i1 %isnull1, label %done, label %ts

ts:                                               ; preds = %skip0
  %p1 = inttoptr i64 %v1 to ptr
  %t = call i64 @_bpf_helper_ext_0502(i64 0, i64 0, i64 0, i64 0, i64 0)
  store i64 %t, ptr %p1, align 8
  br label %done

done:                                             ; preds = %ts, %skip0
  ret void
}
