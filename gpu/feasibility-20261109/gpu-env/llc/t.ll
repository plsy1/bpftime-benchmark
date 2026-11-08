target triple = "nvptx64-nvidia-cuda"
define i64 @bpf_main(i64 %a, i64 %b) {
  %c = add i64 %a, %b
  ret i64 %c
}
