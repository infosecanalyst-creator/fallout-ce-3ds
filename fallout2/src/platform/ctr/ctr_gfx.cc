#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include "ctr_input.h"
#include "ctr_gfx.h"
#include "ctr_dashboard.h"
#include "ctr_rectmap.h"
#include "ctr_sys.h"

#include "vshader_shbin.h"
#include "frame_tex.h"

#include "window_manager.h"
#include "memory.h"

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

void drawRects()
{
    bool dashOnTop = false;
    bool helpOnTop = false;
    const char* hintText = nullptr;

    beginRender(ctr_rectMap.active == DISPLAY_MOVIE);
    drawTopRenderTarget(0x000000);

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

        case DISPLAY_DIALOG:
        case DISPLAY_INVENTORY_TRADE:
            for (int i = 0; i < numRectsInMap[DISPLAY_DIALOG_TOP]; i++) {
                drawRect(rectMaps[DISPLAY_DIALOG_TOP][i]->src_x, rectMaps[DISPLAY_DIALOG_TOP][i]->src_y,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->src_w, rectMaps[DISPLAY_DIALOG_TOP][i]->src_h,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->dst_x, rectMaps[DISPLAY_DIALOG_TOP][i]->dst_y,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->dst_w, rectMaps[DISPLAY_DIALOG_TOP][i]->dst_h
                );
            }
            break;

        case DISPLAY_LOADSAVE:
            for (int i = 0; i < numRectsInMap[DISPLAY_LOADSAVE_TOP]; i++) {
                drawRect(rectMaps[DISPLAY_LOADSAVE_TOP][i]->src_x, rectMaps[DISPLAY_LOADSAVE_TOP][i]->src_y,
                        rectMaps[DISPLAY_LOADSAVE_TOP][i]->src_w, rectMaps[DISPLAY_LOADSAVE_TOP][i]->src_h,
                        rectMaps[DISPLAY_LOADSAVE_TOP][i]->dst_x, rectMaps[DISPLAY_LOADSAVE_TOP][i]->dst_y,
                        rectMaps[DISPLAY_LOADSAVE_TOP][i]->dst_w, rectMaps[DISPLAY_LOADSAVE_TOP][i]->dst_h
                );
            }
            break;

        case DISPLAY_FIELD:
        case DISPLAY_INVENTORY:
        case DISPLAY_INVENTORY_USE:
        case DISPLAY_INVENTORY_LOOT:
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

        case DISPLAY_OPTIONS:
        case DISPLAY_WORLDMAP:
        case DISPLAY_PIPBOY:
        case DISPLAY_FULL:
            if (ctr_input.mode != DISPLAY_MODE_ADAPT) {
                // Whole-screen mode (Select+L): top screen is a zoom lens.
                drawRect(offsetX, (240 - offsetY), 400, 240, 0,   0, 400, 240);
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
    }
    if (hintText != nullptr) {
        ctr_hint_draw(hintText);
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

    // 800px top-screen mode is off: the dashboard and every top-screen
    // layout are drawn for 400x240, identical on hardware and in emulators.
    if (false && is2DS == 1) {
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
static CtrDashboard dashPrev;
static bool dashHavePrev = false;

enum {
    DASH_T_NAME, DASH_T_TIME, DASH_T_HP, DASH_T_AC, DASH_T_AP, DASH_T_WEAPON,
    DASH_T_AMMO, DASH_T_ATTACK, DASH_T_INFO, DASH_T_HP_LABEL, DASH_T_AP_LABEL,
    DASH_T_COUNT
};
static C2D_Text dashText[DASH_T_COUNT];
static C2D_Text dashLines[CTR_DASH_LINES];
static int dashLineCount = 0;

static void dashParse(C2D_Text* t, const char* str)
{
    C2D_TextParse(t, dashTextBuf, str);
    C2D_TextOptimize(t);
}

static void dashRebuild(const CtrDashboard* d)
{
    char buf[128];
    C2D_TextBufClear(dashTextBuf);

    dashParse(&dashText[DASH_T_NAME], d->name);
    dashParse(&dashText[DASH_T_TIME], d->time);
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
        snprintf(buf, sizeof(buf), "Ammo %d / %d", d->ammo, d->ammoMax);
    } else {
        buf[0] = '\0';
    }
    dashParse(&dashText[DASH_T_AMMO], buf);
    dashParse(&dashText[DASH_T_ATTACK], d->attack);

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
    dashParse(&dashText[DASH_T_HP_LABEL], "HP");
    dashParse(&dashText[DASH_T_AP_LABEL], "AP");

    dashLineCount = d->lineCount;
    for (int i = 0; i < d->lineCount; i++) {
        dashParse(&dashLines[i], d->lines[i]);
    }
}

void ctr_dashboard_draw()
{
    if (dashTextBuf == nullptr) {
        dashTextBuf = C2D_TextBufNew(4096);
    }

    CtrDashboard d;
    ctrDashboardGather(&d);
    if (!d.valid) {
        return;
    }
    if (!dashHavePrev || memcmp(&d, &dashPrev, sizeof(d)) != 0) {
        dashRebuild(&d);
        dashPrev = d;
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

    // Header: name + date/time
    text(&dashText[DASH_T_NAME], 8, 2, 0.6f, green);
    text(&dashText[DASH_T_TIME], 392, 5, 0.45f, dim, true);

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

    // Weapon / attack / ammo
    text(&dashText[DASH_T_WEAPON], 8, 67, 0.52f, green);
    text(&dashText[DASH_T_AMMO], 392, 69, 0.45f, (d.hasAmmo && d.ammo == 0) ? red : green, true);
    text(&dashText[DASH_T_ATTACK], 8, 85, 0.47f, dim);

    // Target (combat) or level/status
    text(&dashText[DASH_T_INFO], 8, 103, 0.47f, d.hasTarget ? amber : dim);

    // Message log
    rect(0, 123, 400, 1, dim);
    float y = 127;
    for (int i = 0; i < dashLineCount; i++) {
        text(&dashLines[i], 8, y, 0.45f, i == dashLineCount - 1 ? green : dim);
        y += 15.5f;
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

    if (helpTextBuf == nullptr) {
        helpTextBuf = C2D_TextBufNew(2048);
        C2D_TextParse(&helpTitle, helpTextBuf, "FALLOUT 2");
        C2D_TextOptimize(&helpTitle);
        C2D_TextParse(&helpSub, helpTextBuf, "Select: manual");
        C2D_TextOptimize(&helpSub);
        for (int c = 0; c < 2; c++) {
            for (int r = 0; r < 9; r++) {
                C2D_TextParse(&helpKeys[c][r], helpTextBuf, rows[c][r][0]);
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
            C2D_DrawText(&helpKeys[c][r], C2D_WithColor, x * kx, y, 0.6f, 0.42f * kx, 0.42f, green);
            C2D_DrawText(&helpActs[c][r], C2D_WithColor, (x + (c == 0 ? 68 : 80)) * kx, y, 0.6f, 0.42f * kx, 0.42f, dim);
        }
    }

    C2D_Flush();
    gpuStateReady = false; // citro2d changed the GPU state
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
    drawTop();
    C2D_TargetClear(bottomRenderTarget, C2D_Color32(0, 0, 0, 0xff));
    C2D_SceneBegin(bottomRenderTarget);
    drawBottom();
    C3D_FrameEnd(0);
    gpuStateReady = false; // citro2d changed the GPU state
}

} // namespace fallout
