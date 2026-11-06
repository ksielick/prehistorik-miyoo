#!/bin/sh
# Measurements on the device for prehistorik-miyoo (menus and Enhanced HD), started as Onion
# would (/tmp/cmd_to_run.sh) because the game takes the panel. Own settings in /tmp (never the
# player's conf/), the build to test in $B (default <game dir>/bench/). Results in $B/out/.
#   A  the title menu, idle: frame times, profile (frames 700..1099), shots
#   B  level 1, then the pause menu (MENU injected with Onion's sendkeys): frame times, shots
#   C  Enhanced HD (hd-handheld) level 1, 40 s (no --play-frames: it turns smooth motion off): frame times, profile, stats
# Usage (from the PC): copy this to /tmp, write "sh /tmp/bench_menus.sh" to /tmp/cmd_to_run.sh
# and killall MainUI. RUNS="A B C" picks the runs.
G=/mnt/SDCARD/Roms/PORTS/Games/Prehistorik
B=${B:-$G/bench}
O=$B/out
RUNS=${RUNS:-"A B C"}
SK=/mnt/SDCARD/.tmp_update/bin/sendkeys
rm -rf "$O" /tmp/olbench
mkdir -p "$O" /tmp/olbench
cd "$B" || exit 1
export HOME=/tmp/olbench XDG_CONFIG_HOME=/tmp/olbench/conf OLDUVAI_PANEL=640x480 MIYOO_STATS=1
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null

# run <name> <seconds> <args...>: the game for at most <seconds>, then SIGTERM (it quits and
# writes its frame times), SIGKILL 5 s later
run () {
	n=$1; t=$2; shift 2
	mkdir -p "$O/$n"
	MIYOO_FRAMES="$O/$n/frames.txt" MIYOO_SHOTS_DIR="$O/$n" ./prehistorik --game-dir "$G/game" "$@" > "$O/$n/log.txt" 2>&1 &
	P=$!
	( sleep "$t"; kill "$P" 2>/dev/null; sleep 5; kill -9 "$P" 2>/dev/null ) &
	W=$!
	echo "$P" > /tmp/olbench/pid
}
finish () {
	wait "$P" 2>/dev/null
	echo "exit $?" >> "$O/$1/log.txt"
	kill "$W" 2>/dev/null
	cp /tmp/olbench/*.prof "$O/$1/" 2>/dev/null
	rm -f /tmp/olbench/*.prof
}

for r in $RUNS; do
	case $r in
	A)
		export MIYOO_PROF=/tmp/olbench/A.prof,700,400 MIYOO_SHOTS=350; run A 25 --default-profile dos-handheld
		unset MIYOO_PROF; finish A ;;
	B)
		export MIYOO_SHOTS=250; run B 30 --default-profile dos-handheld --play --level 1
		sleep 12
		$SK 1 1; sleep 0.2; $SK 1 0		# MENU: the pause menu
		sleep 12
		$SK 1 1; sleep 0.2; $SK 1 0		# and closed again
		finish B ;;
	C)
		export OLDUVAI_FRAME_STATS=1 MIYOO_SHOTS=400; run C 40 --default-profile hd-handheld --play --level 1
		unset OLDUVAI_FRAME_STATS; finish C ;;
	esac
done
free > "$O/free.txt"
echo done > "$O/done.txt"
