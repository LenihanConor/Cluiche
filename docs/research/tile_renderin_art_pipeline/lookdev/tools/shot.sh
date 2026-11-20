#!/usr/bin/env bash
# Headless screenshot of the look-dev page (Edge + SwiftShader WebGL).
# Usage: tools/shot.sh <out-name> "<hash args>" [W,H]     e.g. tools/shot.sh freeze "view=3d&season=Freeze"
set -e
EDGE="/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
M="$(cygpath -m "$HERE")"
SIZE="${3:-1600,900}"
mkdir -p "$HERE/shots"
timeout 90 "$EDGE" --headless=new --use-angle=swiftshader --enable-unsafe-swiftshader --allow-file-access-from-files \
  --window-size="$SIZE" --virtual-time-budget=12000 --user-data-dir="$TEMP/lookdev-edge-$$" \
  --screenshot="$M/shots/$1.png" "file:///$M/index.html#$2" >/dev/null 2>&1 || true
rm -rf "$TEMP/lookdev-edge-$$"
echo "$HERE/shots/$1.png"
