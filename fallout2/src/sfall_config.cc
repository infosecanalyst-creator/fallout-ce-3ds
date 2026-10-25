#include "sfall_config.h"

#include <stdio.h>
#include <string.h>

#include "platform_compat.h"

#ifdef __3DS__
#include "platform/ctr/ctr_sys.h"
#endif

namespace fallout {

#define SFALL_CONFIG_FILE_NAME "ddraw.ini"

bool gSfallConfigInitialized = false;
Config gSfallConfig;

bool sfallConfigInit(int argc, char** argv)
{
#ifdef __3DS__
    ctr_debug_log("sfallConfigInit: enter");
#endif

    if (gSfallConfigInitialized) {
        return false;
    }

    if (!configInit(&gSfallConfig)) {
#ifdef __3DS__
        ctr_debug_log("sfallConfigInit: configInit failed");
#endif
        return false;
    }

#ifdef __3DS__
    ctr_debug_log("sfallConfigInit: configInit done");
#endif

    // Initialize defaults.
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DUDE_NATIVE_LOOK_JUMPSUIT_MALE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DUDE_NATIVE_LOOK_JUMPSUIT_FEMALE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DUDE_NATIVE_LOOK_TRIBAL_MALE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DUDE_NATIVE_LOOK_TRIBAL_FEMALE_KEY, "");
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_START_YEAR, 2241);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_START_MONTH, 6);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_START_DAY, 24);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_BIG_FONT_COLOR_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_X_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_Y_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_FONT_COLOR_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_X_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_Y_KEY, 0);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY, 0);
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, "");
    configSetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DISPLAY_KARMA_CHANGES_KEY, false);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_CRITICALS_MODE_KEY, 2);
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_OVERRIDE_CRITICALS_FILE_KEY, "");
    configSetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_REMOVE_CRITICALS_TIME_LIMITS_KEY, false);
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BOOKS_FILE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ELEVATORS_FILE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_CONSOLE_OUTPUT_FILE_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PREMADE_CHARACTERS_FILE_NAMES_KEY, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PREMADE_CHARACTERS_FACE_FIDS_KEY, "");
    configSetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_ENABLED_KEY, false);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_CENTER_MULTIPLIER_KEY, SFALL_CONFIG_BURST_MOD_DEFAULT_CENTER_MULTIPLIER);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_CENTER_DIVISOR_KEY, SFALL_CONFIG_BURST_MOD_DEFAULT_CENTER_DIVISOR);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_TARGET_MULTIPLIER_KEY, SFALL_CONFIG_BURST_MOD_DEFAULT_TARGET_MULTIPLIER);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_BURST_MOD_TARGET_DIVISOR_KEY, SFALL_CONFIG_BURST_MOD_DEFAULT_TARGET_DIVISOR);
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_EXTRA_MESSAGE_LISTS_KEY, "");
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MOVIE_TIMER_ARTIMER1, 90);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MOVIE_TIMER_ARTIMER2, 180);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MOVIE_TIMER_ARTIMER3, 270);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MOVIE_TIMER_ARTIMER4, 360);
    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_AUTO_QUICK_SAVE, 0);
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_VERSION_STRING, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_CONFIG_FILE, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PATCH_FILE, "");

    configSetString(&gSfallConfig, SFALL_CONFIG_SCRIPTS_KEY, SFALL_CONFIG_INI_CONFIG_FOLDER, "");
    configSetString(&gSfallConfig, SFALL_CONFIG_SCRIPTS_KEY, SFALL_CONFIG_GLOBAL_SCRIPT_PATHS, "");

    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PIPBOY_AVAILABLE_AT_GAMESTART, 0);

#ifdef __3DS__
    ctr_debug_log("sfallConfigInit: defaults set");
#endif

    char path[COMPAT_MAX_PATH];
    char* executable = (argc > 0 && argv != nullptr) ? argv[0] : nullptr;
    char* ch = (executable != nullptr) ? strrchr(executable, '\\') : nullptr;
#ifndef _WIN32
    if (executable != nullptr) {
        char* fch = strrchr(executable, '/');
        if (fch != nullptr && (ch == nullptr || fch > ch)) {
            ch = fch;
        }
    }
#endif
    if (ch != nullptr) {
        char saved = *ch;
        *ch = '\0';
        snprintf(path, sizeof(path), "%s%c%s", executable, saved, SFALL_CONFIG_FILE_NAME);
        *ch = saved;
    } else {
        strcpy(path, SFALL_CONFIG_FILE_NAME);
    }

#ifdef __3DS__
    ctr_debug_log("sfallConfigInit: path resolved");
#endif

    configRead(&gSfallConfig, path, false);

#ifdef __3DS__
    ctr_debug_log("sfallConfigInit: configRead done");
#endif

    configParseCommandLineArguments(&gSfallConfig, argc, argv);

#ifdef __3DS__
    ctr_debug_log("sfallConfigInit: complete");
#endif

    gSfallConfigInitialized = true;

    return true;
}

void sfallConfigExit()
{
    if (gSfallConfigInitialized) {
        configFree(&gSfallConfig);
        gSfallConfigInitialized = false;
    }
}

} // namespace fallout
