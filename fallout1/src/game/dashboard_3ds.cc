// 3DS top-screen dashboard: collects what the player needs to read while
// the map is on the touchscreen. Rendered by platform/ctr/ctr_gfx.cc.
#ifdef __3DS__

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "platform/ctr/ctr_dashboard.h"

#include "game/combat.h"
#include "game/combat_defs.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/inventry.h"
#include "game/map.h"
#include "game/object.h"
#include "game/proto.h"
#include "game/proto_types.h"
#include "game/scripts.h"
#include "game/stat.h"
#include "game/stat_defs.h"
#include "game/tile.h"

namespace fallout {

static bool gDashInGame = false;

void ctrDashboardSetInGame(bool inGame)
{
    gDashInGame = inGame;
}

static Object* gDashTarget = NULL;

// True while a game is running (player exists and the interface bar is up).
bool ctrDashboardAvailable()
{
    Object* item;
    return gDashInGame && obj_dude != NULL && intface_get_current_item(&item) == 0;
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
    ctr_obj_for_each(map_elevation, [](Object* obj, void* ctx) {
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
    return obj != NULL && obj != obj_dude && PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER && !critter_is_dead(obj);
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

    int map = map_get_index_number();
    if (map != lastMap || map_elevation != lastElevation) {
        gDashTarget = NULL;
        autoTarget = NULL;
        lastMap = map;
        lastElevation = map_elevation;
    }

    // Walking every tile list costs time, so the pointers are re-verified a few
    // times a second; objects are only freed on map changes (handled above)
    // or by scripts.
    static int verifyCountdown = 0;
    bool verify = --verifyCountdown <= 0;
    if (verify) {
        verifyCountdown = 15;
    }
    if (gDashTarget != NULL && ((verify && !dashObjectOnMap(gDashTarget)) || critter_is_dead(gDashTarget))) {
        gDashTarget = NULL;
    }
    if (gDashTarget != NULL) {
        return gDashTarget;
    }

    if (autoTarget != NULL && ((verify && !dashObjectOnMap(autoTarget)) || critter_is_dead(autoTarget))) {
        autoTarget = NULL;
        searchCountdown = 0;
    }
    if (--searchCountdown > 0 && autoTarget != NULL) {
        return autoTarget;
    }
    searchCountdown = 20;

    Object* attacker = obj_dude->data.critter.combat.whoHitMe;
    if (dashObjectOnMap(attacker) && dashIsLiveCritter(attacker)) {
        autoTarget = attacker;
        return autoTarget;
    }

    struct Nearest {
        Object* best;
        int bestDist;
    } nearest = { NULL, 0 };
    ctr_obj_for_each(map_elevation, [](Object* obj, void* ctx) {
        Nearest* n = (Nearest*)ctx;
        if (dashIsLiveCritter(obj) && obj->data.critter.combat.whoHitMe == obj_dude) {
            int dist = tile_dist(obj_dude->tile, obj->tile);
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
    // The interface caches the hand pointer; while the inventory is open the
    // item can be used up or merged into a stack and freed. Only trust it if
    // it is still in one of the player's hands.
    if (item != NULL && item != inven_right_hand(obj_dude) && item != inven_left_hand(obj_dude)) {
        item = NULL;
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

    // Combat: whose turn, and the target
    if (d->inCombat) {
        d->turn = combat_whose_turn() == obj_dude ? 1 : 2;
        Object* target = dashCurrentTarget();
        if (target != NULL) {
            int thp = critter_get_hits(target);
            int thpMax = stat_level(target, STAT_MAXIMUM_HIT_POINTS);
            d->hasTarget = true;
            snprintf(d->target, sizeof(d->target), "%s", object_name(target));
            snprintf(d->targetState, sizeof(d->targetState), "%s", healthWord(thp, thpMax));
            // The to-hit roll traces the line of fire: only recompute when
            // something it depends on changes, or a few times a second.
            static Object* hitTarget = NULL;
            static int hitModeLast = -1;
            static int hitTileLast = -1;
            static int hitTargetTileLast = -1;
            static int hitValue = 0;
            static int hitCountdown = 0;
            if (target != hitTarget || hitMode != hitModeLast || obj_dude->tile != hitTileLast
                || target->tile != hitTargetTileLast || --hitCountdown <= 0) {
                hitValue = thp > 0 ? determine_to_hit(obj_dude, target, HIT_LOCATION_UNCALLED, hitMode) : 0;
                hitTarget = target;
                hitModeLast = hitMode;
                hitTileLast = obj_dude->tile;
                hitTargetTileLast = target->tile;
                hitCountdown = 15;
            }
            d->targetHit = thp > 0 ? hitValue : 0;
        }
    }

    // The other hand, money, load, sneaking
    Object* other = intface_is_item_right_hand() ? inven_left_hand(obj_dude) : inven_right_hand(obj_dude);
    snprintf(d->otherHand, sizeof(d->otherHand), "%s", other != NULL ? object_name(other) : "Unarmed");
    static int capsCache = 0;
    static int weightCache = 0;
    static int inventoryCountdown = 0;
    if (--inventoryCountdown <= 0) {
        capsCache = item_caps_total(obj_dude);
        weightCache = item_total_weight(obj_dude);
        inventoryCountdown = 15;
    }
    d->caps = capsCache;
    d->weight = weightCache;
    d->weightMax = stat_level(obj_dude, STAT_CARRY_WEIGHT);
    d->sneaking = is_pc_flag(PC_FLAG_SNEAKING);

    // Message log: the monitor stores lines wrapped for the narrow PC
    // interface; join them back into whole messages (each starts with the
    // 0x95 bullet) so the top screen can wrap them at its own width.
    const char* lines[24];
    int lineCount = display_get_recent_lines(lines, 24, &d->logVersion);
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


// ---------------------------------------------------------------------------
// Item card / barter summary
// ---------------------------------------------------------------------------
static CtrItemCard gCard;
static CtrBarter gBarter;

static void cardCopyText(char* dst, size_t size, const char* src)
{
    size_t n = 0;
    if (src != NULL) {
        for (; *src != '\0' && n + 1 < size; src++) {
            unsigned char c = (unsigned char)*src;
            dst[n++] = (c >= 0x20 && c < 0x80) ? (char)c : ' ';
        }
    }
    dst[n] = '\0';
}

static void cardRow(const char* key, const char* fmt, ...)
{
    if (gCard.rowCount >= CTR_CARD_ROWS) {
        return;
    }
    snprintf(gCard.key[gCard.rowCount], sizeof(gCard.key[0]), "%s", key);
    va_list args;
    va_start(args, fmt);
    vsnprintf(gCard.val[gCard.rowCount], sizeof(gCard.val[0]), fmt, args);
    va_end(args);
    gCard.rowCount++;
}

static const char* damageTypeName(int type)
{
    static const char* names[] = { "normal", "laser", "fire", "plasma", "electrical", "EMP", "explosive" };
    return type >= 0 && type < 7 ? names[type] : "";
}

void ctrCardSetItem(void* ptr, int quantity)
{
    Object* item = (Object*)ptr;
    if (item == NULL) {
        return;
    }
    unsigned int version = gCard.version + 1;
    memset(&gCard, 0, sizeof(gCard));
    gCard.version = version;
    gCard.valid = true;
    gCard.quantity = quantity;
    cardCopyText(gCard.name, sizeof(gCard.name), object_name(item));

    Proto* proto = NULL;
    if (proto_ptr(item->pid, &proto) == -1) {
        proto = NULL;
    }

    switch (item_get_type(item)) {
    case ITEM_TYPE_WEAPON:
        snprintf(gCard.type, sizeof(gCard.type), "Weapon");
        if (proto != NULL) {
            ProtoItemWeaponData* w = &(proto->item.data.weapon);
            cardRow("Damage", "%d-%d %s", w->minDamage, w->maxDamage, damageTypeName(w->damageType));
            if (w->actionPointCost2 > 0 && w->maxRange2 > 0) {
                cardRow("Range", "%d / %d", w->maxRange1, w->maxRange2);
                cardRow("AP cost", "%d / %d", w->actionPointCost1, w->actionPointCost2);
            } else {
                cardRow("Range", "%d", w->maxRange1);
                cardRow("AP cost", "%d", w->actionPointCost1);
            }
            if (w->ammoCapacity > 0) {
                const char* ammo = w->ammoTypePid != -1 ? proto_name(w->ammoTypePid) : "";
                char ammoName[40];
                cardCopyText(ammoName, sizeof(ammoName), ammo);
                cardRow("Ammo", "%d / %d  %s", item_w_curr_ammo(item), item_w_max_ammo(item), ammoName);
            }
            if (w->minStrength > 0) {
                cardRow("Min ST", "%d", w->minStrength);
            }
        }
        break;
    case ITEM_TYPE_ARMOR:
        snprintf(gCard.type, sizeof(gCard.type), "Armor");
        cardRow("AC", "%d", item_ar_ac(item));
        cardRow("Normal", "%d / %d%%", item_ar_dt(item, DAMAGE_TYPE_NORMAL), item_ar_dr(item, DAMAGE_TYPE_NORMAL));
        cardRow("Laser", "%d / %d%%", item_ar_dt(item, DAMAGE_TYPE_LASER), item_ar_dr(item, DAMAGE_TYPE_LASER));
        cardRow("Fire", "%d / %d%%", item_ar_dt(item, DAMAGE_TYPE_FIRE), item_ar_dr(item, DAMAGE_TYPE_FIRE));
        cardRow("Plasma", "%d / %d%%", item_ar_dt(item, DAMAGE_TYPE_PLASMA), item_ar_dr(item, DAMAGE_TYPE_PLASMA));
        cardRow("Explode", "%d / %d%%", item_ar_dt(item, DAMAGE_TYPE_EXPLOSION), item_ar_dr(item, DAMAGE_TYPE_EXPLOSION));
        break;
    case ITEM_TYPE_AMMO:
        snprintf(gCard.type, sizeof(gCard.type), "Ammo");
        if (proto != NULL) {
            ProtoItemAmmoData* a = &(proto->item.data.ammo);
            cardRow("AC mod", "%+d", a->armorClassModifier);
            cardRow("DR mod", "%+d%%", a->damageResistanceModifier);
            cardRow("Damage", "x%d / %d", a->damageMultiplier, a->damageDivisor > 0 ? a->damageDivisor : 1);
            cardRow("Rounds", "%d per box", a->quantity);
        }
        break;
    case ITEM_TYPE_DRUG:
        snprintf(gCard.type, sizeof(gCard.type), "Drug");
        break;
    case ITEM_TYPE_CONTAINER:
        snprintf(gCard.type, sizeof(gCard.type), "Container");
        break;
    case ITEM_TYPE_KEY:
        snprintf(gCard.type, sizeof(gCard.type), "Key");
        break;
    default:
        snprintf(gCard.type, sizeof(gCard.type), "Item");
        break;
    }

    cardRow("Weight", "%d lbs", item_weight(item));
    cardRow("Value", "$%d", item_cost(item));
    cardCopyText(gCard.desc, sizeof(gCard.desc), object_description(item));
}

void ctrCardClear()
{
    unsigned int version = gCard.version + 1;
    memset(&gCard, 0, sizeof(gCard));
    gCard.version = version;
}

const CtrItemCard* ctrCardGet()
{
    return &gCard;
}

void ctrBarterSet(const CtrBarter* barter)
{
    gBarter = *barter;
    gBarter.active = true;
}

void ctrBarterEnd()
{
    gBarter.active = false;
}

const CtrBarter* ctrBarterGet()
{
    return &gBarter;
}

} // namespace fallout

#endif
