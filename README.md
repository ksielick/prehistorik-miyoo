# prehistorik-miyoo

Prehistorik (Titus, 1991) on the **Miyoo Mini Plus** with **OnionOS**: a modified build of
[Olduvai](https://github.com/ksokolowski/olduvai), Krzysztof Sokołowski's from-scratch engine
for the game, ported to the device.

> **This is not Olduvai and is not made or endorsed by the Olduvai project.** Olduvai's
> licence (GPL-3.0-or-later) covers the code; the name "Olduvai" and its logo are the
> author's marks, so this modified version carries its own name
> ([LEGAL.md](LEGAL.md), "Project name and marks"). Report problems with this port here, not
> to Olduvai. The original project's README: [docs/OLDUVAI-README.md](docs/OLDUVAI-README.md).

No game content is included: you need your own copy of the game
([GOG's *Prehistorik 1+2*](https://www.gog.com/game/prehistorik_12) or the DOS original).

## Install

**With Port Manager** (when the port is in its catalog): Port Manager › all ports ›
*Prehistorik (1991)* › install.

**By hand:** download `Prehistorik-Miyoo-<version>.zip` from the
[releases](../../releases/latest), unzip it and copy its `Roms` folder to the root of the SD
card, merging it with the one there.

Then copy your game files into `Roms/PORTS/Games/Prehistorik/game/`:
`FILESA.CUR`, `FILESB.CUR`, `FILESA.VGA`, `FILESB.VGA` and `HISTORIK.EXE` (or GOG's
`PREH.SQZ`). If the DOS game is already on the card for DOSBox as
`Roms/DOS/Prehistorik (1991).zip`, the first launch takes the files from it, and the DOS
game's picture (`Roms/DOS/Imgs/Prehistorik (1991).png`) becomes the port's picture too.

The game is in **Games › Ports › Platform › Prehistorik (1991)** (run *~Refresh roms* in the
Ports list if it isn't there yet).

## Play

| Button | |
|---|---|
| D-pad | move |
| A | jump, confirm in menus |
| B | attack, back in menus |
| START, MENU | pause menu (Options, Quit) |
| SELECT + R1 / L1 | quicksave / quickload |

It starts in **Classic DOS**, the 1991 game, with one change made for this port: the pans and
fades between screens run smoothly at 60 Hz (*transitions: classic* in
`conf/olduvai/play.json` brings back the original 12 steps). *Options › Style* switches to
Enhanced HD, which is heavy for this CPU.

## What the port changes

Everything Miyoo-specific is in [`miyoo/`](miyoo/) plus a few `#ifdef MIYOO_MINI` lines in
`src/`:

- `miyoo/platform/`: the part of SDL2 the engine calls, implemented on the device directly
  (no SDL library): a software `SDL_Renderer` into a 640x480 frame that the SoC's 2D engine
  (MI_GFX) turns 180 degrees onto the panel, the buttons as a Nintendo-style controller,
  sound through Onion's audioserver. After the author's other Miyoo ports
  (quake2-miyoomini, redriver2-miyoo, tyrquake-miyoo).
- `miyoo/build/`: the build with the Windows-hosted Arm GNU Toolchain 13.3 for the device's
  glibc 2.28, without CMake (`build_miyoo.sh`); the sources CMake would generate
  (`gen_sources.sh`, cmake -P); the card layout and release zip (`pack_card.sh`).
- `miyoo/card/`: the launcher, the Ports shortcut and picture.
- `src/`: the port's own name on the About screen, in the title menu header and in bug
  reports; smooth screen transitions in Classic.

## Build

From Git Bash on Windows, with the Arm GNU Toolchain (`TC=`) and SDL2 headers (`SDL2_INC=`):

```sh
wsl -e sh miyoo/build/gen_sources.sh   # needs cmake (here in WSL)
miyoo/build/build_miyoo.sh             # -> build/miyoo/olduvai
miyoo/build/pack_card.sh               # -> build/miyoo/card/, Prehistorik-Miyoo-<version>.zip
```

## Licence

GPL-3.0-or-later, as Olduvai ([LICENSE](LICENSE)); third-party parts in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). Olduvai © 2026 Krzysztof Sokołowski.
Prehistorik © 1991 Titus Interactive; all marks belong to their owners.
