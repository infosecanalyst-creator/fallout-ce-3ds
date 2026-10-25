#ifndef FALLOUT_PLATFORM_CTR_PERF_H_
#define FALLOUT_PLATFORM_CTR_PERF_H_

#include <3ds.h>
#include <stdio.h>

namespace fallout {

// Per-frame timing accumulators (in CPU ticks).
// Set by instrumentation in various files, read/reset in mainLoop.
struct CtrPerfFrame {
    u64 tickersExecute;
    u64 updatePrograms;
    u64 updateWindows;
    u64 inputGetInput;
    u64 renderPresent;
    u64 gameHandleKey;
    u64 scriptsHandleReq;
    u64 sfallMainHook;
    u64 frameTotal;
    int moviePlaying;
};

extern CtrPerfFrame g_ctrPerf;
extern FILE* g_ctrPerfLog;
extern int g_ctrPerfFrameNum;

// CPU ticks to microseconds (268MHz ARM11)
inline u64 ctrTicksToUs(u64 ticks) { return ticks * 1000000ULL / SYSCLOCK_ARM11; }

void ctrPerfInit();
void ctrPerfClose();
void ctrPerfFrameBegin();
void ctrPerfFrameEnd();

} // namespace fallout

#endif /* FALLOUT_PLATFORM_CTR_PERF_H_ */
