#!/bin/sh
# katachi.svg から macOS の katachi.icns と Windows の katachi.ico を作り直す。
#
# **ビルド依存ではない。** 図案を変えたときに手元で走らせるだけの道具であり、
# CMake からは呼ばない。生成物 (.icns / .ico) はリポジトリに置いてあるため、
# ビルドする側にラスタライザは要らない。
#
# 要る道具:
#   rsvg-convert  (librsvg)
#   magick        (ImageMagick 7)
#   python3 + Pillow   … .ico の書き出し（256 を PNG 圧縮で格納するため）
#   iconutil      (macOS 同梱)
#
# 使い方:  ./build.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
svg="$here/katachi.svg"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for tool in rsvg-convert magick iconutil python3; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "必要な道具が無い: $tool" >&2
        exit 1
    }
done

# ── macOS ────────────────────────────────────────────────────────
# Big Sur 以降の格子に合わせ、1024 の中に 824 の版面を置いて余白を取る。
mkdir -p "$work/katachi.iconset"

emit_inset() {
    size=$1
    name=$2
    art=$(( size * 824 / 1024 ))
    [ "$art" -lt 1 ] && art=1
    rsvg-convert -w "$art" -h "$art" "$svg" -o "$work/art.png"
    magick "$work/art.png" -background none -gravity center \
        -extent "${size}x${size}" "$work/katachi.iconset/icon_$name.png"
}

set -- 16 16x16 32 16x16@2x 32 32x32 64 32x32@2x 128 128x128 \
       256 128x128@2x 256 256x256 512 256x256@2x 512 512x512 1024 512x512@2x
while [ "$#" -gt 0 ]; do
    emit_inset "$1" "$2"
    shift 2
done

iconutil -c icns "$work/katachi.iconset" -o "$here/katachi.icns"

# ── Windows ──────────────────────────────────────────────────────
# 全面（角丸は図案側が持つ）。256 のみ PNG で格納し、それ以下は BMP。
# 全サイズを PNG にすると小さくなるが、古い読み手での可否を確認していないため採らない。
rsvg-convert -w 256 -h 256 "$svg" -o "$work/ico-src.png"
ICO_SRC="$work/ico-src.png" ICO_OUT="$here/katachi.ico" python3 - <<'PY'
import os
from PIL import Image

src = Image.open(os.environ["ICO_SRC"]).convert("RGBA")
src.save(os.environ["ICO_OUT"], format="ICO",
         sizes=[(256, 256), (128, 128), (64, 64), (48, 48),
                (32, 32), (24, 24), (16, 16)])
PY

echo "作成した: $here/katachi.icns"
echo "作成した: $here/katachi.ico"
