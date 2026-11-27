#ifndef FALLOUT_PLATFORM_CTR_MANUAL_H
#define FALLOUT_PLATFORM_CTR_MANUAL_H

#include <3ds.h>

namespace fallout {

// Left-handed play: the D-pad and A/B/X/Y swap jobs (Y<->up, A<->right, B<->down, X<->left).
// Set from the Start menu; remembered by a marker file.
u32 ctr_remap(u32 keys);

// Left-handed mode is on (D-Pad and A/B/X/Y swapped).
bool ctr_lefty();

// Shows the instruction manual until the player closes it (game paused).
void ctr_manual_run();

// Flips left-handed mode (the manual's LEFT button and the Start menu row).
void ctr_lefty_toggle();

// Start menu (house style). Returns one of these; LEFT HAND flips the setting and the menu
// stays open, so it is never returned.
enum { CTR_MENU_RESUME = 0, CTR_MENU_MANUAL = 1, CTR_MENU_SAVE = 2, CTR_MENU_SETTINGS = 3, CTR_MENU_LEFTY = 4, CTR_MENU_EXIT = 5, CTR_MENU_ACTIONS = 6 };
int ctr_startmenu_run();

// What EXIT GAME can do on the current screen (ctr_input.cc).
enum { CTR_EXIT_SAVES = 0, CTR_EXIT_CLOSES = 1, CTR_EXIT_NOSAVE = 2, CTR_EXIT_NOGAME = 3 };

// START > ACTIONS (FALLOUT-X1 / X7): every chord and D-pad shortcut as a touch button.
// Returns the action picked, CTR_ACT_NONE for BACK / B.
enum { CTR_ACT_NONE = 0, CTR_ACT_AUTOMAP, CTR_ACT_REST, CTR_ACT_HEAL, CTR_ACT_RELOAD, CTR_ACT_QUICKSAVE, CTR_ACT_QUICKLOAD,
    CTR_ACT_HANDS, CTR_ACT_MODE, CTR_ACT_SKILLDEX, CTR_ACT_CHAR, CTR_ACT_ENDCOMBAT };
int ctr_actions_run();

// A YES / NO question in the house style: the question and two big buttons on the touch
// screen, up to three lines of detail on the top screen. The ring starts on NO; B, START and
// NO return false.
bool ctr_ask_run(const char* title, const char* question, const char* sub, const char* top1, const char* top2, const char* top3,
    const char* yesLabel);

} // namespace fallout

#endif /* FALLOUT_PLATFORM_CTR_MANUAL_H */
