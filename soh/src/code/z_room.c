#include "global.h"
#include "vt.h"

#include <assert.h>

static Vec3f sRoomOrigin = { 0.0f, 0.0f, 0.0f };

int32_t OTRfunc_8009728C(PlayState* play, RoomContext* roomCtx, int32_t roomNum);
int32_t OTRfunc_800973FC(PlayState* play, RoomContext* roomCtx);

static void Room_DrawTest01Mesh(PlayState* play, Room* room, uint32_t flags) {
    PolygonType0* mesh = &room->meshHeader->polygon0;
    PolygonDlist* dlist = SEGMENTED_TO_VIRTUAL(mesh->start);
    int32_t i;

    OPEN_DISPS(play->state.gfxCtx);

    if (flags & 1) {
        func_800342EC(&sRoomOrigin, play);
        gSPSegment(POLY_OPA_DISP++, 0x03, room->segment);
        func_80093C80(play);
        gSPMatrix(POLY_OPA_DISP++, &gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD);
    }

    if (flags & 2) {
        func_8003435C(&sRoomOrigin, play);
        gSPSegment(POLY_XLU_DISP++, 0x03, room->segment);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, &gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD);
    }

    for (i = 0; i < mesh->num; i++, dlist++) {
        if ((flags & 1) && dlist->opa != NULL) {
            gSPDisplayList(POLY_OPA_DISP++, dlist->opa);
        }
        if ((flags & 2) && dlist->xlu != NULL) {
            gSPDisplayList(POLY_XLU_DISP++, dlist->xlu);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void func_80096FD4(PlayState* play, Room* room) {
    room->num = -1;
    room->segment = NULL;
}

uint32_t func_80096FE8(PlayState* play, RoomContext* roomCtx) {
    uint32_t maxRoomSize = 0;
    int32_t i;

    for (i = 0; i < play->numRooms; i++) {
        uint32_t roomSize = play->roomList[i].vromEnd - play->roomList[i].vromStart;
        if (maxRoomSize < roomSize) {
            maxRoomSize = roomSize;
        }
    }

    roomCtx->bufPtrs[0] = GAMESTATE_ALLOC_MC(&play->state, maxRoomSize);
    roomCtx->bufPtrs[1] = (void*)((intptr_t)roomCtx->bufPtrs[0] + maxRoomSize);
    roomCtx->unk_30 = 0;
    roomCtx->status = 0;

    OTRfunc_8009728C(play, roomCtx, play->setupEntranceList[play->curSpawn].room);
    return maxRoomSize;
}

int32_t func_8009728C(PlayState* play, RoomContext* roomCtx, int32_t roomNum) {
    return OTRfunc_8009728C(play, roomCtx, roomNum);
}

int32_t func_800973FC(PlayState* play, RoomContext* roomCtx) {
    return OTRfunc_800973FC(play, roomCtx);
}

void Room_Draw(PlayState* play, Room* room, uint32_t flags) {
    if (room->segment == NULL) {
        return;
    }

    gSegments[3] = VIRTUAL_TO_PHYSICAL(room->segment);
    assert(room->meshHeader->base.type == 0);
    Room_DrawTest01Mesh(play, room, flags);
}

void func_80097534(PlayState* play, RoomContext* roomCtx) {
    roomCtx->prevRoom.num = -1;
    roomCtx->prevRoom.segment = NULL;
    func_80031B14(play, &play->actorCtx);
    Audio_SetEnvReverb(play->roomCtx.curRoom.echo);
}
