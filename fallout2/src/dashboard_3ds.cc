// 3DS top-screen dashboard: collects what the player needs to read while
// the map is on the touchscreen. Rendered by platform/ctr/ctr_gfx.cc.
#ifdef __3DS__

#include <stdio.h>
#include <string.h>

#include "platform/ctr/ctr_dashboard.h"

#include "combat.h"
#include "combat_defs.h"
#include "critter.h"
#include "display_monitor.h"
#include "interface.h"
#include "item.h"
#include "inventory.h"
#include "map.h"
#include "object.h"
#include "proto_types.h"
#include "scripts.h"
#include "stat.h"
#include "stat_defs.h"
#include "tile.h"

namespace fallout {

static bool gDashInGame = false;

void ctrDashboardSetInGame(bool inGame)
{
    gDashInGame = inGame;
}

static Object* gDashTarget = nullptr;

// True while a game is running (player exists and the interface bar is up).
bool ctrDashboardAvailable()
{
    Object* item;
    return gDashInGame && gDude != nullptr && interfaceGetActiveItem(&item) == 0;
}

void ctrDashboardSetTarget(void* critter)
{
    gDashTarget = (Object*)critter;
}

// Object pointers are only trusted while the object is still on this map.
static bool dashObjectOnMap(Object* target)
{
    if (target == NULL) {
        return false;
    }
    struct Find {
        Object* target;
        bool found;
    } find = { target, false };
    ctr_obj_for_each(gElevation, [](Object* obj, void* ctx) {
        Find* f = (Find*)ctx;
        if (obj == f->target) {
            f->found = true;
            return false;
        }
        return true;
    }, &find);
    return find.found;
}

static bool dashIsLiveCritter(Object* obj)
{
    return obj != NULL && obj != gDude && PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER && !critterIsDead(obj);
}

// The enemy to show on the dashboard: the last one tapped while it lives,
// otherwise whoever last attacked the player, otherwise the nearest critter
// that is fighting the player. Searched a few times a second, not per frame.
static Object* dashCurrentTarget()
{
    static int lastMap = -1;
    static int lastElevation = -1;
    static Object* autoTarget = NULL;
    static int searchCountdown = 0;

    int map = mapGetCurrentMap();
    if (map != lastMap || gElevation != lastElevation) {
        gDashTarget = NULL;
        autoTarget = NULL;
        lastMap = map;
        lastElevation = gElevation;
    }

    // Walking every tile list costs time, so the pointers are re-verified a few
    // times a second; objects are only freed on map changes (handled above)
    // or by scripts.
    static int verifyCountdown = 0;
    bool verify = --verifyCountdown <= 0;
    if (verify) {
        verifyCountdown = 15;
    }
    if (gDashTarget != NULL && ((verify && !dashObjectOnMap(gDashTarget)) || critterIsDead(gDashTarget))) {
        gDashTarget = NULL;
    }
    if (gDashTarget != NULL) {
        return gDashTarget;
    }

    if (autoTarget != NULL && ((verify && !dashObjectOnMap(autoTarget)) || critterIsDead(autoTarget))) {
        autoTarget = NULL;
        searchCountdown = 0;
    }
    if (--searchCountdown > 0 && autoTarget != NULL) {
        return autoTarget;
    }
    searchCountdown = 20;

    Object* attacker = gDude->data.critter.combat.whoHitMe;
    if (dashObjectOnMap(attacker) && dashIsLiveCritter(attacker)) {
        autoTarget = attacker;
        return autoTarget;
    }

    struct Nearest {
        Object* best;
        int bestDist;
    } nearest = { NULL, 0 };
    ctr_obj_for_each(gElevation, [](Object* obj, void* ctx) {
        Nearest* n = (Nearest*)ctx;
        if (dashIsLiveCritter(obj) && obj->data.critter.combat.whoHitMe == gDude) {
            int dist = tileDistanceBetween(gDude->tile, obj->tile);
            if (n->best == NULL || dist < n->bestDist) {
                n->best = obj;
                n->bestDist = dist;
            }
        }
        return true;
    }, &nearest);
    autoTarget = nearest.best;
    return autoTarget;
}

static const char* attackLabel(Object* weapon, int hitMode)
{
    if (hitMode == HIT_MODE_LEFT_WEAPON_RELOAD || hitMode == HIT_MODE_RIGHT_WEAPON_RELOAD) {
        return "Reload";
    }
    if (hitMode == HIT_MODE_PUNCH || (hitMode >= FIRST_ADVANCED_PUNCH_HIT_MODE && hitMode <= LAST_ADVANCED_PUNCH_HIT_MODE)) {
        return "Punch";
    }
    if (hitMode == HIT_MODE_KICK || (hitMode >= FIRST_ADVANCED_KICK_HIT_MODE && hitMode <= LAST_ADVANCED_KICK_HIT_MODE)) {
        return "Kick";
    }
    bool secondary = hitMode == HIT_MODE_LEFT_WEAPON_SECONDARY || hitMode == HIT_MODE_RIGHT_WEAPON_SECONDARY;
    switch (weaponGetAttackTypeForHitMode(weapon, hitMode)) {
    case ATTACK_TYPE_RANGED:
        return secondary ? "Burst" : "Single shot";
    case ATTACK_TYPE_THROW:
        return "Throw";
    case ATTACK_TYPE_MELEE:
        return secondary ? "Thrust" : "Swing";
    case ATTACK_TYPE_UNARMED:
        return "Unarmed";
    }
    return "Use";
}

static const char* healthWord(int hp, int hpMax)
{
    if (hp <= 0) return "Dead";
    if (hpMax <= 0) return "";
    int pct = hp * 100 / hpMax;
    if (pct >= 100) return "Unhurt";
    if (pct > 75) return "Barely hurt";
    if (pct > 50) return "Wounded";
    if (pct > 25) return "Badly wounded";
    return "Almost dead";
}

void ctrDashboardGather(CtrDashboard* d)
{
    memset(d, 0, sizeof(*d));
    if (gDude == nullptr) {
        return;
    }
    d->valid = true;

    snprintf(d->name, sizeof(d->name), "%s", critterGetName(gDude));
    d->level = pcGetStat(PC_STAT_LEVEL);
    d->xp = pcGetStat(PC_STAT_EXPERIENCE);
    d->xpNext = pcGetExperienceForLevel(d->level + 1);
    d->hp = critterGetHitPoints(gDude);
    d->hpMax = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);
    d->ac = critterGetStat(gDude, STAT_ARMOR_CLASS);
    d->apMax = critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS);
    d->inCombat = isInCombat();
    d->ap = d->inCombat ? gDude->data.critter.combat.ap : d->apMax;
    d->apBonus = d->inCombat ? _combat_free_move : 0;
    d->poison = critterGetPoison(gDude);
    d->rads = critterGetRadiation(gDude);

    int month, day, year;
    gameTimeGetDate(&month, &day, &year);
    static const char* months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
    snprintf(d->time, sizeof(d->time), "%02d %s %d  %s", day, months[(month + 11) % 12], year, gameTimeGetTimeString());

    // Active hand: weapon / item, attack mode, AP cost, ammo
    Object* item = nullptr;
    int hitMode = HIT_MODE_PUNCH;
    bool aiming = false;
    if (interfaceGetActiveItem(&item) != 0 || item == (Object*)-1) {
        item = NULL; // interface not ready (e.g. while a map loads)
    }
    // The interface caches the hand pointer; while the inventory is open the
    // item can be used up or merged into a stack and freed. Only trust it if
    // it is still in one of the player's hands.
    if (item != NULL && item != critterGetItem2(gDude) && item != critterGetItem1(gDude)) {
        item = NULL;
    }
    if (interfaceGetCurrentHitMode(&hitMode, &aiming) != 0) {
        hitMode = HIT_MODE_PUNCH;
        aiming = false;
    }
    if (item != nullptr) {
        snprintf(d->weapon, sizeof(d->weapon), "%s", objectGetName(item));
        if (itemGetType(item) == ITEM_TYPE_WEAPON && ammoGetCapacity(item) > 0) {
            d->hasAmmo = true;
            d->ammo = ammoGetQuantity(item);
            d->ammoMax = ammoGetCapacity(item);
        }
    } else {
        snprintf(d->weapon, sizeof(d->weapon), "Unarmed");
    }
    int apCost = weaponGetActionPointCost(gDude, hitMode, aiming);
    snprintf(d->attack, sizeof(d->attack), "%s%s  %d AP", attackLabel(item, hitMode), aiming ? " (aimed)" : "", apCost);

    // Combat: whose turn, and the target
    if (d->inCombat) {
        d->turn = _combat_whose_turn() == gDude ? 1 : 2;
        Object* target = dashCurrentTarget();
        if (target != NULL) {
            int thp = critterGetHitPoints(target);
            int thpMax = critterGetStat(target, STAT_MAXIMUM_HIT_POINTS);
            d->hasTarget = true;
            snprintf(d->target, sizeof(d->target), "%s", objectGetName(target));
            snprintf(d->targetState, sizeof(d->targetState), "%s", healthWord(thp, thpMax));
            // The to-hit roll traces the line of fire: only recompute when
            // something it depends on changes, or a few times a second.
            static Object* hitTarget = NULL;
            static int hitModeLast = -1;
            static int hitTileLast = -1;
            static int hitTargetTileLast = -1;
            static int hitValue = 0;
            static int hitCountdown = 0;
            if (target != hitTarget || hitMode != hitModeLast || gDude->tile != hitTileLast
                || target->tile != hitTargetTileLast || --hitCountdown <= 0) {
                hitValue = thp > 0 ? _determine_to_hit(gDude, target, HIT_LOCATION_UNCALLED, hitMode) : 0;
                hitTarget = target;
                hitModeLast = hitMode;
                hitTileLast = gDude->tile;
                hitTargetTileLast = target->tile;
                hitCountdown = 15;
            }
            d->targetHit = thp > 0 ? hitValue : 0;
        }
    }

    // The other hand, money, load, sneaking
    Object* other = (interfaceGetCurrentHand() == HAND_RIGHT) ? critterGetItem1(gDude) : critterGetItem2(gDude);
    snprintf(d->otherHand, sizeof(d->otherHand), "%s", other != NULL ? objectGetName(other) : "Unarmed");
    static int capsCache = 0;
    static int weightCache = 0;
    static int inventoryCountdown = 0;
    if (--inventoryCountdown <= 0) {
        capsCache = itemGetTotalCaps(gDude);
        weightCache = objectGetInventoryWeight(gDude);
        inventoryCountdown = 15;
    }
    d->caps = capsCache;
    d->weight = weightCache;
    d->weightMax = critterGetStat(gDude, STAT_CARRY_WEIGHT);
    d->sneaking = dudeHasState(DUDE_STATE_SNEAKING);

    // Message log: the monitor stores lines wrapped for the narrow PC
    // interface; join them back into whole messages (each starts with the
    // 0x95 bullet) so the top screen can wrap them at its own width.
    const char* lines[24];
    int lineCount = displayMonitorGetRecentLines(lines, 24, &d->logVersion);
    char joined[CTR_DASH_MSGS * 2][CTR_DASH_MSG_TEXT];
    int count = 0;
    for (int i = 0; i < lineCount; i++) {
        const unsigned char* src = (const unsigned char*)lines[i];
        bool start = *src == 0x95 || count == 0;
        if (*src == 0x95) {
            src++;
        }
        if (start) {
            if (count == CTR_DASH_MSGS * 2) {
                memmove(joined[0], joined[1], sizeof(joined[0]) * (CTR_DASH_MSGS * 2 - 1));
                count--;
            }
            joined[count++][0] = '\0';
        }
        char* dst = joined[count - 1];
        size_t n = strlen(dst);
        if (!start && n > 0 && n < CTR_DASH_MSG_TEXT - 1) {
            dst[n++] = ' ';
        }
        while (*src == ' ') {
            src++;
        }
        while (*src != '\0' && n < CTR_DASH_MSG_TEXT - 1) {
            unsigned char c = *src++;
            dst[n++] = (c < 0x80) ? (char)c : '?';
        }
        dst[n] = '\0';
    }
    int first = count > CTR_DASH_MSGS ? count - CTR_DASH_MSGS : 0;
    d->msgCount = count - first;
    for (int i = 0; i < d->msgCount; i++) {
        memcpy(d->msgs[i], joined[first + i], CTR_DASH_MSG_TEXT);
    }
}

} // namespace fallout

#endif
