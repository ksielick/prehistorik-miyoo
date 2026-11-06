# prehistorik-miyoo: notes

Measurements, decisions and traps of the Miyoo Mini Plus port. Facts that apply to every
Miyoo port live in the owner's shared notes; this file is about this game only.

## What it is

- Olduvai (C++17, Krzysztof Sokołowski) built for the Miyoo Mini Plus (OnionOS) under the name
  prehistorik-miyoo: Olduvai's licence covers the code, its name and logo are the author's
  marks (LEGAL.md). Upstream takes no external patches, so nothing here goes back as a PR.
- A dynamic ARM executable (MI_GFX needs the device's libraries) for glibc 2.28, libstdc++
  linked statically. No SDL library: `miyoo/platform/` implements the SDL2 calls the engine
  makes.
- Shipped as a Port Manager entry `prehistorik` (sdcard-root zip
  `Prehistorik-Miyoo-<version>.zip`), dir `Roms/PORTS/Games/Prehistorik`, shortcut
  `Roms/PORTS/Shortcuts/Platform/Prehistorik (1991).port`.

## Build

- `miyoo/build/gen_sources.sh` (in WSL, called from PowerShell): the sources CMake would
  generate, through the project's own `cmake -P` scripts. It renames the title menu header
  OLDUVAI → PREHISTORIK and embeds `miyoo/art/icon.png`. From Git Bash the `/mnt` path is
  rewritten and nothing runs: the build id stays stale.
- `miyoo/build/build_miyoo.sh`: the source lists from CMakeLists.txt, `-DMIYOO_MINI`, the
  version from `project(olduvai VERSION ...)`, own `_start` (`start.S`, `-nostartfiles`),
  `glibc_compat.c` reached through `-Wl,--wrap=` (stat64 family, pthread_*, `__isoc23_strto*`,
  hypotf, arc4random, `__libc_single_threaded`), a check that no symbol needs glibc > 2.28.
  A header newer than `olduvai.unstripped` rebuilds every object (header-only changes once
  gave the same binary).
- `miyoo/build/pack_card.sh` (`PORT_VERSION=`): the card tree and the release zip.
- Smoke test on the PC: `MIYOO_HEADLESS=1` under `qemu-arm -B 0x100000000 -L <toolchain libc>`
  in WSL1.

## Platform layer (`miyoo/platform/`)

- `miyoo_sdl.cpp`: software `SDL_Renderer` (textures as 0xAARRGGBB, render targets, logical
  size with letterbox, nearest copy, NONE/BLEND), surfaces, events with event watches, evdev
  buttons as a Nintendo-layout game controller (A confirm/jump, B back/attack, MENU = ESC),
  SIGTERM/SIGINT → `SDL_QUIT` so Onion's power-off saves.
- `miyoo_fb.c`: 640x480 frame in MMA memory, MI_GFX blit with 180° turn, pan thread.
- `miyoo_audio.c`: SDL audio callback into audioserver's FIFO, 48 kHz.
- `miyoo_prof.c`: sampling profiler; `MIYOO_PROF_WALL=1` uses CLOCK_MONOTONIC timers (the
  CPU-clock ones gave ~16 samples in a whole run).
- Environment switches: `MIYOO_FRAMES=<file>` frame times, `MIYOO_STATS=1` present stats,
  `MIYOO_SHOTS` / `MIYOO_SHOTS_DIR` screenshots, `MIYOO_PROF`, `MIYOO_PROF_WALL`,
  `MIYOO_HEADLESS`, `MIYOO_GFX=0` (CPU present), `MIYOO_VSYNC`, `MIYOO_AUDIO=0`.

## Changes in `src/` (all marked `MIYOO_MINI` where the desktop behaviour stays)

- About screen, bug reports, menu header: the port's name and a link to upstream.
- `game_app.cpp`: Classic's screen pans and fades run at 60 Hz instead of 12 steps, unless
  `"transitions": "classic"` in `play.json`.
- `window_util.cpp` / `pacing.hpp`: `pace_period(last, ms)` paces by deadline; used in the
  title menu, the intro and the pause menu. Upstream drew, presented, then slept a fixed
  55 ms: 70 ms a frame on the device (14 fps) instead of 55 (18 Hz).
- `profile_table.hpp`: the `hd-handheld` profile pinned 2 interpolated sub-frames with
  `smooth_vsync` off, i.e. it slept on purpose (36.4 fps with the CPU idle). Pinned to 0
  (auto) and vsync auto on the Miyoo.
- `smooth_present.hpp`: under vsync the fill counts its tick budget from a fixed schedule
  (`deadline += frame_ms`, `SmoothPacer::deadline`), not from its own start, or the time
  between fills is lost: logic ran at 16.89 Hz, then 17.43, now 18.15 Hz (the game's 18.2).

## Measured on the device (2026-11-06, v0.9.11-2)

| | before | now |
|---|---|---|
| Title / pause menu | 70 ms a frame, ~14 fps | 55 ms, 18.3 fps |
| Enhanced HD, level 1 | 36.4 fps, logic 18.18 Hz | 59.7 fps, logic 18.15 Hz, no frame > 50 ms |
| Classic, level 1 | 60 Hz, ~11 ms CPU a frame (sound included) | same, smooth transitions |

Enhanced spends only ~5.7 ms of a frame in the present; the CPU has room left.

## Bench

`miyoo/tools/bench_menus.sh`, on the device as Onion would start it: copy it to `/tmp`, put
`RUNS="A B C" sh /tmp/bench_menus.sh` in `/tmp/cmd_to_run.sh`, `killall MainUI`. Its own
settings in `/tmp` (never the player's `conf/`), the build from `<game dir>/bench/`,
results in `bench/out/` (`done.txt` when finished), read them over SMB.

- A: title menu idle (frame times, profile, shots); B: level 1 then the pause menu (MENU sent
  with Onion's `sendkeys`); C: Enhanced HD level 1 for 40 s.
- **Never `--play-frames` for Enhanced**: the engine turns smooth motion off for frame-counted
  runs, so the result is wrong. Use a time limit and SIGTERM (the game quits and writes its
  frame times).

## Launcher (`miyoo/card/.../launch.sh`)

- Renames the game files to upper case through a temporary name (FAT may not tell the cases
  apart; `.txt` skipped), after deleting macOS `._*` / `.DS_Store` files.
- Game files missing and `Roms/DOS/Prehistorik (1991).zip` on the card: unzips the needed
  ones (busybox `unzip`) into `game/`.
- `Roms/DOS/Imgs/Prehistorik (1991).png` present: copies it over the port's picture when
  `cmp` says it differs (not every start; an update brings the release picture back, the
  next start replaces it again).
- One-time settings migration marked by `conf/.smooth-v2`: players who saved the old
  Enhanced pins (2 sub-frames, vsync off) get the new defaults once, later choices stay.
- `HOME`/`XDG_CONFIG_HOME` set to the game dir (`conf/olduvai/play.json`, quicksave),
  `OLDUVAI_BUG_DIR`, `OLDUVAI_PANEL=640x480`; performance governor and the second core online.
- `playlog.on` in the game dir: each session's frame times go to `playlog/` (stutter reports).
- `exec` the game, so Onion's SIGTERM at power-off reaches it and it saves.

## Release

1. gen_sources (PowerShell), build, `PORT_VERSION=x.y.z-n pack_card.sh`.
2. Check on the device (ask the owner first), then `gh release create vX.Y.Z-N` on
   ksielick/prehistorik-miyoo with the zip. Port Manager takes the latest release whose asset
   matches `^Prehistorik-Miyoo-.*\.zip$`; the catalog needs no change.
3. The entry is in Port Manager's public `catalog.json` since 2026-11-06 (status playable).
