; ModuleID = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/recheck/sink.ll'
source_filename = "/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/recheck/sink.ll"

declare i64 @_bpf_helper_ext_0600(i64, i64, i64, i64, i64) local_unnamed_addr

define i64 @two_keys(i64 %ctx, i64 %c) local_unnamed_addr {
entry:
  %stack = alloca [64 x i8], align 8
  %kp = getelementptr inbounds [64 x i8], ptr %stack, i64 0, i64 56
  %kpi = ptrtoint ptr %kp to i64
  %cond = icmp eq i64 %c, 0
  %not.cond = xor i1 %cond, true
  %. = zext i1 %not.cond to i32
  %.2 = select i1 %cond, i64 1, i64 2
  store i32 %., ptr %kp, align 8
  %r2 = call i64 @_bpf_helper_ext_0600(i64 5, i64 %kpi, i64 1, i64 4097, i64 %.2)
  ret i64 0
}

define i64 @two_maps(i64 %ctx, i64 %c) local_unnamed_addr {
entry:
  %stack = alloca [64 x i8], align 8
  %kp = getelementptr inbounds [64 x i8], ptr %stack, i64 0, i64 56
  %kpi = ptrtoint ptr %kp to i64
  store i32 7, ptr %kp, align 8
  %cond = icmp eq i64 %c, 0
  %. = select i1 %cond, i64 5, i64 6
  %r2 = call i64 @_bpf_helper_ext_0600(i64 %., i64 %kpi, i64 1, i64 4097, i64 1)
  ret i64 0
}
