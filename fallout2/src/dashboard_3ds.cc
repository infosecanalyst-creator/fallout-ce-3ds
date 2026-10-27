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
#include "map.h"
#include "object.h"
#include "proto_types.h"
#include "scripts.h"
#include "stat.h"
#include "stat_defs.h"

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

// The target pointer is only trusted while the object is still on the map.
static bool dashTargetStillExists()
{
    if (gDashTarget == nullptr) {
        return false;
    }
    for (Object* obj = objectFindFirstAtElevation(gElevation); obj != nullptr; obj = objectFindNextAtElevation()) {
        if (obj == gDashTarget) {
            return true;
        }
    }
    gDashTarget = nullptr;
    return false;
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
        item = nullptr; // interface not ready (e.g. while a map loads)
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

    // Combat target: last enemy tapped
    if (d->inCombat && dashTargetStillExists()) {
        int thp = critterGetHitPoints(gDashTarget);
        int thpMax = critterGetStat(gDashTarget, STAT_MAXIMUM_HIT_POINTS);
        d->hasTarget = true;
        snprintf(d->target, sizeof(d->target), "%s", objectGetName(gDashTarget));
        snprintf(d->targetState, sizeof(d->targetState), "%s", healthWord(thp, thpMax));
        d->targetHit = thp > 0 ? _determine_to_hit(gDude, gDashTarget, HIT_LOCATION_UNCALLED, hitMode) : 0;
    }

    // Message log
    const char* lines[CTR_DASH_LINES];
    d->lineCount = displayMonitorGetRecentLines(lines, CTR_DASH_LINES, &d->logVersion);
    for (int i = 0; i < d->lineCount; i++) {
        // The interface font uses 0x95 as its bullet; show a plain one.
        const char* src = lines[i];
        char* dst = d->lines[i];
        int n = 0;
        if ((unsigned char)*src == 0x95) {
            dst[n++] = '>';
            dst[n++] = ' ';
            src++;
        }
        while (*src != '\0' && n < CTR_DASH_TEXT - 1) {
            unsigned char c = (unsigned char)*src++;
            dst[n++] = (c < 0x80) ? (char)c : '?';
        }
        dst[n] = '\0';
    }
}

} // namespace fallout

#endif
