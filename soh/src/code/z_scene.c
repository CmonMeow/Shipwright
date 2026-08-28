#include "global.h"
#include "vt.h"
#include <assert.h>

int32_t Object_Spawn(ObjectContext* objectCtx, int16_t objectId) {

    objectCtx->status[objectCtx->num].id = objectId;
    size_t size = gObjectTable[objectId].vromEnd - gObjectTable[objectId].vromStart;

    osSyncPrintf("OBJECT[%d] SIZE %fK SEG=%x\n", objectId, size / 1024.0f, objectCtx->status[objectCtx->num].segment);

    osSyncPrintf("num=%d adrs=%x end=%x\n", objectCtx->num, (uintptr_t)objectCtx->status[objectCtx->num].segment + size,
                 objectCtx->spaceEnd);

    assert(((objectCtx->num < OBJECT_EXCHANGE_BANK_MAX) &&
            (((uintptr_t)objectCtx->status[objectCtx->num].segment + size) < (uintptr_t)objectCtx->spaceEnd)));

    DmaMgr_SendRequest1(objectCtx->status[objectCtx->num].segment, gObjectTable[objectId].vromStart, size, __FILE__,
                        __LINE__);

    if (objectCtx->num < OBJECT_EXCHANGE_BANK_MAX - 1) {
        objectCtx->status[objectCtx->num + 1].segment =
            (void*)ALIGN16((uintptr_t)objectCtx->status[objectCtx->num].segment + size);
    }

    objectCtx->num++;
    objectCtx->unk_09 = objectCtx->num;

    return objectCtx->num - 1;
}

// SOH [Port] Track when objects are first loaded for a scene
static uint8_t sObjectFirstUpdateSkippedForScene = false;

void Object_InitBank(PlayState* play, ObjectContext* objectCtx) {
    const size_t spaceSize = 1024000;
    int32_t i;

    objectCtx->num = objectCtx->unk_09 = 0;
    objectCtx->mainKeepIndex = objectCtx->subKeepIndex = 0;

    for (i = 0; i < OBJECT_EXCHANGE_BANK_MAX; i++) {
        objectCtx->status[i].id = OBJECT_INVALID;
        objectCtx->status[i].segment = NULL;
    }

    osSyncPrintf(VT_FGCOL(GREEN));
    // "Object exchange bank data %8.3fKB"
    osSyncPrintf("オブジェクト入れ替えバンク情報 %8.3fKB\n", spaceSize / 1024.0f);
    osSyncPrintf(VT_RST);

    objectCtx->spaceStart = objectCtx->status[0].segment = GAMESTATE_ALLOC_MC(&play->state, spaceSize);
    objectCtx->spaceEnd = (void*)((uintptr_t)objectCtx->spaceStart + spaceSize);

    objectCtx->mainKeepIndex = Object_Spawn(objectCtx, OBJECT_GAMEPLAY_KEEP);
    gSegments[4] = VIRTUAL_TO_PHYSICAL(objectCtx->status[objectCtx->mainKeepIndex].segment);

    sObjectFirstUpdateSkippedForScene = false;
}

void Object_UpdateBank(ObjectContext* objectCtx) {
    int32_t i;
    ObjectStatus* status = &objectCtx->status[0];

    // SOH [Port] Skip the first object load after scene init so that actors have their init delayed by one frame
    // This seems to mostly if not nearly resolve actors that depend on console DMA requests ending later
    if (!sObjectFirstUpdateSkippedForScene) {
        sObjectFirstUpdateSkippedForScene = true;
        return;
    }

    for (i = 0; i < objectCtx->num; i++) {
        if (status->id < 0) {
            status->id = -status->id;
        }
        status++;
    }
}

int32_t Object_GetIndex(ObjectContext* objectCtx, int16_t objectId) {
    int32_t i;

    // return 0;

    for (i = 0; i < objectCtx->num; i++) {
        if (ABS(objectCtx->status[i].id) == objectId) {
            return i;
        }
    }

    return -1;
}

int32_t Object_IsLoaded(ObjectContext* objectCtx, int32_t bankIndex) {
    if (objectCtx->status[bankIndex].id > 0) {
        return true;
    } else {
        return false;
    }
}

RomFile gObjectTable[OBJECT_ID_MAX] = {
    [OBJECT_GAMEPLAY_KEEP] = ROM_FILE(gameplay_keep),
    [OBJECT_GAMEPLAY_FIELD_KEEP] = ROM_FILE(gameplay_field_keep),
    [OBJECT_LINK_BOY] = ROM_FILE(object_link_boy),
    [OBJECT_FISH] = ROM_FILE(object_fish),
};

uint32_t gObjectTableSize = ARRAY_COUNT(gObjectTable);
