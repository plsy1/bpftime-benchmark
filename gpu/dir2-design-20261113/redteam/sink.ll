declare i64 @_bpf_helper_ext_0600(i64, i64, i64, i64, i64)

; two static units on one map, different constant keys in one stack slot,
; different site immediates
define i64 @two_keys(i64 %ctx, i64 %c) {
entry:
  %stack = alloca [64 x i8], align 8
  %kp = getelementptr inbounds [64 x i8], ptr %stack, i64 0, i64 56
  %kpi = ptrtoint ptr %kp to i64
  %cond = icmp eq i64 %c, 0
  br i1 %cond, label %a, label %b
a:
  store i32 0, ptr %kp, align 4
  %r1 = call i64 @_bpf_helper_ext_0600(i64 5, i64 %kpi, i64 1, i64 4097, i64 1)
  br label %join
b:
  store i32 1, ptr %kp, align 4
  %r2 = call i64 @_bpf_helper_ext_0600(i64 5, i64 %kpi, i64 1, i64 4097, i64 2)
  br label %join
join:
  ret i64 0
}

; two static units on two different maps (fd differs) with the same key slot
define i64 @two_maps(i64 %ctx, i64 %c) {
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
  %r2 = call i64 @_bpf_helper_ext_0600(i64 6, i64 %kpi, i64 1, i64 4097, i64 1)
  br label %join
join:
  ret i64 0
}
