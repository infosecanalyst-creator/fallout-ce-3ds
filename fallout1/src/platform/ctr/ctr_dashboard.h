#ifndef FALLOUT_PLATFORM_CTR_DASHBOARD_H
#define FALLOUT_PLATFORM_CTR_DASHBOARD_H

// Top-screen information dashboard (3DS). The game side fills a plain
// snapshot; the renderer (ctr_gfx.cc) draws it with citro2d so text is
// crisp and readable instead of a shrunken copy of the interface bar.

namespace fallout {

#define CTR_DASH_MSGS 8
#define CTR_DASH_MSG_TEXT 160

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
    int turn; // 0 = not in combat, 1 = your turn, 2 = someone else's
    char otherHand[48];
    int caps;
    int weight;
    int weightMax;
    bool sneaking;
    unsigned int logVersion;
    int msgCount; // whole messages (the log's own line wrapping undone)
    char msgs[CTR_DASH_MSGS][CTR_DASH_MSG_TEXT];
};

// Game side (dashboard_3ds.cc)
void ctrDashboardGather(CtrDashboard* dash);
bool ctrDashboardAvailable();
void ctrDashboardSetInGame(bool inGame);
void ctrDashboardSetTarget(void* critter);

// Item card: details of the last item touched in the inventory, loot or
// barter screens (filled from the item when it is touched, so it never keeps
// a pointer to an object that may be freed later).
#define CTR_CARD_ROWS 8
struct CtrItemCard {
    bool valid;
    unsigned int version;
    char name[48];
    char type[24];
    int quantity;
    int rowCount;
    char key[CTR_CARD_ROWS][16];
    char val[CTR_CARD_ROWS][48];
    char desc[512];
};
void ctrCardSetItem(void* item, int quantity);
void ctrCardClear();
const CtrItemCard* ctrCardGet();

// Barter summary, updated by the barter screen every frame while it is open.
struct CtrBarter {
    bool active;
    char merchant[40];
    int offer; // value of what the player put on the table
    int ask; // what the merchant wants for their side, after barter skills
    int yourCaps;
    int theirCaps;
};
void ctrBarterSet(const CtrBarter* barter);
void ctrBarterEnd();
const CtrBarter* ctrBarterGet();

// Save/load: the slot under the cursor (implemented in loadsave.cc).
struct CtrSlotInfo {
    bool valid;
    bool empty;
    bool saving;
    int slot;
    char name[32];
    char desc[32];
    char date[40];
    char place[64];
};
bool ctrLoadSaveGetSlot(CtrSlotInfo* info);

// World map (implemented in worldmap.cc).
#define CTR_WORLD_KNOWN 24
struct CtrWorldInfo {
    bool valid;
    bool moving;
    char here[48];
    char dest[48];
    char terrain[32];
    int fuelPercent; // -1 = no car
    int knownCount;
    char known[CTR_WORLD_KNOWN][28];
};
bool ctrWorldmapGetInfo(CtrWorldInfo* info);

// Renderers (ctr_gfx.cc)
void ctr_card_draw(bool barter);
void ctr_loadsave_draw();
void ctr_worldmap_draw();

// Renderer (ctr_gfx.cc)
void ctr_dashboard_draw();
void ctr_help_draw();
void ctr_hint_draw(const char* hint);

} // namespace fallout

#endif /* FALLOUT_PLATFORM_CTR_DASHBOARD_H */
