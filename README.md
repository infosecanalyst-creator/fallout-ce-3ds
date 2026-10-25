# Fallout CE for New 3DS

Fallout 1 and Fallout 2 Community Edition engines running natively on the New Nintendo 3DS.
You must own the games (GOG or Steam); no game data is included.

- `fallout1/`: Fallout Community Edition, based on [MrHuu/fallout1-ce-3ds](https://github.com/MrHuu/fallout1-ce-3ds)
- `fallout2/`: Fallout 2 Community Edition, based on [Deenyoro/fallout2-ce-3ds](https://github.com/Deenyoro/fallout2-ce-3ds), which itself builds on MrHuu's 3DS platform layer

Both are forks of [alexbatalov/fallout1-ce](https://github.com/alexbatalov/fallout1-ce) and [alexbatalov/fallout2-ce](https://github.com/alexbatalov/fallout2-ce).

## What this fork changes

| Area | Change |
|---|---|
| Layout | Map defaults to the **bottom touchscreen** (stylus = mouse), HUD and message log on the top screen. ZL swaps; the choice is remembered. |
| Smart tap | A tap acts on whatever it touches: pick up, talk, loot, open, use, or attack a hostile in combat (running into reach first if needed). Open ground = move. Long-press = action menu. No cursor-mode switching needed. |
| Highlight | Hold R to outline every item and lootable body on the level (touch has no hover); in combat it also lists your hit chance against each visible enemy. |
| Quick heal | Select+A uses a Stimpak (or Super Stimpak) without opening the inventory. |
| Camera | Follows the player and recenters near the edge of the view; C-stick scrolls (New 3DS); Select recenters. |
| HUD shortcuts | Every HUD button on the untouchable top screen has a hardware shortcut (see controls). |
| Defaults | Always run, fast combat, Fallout 2 quicksave without slot/name prompts (`AutoQuickSave=3`), karma change messages. |
| Movie freeze fix | `SDL_OpenAudioDevice` returns 0 on failure, not -1; without `dspfirm.cdc` every movie used to hang forever in `_MVE_sndSync`. Movies now play silently. |
| SD streaming | Fallout 2 reads compressed DAT entries in 32 KB chunks (was 1 KB) with a 32 KB stdio buffer; Fallout 1's DAT handle gets a 16 KB buffer. |
| Build | Single Docker build (`./build.sh`), pinned devkitARM image and SDL2 commit. |

The full audit against stylus and mouse strategy-game conventions is in [docs/UX-AUDIT.md](docs/UX-AUDIT.md).

## Controls

| Input | Action |
|---|---|
| Tap | Act on what you touch (move / pick up / talk / loot / use / attack) |
| Long-press | Action menu (look, use skill, use item on...) |
| Circle Pad | Move cursor |
| C-stick | Scroll map (New 3DS; D-pad scrolls on Old 3DS) |
| A | Left click (hold to drag) |
| B | Active item button: attack with weapon, reload, or use item on a target |
| L | Right click (classic cursor-mode cycle) |
| R (hold) | Highlight items and lootable bodies; in combat also lists your chance to hit each visible enemy |
| Select+A | Quick heal (Stimpak, then Super Stimpak; 4 AP in combat) |
| X / Y | Inventory / Pip-Boy |
| D-pad Up / Down | Swap weapon hand / cycle weapon mode |
| D-pad Left / Right | Skilldex / character sheet |
| ZR | Start combat; in combat, end turn |
| Start+ZR | End combat |
| ZL | Swap map between screens |
| Select | Center map on the player |
| Select+X / Select+Y | Automap / Pip-Boy clock (rest) |
| Select+L | Cycle display mode (whole game screen) |
| Select+Up/Down, 3D slider | Zoom the map |
| Start | Menu / back |
| Start+X / Start+Y | Quicksave / quickload |

## Install

```
sdmc:/3ds/dspfirm.cdc                  (for sound; dump once with the DSP1 homebrew)
sdmc:/3ds/fallout/   fallout-ce.3dsx   MASTER.DAT CRITTER.DAT DATA/ fallout.cfg
sdmc:/3ds/fallout2/  fallout2-ce.3dsx  master.dat critter.dat patch000.dat data/ sound/ fallout2.cfg
```

Recommended settings (already what the author ships on his card):

```
fallout.cfg / fallout2.cfg  [preferences]  running=1  combat_speed=50  player_speedup=1
fallout2/ddraw.ini          [Misc]         AutoQuickSave=3  DisplayKarmaChanges=1
```

The data files come from the Windows GOG/Steam install. `innoextract -I app setup_fallout*.exe` extracts them from the GOG installers on Linux/macOS.

## Build

```
./build.sh            # both games -> out/
./build.sh fallout2   # one game
```

Requires Docker. The first run builds a toolchain image containing SDL2 for 3DS.

## Testing

Tested in the Azahar emulator (New 3DS mode): boot, menus, new game, map/HUD layout, touch movement, inventory, Pip-Boy, screen swap, quicksave/quickload, combat end turn. Azahar doesn't emulate the 800px top-screen mode, so top-screen checks used a build with wide mode disabled.

## License

The engine source is under the upstream [Sustainable Use License](fallout2/LICENSE.md): free use, modification and non-commercial redistribution. It is source-available, not OSI open source. Fallout is © Bethesda Softworks / Interplay; game data is not included.
