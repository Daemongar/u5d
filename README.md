# u5d - Fixes to the original geme

## Introduction

This is an update to U5 decompiled st https://github.com/drcarademono/impera

This is a bug fix and graphics update (640x480 with 32x32 pixel tiles, and full color tiles.)

## Goals

- Update the game to fix any bugs (armor, spell, etc)
- Continue to work on the graphics and audio
- Maintain this build so it runs on Linux and Windows without Dosbox or any emulator
- Instead of all NPCs being different classes but effectively the same, make the classes truly different
  
## Current Status

### Matching Progress

- `ULTIMA.EXE`: approximately 95% matched, excluding assembly functions
- `*.OVL`: approximately 95% matched

### Playability

The game can be played through to the ending using the original data files. I'm working on fixing spell problems, bugs, and issues with the PC version

## Building

### Windows Target

Open `u5win/u5win.slnx` with Visual Studio 2026 and build the solution.

### Linux Target

Requires SDL3 and SDL3_mixer 3.x (the `MIX_*` API). If your distribution doesn't package them, build both from source with CMake and install into `~/.local`, which `Makefile.linux` searches automatically:

```sh
cmake -S SDL -B SDL/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build SDL/build -j && cmake --install SDL/build
cmake -S SDL_mixer -B SDL_mixer/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local \
      -DCMAKE_PREFIX_PATH=$HOME/.local -DSDLMIXER_VENDORED=OFF
cmake --build SDL_mixer/build -j && cmake --install SDL_mixer/build
```

Then build with `make -f Makefile.linux` (or `CONFIG=debug`). The executable is `build/linux/<config>/u5linux`; run it from the directory containing the original game data files.

### Optional Full-Color Tiles

The SDL builds can load an optional `TILES.BMP` atlas from the game data directory.
Use a 24-bit or 32-bit BMP arranged as a row-major grid of 32x32 pixel tiles.
Tile index 0 is the top-left cell; each following index advances left-to-right,
then top-to-bottom. The atlas may have any number of columns, but its width and
height must be multiples of 32. Add cells for the tile indices you want to
replace; indices not present in the atlas continue using the original tileset.

The full-color atlas is optional and the original 16-color tiles remain the
default. No additional image libraries are needed. The legacy DOS graphics
drivers do not load this SDL-specific atlas.

### MS-DOS Target (DJGPP)

Run `make` on a machine with DJGPP installed.

Depending on the environment, you may need to adjust the `CROSS_CC_PREFIX` value in the `Makefile`.

### MS-DOS Target (Original Toolchain)

Run `src\build.bat` on a machine or virtual machine with Microsoft C 5.1 installed.

The compiler installation path is `C:\MSC51`.

Note: executable linking is currently not possible. This target is used only for disassembly matching.

## Music

The SDL targets (Windows and Linux) expect background music as `BGM/01`-`15` (`.ogg` or `.mp3`) in the directory the game is run from (see `src/audio/aud_sdl.c`). These aren't included in this repository. `u5win/ULTMVMSC.P00` is a Commodore 64 program (Minstrel Dragon / UDIC's "Lost Music") containing 15 selectable Ultima V tracks; run `tools/extract_bgm.ps1` (requires VICE and ffmpeg) to render them to the expected `.ogg` files.

Sound effects are synthesized in software (`src/audio/aud_sdl.c`) from the parameters the original driver already dispatches (`AUDIO_DispatchPulse`/`Noise`/`Tone`/`SweepTone`), rather than played back from sampled `SFX/*.wav` files - no extra assets required. A handful of IDs with no reachable dispatch path (`mirror`, `drip1`, `drip2`) remain silent.
