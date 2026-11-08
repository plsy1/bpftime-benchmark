target datalayout = "e-i64:64-i128:128-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

define void @k(ptr %p) {
  store i32 42, ptr %p, align 4
  ret void
}

!nvvm.annotations = !{!0}
!0 = !{ptr @k, !"kernel", i32 1}
