#!/usr/bin/env sh
# Lets the pinned CrossPoint simulator fall back to SDL's software renderer, so
# headless runs (SDL_VIDEODRIVER=dummy) can take screenshots. CI uses xvfb and
# does not need this. Safe to run repeatedly.
#
#   tools/chinesepoint/apply_simulator_software_renderer.sh [simulator-dir]
#
# Default target: the simulator PlatformIO fetched for the SSD1677 simulator env.
set -eu
repo_root=$(cd "$(dirname "$0")/../.." && pwd)
patch_file="$repo_root/tools/chinesepoint/simulator-software-renderer.patch"
target=${1:-"$repo_root/.pio/libdeps/chinesepoint_simulator_x4pro_ssd1677/simulator"}

if [ ! -f "$target/src/HalDisplay.cpp" ]; then
  echo "Simulator source not found at $target; build a simulator env first." >&2
  exit 1
fi
if patch -d "$target" -p1 -R --dry-run -s -f < "$patch_file" >/dev/null 2>&1; then
  echo "Already applied: $target"
  exit 0
fi
patch -d "$target" -p1 -s -N < "$patch_file"
echo "Applied software-renderer fallback: $target"
