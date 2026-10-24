#!/bin/bash
# Build /boot/initrd-btf for kernel $V the way nv-update-initrd does for /boot/initrd:
# start from the current initrd without its modules and modprobe.d, then add the
# modules listed in /etc/nv-update-initrd/list.d/modules_k6.8 for $V and the
# modprobe.d configs. Never touches /boot/initrd.
set -euo pipefail
V=6.8.12-1021-tegra-btf
W=$(mktemp -d /home/jetson/src/l4t-r39.2.1/initrd-root.XXXXXX)
echo "workdir=$W"
cd "$W"
gunzip -c /boot/initrd | sudo -n cpio -i --quiet -f 'usr/lib/modules/*' 'etc/.disable_initrd_bash' \
    'etc/modprobe.d/*' 'usr/lib/modprobe.d/*'
echo "left from old initrd under usr/lib/modules: $(find usr/lib/modules -type f 2>/dev/null | wc -l); modprobe.d: $(find etc/modprobe.d usr/lib/modprobe.d -type f 2>/dev/null | wc -l); disable_initrd_bash: $(ls etc/.disable_initrd_bash 2>/dev/null | wc -l)"
grep -v '^#' /etc/nv-update-initrd/list.d/modules_k6.8 | grep -v '^$' | sed "s/<KERNEL_VERSION>/$V/g" |
while IFS=: read -r src dst; do
    found=0
    for f in $src; do
        [ -e "$f" ] || continue
        found=1
        case "$dst" in
            */) sudo -n mkdir -p "$W$dst"; sudo -n cp -a "$f" "$W$dst" ;;
            *) sudo -n mkdir -p "$W$(dirname "$dst")"; sudo -n cp -a "$f" "$W$dst" ;;
        esac
    done
    [ "$found" = 1 ] || echo "MISSING $src"
done
for d in /etc/modprobe.d /lib/modprobe.d; do
    sudo -n mkdir -p "$W$d"
    for f in "$d"/*.conf; do [ -e "$f" ] && sudo -n cp -a "$f" "$W$d/"; done
done
echo "ko files: $(find lib/modules -name '*.ko' | wc -l)"
ls lib/modules
ls lib/modules/$V | grep -c '^modules\.'
sudo -n sh -c "cd '$W' && find . | cpio -H newc -o --quiet | gzip -9 -n > /boot/initrd-btf"
ls -la /boot/initrd /boot/initrd-btf
