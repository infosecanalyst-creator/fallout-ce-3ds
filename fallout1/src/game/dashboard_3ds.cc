// 3DS top-screen dashboard: collects what the player needs to read while
// the map is on the touchscreen. Rendered by platform/ctr/ctr_gfx.cc.
#ifdef __3DS__

#include <stdio.h>
#include <string.h>

#include "platform/ctr/ctr_dashboard.h"

#include "game/combat.h"
#include "game/combat_defs.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/map.h"
#include "game/object.h"
#include "game/proto_types.h"
#include "game/scripts.h"
#include "game/stat.h"
#include "game/stat_defs.h"

namespace fallout {

static Object* gDashTarget = NULL;

void ctrDashboardSetTarget(void* critter)
{
    gDashTarget = (Object*)critter;
}

// The target pointer is only trusted while the object is still on the map.
static bool dashTargetStillExists()
{
    if (gDashTarget == NULL) {
        return false;
    }
    for (Object* obj = obj_find_first_at(map_elevation); obj != NULL; obj = obj_find_next_at()) {
        if (obj == gDashTarget) {
            return true;
        }
    }
    gDashTarget = NULL;
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
    switch (item_w_subtype(weapon, hitMode)) {
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
    if (obj_dude == NULL) {
        return;
    }
    d->valid = true;

    snprintf(d->name, sizeof(d->name), "%s", critter_name(obj_dude));
    d->level = stat_pc_get(PC_STAT_LEVEL);
    d->xp = stat_pc_get(PC_STAT_EXPERIENCE);
    d->xpNext = stat_pc_min_exp();
    d->hp = critter_get_hits(obj_dude);
    d->hpMax = stat_level(obj_dude, STAT_MAXIMUM_HIT_POINTS);
    d->ac = stat_level(obj_dude, STAT_ARMOR_CLASS);
    d->apMax = stat_level(obj_dude, STAT_MAXIMUM_ACTION_POINTS);
    d->inCombat = isInCombat();
    d->ap = d->inCombat ? obj_dude->data.critter.combat.ap : d->apMax;
    d->apBonus = d->inCombat ? combat_free_move : 0;
    d->poison = critter_get_poison(obj_dude);
    d->rads = critter_get_rads(obj_dude);

    int month, day, year;
    game_time_date(&month, &day, &year);
    static const char* months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
    snprintf(d->time, sizeof(d->time), "%02d %s %d  %s", day, months[(month + 11) % 12], year, game_time_hour_str());

    // Active hand: weapon / item, attack mode, AP cost, ammo
    Object* item = NULL;
    int hitMode = HIT_MODE_PUNCH;
    bool aiming = false;
    if (intface_get_current_item(&item) != 0 || item == (Object*)-1) {
        item = NULL; // interface not ready (e.g. while a map loads)
    }
    if (intface_get_attack(&hitMode, &aiming) != 0) {
        hitMode = HIT_MODE_PUNCH;
        aiming = false;
    }
    if (item != NULL) {
        snprintf(d->weapon, sizeof(d->weapon), "%s", object_name(item));
        if (item_get_type(item) == ITEM_TYPE_WEAPON && item_w_max_ammo(item) > 0) {
            d->hasAmmo = true;
            d->ammo = item_w_curr_ammo(item);
            d->ammoMax = item_w_max_ammo(item);
        }
    } else {
        snprintf(d->weapon, sizeof(d->weapon), "Unarmed");
    }
    int apCost = item_w_mp_cost(obj_dude, hitMode, aiming);
    snprintf(d->attack, sizeof(d->attack), "%s%s  %d AP", attackLabel(item, hitMode), aiming ? " (aimed)" : "", apCost);

    // Combat target: last enemy tapped
    if (d->inCombat && dashTargetStillExists()) {
        int thp = critter_get_hits(gDashTarget);
        int thpMax = stat_level(gDashTarget, STAT_MAXIMUM_HIT_POINTS);
        d->hasTarget = true;
        snprintf(d->target, sizeof(d->target), "%s", object_name(gDashTarget));
        snprintf(d->targetState, sizeof(d->targetState), "%s", healthWord(thp, thpMax));
        d->targetHit = thp > 0 ? determine_to_hit(obj_dude, gDashTarget, HIT_LOCATION_UNCALLED, hitMode) : 0;
    }

    // Message log
    const char* lines[CTR_DASH_LINES];
    d->lineCount = display_get_recent_lines(lines, CTR_DASH_LINES, &d->logVersion);
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
