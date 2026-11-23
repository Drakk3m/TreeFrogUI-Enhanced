#!/bin/sh
# Host packages: build-essential pkg-config libfreetype-dev libharfbuzz-dev
set -eu
cd "$(dirname "$0")/.."
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
ulimit -c 0
${CC:-cc} -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -Wl,--wrap=fopen -Wl,--wrap=system -Wl,--wrap=access -Wl,--wrap=malloc \
    -Wl,--wrap=opendir -Wl,--wrap=mkdir -Wl,--wrap=unlink -Wl,--wrap=rename -Wl,--wrap=sync \
    $(pkg-config --cflags freetype2 harfbuzz) \
    -Iexternal/SheenBidi -Iexternal/SheenBidi/Headers -Iexternal/SheenBidi/Source \
    -DSB_CONFIG_UNITY tests/test_graphical_home.c navigation_model.c filelist.c theme_resource.c render.c render_theme.c font.c \
    recent_games.c theme.c theme_profile.c favorites.c banner.c backlight.c input.c \
    core_override.c ext_filter.c screenshot.c common/i18n.c \
    external/SheenBidi/Source/SheenBidi.c $(pkg-config --libs freetype2 harfbuzz) \
    -lm -ldl -lpthread -o "$scratch/test_graphical_home"
mkdir "$scratch/library"
"$scratch/test_graphical_home" "$scratch/library"
echo 'PASS graphical_home'
