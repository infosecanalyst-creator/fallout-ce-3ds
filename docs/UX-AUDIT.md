# UX audit: Fallout CE on New 3DS

The goal: every common action takes as few inputs as possible, and the information you need is always on screen.

## What it was measured against

- **DS/3DS stylus strategy and RPGs** (Advance Wars DS, Fire Emblem, FF Tactics A2, Civilization Revolution DS, Etrian Odyssey). Conventions: the stylus acts directly on what it touches, with no mode switching; tap means act; hold opens a context menu; the touch screen carries whatever you interact with and the top screen carries what you read; every button on the screen you can't touch has a hardware shortcut.
- **Mouse-driven isometric RPGs and tactics, classic and modern** (Fallout 1/2 with sfall, Baldur's Gate 3, Wasteland 3, Divinity: OS2, XCOM 2, Pillars of Eternity). Conventions: one context-sensitive click (attack hostile, talk friendly, open door, pick up item); hold Alt/Tab to show every lootable; the camera follows the party; walking means running by default; quicksave needs no dialog.
- **Touch ports of Fallout CE** (iOS/Android trackpad scheme, Fallout 2 CE Mobile). Lesson: a touchscreen has no hover, so anything that relies on hovering (cursor modes, item outlines) costs extra taps.

## Findings and fixes

Tap counts are for the default layout: map on the bottom touchscreen, HUD on the top screen.

| Action | Before | After | Change |
|---|---|---|---|
| Pick up an item, talk, loot a body, open a door, use a ladder | 2–3 (B to arrow cursor, tap, B back) | **1 tap** | Smart tap: in move mode a tap on something interactive acts on it; open ground still moves |
| Attack in combat | 3 (B, B to crosshair, tap) | **1 tap** on the enemy | Smart tap targets hostiles in combat (never party members) |
| Action menu (look, use skill, use item on…) | B to arrow, then hold | **Hold** on the object | Long-press works in move mode too |
| Find lootables | Hover each object (impossible on touch) | **Hold R** | Outlines every item and lootable body on the level |
| Keep the player in view | Scroll manually with the D-pad | **Automatic** | The camera recenters when the player nears the edge of the view; Select taps recenter manually |
| Scroll the map | D-pad (taken from quick actions) | **C-stick** | Camera on the right stick, as in modern tactics (D-pad still scrolls on Old 3DS) |
| Start combat | No button: the HUD is on the top screen | **ZR** | ZR starts combat when exploring and ends your turn in combat |
| End combat | HUD button on the untouchable screen | **Start+ZR** | |
| Swap weapon hand / weapon mode | HUD buttons on the untouchable screen | **D-pad Up / Down** | |
| Skilldex / character sheet | HUD buttons on the untouchable screen | **D-pad Left / Right** | |
| Automap / Pip-Boy clock | HUD buttons on the untouchable screen | **Select+X / Select+Y** | |
| Inventory / Pip-Boy | X / Y | X / Y | Unchanged |
| Quicksave (Fallout 2) | Start+X, pick slot, type name, Done | **Start+X** | `AutoQuickSave=3` rotates slots 1–3 with no dialog |
| Walking speed | Walks; run needs Shift (no Shift on 3DS) | **Runs** | `running=1` |
| Combat pacing | Default speed | **Fast** | `combat_speed=50`, `player_speedup=1` |
| Karma feedback (Fallout 2) | Silent | Message on change | `DisplayKarmaChanges=1` |

Every HUD button that sits on the untouchable top screen now has a hardware shortcut. ZL still swaps screens, so the HUD can be put under the stylus at any time.

## Already good, unchanged

- Dialogue: reply options are drawn near 1:1 on the touchscreen as large tap targets, with the speaker's portrait and text on the top screen.
- Inventory, barter and loot: drag-and-drop on the touchscreen, item detail on top.
- Character sheet, Pip-Boy, world map: interactive on the bottom, detail on top; world-map travel is a single tap.
- Save/load: slot list on the bottom, preview image and description on top.

## Not done (needs deeper engine work)

- Hit-chance preview while targeting. Fallout only shows it on the called-shot screen; showing it on the top screen when an enemy is tapped would need combat UI work.
- A quick-heal hotkey. The engine has no "use best healing item" action.
- D-pad navigation of dialogue replies (number keys exist in the engine; the stylus covers this well already).
- A "use active item" shortcut outside combat (the HUD item button on the top screen).
