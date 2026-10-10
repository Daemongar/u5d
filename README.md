# u5d - Fixes to the original game

## Introduction

This is an update to U5 decompiled at https://github.com/drcarademono/impera

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

## Change Details
Ultima V: Changes from the PC-DOS Version
Platforms

Native Windows and Linux versions, rebuilt from decompiled source and using SDL3.
Borderless fullscreen by default, or windowed with fullscreen=0 in ultima5.ini.
Data files load regardless of upper/lower case in their names.
Graphics

Rendered at 640×400 with 32×32 tiles instead of 320×200.
An optional full-color tile set (TILES.BMP) replaces any of the original 16-color tiles.
A softer, less oversaturated version of the 16-color palette.
Sound

Background music from the Commodore 64 version's soundtrack, as .ogg or .mp3 files.
PC-speaker sound effects are recreated in software.
Gameplay

Armor now reduces combat damage. In DOS it had no effect.
Mouse aiming in combat.
Easier conversations: keywords aren't case-sensitive, long replies page automatically, and keyword hints are shown.
Smarter monster targeting.
The whole party earns experience for kills, with a bonus for fighting groups.
Brighter nights outdoors.
Esc opens the quit prompt.
Level-up stat gains are weighted by class.
Bard mana rounds up.
Vas Mani (Great Heal) works during combat.
Game speed is corrected for modern computers.
Bug fixes

Saving works: the save folder is now created.
Clone (In Quas Xen) no longer freezes the game or creates a clone that kills a party member when it dies.
Rel Xen Bet can't turn party members into enemies.
Choosing "Pass" with a storm spell no longer freezes the game.
Several out-of-bounds memory reads from the original code are fixed.
All gameplay changes and fixes can be switched off in platform.h to get the original behavior. A separate DOS build can still reproduce the original program exactly.
Sound effects are synthesized in software (`src/audio/aud_sdl.c`) from the parameters the original driver already dispatches (`AUDIO_DispatchPulse`/`Noise`/`Tone`/`SweepTone`), rather than played back from sampled `SFX/*.wav` files - no extra assets required. A handful of IDs with no reachable dispatch path (`mirror`, `drip1`, `drip2`) remain silent.

## Discoveries

Things learned about the original PC-DOS game while working on the source. They come from reading the decompiled code and the original data files, so they describe the DOS program as written. Where a fix exists, it can be switched off in `src/common/platform.h`.

### Combat and Armor

- **Armor had no effect in combat.** Every character's defense was a stored value of 7, which never changes, regardless of what they wore. The routine that totals equipped armor (`ULTIMA_6da8`) contains a comparison that is always false under the original compiler, and combat never called it anyway. The unequip routine recalculates the total and discards it. `ENABLE_BUG_FIX` adds the armor total on top of the base 7.
- **Item defense values match `DATA.OVL`** byte for byte. The Jewel Shield's defense is 0.
- **In the Doom dungeon**, armor counts only if Mystic Armour is worn. The Protection spell (In Sanct) adds +3.
- **Monsters hit for a fixed amount**; player weapons roll a random amount up to the weapon's maximum. Damage is reduced by a random roll from 1 to the defender's defense, and zero or less is a miss.
- **Fear (In Quas Corp) and Repel Undead (An Xen Corp)** work by setting the monster's HP to 1, which makes it flee.

### Karma

- Karma is a single hidden value from 0 to 99, starting at 75 in `INIT.GAM`. **Ctrl+K** prints it in towns, dungeons and outdoors.
- **Lowered by:** attacking a townsperson (−5), killing one asleep in bed (another −5), killing one with a ship's cannon (−5), naming a resistance member to Blackthorn (−5), opening a chest in a town or castle (−2), picking crops (−1), eating food off a table (−1), insisting on another drink at a tavern (−1), and some conversation answers (−1).
- **Raised by:** completing a shrine quest (+3, or +6 for Spirituality), donating gold at a completed shrine (+1 per 100 gold), freeing a chained prisoner (+2), giving gold to a beggar (+1, at most once per 100 turns, +2 more if it was your last gold), and some conversation answers (+1).
- **Picking up loose items never costs karma**, anywhere, including the Crown, Sceptre and Amulet of Lord British.
- **Dying** multiplies every character's experience by the karma percentage (unless karma is 98 or more), then raises karma to at least 75, so dying repairs a low reputation. The Resurrect spell (In Mani Corp) applies the same experience penalty, can lower the target's level, and refills their mana.
- **Lord British's message** when he raises the party, and at each level-up, is chosen from `KARMA.DAT` in bands of 20 karma.
- Conversation scripts can branch on a minimum karma, so some characters respond differently to a low reputation.

### Spells

- **Clone (In Quas Xen)** could copy a party member, and the copy shared the original's character record, so killing the clone killed the real character. Its placement loop never gave up and could freeze the game. Fixed under `ENABLE_BUG_FIX`.
- **Rel Xen Bet** never checked its target and could turn a party member into a hostile creature. Fixed under `ENABLE_BUG_FIX`.
- **Vas Mani (Great Heal)** was refused until "VICTORY!", although the spell table lists it as castable in combat, so a mid-fight cast wasted the mixture and mana. `ENABLE_VAS_MANI_IN_COMBAT` lets it work.
- **Failed casts waste resources:** the mixed spell is used up before the mana check, and mana is spent before the level check.
- **Choosing "Pass" as a storm spell's direction froze the game.** The FM Towns version added a check, which the SDL builds include.
- **Storm spells** (In Zu, In Nox Hur, In Flam Hur, In Vas Grav Corp) hit everyone in their path, including the party.
- **Charm (An Xen Ex)** toggles the charmed state, so it can free a charmed companion.
- **Rel Hur's "Pass"** does nothing, while the scroll version calms the wind.

### Odds and Ends

- **The traitor Saduj is recognized by his name:** the code treats any party member whose name has 'j' as its fifth letter as hostile.
- **The game requests some files in lower case** (`ibm.ch`, `runes.ch`) while the disk files are upper case. DOS ignored the difference; the Linux build matches file names case-insensitively.
- **Several routines read just outside their arrays**, relying on the DOS memory layout to make the stray reads harmless. These are guarded under `ENABLE_BUG_FIX`.
