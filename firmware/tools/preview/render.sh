#!/bin/sh
set -eu

preview_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$preview_dir/../../.." && pwd)
output_dir=${1:-"$repo_dir/.pio/display-preview"}
gfx_dir="$repo_dir/.pio/libdeps/esp32_c3_jd9853/Adafruit GFX Library"

if [ ! -f "$gfx_dir/Adafruit_GFX.cpp" ]; then
    printf '%s\n' 'Missing Adafruit GFX dependency. Build esp32_c3_jd9853 with PlatformIO first.' >&2
    exit 1
fi

mkdir -p "$output_dir"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra \
    -Wno-unused-parameter -Wno-sign-compare \
    -DARDUINO=10819 \
    -I "$preview_dir/stubs" -I "$repo_dir/firmware/src" -I "$gfx_dir" \
    "$gfx_dir/Adafruit_GFX.cpp" \
    "$repo_dir/firmware/src/core/text_format.cpp" \
    "$repo_dir/firmware/src/core/usage_layout.cpp" \
    "$repo_dir/firmware/src/display/tft_renderer.cpp" \
    "$repo_dir/firmware/src/display/status_screens.cpp" \
    "$preview_dir/main.cpp" -o "$output_dir/render-preview"

"$output_dir/render-preview" "$output_dir"

if command -v sips >/dev/null 2>&1; then
    for preview in "$output_dir"/*.ppm; do
        sips -s format png "$preview" --out "${preview%.ppm}.png" >/dev/null
    done
fi

printf 'Native 296x240 previews: %s\n' "$output_dir"
