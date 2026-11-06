#!/bin/sh
# Build Olduvai for the Miyoo Mini Plus (Cortex-A7, OnionOS, glibc 2.28) with the
# Windows-hosted Arm GNU toolchain, without CMake or make. Run from Git Bash:
#   miyoo/build/build_miyoo.sh [out_dir]
# Default out_dir: <repo>/build/miyoo (git-ignored). Objects go to <out_dir>/obj.
# Only source timestamps are checked: after a header change run with FORCE=1.
#
# The recipe is quake2-miyoomini's miyoo/build/build_miyoo.sh (itself from tyrquake-miyoo):
# a dynamic executable, because the SoC's 2D engine (MI_GFX) is reached with dlopen of the
# vendor libraries.
#   - start.S + -nostartfiles: the toolchain's crt1.o needs __libc_start_main@GLIBC_2.34;
#   - glibc_compat.h (force-included) pins libm, pthread and dl symbols to GLIBC_2.4;
#     glibc_compat.c supplies what libstdc++ (linked statically) wants from newer glibc;
#   - the binary is checked for symbols newer than glibc 2.28 at the end.
# SDL2 is not linked: the engine is compiled against the real SDL2 headers and
# miyoo/platform/ implements the SDL functions it calls.
#
# The CMake-generated sources (menu model, embedded icon, build id) are made by
# cmake -P in WSL (miyoo/build/gen_sources.sh) into <out_dir>/generated.
#
# TC: the toolchain's bin directory. SDL2_INC: the SDL2 headers. OPT: optimisation flags.
# MT32=1: compile in libmt32emu (Roland MT-32 emulation; heavy for this CPU).
set -e
REPO=$(cd "$(dirname "$0")/../.." && (pwd -W 2>/dev/null || pwd))	# (G:/... for the Windows compiler)
OUT=${1:-$REPO/build/miyoo}
TC=${TC:-G:/Emulacja/redriver2-miyoo/deps/arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-linux-gnueabihf/bin}
SDL2_INC=${SDL2_INC:-G:/Emulacja/redriver2-miyoo/deps/SDL2-2.30.9/include}
[ -x "$TC/arm-none-linux-gnueabihf-gcc.exe" ] || [ -x "$TC/arm-none-linux-gnueabihf-gcc" ] ||
	{ echo "no Arm toolchain in $TC: set TC" >&2; exit 1; }
CC="$TC/arm-none-linux-gnueabihf-gcc"
CXX="$TC/arm-none-linux-gnueabihf-g++"
JOBS=${JOBS:-8}
VERSION=$(grep -A3 "^project(olduvai" "$REPO/CMakeLists.txt" | grep -o "VERSION [0-9][0-9.]*" | head -1 | cut -d" " -f2)
cd "$REPO"

for g in menu_model_data.cpp icon_embedded.cpp build_id.cpp; do
	[ -f "$OUT/generated/$g" ] || { echo "missing $OUT/generated/$g: run miyoo/build/gen_sources.sh" >&2; exit 1; }
done

# --- sources (CMakeLists.txt: olduvai_formats, olduvai_presentation, olduvai)
FORMATS=$(sed -n '/^add_library(olduvai_formats/,/^)/p' CMakeLists.txt | grep -oE '(src|third_party)/[^ )]+\.(cpp|c)')
PRESENTATION=$(sed -n '/add_library(olduvai_presentation/,/opl3\.c)/p' CMakeLists.txt | grep -oE '(src|third_party)/[^ )]+\.(cpp|c)')
APP="src/app/main.cpp src/app/config.cpp src/app/legacy_cache.cpp src/app/options_resolve.cpp
src/app/cli_args.cpp src/app/options_build.cpp src/app/first_run.cpp"
EXTRA="src/presentation/audio/opl_music.cpp src/presentation/audio/host_midi.cpp
$OUT/generated/menu_model_data.cpp $OUT/generated/icon_embedded.cpp $OUT/generated/build_id.cpp"
PLATFORM="miyoo/build/glibc_compat.c $(ls miyoo/platform/*.c miyoo/platform/*.cpp 2>/dev/null | tr '\n' ' ')"
SOURCES=$(echo $FORMATS $PRESENTATION $APP $EXTRA $PLATFORM | tr ' ' '\n' | awk '!seen[$0]++')

DEFS="-DOLDUVAI_VERSION=\\\"$VERSION\\\" -DOLDUVAI_HAVE_SDL=1 -DMIYOO_MINI"
INCS="-Isrc -Ithird_party -Ithird_party/xbrz -Ithird_party/stb -Ithird_party/nuked_opl3 -I$OUT/include -I$OUT/include/SDL2 -Imiyoo/platform"
if [ -n "$MT32" ]; then
	MT32SRC=$(ls third_party/mt32emu/src/*.cpp third_party/mt32emu/src/sha1/*.cpp third_party/mt32emu/src/srchelper/*.cpp 2>/dev/null | tr '\n' ' ')
	SOURCES="$SOURCES $MT32SRC"
	DEFS="$DEFS -DOLDUVAI_VENDORED_MT32EMU=1"
	INCS="$INCS -Ithird_party/mt32emu/src -I$OUT/include/mt32emu"
fi

# --- flags (ARMv7 hard-float; -fsigned-char: the code is developed on x86 and arm64)
OPT=${OPT:--O2}
CPUF="-marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
COMMON="$OPT -fomit-frame-pointer -ffp-contract=off -fsigned-char $CPUF -fvisibility=hidden
-Wall -Wno-unused-result -Wno-psabi $DEFS $INCS -include miyoo/build/glibc_compat.h"
mkdir -p "$OUT/obj" "$OUT/include/SDL2"
[ -f "$OUT/include/SDL2/SDL.h" ] || cp "$SDL2_INC"/*.h "$OUT/include/SDL2/"
echo $COMMON "MT32=$MT32" > "$OUT/cflags.new"
cmp -s "$OUT/cflags.new" "$OUT/cflags.stamp" || { cp "$OUT/cflags.new" "$OUT/cflags.stamp"; FORCE=1; }
rm -f "$OUT/cflags.new"
# a header newer than the last build: everything again (only source dates are compared below;
# a changed header once left a binary unchanged, 2026-11-06)
if [ -z "$FORCE" ] && [ -f "$OUT/olduvai.unstripped" ] &&
	[ -n "$(find src miyoo third_party/nuked_opl3 -name '*.h*' -newer "$OUT/olduvai.unstripped" 2>/dev/null | head -1)" ]; then
	echo "  (a header changed: rebuilding everything)"
	FORCE=1
fi
echo $COMMON -std=c++17 > "$OUT/cxxflags.rsp"
echo $COMMON -std=gnu11 > "$OUT/cflags.rsp"
export CC CXX OUT

for s in $SOURCES; do
	o="$OUT/obj/$(echo "$s" | sed "s#^$OUT/##" | tr '/' '_' | sed 's/\.\(cpp\|c\)$/.o/')"
	if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$FORCE" ]; then
		echo "$s $o"
	fi
done | xargs -r -P "$JOBS" -L 1 sh -c '
	case "$0" in
	*.c) C="$CC"; R="$OUT/cflags.rsp" ;;
	*) C="$CXX"; R="$OUT/cxxflags.rsp" ;;
	esac
	X=""
	case "$0" in
	*xbrz.cpp|*mt32emu*|*opl3.c) X="-w" ;;
	esac
	case "$0" in miyoo/build/glibc_compat.c) X="$X -fno-lto -DGLIBC_COMPAT_WRAP" ;; esac
	echo "  CC $0"; "$C" @"$R" $X -c "$0" -o "$1"'

OBJS=""
for s in $SOURCES; do OBJS="$OBJS $OUT/obj/$(echo "$s" | sed "s#^$OUT/##" | tr '/' '_' | sed 's/\.\(cpp\|c\)$/.o/')"; done
WRAPS="stat stat64 lstat64 fstat64 pthread_create pthread_join pthread_detach pthread_once pthread_key_create
pthread_key_delete pthread_getspecific pthread_setspecific pthread_mutex_trylock __isoc23_strtol __isoc23_strtoul
hypotf arc4random"
LIBS="$(for w in $WRAPS; do printf -- "-Wl,--wrap=%s " $w; done) -lm -Wl,--no-as-needed -l:libdl.so.2 -l:libpthread.so.0 -l:librt.so.1"
echo "  LD olduvai"
"$CXX" -no-pie -nostartfiles -static-libgcc -static-libstdc++ -o "$OUT/olduvai" miyoo/build/start.S \
	$CPUF $OBJS $LIBS ${LDEXTRA}

# the Miyoo has glibc 2.28: no symbol may need a newer version
cp "$OUT/olduvai" "$OUT/olduvai.unstripped"	# (for addr2line on crash addresses)
"$TC/arm-none-linux-gnueabihf-strip" "$OUT/olduvai"
NEW=$("$TC/arm-none-linux-gnueabihf-objdump" -T "$OUT/olduvai" | grep -o 'GLIBC_2\.[0-9]*' | sort -u -t. -k2,2n |
	awk -F. '$2 > 28')
if [ -n "$NEW" ]; then
	echo "ERROR: olduvai needs newer glibc than the Miyoo's 2.28:"
	"$TC/arm-none-linux-gnueabihf-objdump" -T "$OUT/olduvai" | grep -E "$(echo $NEW | tr ' ' '|')"
	exit 1
fi
ls -la "$OUT/olduvai"
