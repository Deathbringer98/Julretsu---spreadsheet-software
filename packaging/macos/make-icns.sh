#!/bin/sh
# Builds a macOS .icns icon from the app PNG with the tools that ship with macOS.
set -e
source_png="$1"; output="$2"
iconset="$(dirname "$output")/Julretsu.iconset"
rm -rf "$iconset"; mkdir -p "$iconset"
for size in 16 32 128 256 512; do
  sips -z $size $size "$source_png" --out "$iconset/icon_${size}x${size}.png" >/dev/null
  double=$((size * 2))
  sips -z $double $double "$source_png" --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$output"
rm -rf "$iconset"
