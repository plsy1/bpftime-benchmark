#!/bin/bash
# 编译本目录的小程序。victim 必须 -O0：目标函数太短（4 字节）时 Frida 挂不上，IPC refresh 还会让宿主被 terminate（见附录 B）。
set -e
cd "$(dirname "$0")"
gcc -O2 -o loader loader.c
gcc -O2 -o poke poke.c
gcc -O0 -g -o victim victim.c
echo "built: loader poke victim"
