#ifndef FALLOUT_PLATFORM_CTR_SYS_H_
#define FALLOUT_PLATFORM_CTR_SYS_H_

#include <3ds.h>

namespace fallout {

extern size_t linearHeapAvailable;
extern size_t heapAvailable;

extern size_t linearHeapAvailableAtStart;
extern size_t heapAvailableAtStart;


size_t ctr_sys_check_linear_heap();
size_t ctr_sys_check_heap();

#ifdef _DEBUG_OVERLAY
float ctr_sys_get_fps();

#endif

// Verbose debug.log lines: written only when sdmc:/3ds/fallout2/debug.txt exists.
void ctr_debug_log(const char* msg);
// Rare lines (start-up, failures, exit): always written and flushed.
void ctr_note(const char* msg);
bool ctr_debug_enabled();

// Clean exit when the HOME menu closes the game: stop audio (ndsp) and the
// GPU first, or the system hangs on "Closing software...".
[[noreturn]] void ctr_app_quit();

#ifdef _DEBUG_LINK
void ctr_sys_3dslink_stdio();
#endif

} // namespace fallout

#endif /* FALLOUT_PLATFORM_CTR_SYS_H_ */
