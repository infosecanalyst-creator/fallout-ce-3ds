# Fallout CE for New 3DS

Fallout 1 and Fallout 2 Community Edition engines running natively on the New Nintendo 3DS.
You must own the games (GOG or Steam); no game data is included.

- `fallout1/`: Fallout Community Edition, based on [MrHuu/fallout1-ce-3ds](https://github.com/MrHuu/fallout1-ce-3ds)
- `fallout2/`: Fallout 2 Community Edition, based on [Deenyoro/fallout2-ce-3ds](https://github.com/Deenyoro/fallout2-ce-3ds), which itself builds on MrHuu's 3DS platform layer

Both are forks of [alexbatalov/fallout1-ce](https://github.com/alexbatalov/fallout1-ce) and [alexbatalov/fallout2-ce](https://github.com/alexbatalov/fallout2-ce).

## What this fork changes

| Area | Change |
|---|---|
| Layout | Map defaults to the **bottom touchscreen** (stylus = mouse), HUD and message log on the top screen. ZL swaps; the choice is remembered when returning from menus, saves and the world map. |
| Controls | A = left click (hold to drag), B or L = right click, X = inventory, Y = Pip-Boy, ZR = end turn, Start+X = quicksave, Start+Y = quickload. |
| Movie freeze fix | `SDL_OpenAudioDevice` returns 0 on failure, not -1. When audio couldn't start (e.g. no `sdmc:/3ds/dspfirm.cdc`) the engine believed audio was running and every movie hung forever in `_MVE_sndSync`. Movies now play silently instead. |
| SD streaming | Fallout 2 reads compressed DAT entries in 32 KB chunks (was 1 KB) with a 32 KB stdio buffer per stream; Fallout 1's DAT handle gets a 16 KB buffer. |
| Build | Single Docker build (`./build.sh`), pinned devkitARM image and SDL2 commit; `.3dsx` output only. |

## Controls

| Input | Action |
|---|---|
| Stylus | Click directly on the bottom screen |
| Circle Pad / C-stick | Move cursor |
| D-Pad | Scroll map |
| A | Left click (hold to drag) |
| B / L | Right click (cycle move / attack / look cursor) |
| R (hold) | Left button held |
| X / Y | Inventory / Pip-Boy |
| ZL | Swap map between screens |
| ZR | End turn (combat) |
| Start | Menu / back |
| Start+X / Start+Y | Quicksave / quickload |
| Select | Cycle display mode; Select+Up/Down or the 3D slider zooms the map |

## Install

```
sdmc:/3ds/dspfirm.cdc                  (for sound; dump once with the DSP1 homebrew)
sdmc:/3ds/fallout/   fallout-ce.3dsx   MASTER.DAT CRITTER.DAT DATA/ fallout.cfg
sdmc:/3ds/fallout2/  fallout2-ce.3dsx  master.dat critter.dat patch000.dat data/ sound/ fallout2.cfg
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
