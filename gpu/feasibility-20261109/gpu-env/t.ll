target triple = "nvptx64-nvidia-cuda"
define i64 @bpf_main(i64 %a) {
  %r = add i64 %a, 1
  ret i64 %r
}
