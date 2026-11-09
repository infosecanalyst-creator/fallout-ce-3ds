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

// Start menu: 0 = Manual, 1 = Options, 2 = Close (or B / Start). The menu also has a
// Left hand on/off button, which flips the setting and stays open.
int ctr_startmenu_run();

} // namespace fallout

#endif /* FALLOUT_PLATFORM_CTR_MANUAL_H */
