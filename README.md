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

## Getting Started

The original Ultima V game files are **not** included; you need your own copy (for example the GOG release of Ultima V).

### Windows: Installer (no Visual Studio needed)

Download `u5win-installer.msi` from the [latest release](https://github.com/Daemongar/u5d/releases/latest) and run it. It's a self-contained, per-user installer (no admin rights required, nothing else to install) that adds an "Ultima V" shortcut to the Start Menu. Run it once to install, then launch it from there.

### Everyone else: build it (see [Building](#building))

Once you have the game running (installed or built):

- From your Ultima V folder, it starts straight away.
- From anywhere else, it asks you to choose your Ultima V folder the first time and remembers it (in `%APPDATA%\u5d\UltimaV` on Windows, `~/.local/share/u5d/UltimaV` on Linux).
- `--data <folder>` on the command line picks the folder directly.

A folder counts as Ultima V if it contains `BRIT.DAT`, `DATA.OVL`, `TILES.16` and `INIT.GAM`. GOG installs, which keep `INIT.GAM` and `SAVED.GAM` in `__support/save`, work as they are. Saves (`SAVEGAME/`) and `ultima5.ini` are kept in the game folder.

The full-color `TILES.BMP` is used from the game folder if present, otherwise from next to the program or the repository root, so a fresh build finds the one in this repository. Background music (`BGM/`) is optional; see [Music](#music).

## Building

### Windows Target

Open `u5win/u5win.slnx` with Visual Studio 2026 and build the solution. (If you just want to play the game without installing Visual Studio, use the [installer](#windows-installer-no-visual-studio-needed) instead.)

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

### Settings

Choose **Options** on the title menu, or press **F1** during play, to change settings: arrow keys choose and change, Tab switches between the Display, Sound and Gameplay pages, and Esc saves and returns. Display and sound changes apply immediately.

Settings are stored in `ultima5.ini` in the game data directory:

```ini
[window]
mode=windowed        ; fullscreen (default) | bordered | windowed
width=1280           ; window size in windowed mode
height=960

[display]
keep_aspect=0        ; 1: keep the 4:3 shape with black bars
color_tiles=1        ; use TILES.BMP when present
enhanced_palette=1   ; softer 16-color palette
wide_view=0          ; 1: 17-column map for 16:9 screens

[sound]
music=1
music_volume=8       ; 0-10
sound_effects=1
sound_volume=8       ; 0-10
terrain_footsteps=1

[gameplay]
party_exp=1          ; ... one entry per gameplay change, 1 = on
bard_mana_roundup=1  ; ini only, not on the Options screen
```

- `fullscreen`: borderless, covering the whole screen.
- `bordered`: a normal window with a title bar, maximized to fill the screen.
- `windowed`: a resizable window at `width` x `height`, shrunk to fit smaller screens.

Without `mode`, the older `fullscreen=1` / `fullscreen=0` setting selects fullscreen or bordered. Each gameplay setting only takes effect if its `ENABLE_*` switch in `src/common/platform.h` is compiled in; the original-behavior DOS build is unaffected.

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

The included `TILES.BMP` is generated from the original `TILES.16` by `tools/upscale_tiles.py` (requires Pillow and NumPy):

```sh
python3 tools/upscale_tiles.py /path/to/TILES.16 TILES.BMP --header src/graphics/tile_colors.h
```

It doubles each tile with Scale2x to round off diagonal edges and recolors it with a softened palette and light top/bottom shading, keeping the original black backgrounds. Copy the result into the game data directory. `--header` also regenerates `src/graphics/tile_colors.h`, the palette the game uses for animated tiles; rebuild after changing it.

Animated tiles (water, lava, flags, fireplaces, shorelines, the clock) keep moving in full color with `ENABLE_ANIMATED_COLOR_TILES`: water and lava scroll their atlas tiles, and any other tile the game rewrites is rendered in full color from its current data.

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
Borderless fullscreen by default; bordered fullscreen or a resizable window via mode= in ultima5.ini.
A golden ankh application icon (window, taskbar and Windows executable), generated by tools/make_icon.py.
Data files load regardless of upper/lower case in their names.
Graphics

Rendered at 640×400 with 32×32 tiles instead of 320×200.
An optional full-color tile set (TILES.BMP) replaces any of the original 16-color tiles.
Animated tiles (water, lava, flags, fireplaces, shorelines, the clock) keep animating in full color.
An optional Wide view (16:9) shows a 17-column map outdoors and in towns instead of 11, with the stats panel at the right edge; combat keeps its 11x11 battlefield.
A softer, less oversaturated version of the 16-color palette.
Sound

Background music from the Commodore 64 version's soundtrack, as .ogg or .mp3 files.
PC-speaker sound effects are recreated in software.
Gameplay

Armor now reduces combat damage. In DOS it had no effect.
The Ztats screen shows each character's armor score (the defense combat uses) above Magic.
Mouse aiming in combat.
Easier conversations: keywords aren't case-sensitive, long replies page automatically, and keyword hints are shown.
Smarter monster targeting.
The whole party earns experience for kills, with a bonus for fighting groups.
Brighter nights outdoors.
Esc opens the quit prompt.
Level-up stat gains are weighted by class.
Bard mana rounds up.
Vas Mani (Great Heal) works during combat.
Casting a spell you lack the MP or level for no longer uses up the mixture or MP ("M.P. too low!" / "Level too low!").
Cancelling a spell's target, crosshair, or choosing "Pass" for its direction refunds the mixture and MP.
Poisoned characters no longer die overnight at an inn; they wake healed but still poisoned.
Level-ups grant one stat point per level gained, so skipping levels between apparitions no longer loses points.
Rings of Invisibility and Regeneration can crumble when equipped (1 in 16), a feature the original disabled by a typo.
Druid, Paladin, Ranger and Tinker characters imported from Ultima IV now regain mana.
Overworld footsteps vary by terrain (grass, brush, forest, swamp, desert, hills, mountains, roads) instead of one click everywhere.
Game speed is corrected for modern computers.
Bug fixes

Saving works: the save folder is now created.
Clone (In Quas Xen) no longer freezes the game or creates a clone that kills a party member when it dies.
Rel Xen Bet can't turn party members into enemies.
Choosing "Pass" with a storm spell no longer freezes the game.
Several out-of-bounds memory reads from the original code are fixed.
Characters imported from Ultima IV are limited to Ultima V's stat and HP caps, fixing broken shop prices and an 800-HP Avatar.
A level-up can no longer lower a stat that is already above the cap.
Gameplay changes can be switched on or off in the Options screen (title menu or F1) or in ultima5.ini; bug fixes can be compiled out in platform.h. A separate DOS build can still reproduce the original program exactly.
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
- **Lowered by:** attacking a townsperson (−5), killing one asleep in bed (another −5), killing one with a ship's cannon (−5), revealing a shrine mantra to Blackthorn (−5, and that shrine is ruined), opening a chest in a town or castle (−2), picking crops (−1), eating food off a table (−1), insisting on another drink at a tavern (−1), and some conversation answers (−1).
- **Raised by:** completing a shrine quest (+3, or +6 for Spirituality), donating gold at a completed shrine (+1 per 100 gold), freeing a chained prisoner (+2), giving gold to a beggar (+1, at most once per 100 turns, +2 more if it was your last gold), and some conversation answers (+1).
- **Picking up loose items never costs karma**, anywhere, including the Crown, Sceptre and Amulet of Lord British.
- **Dying** multiplies every character's experience by the karma percentage (unless karma is 98 or more), then raises karma to at least 75, so dying repairs a low reputation. The Resurrect spell (In Mani Corp) applies the same experience penalty, can lower the target's level, and refills their mana.
- **Lord British's message** when he raises the party, and at each level-up, is chosen from `KARMA.DAT` in bands of 20 karma.
- Conversation scripts can branch on a minimum karma, so some characters respond differently to a low reputation.

### Spells

- **Clone (In Quas Xen)** could copy a party member, and the copy shared the original's character record, so killing the clone killed the real character. Its placement loop never gave up and could freeze the game. Fixed under `ENABLE_BUG_FIX`.
- **Rel Xen Bet** never checked its target and could turn a party member into a hostile creature. Fixed under `ENABLE_BUG_FIX`.
- **Vas Mani (Great Heal)** was refused until "VICTORY!", although the spell table lists it as castable in combat, so a mid-fight cast wasted the mixture and mana. `ENABLE_VAS_MANI_IN_COMBAT` lets it work.
- **Failed casts waste resources:** the mixed spell is used up before the mana check, and mana is spent before the level check. Backing out of a spell's target or direction prompt also consumes both. `ENABLE_NO_WASTED_CASTS` checks MP and level first and refunds cancelled casts.
- **Choosing "Pass" as a storm spell's direction froze the game.** The FM Towns version added a check, which the SDL builds include.
- **Storm spells** (In Zu, In Nox Hur, In Flam Hur, In Vas Grav Corp) hit everyone in their path, including the party.
- **Charm (An Xen Ex)** toggles the charmed state, so it can free a charmed companion.
- **Rel Hur's "Pass"** does nothing, while the scroll version calms the wind.

### Party Members

- **The party holds at most six.** Joining has no karma check, although some characters' dialogue depends on karma.
- **Low karma, bad deeds, and death never make a companion leave.** Dead companions stay in the party until resurrected.
- **Leaving someone at an inn** is the only voluntary way to part. The innkeeper asks "Who will stay?"; the companion waits at that inn until you check them out. The Avatar and dead companions can't be left.
- **Blackthorn's interrogation.** When captured, the party is chained and Blackthorn demands a shrine mantra; the answer is checked against the same mantra table the shrines use. The interrogation only happens while he still has a mantra to ask for.
  - **Refusing or lying:** he asks up to four times as the pendulum descends, then "the pendulum blade falls" on a companion.
  - **Revealing the mantra:** costs 5 karma and ruins that shrine, and he still kills a companion: "As a token of my esteem for thine honesty, I will grant thy companion a merciful death!" A ruined shrine can be restored with Yell at the ruins, then meditating with its virtue and mantra.
  - **Captured alone:** the Avatar is spared ("I sense Truth in thee") for the mantra, or thrown in the dungeon for a lie.
  - **The victim is always the first living companion in party order**, so either answer costs a companion whenever one is present.
- **Executed companions are gone for good.** They are removed from the party and marked; the Shrine of the Codex later shows an urn for each ("Thou dost see an urn marked: …"). Nothing in the game brings them back.

### Shops

- **Prices use the shopping character's Intelligence:** price = base × (200 − 3 × INT) / 100, using the active character (or the first able member). Even at the maximum INT of 30 everything costs 110% of base; selling pays INT × 3% of the same base.
- **At INT 30, cheap items** (Dagger, Club, Spear, Flaming Oil, Throwing Axe) sell back for exactly what they cost, because the markup rounds down and the sale price adds 1 gold.
- **Merchants refuse riders:** "GET THAT HORSE OUT OF HERE!"

### Inns and Resting

- **Poisoned characters died overnight at an inn**, even the Avatar: the game restored them to full HP and mana, then announced they had "passed away". `ENABLE_NO_INN_POISON_DEATH` lets them wake still poisoned.
- **Being put to sleep cures poison.** Status is a single value, so sleep replaces "poisoned" and waking sets "good"; a monster's sleep spell (or In Zu) cures poison that an inn bed would have made fatal.
- **A night's rest is charged per party member, dead ones included.**
- **Companions left at an inn take up its rooms**, so storing enough of them there makes the inn report "no room available" when the party tries to rest.
- **Poison costs 1 HP per turn, but only while awake.**

### Levelling Up

- **Promotion is random.** Levels are only granted by an apparition that appears with a 25% chance when camping; inns never summon it, so a character can have the experience for a level and still wait several rests.
- **The apparition fully heals and cures the party** whether or not anyone levels.
- **One stat point per visit, not per level:** jumping from level 2 to 4 between visits granted one point. `ENABLE_STAT_PER_LEVEL` grants one per level.
- **Max HP is level × 30 for every class**; Strength and class play no part.
- **Levels follow experience by doubling:** 100, 200, 400, 800, 1600, 3200 and 6400 experience for levels 2 to 8.

### Ultima IV Import

- **A new game can import the Avatar from an Ultima IV save**, copying Strength, Dexterity, Intelligence, HP, max HP and experience. The import only rejected stats above 70 or HP above 9999, while Ultima V caps stats at 30 and max HP at 240.
- **Above INT 33 the shop price formula overflows** (it mixes signed and unsigned arithmetic): every item costs about 23,600 gold. Above INT 37 selling pays more than the base price, so buying with a lower-INT companion and selling with the Avatar made unlimited gold.
- **"Thou art now… wiser!" could lower a stat:** the level-up bonus capped the stat at 30, cutting an imported 50 down to 30.
- **An imported Avatar kept up to 800 max HP** until death or resurrection recalculated it to level × 30.
- All of these are fixed under `ENABLE_BUG_FIX` by clamping imported values to Ultima V's limits.

### Food

- **Food is eaten at 06:00, 12:00 and 18:00**, one ration per awake, living party member; sleeping companions don't eat.
- **With no food, every living member takes 1–8 damage each hour.**

### Hidden and Unfinished Content

- **Five dormant classes.** The class list is `AMBFDTPRS`: alongside Avatar, Mage, Bard and Fighter are Ultima IV's Druid, Tinker, Paladin, Ranger and Shepherd. No Ultima V character uses them; they appear only through the Ultima IV import, which keeps the old class unless the Ultima IV save shows the game was completed. Combat gives them sprites (Paladins and Rangers look like fighters, Druids like mages, Tinkers and Shepherds like bards), but mana was only ever refilled for the Avatar, Mage and Bard, so an imported Druid, Paladin, Ranger or Tinker never regained any. `ENABLE_U4_CLASS_MANA` applies Ultima V's halving of Ultima IV mana: Druid 3/4, Paladin and Ranger 1/2, Tinker 1/4 of INT (see `src/class_mana.h`).
- **"Ring vanishes!" never happened.** Equipping a Ring of Invisibility or Regeneration was meant to have a 1-in-16 chance of the ring crumbling, but the check compares the number of rings owned with the item IDs (42 and 44). `ENABLE_VANISHING_RINGS` restores it.
- **The Ring of Protection and Spiked Collar did nothing in DOS.** Their only effect is +2 defense, and armor never applied in combat.
- **An empty FLAMES module.** The game ships `FLAMES.OVL`, a 32-byte overlay whose only routine does nothing; its name hints at a cut effect or scene.
- **One footstep for every terrain.** Overworld movement always plays the same two white-noise clicks, whatever the ground; only riding a horse doubles them, and ships are silent. `ENABLE_TERRAIN_FOOTSTEPS` varies the sound by the tile entered.
- **System keys:** Ctrl+V prints the version ("1.16"), Ctrl+S toggles sound, Ctrl+B toggles keyboard buffering, Ctrl+E quits, and Ctrl+K prints karma.

### Odds and Ends

- **The traitor Saduj is recognized by his name:** the code treats any party member whose name has 'j' as its fifth letter as hostile.
- **The game requests some files in lower case** (`ibm.ch`, `runes.ch`) while the disk files are upper case. DOS ignored the difference; the Linux build matches file names case-insensitively.
- **Several routines read just outside their arrays**, relying on the DOS memory layout to make the stray reads harmless. These are guarded under `ENABLE_BUG_FIX`.
