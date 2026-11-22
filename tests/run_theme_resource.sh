#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
${CC:-cc} -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
    -fno-omit-frame-pointer theme_resource.c tests/test_theme_resource.c -lm -o "$scratch/test"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$scratch/test" "$scratch"
