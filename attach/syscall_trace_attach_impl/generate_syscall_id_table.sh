# Writes "name number" for every system call of the target to $1. Some
# numbers are defined through another macro (asm-generic, used by aarch64 and
# riscv: #define __NR_fcntl __NR3264_fcntl), so cpp expands each __NR_ name.
names=`echo '#include <sys/syscall.h>' | cpp -dM | \
sed -n 's/^#define __NR_\([A-Za-z0-9_]*\) .*/\1/p'`
echo "static const char* table=R\"(`
{ echo '#include <sys/syscall.h>'; for name in $names; do echo "$name __NR_$name"; done; } | \
cpp -P | grep -E '^[A-Za-z0-9_]+ [0-9]+$'
`)\";" > $1
