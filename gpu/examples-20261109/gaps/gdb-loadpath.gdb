set startup-with-shell off
set pagination off
set breakpoint pending on
break main
commands
silent
printf "== main reached\n"
continue
end
break cuLibraryLoadData
commands
silent
printf "HIT cuLibraryLoadData image=%p\n", $x1
bt 8
continue
end
break cuModuleLoadData
commands
silent
printf "HIT cuModuleLoadData image=%p\n", $x1
bt 6
continue
end
break cuModuleLoadDataEx
commands
silent
printf "HIT cuModuleLoadDataEx image=%p\n", $x1
bt 6
continue
end
break cuModuleLoadFatBinary
commands
silent
printf "HIT cuModuleLoadFatBinary image=%p\n", $x1
bt 6
continue
end
break cuLibraryGetGlobal
commands
silent
printf "HIT cuLibraryGetGlobal name=%s\n", (char *)$x3
continue
end
break cuLibraryGetKernel
commands
silent
printf "HIT cuLibraryGetKernel name=%s\n", (char *)$x2
continue
end
break cuLibraryGetManaged
commands
silent
printf "HIT cuLibraryGetManaged name=%s\n", (char *)$x3
continue
end
run
info proc mappings
