#!/usr/bin/env bash
set -euo pipefail
rc=$1
output=$2
shift 2
cc -std=c99 -D_POSIX_C_SOURCE=200809L -g -O1 -Wall -Wextra "$@" \
   -I"$rc/include" "$(dirname "$0")/condition-check.c" \
   "$rc"/src/rcheevos/*.c "$rc"/src/rc_compat.c "$rc"/src/rc_util.c \
   "$rc"/src/rhash/md5.c -lm -o "$output"
