#!/bin/bash
set -e
cd "$(dirname "$0")"
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily
INC=$(grep '^CXX_INCLUDES' flags.make | sed 's/^CXX_INCLUDES = //')
DEF=$(grep '^CXX_DEFINES' flags.make | sed 's/^CXX_DEFINES = //')
LIBS=$(tr -d '"' < libs.txt | sed "s#\.\./\.\./#$B/#g; s# \.\./# $B/runtime/#g")
/usr/bin/g++-13 -fno-omit-frame-pointer -O2 -g -DNDEBUG -std=gnu++20 -flto=auto -fPIE $DEF $INC -o exp exp.cpp $LIBS
