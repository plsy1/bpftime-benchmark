declare i64 @_bpf_helper_ext_0600(i64, i64, i64, i64, i64)
declare i64 @_bpf_helper_ext_0505(i64, i64, i64, i64, i64)

; two maps, two sites (fd and site immediates differ), key from a thread-invariant constant
define i64 @two_maps_two_sites(i64 %ctx, i64 %c) {
entry:
  %stack = alloca [64 x i8], align 8
  %kp = getelementptr inbounds [64 x i8], ptr %stack, i64 0, i64 56
  %kpi = ptrtoint ptr %kp to i64
  store i32 7, ptr %kp, align 4
  %cond = icmp eq i64 %c, 0
  br i1 %cond, label %a, label %b
a:
  %r1 = call i64 @_bpf_helper_ext_0600(i64 5, i64 %kpi, i64 1, i64 4097, i64 1)
  br label %join
b:
  %r2 = call i64 @_bpf_helper_ext_0600(i64 6, i64 %kpi, i64 1, i64 4097, i64 2)
  br label %join
join:
  ret i64 0
}
