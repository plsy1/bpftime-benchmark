; ModuleID = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/recheck/sink2.ll'
source_filename = "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/recheck/sink2.ll"

declare i64 @_bpf_helper_ext_0600(i64, i64, i64, i64, i64) local_unnamed_addr

define i64 @two_maps_two_sites(i64 %ctx, i64 %c) local_unnamed_addr {
entry:
  %stack = alloca [64 x i8], align 8
  %kp = getelementptr inbounds [64 x i8], ptr %stack, i64 0, i64 56
  %kpi = ptrtoint ptr %kp to i64
  store i32 7, ptr %kp, align 8
  %cond = icmp eq i64 %c, 0
  br i1 %cond, label %a, label %b

a:                                                ; preds = %entry
  %r1 = call i64 @_bpf_helper_ext_0600(i64 5, i64 %kpi, i64 1, i64 4097, i64 1)
  br label %join

b:                                                ; preds = %entry
  %r2 = call i64 @_bpf_helper_ext_0600(i64 6, i64 %kpi, i64 1, i64 4097, i64 2)
  br label %join

join:                                             ; preds = %b, %a
  ret i64 0
}
