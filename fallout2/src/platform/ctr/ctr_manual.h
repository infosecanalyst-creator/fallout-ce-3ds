#ifndef FALLOUT_PLATFORM_CTR_MANUAL_H
#define FALLOUT_PLATFORM_CTR_MANUAL_H

namespace fallout {

// Shows the instruction manual until the player closes it (game paused).
void ctr_manual_run();

// Start menu: 0 = Manual, 1 = Options, 2 = Close (or B / Start).
int ctr_startmenu_run();

} // namespace fallout

#endif /* FALLOUT_PLATFORM_CTR_MANUAL_H */
