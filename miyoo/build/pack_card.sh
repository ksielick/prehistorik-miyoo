#!/bin/sh
# Lay out the Miyoo port as it goes on the SD card, from a build (build_miyoo.sh), and zip it
# for a release (Port Manager's "sdcard-root" layout):
#   miyoo/build/pack_card.sh [out_dir]
# Output: <out_dir>/card/Roms/PORTS/... (default out_dir <repo>/build/miyoo), to copy over the
# card's Roms/ (Onion's share \\<miyoo>\Roms), and <out_dir>/Prehistorik-Miyoo-<version>.zip
# holding Roms/. No game files: the player adds them to Roms/PORTS/Games/Prehistorik/game/
# (or launch.sh takes them from Roms/DOS/Prehistorik (1991).zip).
# PORT_VERSION: the release's version (default: the engine's, from CMakeLists.txt).
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$REPO/build/miyoo}
CARD="$OUT/card/Roms/PORTS"
GAME="$CARD/Games/Prehistorik"
SRC="$REPO/miyoo/card/Roms/PORTS"
[ -f "$OUT/olduvai" ] || { echo "no $OUT/olduvai: run miyoo/build/build_miyoo.sh" >&2; exit 1; }
VERSION=${PORT_VERSION:-$(grep -A3 "^project(olduvai" "$REPO/CMakeLists.txt" | grep -o "VERSION [0-9][0-9.]*" | head -1 | cut -d" " -f2)}

rm -rf "$OUT/card"
mkdir -p "$GAME/fonts" "$GAME/licenses" "$GAME/game" "$CARD/Shortcuts/Platform" "$CARD/Imgs"
cp "$OUT/olduvai" "$GAME/prehistorik"
cp "$SRC/Games/Prehistorik/launch.sh" "$SRC/Games/Prehistorik/README.txt" "$GAME/"
cp "$REPO"/assets/fonts/*.ttf "$GAME/fonts/"
cp "$SRC/Shortcuts/Platform/Prehistorik (1991).port" "$CARD/Shortcuts/Platform/"
cp "$SRC/Imgs/Prehistorik (1991).png" "$CARD/Imgs/"

# licences (the handheld bundles' set, packaging/port_common.sh), minus the parts not built in
L="$GAME/licenses"
cp "$REPO/LICENSE" "$L/LICENSE.prehistorik-miyoo.txt"
cp "$REPO/LICENSE" "$L/LICENSE.xBRZ.txt"
cp "$REPO/THIRD-PARTY-NOTICES.md" "$L/"
cp "$REPO/third_party/nuked_opl3/LICENSE" "$L/LICENSE.Nuked-OPL3.txt"
if [ -n "$MT32" ]; then cp "$REPO/third_party/mt32emu/COPYING.LESSER.txt" "$L/LICENSE.libmt32emu.txt"; fi
for f in "$REPO"/assets/fonts/*-LICENSE.txt; do
	cp "$f" "$L/LICENSE.$(basename "$f" -LICENSE.txt).txt"
done
echo "Put your own Prehistorik files here: FILESA.CUR FILESB.CUR FILESA.VGA FILESB.VGA HISTORIK.EXE (or PREH.SQZ)." \
	> "$GAME/game/PUT-FILES-HERE.txt"

ZIP="$OUT/Prehistorik-Miyoo-$VERSION.zip"
rm -f "$ZIP"
# (Python's zipfile: no zip tool on the owner's PC; Unix modes kept for the scripts)
PY=$(command -v py >/dev/null 2>&1 && echo "py -3" || echo python3)
$PY "$REPO/miyoo/build/zip_dir.py" "$OUT/card" "$ZIP"
find "$OUT/card" -type f | sed "s#^$OUT/##" | sort
ls -la "$ZIP"
