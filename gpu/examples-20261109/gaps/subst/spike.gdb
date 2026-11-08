set startup-with-shell off
set pagination off
set breakpoint pending on
break cuLibraryLoadData
commands
silent
printf "cuLibraryLoadData: original image=%p, substituting the modified PTX\n", $x1
set $buf = (char *)((void *(*)(unsigned long))malloc)(1260 + 64)
restore /home/jetson/src/bpftime-gpu-work/gaps/subst/globals.mod.ptx binary $buf
set $x1 = $buf
continue
end
run
