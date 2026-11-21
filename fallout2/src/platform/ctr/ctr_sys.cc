#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>

#include <3ds.h>
#include <SDL.h>

#include "audio_engine.h"
#include "platform/ctr/ctr_gfx.h"
#include "platform/ctr/ctr_sys.h"

#include "memory.h"

u32 __ctru_heap_size = 0;
u32 __ctru_linear_heap_size = 25 * 1024 * 1024; // anything lower crashes on launch
u32 __stacksize__ = 128 * 1024;

#ifdef _DEBUG_LINK
#include <sys/socket.h>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32 *SOC_buffer = NULL;
s32 sock = -1;

void socShutdown() {
    printf("waiting for socExit...\n");
    socExit();
}
#endif

namespace fallout {

size_t linearHeapAvailableAtStart;
size_t heapAvailableAtStart;

size_t linearHeapAvailable;
size_t heapAvailable;

size_t ctr_sys_check_linear_heap() {
    const size_t chunkSize = 1 * 1024 * 1024;
    void* buffer[100];
    size_t allocatedChunks = 0;

    for (int i = 0; i < 100; i++) {
        buffer[i] = linearAlloc(chunkSize);
        if (!buffer[i]) {
            printf("Linear heap allocation failed at chunk %d, %zu MB\n", i + 1, (i + 1) * 1);
            break;
        }
        allocatedChunks++;
    }

    for (size_t j = 0; j < allocatedChunks; j++) {
        linearFree(buffer[j]);
    }

    return allocatedChunks * chunkSize;
}

size_t ctr_sys_check_heap() {
    const size_t chunkSize = 1 * 1024 * 1024;
    void* buffer[100];
    size_t allocatedChunks = 0;

    for (int i = 0; i < 100; i++) {
        buffer[i] = malloc(chunkSize);
        if (buffer[i] == NULL) {
            fprintf(stderr, "Heap allocation failed for chunk %d\n", i);
            break;
        }
        allocatedChunks++;
    }

    for (size_t j = 0; j < allocatedChunks; j++) {
        free(buffer[j]);
    }

    return allocatedChunks * chunkSize;
}
// debug.log on the SD card.
// Off unless sdmc:/3ds/fallout2/debug.txt exists (checked once at start-up):
// every line was written and flushed to the SD card, several per second in
// play (gfx timings, map loads, every saved file), and each SD write stalls
// the game. With debug.txt the full log comes back, buffered (no flush per
// line) and rolled over to debug.log.old at 1 MB.
// ctr_note() lines (start-up, failures, exit) are always written and flushed
// at once: they are rare and are what's needed after a crash.
// The file is kept open: opening, appending and closing it per line cost
// 100-200 ms each on the real SD card (see 3ds-sd-card-io-learnings).
static FILE* debugLogFile = nullptr;
static long debugLogBytes = 0;
static int debugLogOn = -1;
static const long kDebugLogCap = 1024 * 1024;

bool ctr_debug_enabled() {
    if (debugLogOn < 0) {
        FILE* f = fopen("sdmc:/3ds/fallout2/debug.txt", "r");
        debugLogOn = f != nullptr;
        if (f != nullptr) fclose(f);
    }
    return debugLogOn != 0;
}

static void debugLogWrite(const char* msg, bool flush) {
    if (debugLogFile != nullptr && debugLogBytes > kDebugLogCap) {
        fclose(debugLogFile);
        debugLogFile = nullptr;
        remove("sdmc:/3ds/fallout2/debug.log.old");
        rename("sdmc:/3ds/fallout2/debug.log", "sdmc:/3ds/fallout2/debug.log.old");
    }
    if (debugLogFile == nullptr) {
        debugLogFile = fopen("sdmc:/3ds/fallout2/debug.log", "w");
        if (debugLogFile == nullptr) return;
        setvbuf(debugLogFile, nullptr, _IOFBF, 16384);
        debugLogBytes = 0;
    }
    int n = fprintf(debugLogFile, "%s\n", msg);
    if (n > 0) debugLogBytes += n;
    if (flush) fflush(debugLogFile);
}

void ctr_debug_log(const char* msg) {
    if (!ctr_debug_enabled()) return;
    debugLogWrite(msg, false);
}

void ctr_note(const char* msg) {
    debugLogWrite(msg, true);
}

#ifdef _DEBUG_OVERLAY
float ctr_sys_get_fps()
{
    static u64 lastTick = 0;
    static float fps = 0.0f;

    u64 currentTick = svcGetSystemTick();
    u64 elapsedTicks = currentTick - lastTick;

    lastTick = currentTick;

    float elapsedSeconds = (float)elapsedTicks / SYSCLOCK_ARM11;
    if (elapsedSeconds > 0.0f) {
        fps = 1.0f / elapsedSeconds;
    }

    return fps;
}
#endif
#ifdef _DEBUG_LINK
void ctr_sys_3dslink_stdio()
{
    int ret;
    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

    if(SOC_buffer == NULL) {
        printf("memalign: failed to allocate\n");
    }

    if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
        printf("socInit: 0x%08X\n", (unsigned int)ret);
    } else {
        atexit(socShutdown);
        sock = link3dsStdio();
	}
}
#endif

void ctr_app_quit()
{
    // The game's own atexit handler (win_exit) tears down the windows and
    // the GPU, but nothing stops audio: the SDL audio thread and ndsp keep
    // running and the system can't close the app. Stop them first.
    ctr_note("exit: closed from the HOME menu");
    audioEngineExit();
    exit(EXIT_SUCCESS);
}

} // namespace fallout
