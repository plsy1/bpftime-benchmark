#!/bin/bash
set -eu
cd "$(dirname "$0")"
gcc-13 -O2 -g -pthread -o thr thr.c
