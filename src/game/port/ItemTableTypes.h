#pragma once

#ifdef __cplusplus
#include <stdint.h>
#endif

#define CHEST_ANIM_SHORT (int16_t)0
#define CHEST_ANIM_LONG (int16_t)1

typedef enum GetItemFrom {
    ITEM_FROM_NPC,
    ITEM_FROM_SKULLTULA,
    ITEM_FROM_FREESTANDING,
    ITEM_FROM_CHEST,
} GetItemFrom;

typedef enum GetItemCategory {
    ITEM_CATEGORY_JUNK,
    ITEM_CATEGORY_LESSER,
    ITEM_CATEGORY_HEALTH,
    ITEM_CATEGORY_BOSS_KEY,
    ITEM_CATEGORY_SMALL_KEY,
    ITEM_CATEGORY_SKULLTULA_TOKEN,
    ITEM_CATEGORY_MAJOR,
} GetItemCategory;

#define GET_ITEM(itemId, objectId, drawId, textId, field, chestAnim, itemCategory, modIndex, getItemId)                \
    {                                                                                                                  \
        itemId, field, (int16_t)((chestAnim != CHEST_ANIM_SHORT ? 1 : -1) * (drawId + 1)), textId, objectId, modIndex, \
            modIndex, getItemId, drawId, true, ITEM_FROM_NPC, itemCategory, itemId, modIndex, NULL                     \
    }

#define GET_ITEM_NONE \
    { ITEM_NONE, 0, 0, 0, 0, 0, 0, 0, 0, false, ITEM_FROM_NPC, ITEM_CATEGORY_JUNK, ITEM_NONE, 0, NULL }

typedef struct PlayState PlayState;
typedef struct GetItemEntry GetItemEntry;
typedef void (*CustomDrawFunc)(PlayState*, GetItemEntry*);

typedef struct GetItemEntry {
    uint16_t itemId;
    uint16_t field;
    int16_t gi;
    uint16_t textId;
    uint16_t objectId;
    uint16_t modIndex;
    uint16_t tableId;
    int16_t getItemId;
    uint16_t gid;
    uint16_t collectable;
    GetItemFrom getItemFrom;
    GetItemCategory getItemCategory;
    uint16_t drawItemId;
    uint16_t drawModIndex;
    CustomDrawFunc drawFunc;
} GetItemEntry;
