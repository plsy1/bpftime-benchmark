set startup-with-shell off
set pagination off
set breakpoint pending on
break cuLibraryLoadData
commands
silent
printf "cuLibraryLoadData code=%p numJitOptions=%d numLibraryOptions=%d\n", $x1, (int)$w4, (int)$w7
set $i = 0
while $i < (int)$w4
printf "  jit option %d value %p\n", ((int *)$x2)[$i], ((void **)$x3)[$i]
set $i = $i + 1
end
set $i = 0
while $i < (int)$w7
printf "  library option %d value %p\n", ((int *)$x5)[$i], ((void **)$x6)[$i]
set $i = $i + 1
end
continue
end
run
