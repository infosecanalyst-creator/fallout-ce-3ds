#ifndef FALLOUT_PLATFORM_CTR_GFX_H_
#define FALLOUT_PLATFORM_CTR_GFX_H_

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>

#include "plib/gnw/svga.h"

#define DISPLAY_TRANSFER_FLAGS                                                                     \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |               \
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |   \
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define TEXTURE_TRANSFER_FLAGS                                                                     \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |               \
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |   \
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

namespace fallout {

typedef struct {
    bool overlayEnable;
    bool overlayEnableExtra;
    bool isWide;
} ctr_gfx_t;
extern ctr_gfx_t ctr_gfx;

void ctr_gfx_draw(SDL_Surface* gSdlSurface);

void ctr_gfx_reinit(void);
void ctr_gfx_toggle_wide(void);
void ctr_gfx_init(void);
void ctr_gfx_exit(void);

// One frame of a full-screen citro2d overlay (the manual) on both screens.
void ctr_gfx_manual_frame(void (*drawTop)(), void (*drawBottom)());

// ZL outside the map: top screen shows a sharp full-size view of this screen
void ctr_toggle_top_lens();
bool ctr_top_lens_active();

} // namespace fallout

#endif /* FALLOUT_PLATFORM_CTR_GFX_H_ */
