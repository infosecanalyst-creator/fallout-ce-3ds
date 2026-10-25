#include "platform/ctr/ctr_perf.h"

#include "game_movie.h"

namespace fallout {

CtrPerfFrame g_ctrPerf;
FILE* g_ctrPerfLog = nullptr;
int g_ctrPerfFrameNum = 0;

static u64 s_perfTotals[9] = {};
static int s_perfLateFrames = 0;
static u64 s_frameStartTick = 0;

void ctrPerfInit()
{
    if (g_ctrPerfLog) return;
    g_ctrPerfLog = fopen("sdmc:/3ds/fallout2/perf_log.csv", "w");
    if (g_ctrPerfLog) {
        fprintf(g_ctrPerfLog,
            "frame,tickers_us,updProgs_us,updWins_us,input_us,render_us,"
            "gameKey_us,scriptsReq_us,sfallMain_us,total_us,movie\n");
        fflush(g_ctrPerfLog);
    }
    g_ctrPerfFrameNum = 0;
    for (int i = 0; i < 9; i++) s_perfTotals[i] = 0;
    s_perfLateFrames = 0;
}

void ctrPerfClose()
{
    if (!g_ctrPerfLog) return;
    int n = g_ctrPerfFrameNum > 0 ? g_ctrPerfFrameNum : 1;
    fprintf(g_ctrPerfLog,
        "\n# summary: %d frames\n"
        "# avg tickers=%llu us, updProgs=%llu us, updWins=%llu us\n"
        "# avg input=%llu us, render=%llu us, gameKey=%llu us\n"
        "# avg scriptsReq=%llu us, sfallMain=%llu us, total=%llu us\n"
        "# frames > 33ms: %d\n",
        g_ctrPerfFrameNum,
        s_perfTotals[0]/n, s_perfTotals[1]/n, s_perfTotals[2]/n,
        s_perfTotals[3]/n, s_perfTotals[4]/n, s_perfTotals[5]/n,
        s_perfTotals[6]/n, s_perfTotals[7]/n, s_perfTotals[8]/n,
        s_perfLateFrames);
    fclose(g_ctrPerfLog);
    g_ctrPerfLog = nullptr;
}

void ctrPerfFrameBegin()
{
    s_frameStartTick = svcGetSystemTick();
    g_ctrPerf = {};
}

void ctrPerfFrameEnd()
{
    if (!g_ctrPerfLog) return;

    u64 totalTicks = svcGetSystemTick() - s_frameStartTick;
    g_ctrPerf.frameTotal = totalTicks;
    g_ctrPerf.moviePlaying = gameMovieIsPlaying() ? 1 : 0;

    g_ctrPerfFrameNum++;

    u64 vals[9] = {
        ctrTicksToUs(g_ctrPerf.tickersExecute),
        ctrTicksToUs(g_ctrPerf.updatePrograms),
        ctrTicksToUs(g_ctrPerf.updateWindows),
        ctrTicksToUs(g_ctrPerf.inputGetInput),
        ctrTicksToUs(g_ctrPerf.renderPresent),
        ctrTicksToUs(g_ctrPerf.gameHandleKey),
        ctrTicksToUs(g_ctrPerf.scriptsHandleReq),
        ctrTicksToUs(g_ctrPerf.sfallMainHook),
        ctrTicksToUs(g_ctrPerf.frameTotal),
    };

    for (int i = 0; i < 9; i++) s_perfTotals[i] += vals[i];
    if (vals[8] > 33333) s_perfLateFrames++;

    fprintf(g_ctrPerfLog, "%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%d\n",
        g_ctrPerfFrameNum,
        vals[0], vals[1], vals[2], vals[3], vals[4],
        vals[5], vals[6], vals[7], vals[8],
        g_ctrPerf.moviePlaying);

    // Flush every 60 frames to avoid too many disk writes
    if (g_ctrPerfFrameNum % 60 == 0) fflush(g_ctrPerfLog);
}

} // namespace fallout
