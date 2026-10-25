#include "win32.h"

#include <stdlib.h>

#include <SDL.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "main.h"
#include "svga.h"
#include "window_manager.h"

#if __APPLE__ && TARGET_OS_IOS
#include "platform/ios/paths.h"
#endif

#ifdef __3DS__
#include "platform/ctr/ctr_sys.h"
#endif

namespace fallout {

// 0x51E444
bool gProgramIsActive = false;

#ifdef _WIN32
HANDLE GNW95_mutex = nullptr;
#endif

int main(int argc, char* argv[])
{
    int rc;

#if _WIN32
    GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() != ERROR_SUCCESS) {
        return 0;
    }
#endif

#if __APPLE__ && TARGET_OS_IOS
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(iOSGetDocumentsPath());
#endif

#if __APPLE__ && TARGET_OS_OSX
    char* basePath = SDL_GetBasePath();
    chdir(basePath);
    SDL_free(basePath);
#endif

#if __ANDROID__
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(SDL_AndroidGetExternalStoragePath());
#endif

#ifdef __3DS__
#ifdef _DEBUG_LINK
    ctr_sys_3dslink_stdio();
#endif
    osSetSpeedupEnable(true);

    const char* basepath = "sdmc:/3ds/fallout2/";
    chdir(basepath);

    ctr_debug_log("=== Fallout 2 CE 3DS v0.0.47 ===");
    ctr_debug_log("chdir done");

    linearHeapAvailableAtStart = ctr_sys_check_linear_heap();
    heapAvailableAtStart = ctr_sys_check_heap();
    ctr_debug_log("heap checks done");

    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    ctr_debug_log("SDL hints set");
#endif

#ifndef __3DS__
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        return EXIT_FAILURE;
    }
    atexit(SDL_Quit);
#endif

    SDL_ShowCursor(SDL_DISABLE);

    gProgramIsActive = true;
#ifdef __3DS__
    ctr_debug_log("calling falloutMain");
#endif
    rc = falloutMain(argc, argv);

#if _WIN32
    CloseHandle(GNW95_mutex);
#endif

    return rc;
}

} // namespace fallout

int main(int argc, char* argv[])
{
    return fallout::main(argc, argv);
}
