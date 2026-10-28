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

## Second pass

| Action | Before | After |
|---|---|---|
| Know your odds before attacking | Only on the called-shot screen (open it per enemy) | **Hold R** in combat: the message log lists "Name: NN% to hit" for every visible enemy with your current weapon/mode |
| Heal | Open inventory (4 AP), find Stimpak, use, close | **Select+A** (same 4 AP in combat); Super Stimpak used if no Stimpak |
| Active item button (attack / reload / use item on…) | HUD button on the untouchable top screen | **B** (L keeps the classic right-click) |
| Accidental display-mode change | Select tapped outside gameplay switched to the full-screen view | Select tap only recenters in gameplay; display mode is **Select+L** only |

## Third pass: dual-screen model (same as the SimGolf / Dungeon Master ports)

| Area | Before | After |
|---|---|---|
| Movement | Circle Pad moved a mouse cursor (duplicating the stylus) | **Circle Pad walks the player** toward any screen direction; one hex per push in combat (AP spent like a tap), continuous when exploring |
| Top screen in gameplay | Fallout's interface bar, rearranged, with dead (untouchable) buttons | **Dashboard** drawn with the system font: HP bar, AP pips, AC, weapon, attack mode + AP cost, ammo, last tapped enemy's health and hit chance, level/XP or poison/rads, date/time, 7-line message log |
| Inventory / loot / Skilldex | Same picture on both screens | Menu on the touchscreen, dashboard on top |

## Fourth pass: hands-on test fixes

Found by playing Fallout 1 in the emulator after a hardware report that the controls felt broken and the top screen was cut off.

| Problem | Fix |
|---|---|
| Circle Pad stopped dead after one blocked step: a failed walk request was never retried while the stick was held | Retries every 150 ms; candidate tiles must be free and reachable |
| Turning the stick while walking did nothing until the current stretch ended | The current walk is cancelled and re-aimed at once |
| Walking into a wall at an angle stopped the player | Slides along the obstacle (tries up to 45 degrees either side) |
| Player kept walking several tiles after letting go | Stops at the next tile on release |
| In combat, a push aimed two hexes away vertically and was rejected | Steps to the free neighbouring hex nearest the stick's angle |
| Options menu couldn't be closed with Start | Start backs out of every in-game menu; B does too (3DS convention) |
| Pip-Boy, world map and options showed a cropped zoom of the game on top | Dashboard on top in every in-game screen; the zoom lens only with Select+L |
| Main menu showed a dashboard for a character that doesn't exist yet | Controls card on the top screen outside a game |
| Character sheet hid the skills list with no clue how to reach it | Hint on the top screen; Circle Pad scroll at half speed |
| C-stick scrolled in jumps | 8 px steps at most 10 per second |
| Fallout 1 couldn't save ("Unable to save game") | Case-insensitive directory listing |

## Fifth pass: QA/UX review follow-up

| Area | Before | After |
|---|---|---|
| Inventory and loot | Item stats only in the original panel at half size | Item card on top: weapon damage/range/AP/ammo/ST, armor DT/DR per damage type, ammo modifiers, weight, value, description |
| Trading | Conversation picture on top; prices at 0.7 scale | Offer value vs. what they want, accept/short-by verdict, their caps, item card |
| Moving items | Drag only | Tap moves an item to the other side when looting or trading |
| Save/load | Stretched crop of the original preview, clipped description | 1:1 preview with the slot's character, date, place and description in the system font |
| World map | HP/AP dashboard | Location, terrain, destination, known places, car fuel (F2) |
| Pip-Boy | Half-size text | Magnified reading pane on top following the stylus |
| Fallout 1 quick save | Slot and name dialog | Silent rotation through slots 1-3 |
| Top screen sharpness | 400 px only | Select+R: 800 px mode (opt-in, untested in emulators, which can't show it) |
| Loading | Every file open scanned directories for case | Direct open first |

## Not done

- D-pad navigation of dialogue replies: the replies are already large stylus targets on the touchscreen, so a hardware path adds little.
