#!/bin/sh
# The sources CMake would generate (menu model from assets/data/menus.json, the embedded
# icon, the build id), made with cmake -P so build_miyoo.sh needs no CMake configure.
# Run where cmake is (WSL on the owner's PC):
#   wsl -e sh miyoo/build/gen_sources.sh [out_dir]
# prehistorik-miyoo is a modified Olduvai under its own name (LEGAL.md "Project name and
# marks"): the title menu's header reads PREHISTORIK and the embedded icon is the port's own
# picture (miyoo/art/icon.png), not the Olduvai bone.
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$REPO/build/miyoo}
G="$OUT/generated"
mkdir -p "$G"
cd "$REPO"
sed 's/"header": "OLDUVAI"/"header": "PREHISTORIK"/' assets/data/menus.json > "$G/menus.json"
grep -q '"header": "PREHISTORIK"' "$G/menus.json" || { echo "menus.json: no OLDUVAI header to rename" >&2; exit 1; }
cmake -DIN="$G/menus.json" -DOUT="$G/menu_model_data.cpp" -P cmake/gen_menu_model.cmake
cmake -DIN=miyoo/art/icon.png -DOUT="$G/icon_embedded.cpp" -DSYMBOL=embedded_icon_png \
	-P cmake/embed_binary.cmake
cmake -DSRC_DIR="$REPO" -DOUT="$G/build_id.cpp" -DGIT="$(command -v git)" -P cmake/build_id.cmake
ls -la "$G"
