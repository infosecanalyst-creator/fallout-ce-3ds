# Changelog

## 2026-11-27

- Same as the other 3DS ports (cross-port pass). START menu in the house order: RESUME, MANUAL, SAVE GAME, LOAD GAME, SETTINGS, ACTIONS, LEFT HAND, EXIT GAME. SAVE is now called SAVE GAME, and LOAD GAME is new: it opens the game's own load screen (the same as Ctrl+L), on the map only, like SAVE GAME.
- Questions read YES / NO: EXIT GAME asks "Exit Fallout?" (Fallout 2: "Exit Fallout 2?") with "It is saved first (slot 10)." (the buttons said "YES, EXIT"); quick load's button says YES (was "YES, LOAD"). The ring still starts on NO and B is NO.

## 2026-11-21 (audit fixes)

Both games:

- Quick save (Start+X) moved to slots 7, 8 and 9, next to the exit save in slot 10. It only overwrites an empty slot or an older quick save, never a save you made yourself; if all three hold your own saves it says so and saves nothing. Quick saves made by older builds in slots 1-3 are now ordinary saves (quick load still finds them).
- Quick load (Start+Y) asks first: "Load the last quick save?" with the ring on NO.
- Fallout 2: quick save and quick load work this way out of the box. They no longer depend on AutoQuickSave=3 in ddraw.ini (without that line Start+X used to open a slot picker). Karma messages are built in too.
- EXIT GAME: one question in the house style (ring on NO, B cancels). YES saves to slot 10 and closes the program, straight back to the Homebrew Menu. Nothing is written before YES. With the inventory, Pip-Boy, Skilldex or automap open it closes that window first, then saves. If the save does not work it says NOT SAVED and asks again.
- START works on the main menu (Manual, Left hand, Exit game).
- New START > ACTIONS page: automap, rest, quick heal, reload, quick save, quick load, swap hands, attack mode, Skilldex, character sheet and end combat as touch buttons, each with its button shortcut shown on the top screen.
- START > SETTINGS opens Preferences directly.
- LOAD GAME opens on the newest save (the exit save after EXIT GAME).
- Hint strip: follows left-hand mode, has its own line on the automap, in conversations, on the save / load and options screens, and takes turns through the shortcuts on the map. It shrinks to fit.
- Manual: the last line is never cut by the footer; house buttons with a gold ring, a click and a pressed look; LEFT and CLOSE buttons; shorter section names; the text matches all of the above.
- The left-hand and sharp-screen settings say so when the SD card refuses to store them.
- Fallout 1: starting with too little free memory waits up to 5 seconds and then shows a message instead of crashing.
- Fallout 2: the "master datafile" message names the file as the manual does.

## 2026-11-21

- Start menu: the buttons give the same quiet click and visible press as the rest of the port. Save and Settings are greyed out (and say why) when you are not playing on the map. Exit game says whether it can save first.
- The low-health / hit / poisoned alert on the top screen is steadier and forgets its old state when you load a game.
- The dashboard never reads a stale target after an enemy goes away.
- Fallout 2 debug.log is off by default and small when on (create debug.txt to switch it on); it no longer slows the game down.
- Saving is safer: a failed save puts the previous save back and says it failed, instead of saying "saved". HOME > Close during play now does the same slot-10 exit save as Exit game and waits for it to finish.
- Install path: the programs are /3ds/fallout-ce.3dsx and /3ds/fallout2-ce.3dsx.
- The Start menu shows the build version. The in-game manual and the Markdown manuals are updated to match.

## 2026-11-20

- Manual: drag the stylus to scroll, with a little momentum; section buttons act on a tap.
- Alert strip on the map screen (low health, hit, poisoned, your turn).
- Load/save screen: tapping the DONE / CANCEL words works like the red buttons.
- Third-party notices added for the 3DS builds.
