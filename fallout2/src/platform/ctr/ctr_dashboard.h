#ifndef FALLOUT_PLATFORM_CTR_DASHBOARD_H
#define FALLOUT_PLATFORM_CTR_DASHBOARD_H

// Top-screen information dashboard (3DS). The game side fills a plain
// snapshot; the renderer (ctr_gfx.cc) draws it with citro2d so text is
// crisp and readable instead of a shrunken copy of the interface bar.

namespace fallout {

#define CTR_DASH_LINES 7
#define CTR_DASH_TEXT 80

struct CtrDashboard {
    bool valid;
    char name[40];
    int level;
    int xp;
    int xpNext;
    int hp;
    int hpMax;
    int ac;
    bool inCombat;
    int ap;
    int apMax;
    int apBonus;
    char weapon[48];
    char attack[48];
    bool hasAmmo;
    int ammo;
    int ammoMax;
    bool hasTarget;
    char target[48];
    char targetState[24];
    int targetHit;
    int poison;
    int rads;
    char time[48];
    unsigned int logVersion;
    int lineCount;
    char lines[CTR_DASH_LINES][CTR_DASH_TEXT];
};

// Game side (dashboard_3ds.cc)
void ctrDashboardGather(CtrDashboard* dash);
void ctrDashboardSetTarget(void* critter);

// Renderer (ctr_gfx.cc)
void ctr_dashboard_draw();

} // namespace fallout

#endif /* FALLOUT_PLATFORM_CTR_DASHBOARD_H */
