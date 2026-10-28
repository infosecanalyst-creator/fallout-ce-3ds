# Fallout CE for New 3DS

Fallout 1 and Fallout 2 Community Edition engines running natively on the New Nintendo 3DS.
You must own the games (GOG or Steam); no game data is included.

- `fallout1/`: Fallout Community Edition, based on [MrHuu/fallout1-ce-3ds](https://github.com/MrHuu/fallout1-ce-3ds)
- `fallout2/`: Fallout 2 Community Edition, based on [Deenyoro/fallout2-ce-3ds](https://github.com/Deenyoro/fallout2-ce-3ds), which itself builds on MrHuu's 3DS platform layer

Both are forks of [alexbatalov/fallout1-ce](https://github.com/alexbatalov/fallout1-ce) and [alexbatalov/fallout2-ce](https://github.com/alexbatalov/fallout2-ce).

## What this fork changes

| Area | Change |
|---|---|
| Layout | The **bottom touchscreen is the game** (stylus = mouse); the **top screen is an information dashboard**: HP bar, AP pips, AC, weapon / attack mode / AP cost / ammo, whose turn it is in combat, the enemy you last tapped or the one attacking you (health and hit chance), the other hand, level/XP, caps and carry weight (or poison/rads), date and time, a full-width message log and a one-line button hint on screens that need one. Inventory, loot and Skilldex also keep the dashboard on top instead of a duplicate picture. ZL swaps map and HUD. |
| Movement | The **Circle Pad walks the player** in any direction, like Dungeon Master / SimGolf: continuous when exploring (turns instantly, slides along walls, stops on release); in combat each push steps one hex to the neighbour nearest the stick's angle. |
| Smart tap | A tap acts on whatever it touches: pick up, talk, loot, open, use, or attack a hostile in combat (running into reach first if needed). Open ground = move. Long-press = action menu. No cursor-mode switching needed. |
| Highlight | Hold R to outline every item and lootable body on the level (touch has no hover); in combat it also lists your hit chance against each visible enemy. |
| Quick heal, reload, take all | Select+A uses a Stimpak (or Super Stimpak) and Select+B reloads, without opening the inventory; X takes everything from a body or container. |
| Back | B and Start back out of any in-game menu (inventory, Pip-Boy, character sheet, Skilldex, automap, options). |
| Out-of-game top screen | Main menu and other screens outside a game show a controls card on the top screen. The character sheet shows a hint that the Circle Pad scrolls down to the skills. |
| Camera | Follows the player and recenters near the edge of the view; C-stick scrolls (New 3DS); Select recenters. |
| HUD shortcuts | Every HUD button on the untouchable top screen has a hardware shortcut (see controls). |
| Defaults | Always run, fast combat, Fallout 2 quicksave without slot/name prompts (`AutoQuickSave=3`), karma change messages. |
| Fallout 1 saving | Saving failed when the SD folder's case differed from the config (`DATA` vs `master_patches=data`): the slot file list came back empty. Directory listing now falls back to upper case. |
| Movie freeze fix | `SDL_OpenAudioDevice` returns 0 on failure, not -1; without `dspfirm.cdc` every movie used to hang forever in `_MVE_sndSync`. Movies now play silently. |
| SD streaming | Fallout 2 reads compressed DAT entries in 32 KB chunks (was 1 KB) with a 32 KB stdio buffer; Fallout 1's DAT handle gets a 16 KB buffer. |
| Build | Single Docker build (`./build.sh`), pinned devkitARM image and SDL2 commit. |

The full audit against stylus and mouse strategy-game conventions is in [docs/UX-AUDIT.md](docs/UX-AUDIT.md).

## Manual

Each game has a built-in manual: press **Select** on the main menu or any menu, or **Select+Start** on the map. The same text is in [docs/MANUAL-FALLOUT1.md](docs/MANUAL-FALLOUT1.md) and [docs/MANUAL-FALLOUT2.md](docs/MANUAL-FALLOUT2.md). Edit `docs/manual-source.txt` and run `python3 tools/gen_manual.py` to update both the in-game and Markdown versions.

## Controls

| Input | Action |
|---|---|
| Tap | Act on what you touch (move / pick up / talk / loot / use / attack) |
| Long-press | Action menu (look, use skill, use item on...) |
| Circle Pad | Walk (the stylus is the pointer; Circle Pad moves the cursor in menus and scrolls the character sheet) |
| C-stick | Scroll map (New 3DS; D-pad scrolls on Old 3DS) |
| A | Left click (hold to drag) |
| B | On the map: active item button (attack with weapon, reload, or use item on a target); press again to cancel. In menus: back (right click in conversations) |
| Select+B | Reload the active weapon |
| X (looting) | Take all |
| L | Right click (classic cursor-mode cycle) |
| R (hold) | Highlight items and lootable bodies; in combat also lists your chance to hit each visible enemy |
| Select+A | Quick heal (Stimpak, then Super Stimpak; 4 AP in combat) |
| X / Y | Inventory / Pip-Boy |
| D-pad Up / Down | Swap weapon hand / cycle weapon mode |
| D-pad Left / Right | Skilldex / character sheet |
| ZR | Start combat; in combat, end turn |
| Start+ZR | End combat |
| ZL | Swap map between screens |
| Select | Center map on the player (in menus: open the manual) |
| Select+X / Select+Y | Automap / Pip-Boy clock (rest) |
| Select+L | Cycle display mode (whole game screen) |
| Select+Up/Down, 3D slider | Zoom the map |
| Start | Options menu / back |
| Start+X / Start+Y | Quicksave / quickload |
| Select (menus) / Select+Start (map) | Manual |

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

Tested in the Azahar emulator (New 3DS mode), both games: main menu, new game, save and load, Circle Pad walking in all directions (exploring and in combat), C-stick scrolling, camera follow, tap-to-attack, R highlight, quick heal, every menu's top screen, and backing out with B / Start. The 800px wide top-screen mode is off, so the emulator and the console show the same picture.

## License

The engine source is under the upstream [Sustainable Use License](fallout2/LICENSE.md): free use, modification and non-commercial redistribution. It is source-available, not OSI open source. Fallout is © Bethesda Softworks / Interplay; game data is not included.
