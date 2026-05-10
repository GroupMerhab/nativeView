#!/bin/sh
# Download Vue 3 prod build for local bundling (no CDN at runtime)
set -e
DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${DIR}/js/vue.js"
URL="https://cdn.jsdelivr.net/npm/vue@3/dist/vue.global.prod.js"
mkdir -p "$(dirname "$OUT")"
echo "Downloading Vue 3 from $URL ..."
curl -sL "$URL" -o "$OUT"
echo "Saved to $OUT ($(wc -c < "$OUT") bytes)"
echo "Run: cmake --build build --target nv-ui"
