#!/bin/sh
# Prehistorik (1991) through the Olduvai engine on the Miyoo Mini Plus with OnionOS.
# Started by the Ports shortcut (Roms/PORTS/Shortcuts/Platform/Prehistorik (1991).port) through
# Onion's launch_standalone.sh, or over SSH: sh launch.sh [olduvai arguments]
# After quake2-miyoomini's launch.sh. The player's own game files go in game/ here:
# FILESA.CUR, FILESB.CUR, FILESA.VGA, FILESB.VGA and HISTORIK.EXE (or GOG's PREH.SQZ).
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR" || exit 1
mkdir -p game

# Clean macOS metadata files (._* and .DS_Store)
find game -name "._*" -type f -delete 2>/dev/null
find game -name ".DS_Store" -type f -delete 2>/dev/null

have_game () {
	[ -f game/FILESA.CUR ] && [ -f game/FILESB.CUR ] && [ -f game/FILESA.VGA ] && [ -f game/FILESB.VGA ] &&
		{ [ -f game/HISTORIK.EXE ] || [ -f game/PREH.SQZ ]; }
}

# Game files copied from a PC may come in lower or mixed case: renamed to upper case through
# a temporary name (FAT may not tell the cases apart)
for f in game/*; do
	[ -f "$f" ] || continue
	b=$(basename "$f")
	case "$b" in *.txt|*.TXT) continue ;; esac
	u=$(echo "$b" | tr 'a-z' 'A-Z')
	[ "$b" = "$u" ] && continue
	mv "$f" game/.rename_tmp && mv game/.rename_tmp "game/$u"
done

# The DOS game already on the card (Roms/DOS/Prehistorik*.zip, for DOSBox): its files are
# taken from the zip once, if game/ doesn't have them yet
if ! have_game; then
	for z in /mnt/SDCARD/Roms/DOS/Prehistorik\ \(1991\)*.zip /mnt/SDCARD/Roms/DOS/Prehistorik.zip; do
		[ -f "$z" ] || continue
		rm -rf /tmp/olduvai_unzip && mkdir -p /tmp/olduvai_unzip
		unzip -o -q "$z" -d /tmp/olduvai_unzip 2>/dev/null
		for n in FILESA.CUR FILESB.CUR FILESA.VGA FILESB.VGA HISTORIK.EXE PREH.SQZ; do
			src=$(find /tmp/olduvai_unzip -iname "$n" -type f 2>/dev/null | head -1)
			[ -n "$src" ] && cp "$src" "game/$n"
		done
		rm -rf /tmp/olduvai_unzip
		have_game && break
	done
fi

if ! have_game; then
	printf 'Prehistorik files missing: put\nFILESA.CUR FILESB.CUR FILESA.VGA\nFILESB.VGA HISTORIK.EXE (or PREH.SQZ)\nin Roms/PORTS/Games/Olduvai/game/' > /tmp/MissingPortFile.tmp
	exit 1
fi

# settings (play.json), the quicksave and bug reports stay here, in the game's folder on the
# card: Onion's HOME (/home/root) doesn't exist on its read-only root
export HOME="$DIR"
export XDG_CONFIG_HOME="$DIR/conf"
export OLDUVAI_BUG_DIR="$DIR/bug_reports"
export OLDUVAI_PANEL=640x480

{ echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor; } 2>/dev/null
CPU1=/sys/devices/system/cpu/cpu1/online
[ -f "$CPU1" ] && [ "$(cat "$CPU1" 2>/dev/null)" = 0 ] && { echo 1 > "$CPU1"; } 2>/dev/null

# play log (for stutter checks): with a file playlog.on here, each frame's time and counts go
# to playlog/frames-<date>.txt, written when the game quits (miyoo_sdl.cpp MIYOO_FRAMES)
if [ -f playlog.on ]; then
	mkdir -p playlog
	export MIYOO_FRAMES="$DIR/playlog/frames-$(date +%Y%m%d-%H%M%S).txt"
fi

chmod +x ./olduvai 2>/dev/null
# Classic DOS by default (the 1991 game at full speed on this CPU); Options - Style switches to
# Enhanced HD and the choice is kept in conf/olduvai/play.json
# (exec: Onion's SIGTERM at power-off reaches the game itself, which saves and quits)
exec ./olduvai --game-dir ./game --default-profile dos-handheld "$@" > log.txt 2>&1
