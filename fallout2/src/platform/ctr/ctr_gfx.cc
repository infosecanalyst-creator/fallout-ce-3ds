#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include "ctr_manual.h"
#include "ctr_input.h"
#include "ctr_gfx.h"
#include "ctr_dashboard.h"
#include "ctr_rectmap.h"
#include "ctr_sys.h"

#include "vshader_shbin.h"
#include "frame_tex.h"

#include "window_manager.h"
#include "memory.h"
#include "mouse.h"

namespace fallout {

ctr_gfx_t ctr_gfx;
ctr_rectMap_t ctr_rectMap;

static DVLB_s *vshader_dvlb;
static shaderProgram_s program;
static int8_t uLoc_projection;

static C3D_Mtx topProjection;
static C3D_Mtx bottomProjection;

static C3D_RenderTarget *topRenderTarget;
static C3D_RenderTarget *bottomRenderTarget;

static uint8_t *renderTextureData;
static const uint16_t renderTextureWidth = 1024;
static const uint16_t renderTextureHeight = 512;
static const uint32_t renderTextureStride = renderTextureWidth * 2; // RGB565: 2 bytes per pixel
static const uint32_t renderTextureByteCount = renderTextureStride * renderTextureHeight;

static C3D_Tex render_tex;
static C3D_Tex static_tex;
static int activeTexFilter;

static bool isN3DS;
static bool targetTop;

static C2D_TextBuf overlayTextBuf;

void beginRender(bool vSync);
void drawTopRenderTarget(uint32_t clearColor);
void drawBottomRenderTarget(uint32_t clearColor);
void finishRender();

void drawRects();
void drawFrameRect();
static void ctr_movie_card_draw();
// Shown on the top screen for 2.5 s after Select+R, so the change is visible.
static u64 ctrWideHintUntil = 0;
static const char* ctrWideHint = nullptr;


static bool loadTextureFromMem(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size)
{
	Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, cube, true);
	if (!t3x)
		return false;

	Tex3DS_TextureFree(t3x);
	return true;
}

static bool loadTextureFromFile(C3D_Tex* tex, C3D_TexCube* cube, const char* filePath)
{
	FILE* file = fopen(filePath, "rb");
	if (!file)
		return false;

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	void* data = malloc(size);
	if (!data) {
		fclose(file);
		return false;
	}

	size_t bytesRead = fread(data, 1, size, file);
	fclose(file);

	if (bytesRead != size) {
		free(data);
		return false;
	}

	bool result = loadTextureFromMem(tex, cube, data, size);

	free(data);

	return result;
}

#ifdef _DEBUG_OVERLAY
void show_overlay(void)
{
    if (!ctr_gfx.overlayEnable && !ctr_gfx.overlayEnableExtra)
        return;

    C2D_Prepare();
    C2D_SceneTarget(bottomRenderTarget);
    C2D_TextBufClear(overlayTextBuf);
    C2D_Text dynamicText;

    char fpsBuffer[32];
    char buffer[512];

    if ((ctr_gfx.overlayEnable) && (ctr_gfx.overlayEnableExtra)) {
        snprintf(fpsBuffer, sizeof(buffer), "FPS: %5.2f", ctr_sys_get_fps());
        snprintf(buffer, sizeof(buffer),
                "%s\n\ngpu: %5.2f%%  cpu: %5.2f%%  cmdBuf:%5.2f%%\nisWide: %s\n\nLinear Heap at start: %9.2f MB\nHeap at start:           %9.2f MB\nLinear Heap current : %9.2f MB\nHeap current :           %9.2f MB",
                fpsBuffer,
                C3D_GetDrawingTime()*6, C3D_GetProcessingTime()*6, C3D_GetCmdBufUsage()*100, ctr_gfx.isWide?"true":"false",
                linearHeapAvailableAtStart / (1024.0f * 1024.0f),
                heapAvailableAtStart / (1024.0f * 1024.0f),
                linearHeapAvailable / (1024.0f * 1024.0f),
                heapAvailable / (1024.0f * 1024.0f)
        );
        C2D_DrawRectSolid(10, 10, 0.9, 300, 25, C2D_Color32f(0, 1, 0, 0.5));
        C2D_DrawRectSolid(10, 42, 0.9, 300, 65, C2D_Color32f(1, 0, 1, 0.5));
		C2D_DrawRectSolid(10,114, 0.9, 300, 75, C2D_Color32f(0, 0, 1, 0.5));
    } else if (ctr_gfx.overlayEnable) {
        snprintf(fpsBuffer, sizeof(buffer), "FPS: %5.2f", ctr_sys_get_fps());
        snprintf(buffer, sizeof(buffer), "%s", fpsBuffer);
        C2D_DrawRectSolid(10, 10, 0.9, 300, 25, C2D_Color32f(0, 1, 0, 0.5));
    } else if (ctr_gfx.overlayEnableExtra) {
        snprintf(buffer, sizeof(buffer),
                "gpu: %5.2f%%  cpu: %5.2f%%  cmdBuf:%5.2f%%\nisWide: %s\n\nLinear Heap at start: %9.2f MB\nHeap at start:           %9.2f MB\nLinear Heap current : %9.2f MB\nHeap current :           %9.2f MB",
                C3D_GetDrawingTime()*6, C3D_GetProcessingTime()*6, C3D_GetCmdBufUsage()*100, ctr_gfx.isWide?"true":"false",
                linearHeapAvailableAtStart / (1024.0f * 1024.0f),
                heapAvailableAtStart / (1024.0f * 1024.0f),
                linearHeapAvailable / (1024.0f * 1024.0f),
                heapAvailable / (1024.0f * 1024.0f)
        );
        C2D_DrawRectSolid(10, 10, 0.9, 300, 65, C2D_Color32f(1, 0, 1, 0.5));
        C2D_DrawRectSolid(10, 80, 0.9, 300, 75, C2D_Color32f(0, 0, 1, 0.5));
    }
    C2D_TextParse(&dynamicText, overlayTextBuf, buffer);
    C2D_TextOptimize(&dynamicText);
    C2D_DrawText(&dynamicText, C2D_WithColor, 20.0f, 15.0f, 1.0f, 0.5f, 0.5f, C2D_Color32f(1, 1, 1, 1));
    C2D_Flush();
}
#endif

inline void setTextureFilter(int linear)
{
    int newTexFilter;

    if (linear != -1)
        newTexFilter = linear;
    else
        newTexFilter = isN3DS ? GPU_LINEAR : GPU_NEAREST;

    if (newTexFilter != activeTexFilter) {
        C3D_TexSetFilter(&render_tex, GPU_LINEAR, (GPU_TEXTURE_FILTER_PARAM) newTexFilter);
        activeTexFilter = newTexFilter;
    }
}

inline void _drawRect(const float texW , const float texH, const float subTexX, const float subTexY, const float subTexW, const float subTexH,
        const float posX, const float posY, const float width, const float height)
{
    const float vertZ = 0.5f;
    const float vertW = 1.f;

    C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);
    {
        // Bottom left corner
        C3D_ImmSendAttrib(posX, posY, vertZ, vertW);                                   // v0 = position xyzw
        C3D_ImmSendAttrib(subTexX / texW, 1.f - (subTexY + subTexH) / texH, 0.f, 0.f); // v1 = texcoord uv

        // Bottom right corner
        C3D_ImmSendAttrib(width + posX, posY, vertZ, vertW);
        C3D_ImmSendAttrib((subTexX + subTexW) / texW, 1.f - (subTexY + subTexH) / texH, 0.f, 0.f);

        // Top left corner
        C3D_ImmSendAttrib(posX, height + posY, vertZ, vertW);
        C3D_ImmSendAttrib(subTexX / texW, 1.f - subTexY / texH, 0.f, 0.f);

        // Top right corner
        C3D_ImmSendAttrib(width + posX, height + posY, vertZ, vertW);
        C3D_ImmSendAttrib((subTexX + subTexW) / texW, 1.f - subTexY / texH, 0.f, 0.f);
    }
    C3D_ImmDrawEnd();
}

inline void drawRect_tex(const float subTexX, const float subTexY, const float subTexW, const float subTexH,
        const float posX, const float posY, const float width, const float height)
{
    const float texW = 512;
    const float texH = 512;

    _drawRect(texW, texH, subTexX, subTexY, subTexW, subTexH, posX, posY, width, height);
}

inline void drawRect(const float subTexX, const float subTexY, const float subTexW, const float subTexH,
        const float posX, const float posY, const float width, const float height)
{
    float newPosX = posX;
    float newWidth = width;

    const float texW = renderTextureWidth;
    const float texH = renderTextureHeight;

    if (targetTop && ctr_gfx.isWide) {
        newPosX *= 2.f;
        newWidth *= 2.f;
    }

    _drawRect(texW, texH, subTexX, subTexY, subTexW, subTexH, newPosX, posY, newWidth, height);
}

// ---------------------------------------------------------------------------
// Top-screen lens: on any screen except the map, ZL switches the top screen
// between its info card and a sharp, full-size (1:1) view of the part of the
// screen around the stylus. Remembered per screen. Every screen starts with
// the lens off (both screens filled; zoom only on a button press).
// ---------------------------------------------------------------------------
static bool ctrLens[DISPLAY_LAST];
static bool ctrLensInit = false;

static bool ctrLensSupported(int r)
{
    return r != DISPLAY_FIELD && r != DISPLAY_GUI && r != DISPLAY_MOVIE && r != DISPLAY_SPLASH
        && r != DISPLAY_LAST;
}

static void ctrLensDefaults()
{
    if (!ctrLensInit) {
        ctrLensInit = true;
        // No screen starts zoomed: ZL turns the lens on where wanted.
    }
}

bool ctr_top_lens_active()
{
    ctrLensDefaults();
    int r = ctr_rectMap.active;
    return ctr_input.mode == DISPLAY_MODE_ADAPT && ctrLensSupported(r) && ctrLens[r];
}

void ctr_toggle_top_lens()
{
    ctrLensDefaults();
    int r = ctr_rectMap.active;
    if (ctrLensSupported(r)) ctrLens[r] = !ctrLens[r];
}

static void ctrDrawTopLens()
{
    int mx, my;
    mouseGetPosition(&mx, &my);
    int minX = 0, maxX = 640, minY = 0, maxY = 480;
    if (ctr_rectMap.active == DISPLAY_PIPBOY) {
        // Just the Pip-Boy's own screen.
        minX = 240; minY = 46; maxY = 456;
    }
    int srcX = mx - 200, srcY = my - 120;
    if (srcX > maxX - 400) srcX = maxX - 400;
    if (srcX < minX) srcX = minX;
    if (srcY > maxY - 240) srcY = maxY - 240;
    if (srcY < minY) srcY = minY;
    drawRect(srcX, srcY, 400, 240, 0, 0, 400, 240);
}

void drawRects()
{
#if defined(CTR_EMUTEST) && defined(CTR_DEBUG_LOG)
    {
        // Emulator test builds: heap use, to spot leaks over a long session.
        static int frames = 0;
        if (++frames % 300 == 0) {
            struct mallinfo mi = mallinfo();
            char mb[96];
            snprintf(mb, sizeof(mb), "MEM frame=%d inuse=%d arena=%d", frames, mi.uordblks, mi.arena);
            ctr_debug_log(mb);
        }
    }
#endif
    bool dashOnTop = false;
    bool helpOnTop = false;
    int cardOnTop = 0; // 1 = item card, 2 = barter
    bool loadsaveOnTop = false;
    bool worldOnTop = false;
    const char* hintText = nullptr;

    {
        // A new inventory/loot screen starts without a stale item card.
        static int lastActive = -1;
        int active = ctr_rectMap.active;
        auto isInv = [](int r) {
            return r == DISPLAY_INVENTORY || r == DISPLAY_INVENTORY_USE || r == DISPLAY_INVENTORY_LOOT
                || r == DISPLAY_INVENTORY_TRADE;
        };
        // (Only when coming from the map or a conversation: the quantity
        // dialog in between keeps the card.)
        if (isInv(active) && (lastActive == DISPLAY_FIELD || lastActive == DISPLAY_GUI
                || lastActive == DISPLAY_DIALOG || lastActive == -1)) {
            ctrCardClear();
        }
        lastActive = active;
    }

    beginRender(ctr_rectMap.active == DISPLAY_MOVIE);
    drawTopRenderTarget(0x000000);

    if (ctr_top_lens_active()) {
        ctrDrawTopLens();
    } else
    switch (ctr_rectMap.active)
    {
        case DISPLAY_SPLASH:
            break;

        case DISPLAY_MOVIE:
            drawRect(rectMaps[ctr_rectMap.active][0]->src_x, rectMaps[ctr_rectMap.active][0]->src_y,
                    rectMaps[ctr_rectMap.active][0]->src_w, rectMaps[ctr_rectMap.active][0]->src_h,
                    rectMaps[ctr_rectMap.active][0]->dst_x, rectMaps[ctr_rectMap.active][0]->dst_y,
                    rectMaps[ctr_rectMap.active][0]->dst_w, rectMaps[ctr_rectMap.active][0]->dst_h
            );
            break;

        case DISPLAY_INVENTORY_TRADE:
            // Barter: offer summary and the touched item's details.
            cardOnTop = 2;
            break;

        case DISPLAY_DIALOG:
            for (int i = 0; i < numRectsInMap[DISPLAY_DIALOG_TOP]; i++) {
                drawRect(rectMaps[DISPLAY_DIALOG_TOP][i]->src_x, rectMaps[DISPLAY_DIALOG_TOP][i]->src_y,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->src_w, rectMaps[DISPLAY_DIALOG_TOP][i]->src_h,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->dst_x, rectMaps[DISPLAY_DIALOG_TOP][i]->dst_y,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->dst_w, rectMaps[DISPLAY_DIALOG_TOP][i]->dst_h
                );
            }
            break;

        case DISPLAY_LOADSAVE:
            // The slot's preview picture at 1:1; the slot card is drawn around it.
            drawRect(366, 58, 224, 133, 8, 26, 224, 133);
            loadsaveOnTop = true;
            break;

        case DISPLAY_INVENTORY:
        case DISPLAY_INVENTORY_USE:
        case DISPLAY_INVENTORY_LOOT:
            cardOnTop = ctrDashboardAvailable() ? 1 : 0;
            dashOnTop = cardOnTop == 0;
            break;

        case DISPLAY_FIELD:
        case DISPLAY_SKILLDEX:
        case DISPLAY_PAUSE:
            // Map (or a menu) is on the touchscreen: the top screen is the
            // information dashboard, drawn after the bottom screen.
            dashOnTop = true;
            break;

        case DISPLAY_GUI:
            drawRect(rectMaps[DISPLAY_FIELD][0]->src_x, rectMaps[DISPLAY_FIELD][0]->src_y,
                    rectMaps[DISPLAY_FIELD][0]->src_w, rectMaps[DISPLAY_FIELD][0]->src_h,
                    rectMaps[DISPLAY_FIELD][0]->dst_x, rectMaps[DISPLAY_FIELD][0]->dst_y,
                    rectMaps[DISPLAY_FIELD][0]->dst_w, rectMaps[DISPLAY_FIELD][0]->dst_h);
            break;

        case DISPLAY_CHAR_PERK:
            drawRect(rectMaps[DISPLAY_CHAR_PERK_TOP][0]->src_x, rectMaps[DISPLAY_CHAR_PERK_TOP][0]->src_y,
                    rectMaps[DISPLAY_CHAR_PERK_TOP][0]->src_w, rectMaps[DISPLAY_CHAR_PERK_TOP][0]->src_h,
                    rectMaps[DISPLAY_CHAR_PERK_TOP][0]->dst_x, rectMaps[DISPLAY_CHAR_PERK_TOP][0]->dst_y,
                    rectMaps[DISPLAY_CHAR_PERK_TOP][0]->dst_w, rectMaps[DISPLAY_CHAR_PERK_TOP][0]->dst_h
            );
            break;

        case DISPLAY_CHAR:
            hintText = "Circle Pad / C-Stick: scroll down for skills";
            for (int i = 0; i < numRectsInMap[DISPLAY_CHAR_TOP]; i++) {
                drawRect(rectMaps[DISPLAY_CHAR_TOP][i]->src_x, rectMaps[DISPLAY_CHAR_TOP][i]->src_y,
                        rectMaps[DISPLAY_CHAR_TOP][i]->src_w, rectMaps[DISPLAY_CHAR_TOP][i]->src_h,
                        rectMaps[DISPLAY_CHAR_TOP][i]->dst_x, rectMaps[DISPLAY_CHAR_TOP][i]->dst_y,
                        rectMaps[DISPLAY_CHAR_TOP][i]->dst_w, rectMaps[DISPLAY_CHAR_TOP][i]->dst_h
                );
            }
            break;

        case DISPLAY_CHAR_SELECT:
            drawRect(rectMaps[DISPLAY_CHAR_SELECT_TOP][0]->src_x, rectMaps[DISPLAY_CHAR_SELECT_TOP][0]->src_y,
                    rectMaps[DISPLAY_CHAR_SELECT_TOP][0]->src_w, rectMaps[DISPLAY_CHAR_SELECT_TOP][0]->src_h,
                    rectMaps[DISPLAY_CHAR_SELECT_TOP][0]->dst_x, rectMaps[DISPLAY_CHAR_SELECT_TOP][0]->dst_y,
                    rectMaps[DISPLAY_CHAR_SELECT_TOP][0]->dst_w, rectMaps[DISPLAY_CHAR_SELECT_TOP][0]->dst_h
            );
            break;

        case DISPLAY_WORLDMAP:
            if (ctr_input.mode == DISPLAY_MODE_ADAPT && ctrDashboardAvailable()) {
                worldOnTop = true;
                break;
            }
            drawRect(offsetX, (240 - offsetY), 400, 240, 0,   0, 400, 240);
            break;

        case DISPLAY_PIPBOY:
            if (ctr_input.mode == DISPLAY_MODE_ADAPT) {
                dashOnTop = ctrDashboardAvailable();
                helpOnTop = !dashOnTop;
                break;
            }
            drawRect(offsetX, (240 - offsetY), 400, 240, 0,   0, 400, 240);
            break;

        case DISPLAY_OPTIONS:
            if (ctr_input.mode == DISPLAY_MODE_ADAPT) {
                dashOnTop = ctrDashboardAvailable();
                helpOnTop = !dashOnTop;
                break;
            }
            drawRect(offsetX, (240 - offsetY), 400, 240, 0,   0, 400, 240);
            break;

        case DISPLAY_FULL:
            if (ctr_input.mode != DISPLAY_MODE_ADAPT) {
                // Whole-screen mode (Select+L): the touch screen shows the
                // whole picture, the top shows the dashboard. ZL switches the
                // top to a 1:1 zoom lens that follows the stylus (Ryan: both
                // screens filled, never duplicated; zoom only on a button press).
                if (!ctrLens[DISPLAY_FULL]) {
                    // The touch screen already shows the whole picture:
                    // the top shows the dashboard, never a duplicate.
                    if (ctrDashboardAvailable()) dashOnTop = true; else helpOnTop = true;
                } else {
                    drawRect(offsetX, (240 - offsetY), 400, 240, 0,   0, 400, 240);
                }
            } else if (ctrDashboardAvailable()) {
                dashOnTop = true;
            } else {
                // Main menu and other out-of-game screens: controls card.
                helpOnTop = true;
            }
            break;

        default:
            if (ctrDashboardAvailable()) {
                // Any other in-game window (automap, elevator, dialog boxes,
                // death screen...): the dashboard, never a duplicate picture.
                dashOnTop = true;
                break;
            }
            helpOnTop = true;
            break;
    }

    drawBottomRenderTarget(0x000000);

    switch (ctr_rectMap.active)
    {
		case DISPLAY_DIALOG:
        case DISPLAY_MOVIE:
        case DISPLAY_SKILLDEX:
        case DISPLAY_INVENTORY_TRADE:
        case DISPLAY_WORLDMAP:
            drawFrameRect();
            break;
        case DISPLAY_CHAR_SELECT:
            C3D_TexBind(0, &static_tex);
            drawRect_tex(0, 240, 320, 240, 0, 0, 320, 240);
            C3D_TexBind(0, &render_tex);
            break;
        default:
            break;
    }

    switch (ctr_rectMap.active)
    {
        case DISPLAY_MOVIE:
            setTextureFilter(1);
            drawRect(rectMaps[DISPLAY_MOVIE_SUB][0]->src_x, rectMaps[DISPLAY_MOVIE_SUB][0]->src_y,
                    rectMaps[DISPLAY_MOVIE_SUB][0]->src_w, rectMaps[DISPLAY_MOVIE_SUB][0]->src_h,
                    rectMaps[DISPLAY_MOVIE_SUB][0]->dst_x, rectMaps[DISPLAY_MOVIE_SUB][0]->dst_y,
                    rectMaps[DISPLAY_MOVIE_SUB][0]->dst_w, rectMaps[DISPLAY_MOVIE_SUB][0]->dst_h);
            setTextureFilter(-1);
            break;

        case DISPLAY_SPLASH:
            setTextureFilter(1);
            drawRect(rectMaps[DISPLAY_FULL][0]->src_x, rectMaps[DISPLAY_FULL][0]->src_y,
                    rectMaps[DISPLAY_FULL][0]->src_w, rectMaps[DISPLAY_FULL][0]->src_h,
                    rectMaps[DISPLAY_FULL][0]->dst_x, rectMaps[DISPLAY_FULL][0]->dst_y,
                    rectMaps[DISPLAY_FULL][0]->dst_w, rectMaps[DISPLAY_FULL][0]->dst_h
            );
            setTextureFilter(-1);
            break;

        case DISPLAY_DIALOG:
            for (int i = 0; i < numRectsInMap[ctr_rectMap.active]; i++) {
                drawRect(rectMaps[ctr_rectMap.active][i]->src_x, rectMaps[ctr_rectMap.active][i]->src_y,
                        rectMaps[ctr_rectMap.active][i]->src_w, rectMaps[ctr_rectMap.active][i]->src_h,
                        rectMaps[ctr_rectMap.active][i]->dst_x, rectMaps[ctr_rectMap.active][i]->dst_y,
                        rectMaps[ctr_rectMap.active][i]->dst_w, rectMaps[ctr_rectMap.active][i]->dst_h
                );
            }
            break;

        case DISPLAY_LOADSAVE:

            for (int i = 0; i < numRectsInMap[DISPLAY_LOADSAVE_BACK]; i++) {
                drawRect(rectMaps[DISPLAY_LOADSAVE_BACK][i]->src_x, rectMaps[DISPLAY_LOADSAVE_BACK][i]->src_y,
                        rectMaps[DISPLAY_LOADSAVE_BACK][i]->src_w, rectMaps[DISPLAY_LOADSAVE_BACK][i]->src_h,
                        rectMaps[DISPLAY_LOADSAVE_BACK][i]->dst_x, rectMaps[DISPLAY_LOADSAVE_BACK][i]->dst_y,
                        rectMaps[DISPLAY_LOADSAVE_BACK][i]->dst_w, rectMaps[DISPLAY_LOADSAVE_BACK][i]->dst_h
                );
            }

            drawRect(rectMaps[DISPLAY_LOADSAVE_SLOT][0]->src_x,
			        rectMaps[DISPLAY_LOADSAVE_SLOT][0]->src_y + getSaveSlotOffset(),
                    rectMaps[DISPLAY_LOADSAVE_SLOT][0]->src_w, rectMaps[DISPLAY_LOADSAVE_SLOT][0]->src_h,
                    rectMaps[DISPLAY_LOADSAVE_SLOT][0]->dst_x, rectMaps[DISPLAY_LOADSAVE_SLOT][0]->dst_y,
                    rectMaps[DISPLAY_LOADSAVE_SLOT][0]->dst_w, rectMaps[DISPLAY_LOADSAVE_SLOT][0]->dst_h
            );
            for (int i = 0; i < numRectsInMap[ctr_rectMap.active]; i++) {
                drawRect(rectMaps[ctr_rectMap.active][i]->src_x, rectMaps[ctr_rectMap.active][i]->src_y,
                        rectMaps[ctr_rectMap.active][i]->src_w, rectMaps[ctr_rectMap.active][i]->src_h,
                        rectMaps[ctr_rectMap.active][i]->dst_x, rectMaps[ctr_rectMap.active][i]->dst_y,
                        rectMaps[ctr_rectMap.active][i]->dst_w, rectMaps[ctr_rectMap.active][i]->dst_h
                );
            }
            break;

        case DISPLAY_CHAR:

            if (offsetY_char <= 240) {
                drawRect(rectMaps[DISPLAY_CHAR][0]->src_x, rectMaps[DISPLAY_CHAR][0]->src_y+offsetY_char,
                        rectMaps[DISPLAY_CHAR][0]->src_w, rectMaps[DISPLAY_CHAR][0]->src_h,
                        rectMaps[DISPLAY_CHAR][0]->dst_x, rectMaps[DISPLAY_CHAR][0]->dst_y,
                        rectMaps[DISPLAY_CHAR][0]->dst_w, rectMaps[DISPLAY_CHAR][0]->dst_h
                );
            } else if ((offsetY_char > 240) && (offsetY_char <= 480)) {
                drawRect(rectMaps[DISPLAY_CHAR][0]->src_x, rectMaps[DISPLAY_CHAR][0]->src_y+240,
                        rectMaps[DISPLAY_CHAR][0]->src_w, rectMaps[DISPLAY_CHAR][0]->src_h,
                        rectMaps[DISPLAY_CHAR][0]->dst_x, rectMaps[DISPLAY_CHAR][0]->dst_y-(240-offsetY_char),
                        rectMaps[DISPLAY_CHAR][0]->dst_w, rectMaps[DISPLAY_CHAR][0]->dst_h
                );
                drawRect(rectMaps[DISPLAY_CHAR][1]->src_x, rectMaps[DISPLAY_CHAR][1]->src_y,
                        rectMaps[DISPLAY_CHAR][1]->src_w, rectMaps[DISPLAY_CHAR][1]->src_h,
                        rectMaps[DISPLAY_CHAR][1]->dst_x+10, rectMaps[DISPLAY_CHAR][0]->dst_y-(480-offsetY_char),
                        rectMaps[DISPLAY_CHAR][1]->dst_w, rectMaps[DISPLAY_CHAR][1]->dst_h
                );
            } else if (offsetY_char > 480) {
                drawRect(rectMaps[DISPLAY_CHAR][1]->src_x, rectMaps[DISPLAY_CHAR][1]->src_y+(offsetY_char-480),
                        rectMaps[DISPLAY_CHAR][1]->src_w, rectMaps[DISPLAY_CHAR][1]->src_h,
                        rectMaps[DISPLAY_CHAR][1]->dst_x+10, rectMaps[DISPLAY_CHAR][1]->dst_y,
                        rectMaps[DISPLAY_CHAR][1]->dst_w, rectMaps[DISPLAY_CHAR][1]->dst_h
                );
            }
            for (int i = 2; i < 4; i++) {
                drawRect(rectMaps[ctr_rectMap.active][i]->src_x, rectMaps[ctr_rectMap.active][i]->src_y,
                        rectMaps[ctr_rectMap.active][i]->src_w, rectMaps[ctr_rectMap.active][i]->src_h,
                        rectMaps[ctr_rectMap.active][i]->dst_x, rectMaps[ctr_rectMap.active][i]->dst_y,
                        rectMaps[ctr_rectMap.active][i]->dst_w, rectMaps[ctr_rectMap.active][i]->dst_h
                );

            }
            if (isAgeWindow){
                drawRect(rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->src_x, rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->src_y,
                        rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->src_w, rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->src_h,
                        rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->dst_x, rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->dst_y,
                        rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->dst_w, rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->dst_h
                );
            }
            if (isSexWindow){
                drawRect(rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->src_x, rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->src_y,
                        rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->src_w, rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->src_h,
                        rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->dst_x, rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->dst_y,
                        rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->dst_w, rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->dst_h
                );
            }
            break;

        case DISPLAY_GUI:
            for (int i = 0; i < getIndicatorSlotNum(); i++) {
                drawRect(rectMaps[DISPLAY_GUI_INDICATOR][i]->src_x, rectMaps[DISPLAY_GUI_INDICATOR][i]->src_y,
                        rectMaps[DISPLAY_GUI_INDICATOR][i]->src_w, rectMaps[DISPLAY_GUI_INDICATOR][i]->src_h,
                        rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_x, rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_y,
                        rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_w, rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_h
                );
            }

        case DISPLAY_FULL:
            if (ctr_input.mode == DISPLAY_MODE_FULL_TOP)
                break;

        default:
            // A small window (message box, quit question, elevator...) that
            // doesn't fill the touch screen sits over the whole game picture,
            // never over black bands.
            if (ctr_rectMap.active == DISPLAY_DYNAMIC) {
                const rectMap_t* r = rectMaps[DISPLAY_DYNAMIC][0];
                if (r->dst_w < 319.f || r->dst_h < 239.f) {
                    drawRect(0, 0, 640, 480, 0, 0, 320, 240);
                }
            }
            for (int i = 0; i < numRectsInMap[ctr_rectMap.active]; i++) {
                drawRect(rectMaps[ctr_rectMap.active][i]->src_x, rectMaps[ctr_rectMap.active][i]->src_y,
                        rectMaps[ctr_rectMap.active][i]->src_w, rectMaps[ctr_rectMap.active][i]->src_h,
                        rectMaps[ctr_rectMap.active][i]->dst_x, rectMaps[ctr_rectMap.active][i]->dst_y,
                        rectMaps[ctr_rectMap.active][i]->dst_w, rectMaps[ctr_rectMap.active][i]->dst_h
                );
            }
            break;
    }

#ifdef _DEBUG_OVERLAY
    show_overlay();
    gpuStateReady = false; // C2D overlay invalidates GPU state
#endif

    if (dashOnTop) {
        ctr_dashboard_draw();
    } else if (helpOnTop) {
        ctr_help_draw();
    } else if (cardOnTop != 0) {
        ctr_card_draw(cardOnTop == 2);
    } else if (loadsaveOnTop) {
        ctr_loadsave_draw();
    } else if (worldOnTop) {
        ctr_worldmap_draw();
    }
    if (ctrWideHint != nullptr && osGetTime() < ctrWideHintUntil) {
        hintText = ctrWideHint;
    }
    if (hintText != nullptr) {
        ctr_hint_draw(hintText);
    }
    if (ctr_rectMap.active == DISPLAY_MOVIE && !ctrMovieSubtitles) {
        ctr_movie_card_draw();
    }

    finishRender();
}

void drawFrameRect()
{
    C3D_TexBind(0, &static_tex);
    drawRect_tex(0, 0, 320, 240, 0, 0, 320, 240);
    C3D_TexBind(0, &render_tex);
}

// Pre-computed RGB565 palette lookup table
static uint16_t rgb565Palette[256];
static bool paletteDirty = true;

// Dirty rect tracking for partial conversion
static int dirtyY0 = 0;
static int dirtyY1 = -1; // -1 means no dirty info (full surface)

// GPU state caching
static bool gpuStateReady = false;

static inline void updateRgb565Palette(SDL_Color* palette)
{
    if (!paletteDirty) return;
    for (int i = 0; i < 256; i++) {
        rgb565Palette[i] = ((palette[i].r >> 3) << 11)
                         | ((palette[i].g >> 2) << 5)
                         | (palette[i].b >> 3);
    }
    paletteDirty = false;
}

void ctr_gfx_mark_palette_dirty(void)
{
    paletteDirty = true;
}

void ctr_gfx_set_dirty_rect(int y0, int y1)
{
    if (dirtyY1 < 0) {
        // First dirty rect this frame
        dirtyY0 = y0;
        dirtyY1 = y1;
    } else {
        // Union with existing dirty range
        if (y0 < dirtyY0) dirtyY0 = y0;
        if (y1 > dirtyY1) dirtyY1 = y1;
    }
}

void ctr_gfx_invalidate_gpu_state(void)
{
    gpuStateReady = false;
}

void ctr_gfx_draw(SDL_Surface* gSdlSurface)
{
#ifdef _DEBUG_OVERLAY
    static int gfxFrameCount = 0;
    static u64 gfxTotalConvert = 0;
    static u64 gfxTotalTransfer = 0;
    static u64 gfxTotalDraw = 0;
    u64 tStart = osGetTime();
#endif

    // Opt 5: Only re-init GPU state when invalidated (e.g. after C2D overlay draws)
    if (!gpuStateReady) {
        C3D_BindProgram(&program);

        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

        gpuStateReady = true;
    }

    Uint8* dst = (Uint8*)renderTextureData;
    Uint8* src = (Uint8*)gSdlSurface->pixels;
    SDL_Color* palette = gSdlSurface->format->palette->colors;

    int surfacePitch = gSdlSurface->pitch;
    int surfaceWidth = gSdlSurface->w;
    int surfaceHeight = gSdlSurface->h;

    // Determine which rows need conversion
    int startY = 0;
    int endY = surfaceHeight;

    // During movie playback, only convert the movie area rows
    if (ctr_rectMap.active == DISPLAY_MOVIE) {
        if (numRectsInMap[DISPLAY_MOVIE] > 0) {
            startY = rectMaps[DISPLAY_MOVIE][0]->src_y;
            endY = startY + rectMaps[DISPLAY_MOVIE][0]->src_h;
            if (endY > surfaceHeight) endY = surfaceHeight;
            if (startY < 0) startY = 0;
            if (numRectsInMap[DISPLAY_MOVIE_SUB] > 0) {
                int subEnd = rectMaps[DISPLAY_MOVIE_SUB][0]->src_y + rectMaps[DISPLAY_MOVIE_SUB][0]->src_h;
                if (subEnd > endY) {
                    endY = (subEnd <= surfaceHeight) ? subEnd : surfaceHeight;
                }
            }
        }
    } else if (dirtyY1 >= 0) {
        // Opt 8: Use dirty rect bounds for non-movie modes
        startY = dirtyY0;
        endY = dirtyY1;
        if (startY < 0) startY = 0;
        if (endY > surfaceHeight) endY = surfaceHeight;
    }
    // Reset dirty tracking for next frame
    dirtyY1 = -1;

    updateRgb565Palette(palette);

    for (int y = startY; y < endY; y++) {
        uint32_t* rowDst32 = (uint32_t*)(dst + y * renderTextureStride);
        Uint8* rowSrc = src + y * surfacePitch;

        int x = 0;
        // Process 4 pixels at a time: pack 2 RGB565 values per 32-bit store
        for (; x <= surfaceWidth - 4; x += 4) {
            uint32_t s = *(uint32_t*)(rowSrc + x);
            rowDst32[x >> 1]       = rgb565Palette[s & 0xFF]
                                   | ((uint32_t)rgb565Palette[(s >> 8) & 0xFF] << 16);
            rowDst32[(x >> 1) + 1] = rgb565Palette[(s >> 16) & 0xFF]
                                   | ((uint32_t)rgb565Palette[s >> 24] << 16);
        }
        uint16_t* rowDst16 = (uint16_t*)(dst + y * renderTextureStride);
        for (; x < surfaceWidth; x++) {
            rowDst16[x] = rgb565Palette[rowSrc[x]];
        }
    }

#ifdef _DEBUG_OVERLAY
    u64 tAfterConvert = osGetTime();
#endif

    // Opt 3/9: Cap transfer height to actual content, 8-aligned for GPU tiles
    int transferHeight = (endY + 7) & ~7;
    if (transferHeight > (int)renderTextureHeight) transferHeight = (int)renderTextureHeight;

    if (startY > 0 || transferHeight < (int)renderTextureHeight) {
        uint32_t flushOffset = startY * renderTextureStride;
        uint32_t flushSize = (transferHeight - startY) * renderTextureStride;
        GSPGPU_FlushDataCache(renderTextureData + flushOffset, flushSize);
    } else {
        GSPGPU_FlushDataCache(renderTextureData, renderTextureByteCount);
    }

    C3D_SyncDisplayTransfer((u32*)renderTextureData, GX_BUFFER_DIM(renderTextureWidth, transferHeight),
            (u32*)render_tex.data, GX_BUFFER_DIM(renderTextureWidth, transferHeight), TEXTURE_TRANSFER_FLAGS);

#ifdef _DEBUG_OVERLAY
    u64 tAfterTransfer = osGetTime();
#endif

    C3D_TexBind(0, &render_tex);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    drawRects();

#ifdef _DEBUG_OVERLAY
    u64 tAfterDraw = osGetTime();

    gfxTotalConvert += (tAfterConvert - tStart);
    gfxTotalTransfer += (tAfterTransfer - tAfterConvert);
    gfxTotalDraw += (tAfterDraw - tAfterTransfer);
    gfxFrameCount++;

    if (gfxFrameCount == 15) {
        char buf[128];
        snprintf(buf, sizeof(buf), "gfx15f: conv=%lums xfer=%lums draw=%lums rows=%d-%d",
            (unsigned long)gfxTotalConvert, (unsigned long)gfxTotalTransfer, (unsigned long)gfxTotalDraw, startY, endY);
        ctr_debug_log(buf);
        gfxFrameCount = 0;
        gfxTotalConvert = 0;
        gfxTotalTransfer = 0;
        gfxTotalDraw = 0;
    }
#endif
}

void beginRender(bool vSync)
{
    C3D_FrameBegin(vSync ? C3D_FRAME_SYNCDRAW : 0);
}

void drawTopRenderTarget(uint32_t clearColor)
{
    C3D_RenderTargetClear(topRenderTarget, C3D_CLEAR_ALL, clearColor, 0);
    C3D_FrameDrawOn(topRenderTarget);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &topProjection);
    targetTop = true;
}

void drawBottomRenderTarget(uint32_t clearColor)
{
    C3D_RenderTargetClear(bottomRenderTarget, C3D_CLEAR_ALL, clearColor, 0);
    C3D_FrameDrawOn(bottomRenderTarget);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &bottomProjection);
    targetTop = false;
}

void finishRender()
{
    C3D_FrameEnd(0);
}

void initTransferTexture()
{
    renderTextureData = (uint8_t *)linearAlloc(renderTextureByteCount);
    if (renderTextureData == nullptr) {
        ctr_debug_log("initTransferTexture: linearAlloc FAILED");
        showMesageBox("Failed to allocate render texture\n");
        exit(EXIT_FAILURE);
    }

    memset(renderTextureData, 0, renderTextureByteCount);

    C3D_TexInitVRAM(&render_tex, renderTextureWidth, renderTextureHeight, GPU_RGB565);
    C3D_TexSetWrap(&render_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    setTextureFilter(-1);
}

static bool ctrWideAllowed = false;
static const char* kWideSettingFile = "3ds-display.cfg";

static bool ctr_gfx_wide_setting()
{
    FILE* f = fopen(kWideSettingFile, "r");
    if (f == NULL) {
        return false;
    }
    int wide = 0;
    if (fscanf(f, "wide=%d", &wide) != 1) {
        wide = 0;
    }
    fclose(f);
    return wide != 0;
}

// Select+R: switch the top screen between 400 and 800 pixels wide.
void ctr_gfx_toggle_wide()
{
    if (!ctrWideAllowed) {
        ctrWideHint = "Sharp 800-pixel top screen: not available on this system";
        ctrWideHintUntil = osGetTime() + 2500;
        return;
    }
    ctr_gfx.isWide = !ctr_gfx.isWide;
    gfxSetWide(ctr_gfx.isWide);
    ctr_gfx_reinit();
    FILE* f = fopen(kWideSettingFile, "w");
    if (f != NULL) {
        fprintf(f, "wide=%d\n", ctr_gfx.isWide ? 1 : 0);
        fclose(f);
    }
    ctrWideHint = ctr_gfx.isWide ? "Sharp 800-pixel top screen: ON  (Select+R turns it off)"
                                 : "Sharp 800-pixel top screen: OFF  (Select+R turns it on)";
    ctrWideHintUntil = osGetTime() + 2500;
}

void ctr_gfx_reinit()
{
    C3D_RenderTargetDelete(topRenderTarget);

    topRenderTarget = C3D_RenderTargetCreate(
            GSP_SCREEN_WIDTH, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, GPU_RB_RGB8, GPU_RB_DEPTH16);
    C3D_RenderTargetSetOutput(topRenderTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    C3D_BindProgram(&program);

    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

    C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

    Mtx_OrthoTilt(&topProjection, 0, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, 0, GSP_SCREEN_WIDTH, 0.1f, 1.f, true);
    gpuStateReady = false;
}

void ctr_gfx_init()
{
    ctr_debug_log("ctr_gfx_init: start");

    if(gspHasGpuRight()) {
        ctr_debug_log("ctr_gfx_init: gspHasGpuRight, calling gfxExit");
        gfxExit();
    }

    u8 is2DS;
    cfguInit();
    CFGU_GetModelNintendo2DS(&is2DS);
    APT_CheckNew3DS(&isN3DS);
    ctr_debug_log("ctr_gfx_init: cfgu done");

    gfxInitDefault();
    ctr_debug_log("ctr_gfx_init: gfxInitDefault done");

    // 800px top-screen mode (sharper text) is optional: Select+R toggles it
    // and the choice is remembered. The original 2DS can't do it.
    ctrWideAllowed = is2DS == 1; // CFGU_GetModelNintendo2DS gives 0 on a 2DS, 1 on every 3DS
    if (ctrWideAllowed && ctr_gfx_wide_setting()) {
        gfxSetWide(true);
        ctr_gfx.isWide = gfxIsWide();
    } else
        ctr_gfx.isWide = false;

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    ctr_debug_log("ctr_gfx_init: C3D_Init done");

    topRenderTarget = C3D_RenderTargetCreate(
            GSP_SCREEN_WIDTH, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, GPU_RB_RGB8, GPU_RB_DEPTH16);
    C3D_RenderTargetSetOutput(topRenderTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    bottomRenderTarget = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, GPU_RB_RGB8, GPU_RB_DEPTH16);
    C3D_RenderTargetSetOutput(bottomRenderTarget, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
    ctr_debug_log("ctr_gfx_init: render targets done");

    vshader_dvlb = DVLB_ParseFile((uint32_t *)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    C3D_BindProgram(&program);
    ctr_debug_log("ctr_gfx_init: shader done");

    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

    C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

    Mtx_OrthoTilt(&topProjection, 0, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, 0, GSP_SCREEN_WIDTH, 0.1f, 1.f, true);
    Mtx_OrthoTilt(&bottomProjection, 0, GSP_SCREEN_HEIGHT_BOTTOM, 0, GSP_SCREEN_WIDTH, 0.1f, 1.f, true);

    ctr_debug_log("ctr_gfx_init: calling initTransferTexture");
    initTransferTexture();
    ctr_debug_log("ctr_gfx_init: initTransferTexture done");

    ctr_debug_log("ctr_gfx_init: calling romfsInit");
    romfsInit();
    ctr_debug_log("ctr_gfx_init: romfsInit done");

    if (!loadTextureFromFile(&static_tex, NULL, "romfs:/gfx/frame_tex.t3x")) {
        ctr_debug_log("ctr_gfx_init: loadTextureFromFile FAILED");
        showMesageBox("loadTextureFromFile failed\n");
        exit(EXIT_FAILURE);
    }
    ctr_debug_log("ctr_gfx_init: frame texture loaded");

    C3D_TexSetFilter(&static_tex, GPU_LINEAR, GPU_NEAREST);
    C3D_TexSetWrap(&static_tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    overlayTextBuf = C2D_TextBufNew(4096);
    ctr_debug_log("ctr_gfx_init: complete");
}

void ctr_gfx_exit()
{
    shaderProgramFree(&program);
    DVLB_Free(vshader_dvlb);

    linearFree(renderTextureData);
    C3D_TexDelete(&render_tex);
    C3D_TexDelete(&static_tex);

    C2D_TextBufDelete(overlayTextBuf);
    C2D_Fini();

    C3D_Fini();
    romfsExit();
    cfguExit();
}


// ---------------------------------------------------------------------------
// Top-screen dashboard (citro2d text, rebuilt only when the data changes)
// ---------------------------------------------------------------------------
static C2D_TextBuf dashTextBuf = nullptr;
static C2D_TextBuf dashMeasureBuf = nullptr;
static CtrDashboard dashPrev;
static bool dashHavePrev = false;
static const char* dashHintPrev = nullptr;

#define DASH_LOG_LINES 16
static const float kDashLogScale = 0.45f;

enum {
    DASH_T_NAME, DASH_T_TIME, DASH_T_HP, DASH_T_AC, DASH_T_AP, DASH_T_WEAPON,
    DASH_T_AMMO, DASH_T_ATTACK, DASH_T_OTHER, DASH_T_INFO, DASH_T_INFO_RIGHT,
    DASH_T_HP_LABEL, DASH_T_AP_LABEL, DASH_T_HINT,
    DASH_T_COUNT
};
static C2D_Text dashText[DASH_T_COUNT];
static C2D_Text dashLog[DASH_LOG_LINES];
static bool dashLogFirst[DASH_LOG_LINES]; // first line of a message (gets the bullet)
static bool dashLogNewest[DASH_LOG_LINES];
static int dashLogCount = 0;

static void dashParse(C2D_Text* t, const char* str)
{
    C2D_TextParse(t, dashTextBuf, str);
    C2D_TextOptimize(t);
}

static float dashMeasure(const char* str)
{
    C2D_Text t;
    C2D_TextBufClear(dashMeasureBuf);
    C2D_TextParse(&t, dashMeasureBuf, str);
    float w = 0, h = 0;
    C2D_TextGetDimensions(&t, kDashLogScale, kDashLogScale, &w, &h);
    return w;
}

// Word-wraps one message at the top screen's width; returns lines written.
static int dashWrap(const char* msg, char out[][CTR_DASH_MSG_TEXT], int maxLines, float width)
{
    int lines = 0;
    char line[CTR_DASH_MSG_TEXT] = "";
    char trial[CTR_DASH_MSG_TEXT * 2];
    const char* p = msg;
    while (*p != '\0' && lines < maxLines) {
        while (*p == ' ') p++;
        const char* end = p;
        while (*end != '\0' && *end != ' ') end++;
        if (end == p) break;
        snprintf(trial, sizeof(trial), "%s%s%.*s", line, line[0] != '\0' ? " " : "", (int)(end - p), p);
        if (line[0] != '\0' && dashMeasure(trial) > width) {
            snprintf(out[lines++], CTR_DASH_MSG_TEXT, "%s", line);
            snprintf(line, sizeof(line), "%.*s", (int)(end - p), p);
        } else {
            snprintf(line, sizeof(line), "%s", trial);
        }
        p = end;
    }
    if (line[0] != '\0' && lines < maxLines) {
        snprintf(out[lines++], CTR_DASH_MSG_TEXT, "%s", line);
    }
    return lines;
}

static void dashRebuild(const CtrDashboard* d, const char* hint, int logLines)
{
    char buf[160];
    C2D_TextBufClear(dashTextBuf);

    if (d->sneaking) {
        snprintf(buf, sizeof(buf), "%s  [SNEAKING]", d->name);
        dashParse(&dashText[DASH_T_NAME], buf);
    } else {
        dashParse(&dashText[DASH_T_NAME], d->name);
    }
    if (d->turn == 1) {
        dashParse(&dashText[DASH_T_TIME], "COMBAT - YOUR TURN");
    } else if (d->turn == 2) {
        dashParse(&dashText[DASH_T_TIME], "COMBAT - ENEMY TURN");
    } else {
        dashParse(&dashText[DASH_T_TIME], d->time);
    }
    snprintf(buf, sizeof(buf), "%d / %d", d->hp, d->hpMax);
    dashParse(&dashText[DASH_T_HP], buf);
    snprintf(buf, sizeof(buf), "AC %d", d->ac);
    dashParse(&dashText[DASH_T_AC], buf);
    if (d->inCombat) {
        if (d->apBonus > 0) {
            snprintf(buf, sizeof(buf), "%d / %d  +%d move", d->ap, d->apMax, d->apBonus);
        } else {
            snprintf(buf, sizeof(buf), "%d / %d", d->ap, d->apMax);
        }
    } else {
        snprintf(buf, sizeof(buf), "%d", d->apMax);
    }
    dashParse(&dashText[DASH_T_AP], buf);
    dashParse(&dashText[DASH_T_WEAPON], d->weapon);
    if (d->hasAmmo) {
        if (d->ammo == 0) {
            snprintf(buf, sizeof(buf), "Ammo 0 / %d - Select+B reload", d->ammoMax);
        } else {
            snprintf(buf, sizeof(buf), "Ammo %d / %d", d->ammo, d->ammoMax);
        }
    } else {
        buf[0] = '\0';
    }
    dashParse(&dashText[DASH_T_AMMO], buf);
    dashParse(&dashText[DASH_T_ATTACK], d->attack);
    snprintf(buf, sizeof(buf), "Other hand: %s", d->otherHand);
    dashParse(&dashText[DASH_T_OTHER], buf);

    if (d->hasTarget) {
        if (d->targetHit > 0) {
            snprintf(buf, sizeof(buf), "Target: %s - %s - %d%% to hit", d->target, d->targetState, d->targetHit);
        } else {
            snprintf(buf, sizeof(buf), "Target: %s - %s", d->target, d->targetState);
        }
    } else if (d->poison > 0 || d->rads > 0) {
        snprintf(buf, sizeof(buf), "Level %d   Poison %d   Rads %d", d->level, d->poison, d->rads);
    } else {
        snprintf(buf, sizeof(buf), "Level %d   XP %d / %d", d->level, d->xp, d->xpNext);
    }
    dashParse(&dashText[DASH_T_INFO], buf);
    if (!d->hasTarget) {
        snprintf(buf, sizeof(buf), "$%d   Wt %d / %d", d->caps, d->weight, d->weightMax);
    } else {
        buf[0] = '\0';
    }
    dashParse(&dashText[DASH_T_INFO_RIGHT], buf);
    dashParse(&dashText[DASH_T_HP_LABEL], "HP");
    dashParse(&dashText[DASH_T_AP_LABEL], "AP");
    dashParse(&dashText[DASH_T_HINT], hint != nullptr ? hint : "");

    // Message log: newest messages, wrapped at full width, bottom-aligned.
    static char wrapped[DASH_LOG_LINES * 2][CTR_DASH_MSG_TEXT];
    static bool first[DASH_LOG_LINES * 2];
    static bool newest[DASH_LOG_LINES * 2];
    int total = 0;
    for (int m = d->msgCount - 1; m >= 0 && total < logLines; m--) {
        char lines[6][CTR_DASH_MSG_TEXT];
        int n = dashWrap(d->msgs[m], lines, 6, 372.f);
        // Prepend this message's lines (we walk from newest to oldest).
        int keep = n;
        if (total + keep > logLines) {
            keep = logLines - total; // only the last lines of an old message fit
        }
        memmove(wrapped[keep], wrapped[0], sizeof(wrapped[0]) * total);
        memmove(first + keep, first, sizeof(bool) * total);
        memmove(newest + keep, newest, sizeof(bool) * total);
        for (int i = 0; i < keep; i++) {
            int src = n - keep + i;
            memcpy(wrapped[i], lines[src], CTR_DASH_MSG_TEXT);
            first[i] = src == 0;
            newest[i] = m == d->msgCount - 1;
        }
        total += keep;
    }
    if (total == 0 && logLines > 0) {
        // Nothing said yet: label the panel so it doesn't read as an empty band.
        snprintf(wrapped[0], CTR_DASH_MSG_TEXT, "Message log: what you see and hear appears here.");
        first[0] = true;
        newest[0] = false;
        total = 1;
    }
    dashLogCount = total;
    for (int i = 0; i < total; i++) {
        dashParse(&dashLog[i], wrapped[i]);
        dashLogFirst[i] = first[i];
        dashLogNewest[i] = newest[i];
    }
}

// One-line button reminder for the screen on the touchscreen.
static const char* dashHint(const CtrDashboard* d)
{
    switch (ctr_rectMap.active) {
    case DISPLAY_FIELD:
        if (d->turn == 1) {
            return "ZR end turn    B attack    Hold R odds    Select+A heal";
        }
        return nullptr;
    case DISPLAY_INVENTORY:
        return "Drag items to hands or armor    Hold for menu    B close";
    case DISPLAY_INVENTORY_LOOT:
        return "Drag items across    X take all    B close";
    case DISPLAY_INVENTORY_USE:
        return "Tap the item to use    B close";
    case DISPLAY_SKILLDEX:
        return "Tap a skill, then what to use it on    B close";
    case DISPLAY_WORLDMAP:
        return "Tap a place to travel there";
    case DISPLAY_PIPBOY:
        return "Tap the buttons on the left    B close";
    default:
        return nullptr;
    }
}

void ctr_dashboard_draw()
{
    if (dashTextBuf == nullptr) {
        dashTextBuf = C2D_TextBufNew(4096);
        dashMeasureBuf = C2D_TextBufNew(512);
    }

    CtrDashboard d;
    ctrDashboardGather(&d);
    if (!d.valid) {
        return;
    }
    const char* hint = dashHint(&d);

    // Layout: log from y=127 to the hint strip (or the bottom).
    const float logTop = 127.f;
    const float logBottom = hint != nullptr ? 222.f : 238.f;
    const float logStep = 14.5f;
    int logLines = (int)((logBottom - logTop) / logStep);
    if (logLines > DASH_LOG_LINES) logLines = DASH_LOG_LINES;

    if (!dashHavePrev || hint != dashHintPrev || memcmp(&d, &dashPrev, sizeof(d)) != 0) {
        dashRebuild(&d, hint, logLines);
        dashPrev = d;
        dashHintPrev = hint;
        dashHavePrev = true;
    }

    // 800px wide mode doubles horizontal resolution: scale x to match.
    const float kx = ctr_gfx.isWide ? 2.f : 1.f;
    const u32 bg = C2D_Color32(0x0b, 0x14, 0x0b, 0xff);
    const u32 panel = C2D_Color32(0x16, 0x26, 0x16, 0xff);
    const u32 green = C2D_Color32(0x5c, 0xff, 0x5c, 0xff);
    const u32 dim = C2D_Color32(0x3c, 0xb0, 0x3c, 0xff);
    const u32 amber = C2D_Color32(0xff, 0xc8, 0x40, 0xff);
    const u32 red = C2D_Color32(0xff, 0x50, 0x40, 0xff);

    auto rect = [&](float x, float y, float w, float h, u32 c) {
        C2D_DrawRectSolid(x * kx, y, 0.5f, w * kx, h, c);
    };
    auto text = [&](C2D_Text* t, float x, float y, float sc, u32 c, bool right = false) {
        C2D_DrawText(t, C2D_WithColor | (right ? C2D_AlignRight : 0), x * kx, y, 0.6f, sc * kx, sc, c);
    };

    C2D_Prepare();
    C2D_SceneBegin(topRenderTarget); // also makes the top screen the draw target

    rect(0, 0, 400, 240, bg);

    // Header: name + date/time, or whose turn it is in combat
    text(&dashText[DASH_T_NAME], 8, 2, 0.6f, d.sneaking ? amber : green);
    text(&dashText[DASH_T_TIME], 392, 5, 0.45f, d.turn == 1 ? amber : (d.turn == 2 ? red : dim), true);

    // HP bar
    text(&dashText[DASH_T_HP_LABEL], 8, 25, 0.5f, green);
    rect(36, 27, 270, 14, panel);
    float hpRatio = d.hpMax > 0 ? (float)d.hp / (float)d.hpMax : 0.f;
    if (hpRatio < 0) hpRatio = 0;
    if (hpRatio > 1) hpRatio = 1;
    u32 hpColor = hpRatio > 0.5f ? green : (hpRatio > 0.25f ? amber : red);
    rect(36, 27, 270 * hpRatio, 14, hpColor);
    text(&dashText[DASH_T_HP], 171, 26, 0.42f, C2D_Color32(0, 0, 0, 0xff));
    text(&dashText[DASH_T_AC], 392, 25, 0.5f, green, true);

    // AP pips
    text(&dashText[DASH_T_AP_LABEL], 8, 47, 0.5f, green);
    int pips = d.apMax > 14 ? 14 : d.apMax;
    for (int i = 0; i < pips; i++) {
        bool on = d.inCombat ? (i < d.ap) : true;
        rect(36 + i * 15, 50, 12, 10, on ? (d.inCombat ? green : dim) : panel);
    }
    text(&dashText[DASH_T_AP], 36 + pips * 15 + 6, 47, 0.45f, d.inCombat ? green : dim);

    // Weapon / attack / ammo / other hand
    text(&dashText[DASH_T_WEAPON], 8, 67, 0.52f, green);
    text(&dashText[DASH_T_AMMO], 392, 69, 0.45f, (d.hasAmmo && d.ammo == 0) ? red : green, true);
    text(&dashText[DASH_T_ATTACK], 8, 85, 0.47f, dim);
    text(&dashText[DASH_T_OTHER], 392, 86, 0.42f, dim, true);

    // Target (combat) or level/status, caps and load
    text(&dashText[DASH_T_INFO], 8, 103, 0.47f, d.hasTarget ? amber : dim);
    text(&dashText[DASH_T_INFO_RIGHT], 392, 104, 0.42f, (d.weightMax > 0 && d.weight > d.weightMax) ? red : dim, true);

    // Message log
    rect(0, 123, 400, 1, dim);
    float y = logTop;
    for (int i = 0; i < dashLogCount; i++) {
        float x = 8;
        if (dashLogFirst[i]) {
            C2D_DrawRectSolid(9 * kx, y + 6, 0.55f, 3 * kx, 3, dashLogNewest[i] ? green : dim);
        }
        x = 16;
        text(&dashLog[i], x, y, kDashLogScale, dashLogNewest[i] ? green : dim);
        y += logStep;
    }

    if (hint != nullptr) {
        rect(0, 224, 400, 16, panel);
        C2D_DrawText(&dashText[DASH_T_HINT], C2D_WithColor | C2D_AlignCenter, 200 * kx, 225, 0.6f, 0.42f * kx, 0.42f, amber);
    }

    C2D_Flush();
    gpuStateReady = false; // citro2d changed the GPU state
}


// ---------------------------------------------------------------------------
// Controls card: top screen outside a running game (main menu, char select)
// ---------------------------------------------------------------------------
static C2D_TextBuf helpTextBuf = nullptr;
static C2D_Text helpTitle, helpSub;
static C2D_Text helpKeys[2][9], helpActs[2][9];

void ctr_help_draw()
{
    static const char* const rows[2][9][2] = {
        {
            { "Tap", "Act on what you touch" },
            { "Hold", "Action menu" },
            { "Circle Pad", "Walk" },
            { "C-Stick", "Scroll map" },
            { "A / L", "Click / cursor mode" },
            { "B", "Use weapon or item" },
            { "R (hold)", "Show lootables" },
            { "X / Y", "Inventory / Pip-Boy" },
            { "D-Pad", "Hand/mode/skill/char" },
        },
        {
            { "ZR", "Combat / end turn" },
            { "Start+ZR", "End combat" },
            { "ZL", "Swap screens" },
            { "Select", "Center on player" },
            { "Select+A", "Quick heal" },
            { "Select+X / Y", "Automap / Rest" },
            { "Select+L", "Whole-screen view" },
            { "Select+Up/Dn", "Zoom" },
            { "Start+X / Y", "Quick save / load" },
        },
    };

    // Left-handed mode swaps the D-Pad and A/B/X/Y: the key column names what the player presses.
    static const char* const leftyKeys[2][9] = {
        { nullptr, nullptr, nullptr, nullptr, "Right / L", "Down", nullptr, "Left / Up", "A/B/X/Y" },
        { nullptr, nullptr, nullptr, nullptr, "Select+Right", "Select+Left/Up", nullptr, "Select+Y/B", "Start+Left/Up" },
    };
    static int helpLefty = -1;
    if (helpTextBuf != nullptr && helpLefty != (int)ctr_lefty()) {
        C2D_TextBufDelete(helpTextBuf);
        helpTextBuf = nullptr;
    }
    if (helpTextBuf == nullptr) {
        helpLefty = ctr_lefty();
        helpTextBuf = C2D_TextBufNew(2048);
        C2D_TextParse(&helpTitle, helpTextBuf, "FALLOUT 2");
        C2D_TextOptimize(&helpTitle);
        C2D_TextParse(&helpSub, helpTextBuf, "Select: manual");
        C2D_TextOptimize(&helpSub);
        for (int c = 0; c < 2; c++) {
            for (int r = 0; r < 9; r++) {
                const char* key = helpLefty && leftyKeys[c][r] ? leftyKeys[c][r] : rows[c][r][0];
                C2D_TextParse(&helpKeys[c][r], helpTextBuf, key);
                C2D_TextOptimize(&helpKeys[c][r]);
                C2D_TextParse(&helpActs[c][r], helpTextBuf, rows[c][r][1]);
                C2D_TextOptimize(&helpActs[c][r]);
            }
        }
    }

    const float kx = ctr_gfx.isWide ? 2.f : 1.f;
    const u32 bg = C2D_Color32(0x0b, 0x14, 0x0b, 0xff);
    const u32 green = C2D_Color32(0x5c, 0xff, 0x5c, 0xff);
    const u32 dim = C2D_Color32(0x3c, 0xb0, 0x3c, 0xff);
    const u32 amber = C2D_Color32(0xff, 0xc8, 0x40, 0xff);

    C2D_Prepare();
    C2D_SceneBegin(topRenderTarget);
    C2D_DrawRectSolid(0, 0, 0.5f, 400 * kx, 240, bg);

    C2D_DrawText(&helpTitle, C2D_WithColor, 8 * kx, 4, 0.6f, 0.8f * kx, 0.8f, amber);
    C2D_DrawText(&helpSub, C2D_WithColor | C2D_AlignRight, 392 * kx, 12, 0.6f, 0.5f * kx, 0.5f, dim);
    C2D_DrawRectSolid(0, 34, 0.5f, 400 * kx, 1, dim);

    for (int c = 0; c < 2; c++) {
        float x = c == 0 ? 6.f : 206.f;
        for (int r = 0; r < 9; r++) {
            float y = 42 + r * 21.5f;
            // shrink a long key name to its column so it never runs into the action text
            const float colW = (c == 0 ? 68.f : 80.f) - 4.f;
            float kw = 0, kh = 0;
            C2D_TextGetDimensions(&helpKeys[c][r], 0.42f, 0.42f, &kw, &kh);
            const float ks = kw > colW ? 0.42f * colW / kw : 0.42f;
            C2D_DrawText(&helpKeys[c][r], C2D_WithColor, x * kx, y, 0.6f, ks * kx, ks, green);
            C2D_DrawText(&helpActs[c][r], C2D_WithColor, (x + (c == 0 ? 68 : 80)) * kx, y, 0.6f, 0.42f * kx, 0.42f, dim);
        }
    }

    C2D_Flush();
    gpuStateReady = false; // citro2d changed the GPU state
}


// Touch screen during a movie: a card in the frame's middle instead of an
// empty black band (subtitles use that band when they are on).
static void ctr_movie_card_draw()
{
    static C2D_TextBuf buf = nullptr;
    static C2D_Text t1, t2, t3;
    if (buf == nullptr) {
        buf = C2D_TextBufNew(256);
        C2D_TextParse(&t1, buf, "Movie");
        C2D_TextParse(&t2, buf, "Tap the screen or press any");
        C2D_TextParse(&t3, buf, "button to skip it");
        C2D_TextOptimize(&t1);
        C2D_TextOptimize(&t2);
        C2D_TextOptimize(&t3);
    }
    C2D_Prepare();
    C2D_SceneBegin(bottomRenderTarget);
    C2D_DrawRectSolid(0, 58, 0.5f, 320, 124, C2D_Color32(0x16, 0x26, 0x16, 0xff));
    C2D_DrawRectSolid(0, 58, 0.5f, 320, 2, C2D_Color32(0x3c, 0xb0, 0x3c, 0xff));
    C2D_DrawRectSolid(0, 180, 0.5f, 320, 2, C2D_Color32(0x3c, 0xb0, 0x3c, 0xff));
    C2D_DrawText(&t1, C2D_WithColor | C2D_AlignCenter, 160, 78, 0.6f, 0.7f, 0.7f, C2D_Color32(0xff, 0xc8, 0x40, 0xff));
    C2D_DrawText(&t2, C2D_WithColor | C2D_AlignCenter, 160, 118, 0.6f, 0.5f, 0.5f, C2D_Color32(0x5c, 0xff, 0x5c, 0xff));
    C2D_DrawText(&t3, C2D_WithColor | C2D_AlignCenter, 160, 140, 0.6f, 0.5f, 0.5f, C2D_Color32(0x5c, 0xff, 0x5c, 0xff));
    C2D_Flush();
    gpuStateReady = false;
}


// Small one-line hint strip along the bottom of the top screen.
void ctr_hint_draw(const char* hint)
{
    static C2D_TextBuf hintBuf = nullptr;
    static C2D_Text hintTextObj;
    static const char* hintLast = nullptr;
    if (hintBuf == nullptr) {
        hintBuf = C2D_TextBufNew(256);
    }
    if (hint != hintLast) {
        C2D_TextBufClear(hintBuf);
        C2D_TextParse(&hintTextObj, hintBuf, hint);
        C2D_TextOptimize(&hintTextObj);
        hintLast = hint;
    }
    const float kx = ctr_gfx.isWide ? 2.f : 1.f;
    C2D_Prepare();
    C2D_SceneBegin(topRenderTarget);
    C2D_DrawRectSolid(0, 224, 0.5f, 400 * kx, 16, C2D_Color32(0, 0, 0, 0xc0));
    C2D_DrawText(&hintTextObj, C2D_WithColor | C2D_AlignCenter, 200 * kx, 225, 0.6f, 0.42f * kx, 0.42f, C2D_Color32(0xff, 0xc8, 0x40, 0xff));
    C2D_Flush();
    gpuStateReady = false;
}


void ctr_gfx_manual_frame(void (*drawTop)(), void (*drawBottom)())
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_Prepare();
    C2D_TargetClear(topRenderTarget, C2D_Color32(0, 0, 0, 0xff));
    C2D_SceneBegin(topRenderTarget);
    C2D_ViewReset();
    if (ctr_gfx.isWide) {
        C2D_ViewScale(2.f, 1.f); // the manual is laid out for 400 pixels
    }
    drawTop();
    C2D_ViewReset();
    C2D_TargetClear(bottomRenderTarget, C2D_Color32(0, 0, 0, 0xff));
    C2D_SceneBegin(bottomRenderTarget);
    drawBottom();
    C3D_FrameEnd(0);
    gpuStateReady = false; // citro2d changed the GPU state
}


// ---------------------------------------------------------------------------
// Top-screen cards: item details (inventory, loot, barter), barter summary,
// save/load slot, world map. Text is re-parsed only when the data changes.
// ---------------------------------------------------------------------------
static C2D_TextBuf cardBuf = nullptr;

struct CardLine {
    C2D_Text text;
    float x, y, scale;
    u32 color;
    u32 flags;
};
#define CARD_MAX_LINES 64
static CardLine cardLines[CARD_MAX_LINES];
static int cardLineCount = 0;
static unsigned int cardKey = 0;

static const u32 kCardBg = C2D_Color32(0x0b, 0x14, 0x0b, 0xff);
static const u32 kCardPanel = C2D_Color32(0x16, 0x26, 0x16, 0xff);
static const u32 kCardGreen = C2D_Color32(0x5c, 0xff, 0x5c, 0xff);
static const u32 kCardDim = C2D_Color32(0x8c, 0xd8, 0x8c, 0xff);
static const u32 kCardLine = C2D_Color32(0x3c, 0xb0, 0x3c, 0xff);
static const u32 kCardAmber = C2D_Color32(0xff, 0xc8, 0x40, 0xff);
static const u32 kCardRed = C2D_Color32(0xff, 0x50, 0x40, 0xff);

static void cardBegin()
{
    if (cardBuf == nullptr) {
        cardBuf = C2D_TextBufNew(6144);
    }
    if (dashMeasureBuf == nullptr) {
        dashMeasureBuf = C2D_TextBufNew(512);
    }
    C2D_TextBufClear(cardBuf);
    cardLineCount = 0;
}

static void cardAdd(const char* str, float x, float y, float scale, u32 color, u32 flags = 0)
{
    if (str == nullptr || str[0] == '\0' || cardLineCount >= CARD_MAX_LINES) {
        return;
    }
    CardLine* l = &cardLines[cardLineCount++];
    C2D_TextParse(&l->text, cardBuf, str);
    C2D_TextOptimize(&l->text);
    l->x = x;
    l->y = y;
    l->scale = scale;
    l->color = color;
    l->flags = flags;
}

// Adds a wrapped paragraph; returns the y below it.
static float cardParagraph(const char* str, float x, float y, float width, float bottom, u32 color)
{
    char line[160] = "";
    char trial[320];
    const char* p = str;
    while (*p != '\0' && y + 14.f <= bottom) {
        while (*p == ' ') p++;
        const char* end = p;
        while (*end != '\0' && *end != ' ') end++;
        if (end == p) break;
        snprintf(trial, sizeof(trial), "%s%s%.*s", line, line[0] != '\0' ? " " : "", (int)(end - p), p);
        if (line[0] != '\0' && dashMeasure(trial) > width) {
            cardAdd(line, x, y, kDashLogScale, color);
            y += 14.f;
            snprintf(line, sizeof(line), "%.*s", (int)(end - p), p);
        } else {
            snprintf(line, sizeof(line), "%s", trial);
        }
        p = end;
    }
    if (line[0] != '\0' && y + 14.f <= bottom) {
        cardAdd(line, x, y, kDashLogScale, color);
        y += 14.f;
    }
    return y;
}

static unsigned int cardHash(const void* data, size_t size, unsigned int seed)
{
    const unsigned char* p = (const unsigned char*)data;
    unsigned int h = seed ^ 2166136261u;
    for (size_t i = 0; i < size; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

static void cardFlush(bool background, float bgTop = 0)
{
    const float kx = ctr_gfx.isWide ? 2.f : 1.f;
    C2D_Prepare();
    C2D_SceneBegin(topRenderTarget);
    if (background) {
        C2D_DrawRectSolid(0, bgTop, 0.4f, 400 * kx, 240 - bgTop, kCardBg);
    }
    for (int i = 0; i < cardLineCount; i++) {
        CardLine* l = &cardLines[i];
        C2D_DrawText(&l->text, C2D_WithColor | l->flags, l->x * kx, l->y, 0.6f, l->scale * kx, l->scale, l->color);
    }
}

static void cardRect(float x, float y, float w, float h, u32 color)
{
    const float kx = ctr_gfx.isWide ? 2.f : 1.f;
    C2D_DrawRectSolid(x * kx, y, 0.5f, w * kx, h, color);
}

// Item rows in two columns starting at y; returns the y below them.
static float cardItemRows(const CtrItemCard* c, float y)
{
    int half = (c->rowCount + 1) / 2;
    for (int i = 0; i < c->rowCount; i++) {
        int col = i / half;
        int row = i % half;
        float x = col == 0 ? 8.f : 204.f;
        cardAdd(c->key[i], x, y + row * 15.f, 0.45f, kCardGreen);
        cardAdd(c->val[i], x + 62.f, y + row * 15.f, 0.45f, kCardDim);
    }
    return y + half * 15.f;
}

static void cardFooter(const CtrDashboard* d)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "HP %d/%d    Carry %d/%d lbs    $%d", d->hp, d->hpMax, d->weight, d->weightMax, d->caps);
    cardAdd(buf, 200, 225, 0.42f, (d->weightMax > 0 && d->weight > d->weightMax) ? kCardRed : kCardDim, C2D_AlignCenter);
}

void ctr_card_draw(bool barter)
{
    CtrDashboard d;
    ctrDashboardGather(&d);
    const CtrItemCard* c = ctrCardGet();
    const CtrBarter* b = ctrBarterGet();

    unsigned int key = cardHash(c, sizeof(*c), barter ? 1 : 2);
    key = cardHash(&d.hp, sizeof(d.hp), key);
    key = cardHash(&d.weight, sizeof(d.weight), key);
    key = cardHash(&d.caps, sizeof(d.caps), key);
    if (barter) {
        key = cardHash(b, sizeof(*b), key);
    }
    if (key != cardKey || cardBuf == nullptr) {
        cardKey = key;
        cardBegin();
        char buf[160];
        float y = 26.f;
        if (barter && b->active) {
            snprintf(buf, sizeof(buf), "Trading with %s", b->merchant);
            cardAdd(buf, 8, 2, 0.56f, kCardAmber);
            snprintf(buf, sizeof(buf), "Your offer  $%d", b->offer);
            cardAdd(buf, 8, 26, 0.5f, kCardGreen);
            snprintf(buf, sizeof(buf), "They want  $%d", b->ask);
            cardAdd(buf, 392, 26, 0.5f, kCardGreen, C2D_AlignRight);
            if (b->offer == 0) {
                cardAdd("Drag items onto your table to make an offer, then tap Offer", 200, 44, 0.42f, kCardDim, C2D_AlignCenter);
            } else if (b->offer >= b->ask) {
                cardAdd("They will accept this trade: tap Offer", 200, 44, 0.45f, kCardGreen, C2D_AlignCenter);
            } else {
                snprintf(buf, sizeof(buf), "Not enough: add $%d more", b->ask - b->offer);
                cardAdd(buf, 200, 44, 0.45f, kCardRed, C2D_AlignCenter);
            }
            snprintf(buf, sizeof(buf), "Their caps $%d", b->theirCaps);
            cardAdd(buf, 392, 2 + 4, 0.42f, kCardDim, C2D_AlignRight);
            y = 66.f;
        }
        if (c->valid) {
            float titleY = barter ? y : 2.f;
            float scale = barter ? 0.5f : 0.58f;
            if (c->quantity > 1) {
                snprintf(buf, sizeof(buf), "%s  x%d", c->name, c->quantity);
            } else {
                snprintf(buf, sizeof(buf), "%s", c->name);
            }
            cardAdd(buf, 8, titleY, scale, barter ? kCardGreen : kCardAmber);
            cardAdd(c->type, 392, titleY + 3, 0.45f, kCardLine, C2D_AlignRight);
            y = barter ? y + 18.f : 26.f;
            y = cardItemRows(c, y) + 4.f;
            cardParagraph(c->desc, 8, y, 384, 222, kCardDim);
        } else if (!barter) {
            cardAdd("Inventory", 8, 2, 0.58f, kCardAmber);
            cardAdd("Touch an item to see its details here.", 200, 110, 0.5f, kCardDim, C2D_AlignCenter);
        } else {
            cardAdd("Touch an item to see its details here.", 200, 130, 0.45f, kCardDim, C2D_AlignCenter);
        }
        cardFooter(&d);
    }

    cardFlush(true);
    cardRect(0, 22, 400, 1, kCardLine);
    if (barter && b->active) {
        cardRect(0, 62, 400, 1, kCardLine);
    }
    cardRect(0, 223, 400, 1, kCardLine);
    C2D_Flush();
    gpuStateReady = false;
}

void ctr_loadsave_draw()
{
    CtrSlotInfo s;
    if (!ctrLoadSaveGetSlot(&s) || !s.valid) {
        return;
    }
    unsigned int key = cardHash(&s, sizeof(s), 3);
    if (key != cardKey || cardBuf == nullptr) {
        cardKey = key;
        cardBegin();
        char buf[96];
        snprintf(buf, sizeof(buf), "%s - slot %d", s.saving ? "Save game" : "Load game", s.slot);
        cardAdd(buf, 8, 2, 0.56f, kCardAmber);
        float x = 240.f;
        if (s.empty) {
            cardAdd("Empty slot", x, 30, 0.5f, kCardGreen);
            cardAdd(s.saving ? "Tap Done to save here." : "Nothing saved here.", x, 50, 0.42f, kCardDim);
        } else {
            cardAdd(s.name, x, 30, 0.5f, kCardGreen);
            cardAdd(s.date, x, 50, 0.42f, kCardDim);
            float y = cardParagraph(s.place, x, 68, 152, 160, kCardDim);
            (void)y;
            cardAdd("Description", 8, 166, 0.42f, kCardLine);
            cardParagraph(s.desc, 8, 182, 384, 222, kCardGreen);
        }
        cardAdd(s.saving ? "Tap a slot, then Done" : "Tap a slot, then Done to load it", 200, 225, 0.42f, kCardDim, C2D_AlignCenter);
    }
    // The preview picture is drawn by the normal top-screen pass at 1:1; only
    // the area around it is painted here.
    const float kx = ctr_gfx.isWide ? 2.f : 1.f;
    C2D_Prepare();
    C2D_SceneBegin(topRenderTarget);
    C2D_DrawRectSolid(0, 0, 0.4f, 400 * kx, 24, kCardBg);
    C2D_DrawRectSolid(0, 24, 0.4f, 8 * kx, 136, kCardBg);
    C2D_DrawRectSolid(232 * kx, 24, 0.4f, 168 * kx, 136, kCardBg);
    C2D_DrawRectSolid(0, 160, 0.4f, 400 * kx, 80, kCardBg);
    cardFlush(false);
    cardRect(0, 22, 400, 1, kCardLine);
    cardRect(0, 223, 400, 1, kCardLine);
    C2D_Flush();
    gpuStateReady = false;
}

void ctr_worldmap_draw()
{
    CtrWorldInfo w;
    if (!ctrWorldmapGetInfo(&w) || !w.valid) {
        return;
    }
    CtrDashboard d;
    ctrDashboardGather(&d);
    unsigned int key = cardHash(&w, sizeof(w), 4);
    key = cardHash(d.time, sizeof(d.time), key);
    key = cardHash(&d.hp, sizeof(d.hp), key);
    if (key != cardKey || cardBuf == nullptr) {
        cardKey = key;
        cardBegin();
        char buf[128];
        cardAdd("World map", 8, 2, 0.56f, kCardAmber);
        cardAdd(d.time, 392, 5, 0.45f, kCardDim, C2D_AlignRight);
        snprintf(buf, sizeof(buf), "Location: %s", w.here);
        cardAdd(buf, 8, 28, 0.5f, kCardGreen);
        if (w.terrain[0] != '\0') {
            snprintf(buf, sizeof(buf), "Terrain: %s", w.terrain);
            cardAdd(buf, 8, 46, 0.45f, kCardDim);
        }
        if (w.moving) {
            snprintf(buf, sizeof(buf), "Heading for: %s", w.dest);
            cardAdd(buf, 8, 64, 0.5f, kCardAmber);
        } else {
            cardAdd("Tap the map, or a place on the right, to travel.", 8, 64, 0.45f, kCardDim);
        }
        if (w.fuelPercent >= 0) {
            snprintf(buf, sizeof(buf), "Car fuel: %d%%", w.fuelPercent);
            cardAdd(buf, 392, 46, 0.45f, w.fuelPercent < 20 ? kCardRed : kCardDim, C2D_AlignRight);
        }
        snprintf(buf, sizeof(buf), "HP %d/%d", d.hp, d.hpMax);
        cardAdd(buf, 392, 28, 0.45f, d.hpMax > 0 && d.hp * 2 < d.hpMax ? kCardRed : kCardDim, C2D_AlignRight);
        cardAdd("Known places", 8, 88, 0.45f, kCardLine);
        for (int i = 0; i < w.knownCount; i++) {
            int col = i % 3;
            int row = i / 3;
            float yy = 104 + row * 15.f;
            if (yy > 208) break;
            cardAdd(w.known[i], 8 + col * 130.f, yy, 0.42f, kCardDim);
        }
        cardAdd("Encounters can stop you on the way. Tap a town circle to enter it.", 200, 225, 0.4f, kCardDim, C2D_AlignCenter);
    }
    cardFlush(true);
    cardRect(0, 22, 400, 1, kCardLine);
    cardRect(0, 84, 400, 1, kCardLine);
    cardRect(0, 223, 400, 1, kCardLine);
    C2D_Flush();
    gpuStateReady = false;
}

} // namespace fallout
