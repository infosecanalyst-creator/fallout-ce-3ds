#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ctr_input.h"
#include "ctr_dashboard.h"
#include "ctr_manual.h"
#include "ctr_gfx.h"
#include "ctr_rectmap.h"
#include "ctr_sys.h"

#include "platform_compat.h"

#include "plib/gnw/gnw.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/input.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/touch.h"
#include "plib/gnw/debug.h"

#include "game/gdialog.h"
#include "game/map.h"
#include "game/scripts.h"
#include "game/combat.h"
#include "game/gmouse.h"
#include "game/object.h"
#include "game/tile.h"
#include "game/intface.h"
#include "game/inventry.h"

namespace fallout {

#define FLAG_NONE 0x00
#define FLAG_SELECT_DDOWN 0x01
#define FLAG_SELECT_DUP 0x02
#define FLAG_SELECT_DLEFT 0x04
#define FLAG_SELECT_DRIGHT 0x08
#define FLAG_START_DDOWN 0x10
#define FLAG_START_DUP 0x20
#define FLAG_START_DLEFT 0x40
#define FLAG_START_DRIGHT 0x80
#define FLAG_SELECT_L 0x100
#define FLAG_START_ZR 0x200
#define FLAG_SELECT_X 0x400
#define FLAG_SELECT_Y 0x800
#define FLAG_SELECT_A 0x1000
#define FLAG_SELECT_B 0x2000
#define FLAG_SELECT_R 0x4000

#define KEY_PRESS(x) ((ctr_input.frame.kHeld & (x)) && (!(oldpad & (x))))
#define KEY_HELD(x) (ctr_input.frame.kHeld & (x))
#define KEY_RELEASED(x) ((!(ctr_input.frame.kHeld & (x))) && (oldpad & (x)))

#define MAX_OFFSET 240

#define ABOUT_INPUT_MAX_SIZE 128
#define MAX_DICT_WORDS 100

ctr_input_t ctr_input;

static u16 key_combi_flags = FLAG_NONE;
static u32 oldpad = 0;
static bool scaled_rect_top = false;
static float previousSliderState = -1.0f;
static float manualScaleFactor = 0.0f;

int offsetX = 0;
int offsetY = 0;

static bool ctr_is_n3ds(void)
{
    static int cached = -1;
    if (cached < 0) {
        bool n3ds = false;
        APT_CheckNew3DS(&n3ds);
        cached = n3ds ? 1 : 0;
    }
    return cached == 1;
}

static bool ctr_in_gameplay(void)
{
    // The logical screen: Select+L's whole-screen view still shows the map.
    return (ctr_rectMap.main == DISPLAY_FIELD) || (ctr_rectMap.main == DISPLAY_GUI);
}

// Keep the player inside the visible part of the map. The 3DS screen only
// shows part of the 640x480 game view, so without this the character walks
// out of sight and has to be found again by scrolling.
static int followLastTile = -1;

static void ctr_follow_dude(void)
{
    if (!ctr_in_gameplay() || obj_dude == NULL) {
        followLastTile = -1;
        return;
    }
    if (obj_dude->tile == followLastTile || obj_dude->elevation != map_elevation) {
        return;
    }
    followLastTile = obj_dude->tile;

    int x, y;
    tile_coord(obj_dude->tile, &x, &y, map_elevation);
    x += 16;
    y += 8;

    rectMap_t* r = rectMaps[DISPLAY_FIELD][0];

    // Visible part of the game view (the map area ends at y=380, above the HUD).
    const float viewW = 640.f, viewH = 380.f;
    float w = r->src_w < viewW ? r->src_w : viewW;
    float h = r->src_h < viewH ? r->src_h : viewH;
    float marginX = w * 0.2f;
    float marginY = h * 0.2f;

    auto inside = [&](float sx, float sy) {
        return x >= sx + marginX && x <= sx + w - marginX
            && y >= sy + marginY && y <= sy + h - marginY;
    };

    if (inside(r->src_x, r->src_y)) {
        return;
    }

    // First slide the 3DS window over the game view...
    float nx = x - w / 2.f;
    float ny = y - h / 2.f;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > offsetX_field_scaled_max) nx = offsetX_field_scaled_max;
    if (ny > offsetY_field_scaled_max) ny = offsetY_field_scaled_max;

    if (!inside(nx, ny)) {
        // ...and if the player is near the edge of the game view itself,
        // scroll the map so the player is centered (the Home key).
        GNW_add_input_buffer(KEY_HOME);
        nx = 320.f - w / 2.f;
        ny = 190.f - h / 2.f;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx > offsetX_field_scaled_max) nx = offsetX_field_scaled_max;
        if (ny > offsetY_field_scaled_max) ny = offsetY_field_scaled_max;
    }

    offsetX_field = nx;
    offsetY_field = ny;
    r->src_x = nx;
    r->src_y = ny;
}


// Map scrolling from the C-stick / D-pad: the engine scrolls a whole tile
// chunk per call, so limit it to ~10 steps a second instead of every frame.
static void ctr_map_scroll(int dx, int dy)
{
    static u64 last = 0;
    u64 now = osGetTime();
    if (now - last < 100) {
        return;
    }
    last = now;
    map_scroll(dx, dy);
}

static SwkbdDictWord words[MAX_DICT_WORDS];
static int num_words;
static int dictId = -1;


void ctr_input_center_mouse(void)
{
    mouse_hide();
    mouse_set_position(offsetX_field + (rectMaps[DISPLAY_FIELD][0]->src_w / 2),
            offsetY_field + (rectMaps[DISPLAY_FIELD][0]->src_h / 2));
    mouse_show();
}

void input_touch_to_texture(rectMap_e rectmap, int touch_x, int touch_y, int *originalX, int *originalY)
{
    for (int i = 0; i < numRectsInMap[rectmap]; ++i) {

    rectMap_t *dstRect = rectMaps[rectmap][i];

        if (touch_x >= dstRect->dst_x && touch_x < (dstRect->dst_x + dstRect->dst_w) &&
            touch_y >= (240 - (dstRect->dst_y + dstRect->dst_h)) && touch_y < (240 - dstRect->dst_y)) {

            rectMap_t *srcRect = rectMaps[rectmap][i];

            float x_scale = dstRect->dst_w / srcRect->src_w;
            float y_scale = dstRect->dst_h / srcRect->src_h;

            float adjusted_touch_x = touch_x - dstRect->dst_x;
            float adjusted_touch_y = touch_y - (240 - (dstRect->dst_y + dstRect->dst_h));

            *originalX = (adjusted_touch_x / x_scale) + srcRect->src_x;
            *originalY = (adjusted_touch_y / y_scale) + srcRect->src_y;
        }
    }
}

void ctr_input_get_touch(int *newX, int *newY)
{
    switch (ctr_rectMap.active)
    {
        case DISPLAY_FULL:
            *newX = (ctr_input.frame.touchX * screenGetWidth()) / 320;
            *newY = (ctr_input.frame.touchY * screenGetHeight()) / 240;
            break;
        case DISPLAY_CHAR:
            input_touch_to_texture(DISPLAY_CHAR,
                    ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);

            if ((ctr_input.frame.touchY > 30) && (ctr_input.frame.touchY < 215)) {
                if (offsetY_char <= 240) {
                    *newY += offsetY_char;
                    *newX -= 330;
                } else if ((offsetY_char > 240) && (offsetY_char <= 480)) {
                    int visableTop = 480 - offsetY_char;

                    if (ctr_input.frame.touchY <= visableTop) {
                        *newY += offsetY_char;
                        *newX -= 330;
                    } else {
                        *newY += (-visableTop);
                        *newX -= 10;
                    }
                } else if (offsetY_char > 480) {
                    *newY += offsetY_char-480;
                    *newX -= 10;
                }
            }
            if (isAgeWindow)
                input_touch_to_texture(DISPLAY_CHAR_EDIT_AGE,
                        ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);

            if (isSexWindow)
                input_touch_to_texture(DISPLAY_CHAR_EDIT_SEX,
                        ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
        case DISPLAY_LOADSAVE:
            input_touch_to_texture(DISPLAY_LOADSAVE_SLOT,
                    ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            *newY += getSaveSlotOffset();
        default:
            input_touch_to_texture(ctr_rectMap.active,
                    ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
    }
}

void ctr_input_process(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if ((ctr_input.input == INPUT_TOUCH) &&
                (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION)) {
            float touchX = e.tfinger.x * 320;
            float touchY = e.tfinger.y * 240;

            offsetX = touchX * MAX_OFFSET / 320;
            offsetY = 240 - (touchY * MAX_OFFSET / 240);

            ctr_input.frame.touchX = touchX;
            ctr_input.frame.touchY = touchY;
        }

        switch (e.type) {
        case SDL_FINGERDOWN:
            touch_handle_start(&(e.tfinger));
            break;
        case SDL_FINGERMOTION:
            touch_handle_move(&(e.tfinger));
            break;
        case SDL_FINGERUP:
            touch_handle_end(&(e.tfinger));
            break;

        case SDL_QUIT:
#ifdef __3DS__
            ctr_app_quit();
#else
            exit(EXIT_SUCCESS);
#endif
            break;
        }
    }
    touch_process_gesture();

    ctr_input.frame.kHeld = hidKeysHeld();

    // Runs every frame, including while a touch gesture is being handled.
    ctr_follow_dude();

    // Circle Pad = walk the player, Dungeon Master / SimGolf style.
    if (ctr_in_gameplay()) {
        circlePosition cp;
        hidCircleRead(&cp);
        if (abs(cp.dx) > 40 || abs(cp.dy) > 40) {
            gameStickMove(cp.dx, cp.dy);
        } else {
            gameStickMove(0, 0);
        }
        // A tap takes over from the stick.
        if (KEY_PRESS(KEY_TOUCH)) {
            gameStickReset();
        }
    } else {
        gameStickReset();
    }

    if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
        float scaleFactorToUse;
        float currentSliderState = osGet3DSliderState();
        int sliderChanged = (currentSliderState != previousSliderState);

        if (key_combi_flags & (FLAG_SELECT_DDOWN | FLAG_SELECT_DUP)) {
            scaleFactorToUse = manualScaleFactor;
        } else if (sliderChanged) {
            scaleFactorToUse = currentSliderState;
            previousSliderState = currentSliderState;
        } else if (scaled_rect_top == (ctr_rectMap.active == DISPLAY_GUI)) {
            return;
        } else {
            scaleFactorToUse = -1.0f;
        }
        scaled_rect_top = (ctr_rectMap.active == DISPLAY_GUI);

        if (setRectMapScaled(scaled_rect_top, scaleFactorToUse) == 0) {
            mouse_hide();
            mouse_set_position(offsetX_field + (rectMaps[DISPLAY_FIELD][0]->src_w / 2),
                    offsetY_field + (rectMaps[DISPLAY_FIELD][0]->src_h / 2));
            mouse_show();
        }
    }
}

void input_frame_touch(void)
{
    return;
}

void input_frame_axis(int *delta_x, int *delta_y)
{
    int x, y;
    int moveX;
    int moveY;

    if ((ctr_rectMap.active == DISPLAY_FULL) || (ctr_rectMap.active == DISPLAY_FIELD) ||
            (ctr_rectMap.active == DISPLAY_GUI) || (ctr_rectMap.active == DISPLAY_WORLDMAP) ||
            (ctr_rectMap.active == DISPLAY_CHAR_SELECT) || (ctr_rectMap.active == DISPLAY_CHAR) ||
            (ctr_rectMap.active == DISPLAY_MAIN)) {

        circlePosition circle;
        hidCircleRead(&circle);

        circlePosition cstick;
        hidCstickRead(&cstick);

        // Circle Pad = cursor. On New 3DS the C-stick scrolls the map during
        // gameplay (camera on the right stick, as in modern tactics games).
        if (ctr_in_gameplay() && ctr_is_n3ds()) {
            cstick.dx = 0;
            cstick.dy = 0;
        }
        // During gameplay the Circle Pad walks the player (see
        // ctr_input_process); the stylus is the pointer.
        if (ctr_in_gameplay()) {
            circle.dx = 0;
            circle.dy = 0;
        }
        moveX = (int)circle.dx / 15 + (int)cstick.dx / 15;
        moveY = -((int)circle.dy / 15 + (int)cstick.dy / 15);

        if (abs(moveX) < 2) {
            moveX = 0;
        }
        if (abs(moveY) < 2) {
            moveY = 0;
        }

        if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
            if ((moveX != 0) || (moveY != 0)) {
                mouse_get_position(&x, &y);
                if (y > 380) {
                    ctr_input_center_mouse();
                } else if (y + moveY >= 380) {
                    mouse_hide();
                    mouse_set_position(x, 379);
                    mouse_show();
                } else {
                    *delta_x += moveX;
                    *delta_y += moveY;
                }
            }
        } else if (ctr_rectMap.active == DISPLAY_CHAR) {
            if (!isAgeWindow && !isSexWindow) {
                offsetY_char += moveY / 2; // half speed: easier to stop on traits/perks

                if (offsetY_char < 0)
                    offsetY_char=0;

                if (offsetY_char > 525)
                    offsetY_char=525;
            }
        } else {
            *delta_x += moveX;
            *delta_y += moveY;
        }
    }
}

void input_frame_buttons(int *delta_x, int *delta_y, int *buttons)
{
    // The manual: Select+Start anywhere, or a Select tap outside the map.
    if ((KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_START)) ||
            (KEY_RELEASED(KEY_SELECT) && key_combi_flags == FLAG_NONE && !ctr_in_gameplay() &&
             ctr_rectMap.active != DISPLAY_MOVIE && ctr_rectMap.active != DISPLAY_SPLASH)) {
        ctr_manual_run();
        key_combi_flags = FLAG_NONE;
        hidScanInput();
        ctr_input.frame.kHeld = hidKeysHeld();
        oldpad = ctr_input.frame.kHeld;
        return;
    }

#ifdef _DEBUG_OVERLAY
    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_DUP)) {
        ctr_gfx.overlayEnable = !ctr_gfx.overlayEnable;
        key_combi_flags |= FLAG_START_DUP;
    }

    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_DDOWN)) {
        ctr_gfx.overlayEnableExtra = !ctr_gfx.overlayEnableExtra;
        key_combi_flags |= FLAG_START_DDOWN;
    }

    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_DRIGHT)) {
        linearHeapAvailable = ctr_sys_check_linear_heap();
        heapAvailable = ctr_sys_check_heap();
        key_combi_flags |= FLAG_START_DRIGHT;
    }

    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_DLEFT)) {
        ctr_gfx.isWide = !ctr_gfx.isWide;
        gfxSetWide(ctr_gfx.isWide);
        ctr_gfx_reinit();
        key_combi_flags |= FLAG_SELECT_DLEFT;
    }
#endif
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_DDOWN)) {
        manualScaleFactor -= 0.2f; // zoom out
        if (manualScaleFactor < 0.0f) {
            manualScaleFactor = 0.0f;
        }
        key_combi_flags |= FLAG_SELECT_DDOWN;
    }

    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_DUP)) {
        manualScaleFactor += 0.2f; // zoom in
        if (manualScaleFactor > 1.0f) {
            manualScaleFactor = 1.0f;
        }
        key_combi_flags |= FLAG_SELECT_DUP;
    }

    // Start+X = quicksave (F6), Start+Y = quickload (F7)
    if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
        if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_X)) {
            GNW_add_input_buffer(KEY_F6);
            key_combi_flags |= FLAG_START_DUP;
        }
        if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_Y)) {
            GNW_add_input_buffer(KEY_F7);
            key_combi_flags |= FLAG_START_DDOWN;
        }
    }

    if (KEY_RELEASED(KEY_START)) {
        if (key_combi_flags) {
            key_combi_flags = FLAG_NONE;
        } else {
            // In a running game Start always backs out (options menu,
            // elevator, any dialog box); outside it, not on the main menu.
            if ((ctr_rectMap.active != DISPLAY_MOVIE) &&
                    (ctrDashboardAvailable() ||
                     ((ctr_rectMap.active != DISPLAY_MAIN) && (ctr_rectMap.active != DISPLAY_FULL)))) {
                // Start opens the small menu: Manual, Options (the old
                // Start action), Close. B still backs out of dialogs.
                int choice = ctr_startmenu_run();
                if (choice == 0) {
                    ctr_manual_run();
                } else if (choice == 1) {
                    GNW_add_input_buffer(KEY_ESCAPE);
                }
                key_combi_flags = FLAG_NONE;
                hidScanInput();
                ctr_input.frame.kHeld = hidKeysHeld();
                oldpad = ctr_input.frame.kHeld;
                return;
            }
        }
    }

    // Select+L = cycle display mode (whole game screen on the bottom screen)
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_L)) {
        ctr_input.mode = ((ctr_input.mode + 1) % DISPLAY_MODE_FULL_TOP); // DISPLAY_MODE_LAST
        setActiveRectMap(DISPLAY_LAST);
        key_combi_flags |= FLAG_SELECT_L;
    }

    // The HUD buttons sit on the top screen while the map is on the touch
    // screen, so every HUD button also has a hardware shortcut:
    // Select+X = automap, Select+Y = Pip-Boy clock / rest
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_X) && ctr_in_gameplay()) {
        GNW_add_input_buffer(KEY_TAB);
        key_combi_flags |= FLAG_SELECT_X;
    }
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_Y) && ctr_in_gameplay()) {
        GNW_add_input_buffer(KEY_LOWERCASE_Z);
        key_combi_flags |= FLAG_SELECT_Y;
    }

#ifdef CTR_EMUTEST
    // Emulator testing only: Select+ZL leaves the map for the world map.
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_ZL) && ctr_in_gameplay()) {
        MapTransition transition;
        memset(&transition, 0, sizeof(transition));
        transition.map = -2;
        map_leave_map(&transition);
        key_combi_flags |= FLAG_SELECT_R;
    }
#endif

#ifdef CTR_EMUTEST
    // Emulator testing only: Select+ZR quits the way the HOME menu does.
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_ZR)) {
        SDL_Event q;
        q.type = SDL_QUIT;
        SDL_PushEvent(&q);
        key_combi_flags |= FLAG_SELECT_R;
    }
#endif

    // Select+R = sharper 800-pixel top screen on/off
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_R)) {
        ctr_gfx_toggle_wide();
        key_combi_flags |= FLAG_SELECT_R;
    }

    // Select+B = reload the active weapon
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_B) && ctr_in_gameplay() && intface_is_enabled()) {
        intface_quick_reload();
        key_combi_flags |= FLAG_SELECT_B;
    }

    // Select+A = quick heal (Stimpak, then Super Stimpak)
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_A) && ctr_in_gameplay()) {
        inven_quick_heal();
        key_combi_flags |= FLAG_SELECT_A;
    }

    // Start+ZR = end combat
    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_ZR) && ctr_in_gameplay()) {
        GNW_add_input_buffer(KEY_RETURN);
        key_combi_flags |= FLAG_START_ZR;
    }

    if (KEY_RELEASED(KEY_SELECT)) {
        if (key_combi_flags) {
            key_combi_flags = FLAG_NONE;
        } else if (ctr_in_gameplay()) {
            // Select tap = center the map on the player
            GNW_add_input_buffer(KEY_HOME);
            followLastTile = -1;
        }
    }

    // R (hold) = highlight every item and lootable body on the level
    if (ctr_in_gameplay() && KEY_PRESS(KEY_R) && !KEY_HELD(KEY_SELECT)) {
        gmouse_highlight_all(true);
    }
    if (KEY_RELEASED(KEY_R)) {
        gmouse_highlight_all(false);
    }

    if (key_combi_flags == FLAG_NONE) {
        // L = right click (cycles the cursor: move / attack / examine).
        // B = the HUD's active-item button during gameplay (attack with the
        // weapon, reload, or use the item on a target); right click elsewhere.
        // B outside the map = Back (3DS convention) while a game is running,
        // except in conversations; elsewhere it stays a right click.
        bool bIsBack = !ctr_in_gameplay() &&
                ctr_rectMap.active != DISPLAY_DIALOG && ctr_rectMap.active != DISPLAY_MOVIE &&
                (ctrDashboardAvailable() ||
                 (ctr_rectMap.active != DISPLAY_MAIN && ctr_rectMap.active != DISPLAY_FULL));
        // X on a body or container = Take All (the game's own shortcut)
        if (KEY_PRESS(KEY_X) && ctr_rectMap.active == DISPLAY_INVENTORY_LOOT
                && !KEY_HELD(KEY_SELECT) && !KEY_HELD(KEY_START)) {
            GNW_add_input_buffer(KEY_UPPERCASE_A);
        }
        if (KEY_PRESS(KEY_B) && bIsBack && !KEY_HELD(KEY_SELECT) && !KEY_HELD(KEY_START)) {
            GNW_add_input_buffer(KEY_ESCAPE);
        }
        if (KEY_PRESS(KEY_L) || (KEY_PRESS(KEY_B) && !ctr_in_gameplay() && !bIsBack)) {
            *buttons |= MOUSE_STATE_RIGHT_BUTTON_DOWN;
        }
        // On the map: B cancels attack/use targeting, otherwise uses the
        // active item. Only on the player's turn with the interface enabled.
        if (KEY_PRESS(KEY_B) && ctr_in_gameplay() && !KEY_HELD(KEY_SELECT) && !KEY_HELD(KEY_START)
                && intface_is_enabled() && (combat_state & 0x02) != 0) {
            if (gmouse_3d_get_mode() != GAME_MOUSE_MODE_MOVE) {
                gmouse_3d_set_mode(GAME_MOUSE_MODE_MOVE);
            } else {
                intface_use_item();
            }
        }

        // A = left click on every screen (hold to drag)
        if (KEY_HELD(KEY_A) && !KEY_HELD(KEY_SELECT)) {
            *buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
        }

        if ((ctr_rectMap.active == DISPLAY_FULL) ||
                (ctr_rectMap.active == DISPLAY_WORLDMAP) || (ctr_rectMap.active == DISPLAY_CHAR_SELECT) ||
                (ctr_rectMap.active == DISPLAY_CHAR) || (ctr_rectMap.active == DISPLAY_MAIN)) {
            if (KEY_HELD(KEY_R)) {
                *buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
            }
        }

        bool inGameplay = (ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI);

        // ZL = swap which screen shows the map (choice is remembered).
        // On every other screen ZL switches the top screen between its info
        // card and a sharp full-size view of that screen.
        if (KEY_PRESS(KEY_ZL) && inGameplay && !KEY_HELD(KEY_SELECT)) {
            toggleGameplayRectMap();
        } else if (KEY_PRESS(KEY_ZL) && !inGameplay && !KEY_HELD(KEY_SELECT)
                && ctr_rectMap.active != DISPLAY_MOVIE) {
            ctr_toggle_top_lens();
        }

        // The button that opened a screen also closes it: X inventory,
        // Y Pip-Boy, X automap, D-pad left Skilldex, D-pad right character
        // sheet (Done, so spent points are kept).
        if (!inGameplay && ctrDashboardAvailable() && !KEY_HELD(KEY_START) && !KEY_HELD(KEY_SELECT)) {
            int a = ctr_rectMap.active;
            if ((KEY_PRESS(KEY_X) && (a == DISPLAY_INVENTORY || (a == DISPLAY_DYNAMIC && ctrAutomapOpen)))
                    || (KEY_PRESS(KEY_Y) && a == DISPLAY_PIPBOY)
                    || (KEY_PRESS(KEY_DLEFT) && a == DISPLAY_SKILLDEX)) {
                GNW_add_input_buffer(KEY_ESCAPE);
            } else if (KEY_PRESS(KEY_DRIGHT) && a == DISPLAY_CHAR) {
                GNW_add_input_buffer(KEY_RETURN);
            }
        }

        // ZR = start combat when exploring, end turn in combat
        if (KEY_PRESS(KEY_ZR) && inGameplay) {
            GNW_add_input_buffer(isInCombat() ? KEY_SPACE : KEY_LOWERCASE_A);
        }

        // X = inventory, Y = Pip-Boy
        if (inGameplay && !KEY_HELD(KEY_START) && !KEY_HELD(KEY_SELECT)) {
            if (KEY_PRESS(KEY_X)) {
                GNW_add_input_buffer(KEY_LOWERCASE_I);
            }
            if (KEY_PRESS(KEY_Y)) {
                GNW_add_input_buffer(KEY_LOWERCASE_P);
            }
        }

        if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
            bool scrollUp, scrollDown, scrollLeft, scrollRight;
            if (ctr_is_n3ds()) {
                circlePosition cs;
                hidCstickRead(&cs);
                scrollUp = cs.dy > 40;
                scrollDown = cs.dy < -40;
                scrollLeft = cs.dx < -40;
                scrollRight = cs.dx > 40;

                // D-pad = quick actions: Up swap weapon hand, Down weapon mode,
                // Left Skilldex, Right character sheet
                if (KEY_PRESS(KEY_DUP)) GNW_add_input_buffer(KEY_LOWERCASE_B);
                if (KEY_PRESS(KEY_DDOWN)) GNW_add_input_buffer(KEY_LOWERCASE_N);
                if (KEY_PRESS(KEY_DLEFT)) GNW_add_input_buffer(KEY_LOWERCASE_S);
                if (KEY_PRESS(KEY_DRIGHT)) GNW_add_input_buffer(KEY_LOWERCASE_C);
            } else {
                scrollUp = KEY_HELD(KEY_DUP);
                scrollDown = KEY_HELD(KEY_DDOWN);
                scrollLeft = KEY_HELD(KEY_DLEFT);
                scrollRight = KEY_HELD(KEY_DRIGHT);
            }

            offsetX_field = rectMaps[DISPLAY_FIELD][0]->src_x;
            offsetY_field = rectMaps[DISPLAY_FIELD][0]->src_y;

            int edgeDx = 0, edgeDy = 0;
            if (scrollUp) {
                if (offsetY_field > 0) {
                    int deltaY = (offsetY_field >= 8) ? 8 : offsetY_field;
                    offsetY_field -= deltaY;
                    if (ctr_rectMap.active != DISPLAY_GUI) {
                        *delta_y -= deltaY;
                    } else {
                        int x, y;
                        mouse_get_position(&x, &y);
                        if (y < 380) {
                            *delta_y -= deltaY;
                        }
                    }
                } else {
                    edgeDy = -1;
                }
            }
            if (scrollDown) {
                if (offsetY_field < offsetY_field_scaled_max) {
                    int deltaY = (offsetY_field_scaled_max - offsetY_field >= 8) ?
                                 8 : offsetY_field_scaled_max - offsetY_field;
                    offsetY_field += deltaY;
                    if (ctr_rectMap.active != DISPLAY_GUI) {
                        *delta_y += deltaY;
                    } else {
                        int x, y;
                        mouse_get_position(&x, &y);
                        if (y < 380) {
                            *delta_y += deltaY;
                        }
                    }
                } else {
                    edgeDy = 1;
                }
            }
            if (scrollLeft) {
                if (offsetX_field > 0) {
                    int deltaX = (offsetX_field >= 8) ? 8 : offsetX_field;
                    offsetX_field -= deltaX;
                    if (ctr_rectMap.active != DISPLAY_GUI) {
                        *delta_x -= deltaX;
                    } else {
                        int x, y;
                        mouse_get_position(&x, &y);
                        if (y < 380) {
                            *delta_x -= deltaX;
                        }
                    }
                } else {
                    edgeDx = -1;
                }
            }
            if (scrollRight) {
                if (offsetX_field < offsetX_field_scaled_max) {
                    int deltaX = (offsetX_field_scaled_max - offsetX_field >= 8) ?
                                 8 : offsetX_field_scaled_max - offsetX_field;
                    offsetX_field += deltaX;
                    if (ctr_rectMap.active != DISPLAY_GUI) {
                        *delta_x += deltaX;
                    } else {
                        int x, y;
                        mouse_get_position(&x, &y);
                        if (y < 380) {
                            *delta_x += deltaX;
                        }
                    }
                } else {
                    edgeDx = 1;
                }
            }
            if (edgeDx != 0 || edgeDy != 0) {
                ctr_map_scroll(edgeDx, edgeDy); // one throttled call, so diagonals work
            }
            rectMaps[DISPLAY_FIELD][0]->src_x = offsetX_field;
            rectMaps[DISPLAY_FIELD][0]->src_y = offsetY_field;
        } else {
            if (KEY_HELD(KEY_DUP)) {
                map_scroll(0, -1);
                *buttons |= KEY_ARROW_UP;
            }
            if (KEY_HELD(KEY_DDOWN)) {
                map_scroll(0, 1);
                *buttons |= KEY_ARROW_DOWN;
            }
            if (KEY_HELD(KEY_DLEFT)) {
                map_scroll(-1, 0);
            }
            if (KEY_HELD(KEY_DRIGHT)) {
                map_scroll(1, 0);
            }
        }
    }
    oldpad = ctr_input.frame.kHeld;
}

int ctr_input_frame(void)
{
    int delta_x = 0;
    int delta_y = 0;
    int buttons = 0;

    input_frame_touch();
    input_frame_axis(&delta_x, &delta_y);
    input_frame_buttons(&delta_x, &delta_y, &buttons);

    if ((delta_x != 0) || (delta_y != 0) || (buttons != 0)) {
        mouse_simulate_input(delta_x, delta_y, buttons);
        return 1;
    }

    return 0;
}

bool ctr_input_key_pressed(void)
{
    bool pressed = (ctr_input.frame.kHeld && (!(oldpad)));
    oldpad = ctr_input.frame.kHeld;

    return pressed;
}

void to_lowercase(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// Adds a dialogue topic to the keyboard's suggestions (bounded, and without
// lower-casing the game's own message strings in place).
static void swkbd_add_word(const char* str)
{
    if (num_words >= MAX_DICT_WORDS || str == NULL || compat_stricmp(str, "Error") == 0) {
        return;
    }
    char lower[64];
    snprintf(lower, sizeof(lower), "%s", str);
    to_lowercase(lower);
    swkbdSetDictWord(&words[num_words], lower, lower);
    num_words++;
}

void swkbd_about_lookup_word(void)
{
    Script* scr;

    int message_list_id;
    int message_id;
    char* str;

    num_words = 0;

    while (num_words < MAX_DICT_WORDS) {
        if (scr_ptr(dialog_target->sid, &scr) != -1) {
            message_list_id = scr->scr_script_idx + 1;
            for (message_id = 1000; message_id < 1100; message_id++) {
                str = scr_get_msg_str(message_list_id, message_id);

                swkbd_add_word(str);
            }
        }
        message_list_id = 1;
        for (message_id = 600 * map_data.field_34 + 1000; message_id < 600 * map_data.field_34 + 1100; message_id++) {
            str = scr_get_msg_str(message_list_id, message_id);

            swkbd_add_word(str);
        }
        break;
    }
}

void ctr_input_swkdb_init(void)
{
    if (dictId == dialog_target->sid)
        return;

    dictId = dialog_target->sid;
    swkbd_about_lookup_word();
}

int ctr_input_swkbd(const char *hintText, const char *inText, char *outText, size_t outSize)
{
    SwkbdState swkbd;
    static SwkbdStatusData swkbdStatus;
    static SwkbdLearningData swkbdLearning;
    char mybuf[ABOUT_INPUT_MAX_SIZE];

    if (outSize > sizeof(mybuf)) {
        outSize = sizeof(mybuf);
    }

    // Limit the keyboard to what the caller's buffer holds.
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, (int)outSize - 1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetInitialText(&swkbd, inText);
    swkbdSetHintText(&swkbd, hintText);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "OK", true);
    swkbdSetFeatures(&swkbd,
            SWKBD_PREDICTIVE_INPUT | SWKBD_ALLOW_HOME | SWKBD_ALLOW_RESET | SWKBD_ALLOW_POWER);
    if (num_words > 0 && strcmp(hintText, "Ask about what?") == 0) {
        swkbdSetDictionary(&swkbd, words, num_words);
    }

    static bool reload = false;
    swkbdSetStatusData(&swkbd, &swkbdStatus, reload, true);
    swkbdSetLearningData(&swkbd, &swkbdLearning, reload, true);
    reload = true;

    SwkbdButton button = swkbdInputText(&swkbd, mybuf, sizeof(mybuf));

    // Buttons still held as the keyboard closes must not reach the game.
    do {
        hidScanInput();
        gspWaitForVBlank();
    } while (hidKeysHeld() != 0 && aptMainLoop());
    ctr_input.frame.kHeld = 0;
    oldpad = 0;

    if (button == SWKBD_BUTTON_RIGHT) {
        snprintf(outText, outSize, "%s", mybuf);
        return 1;
    }
    // Cancelled (or the keyboard could not open): leave the text as it was.
    return button == SWKBD_BUTTON_LEFT ? -1 : -2;
}

void ctr_input_init(void)
{
    ctr_input.input = INPUT_TOUCH;
    previousSliderState = osGet3DSliderState();

    if (setRectMapScaled(true, 0) == 0) {
        mouse_hide();
        mouse_set_position(offsetX_field + (rectMaps[DISPLAY_FIELD][0]->src_w / 2),
                offsetY_field + (rectMaps[DISPLAY_FIELD][0]->src_h / 2));
        mouse_show();
    }
}

void ctr_input_exit(void)
{
    return;
}

} // namespace fallout
