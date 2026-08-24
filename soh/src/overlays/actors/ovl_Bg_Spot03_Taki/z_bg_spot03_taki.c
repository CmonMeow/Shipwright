/*
 * File: z_bg_spot03_taki.c
 * Overlay: ovl_Bg_Spot03_Taki
 * Description: Zora's River Waterfall
 */

#include "z_bg_spot03_taki.h"
#include "objects/object_spot03_object/object_spot03_object.h"
#include "soh/ResourceManagerHelpers.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void BgSpot03Taki_Init(Actor* thisx, PlayState* play);
void BgSpot03Taki_Destroy(Actor* thisx, PlayState* play);
void BgSpot03Taki_Update(Actor* thisx, PlayState* play);
void BgSpot03Taki_Draw(Actor* thisx, PlayState* play);

const ActorInit Bg_Spot03_Taki_InitVars = {
    ACTOR_BG_SPOT03_TAKI,
    ACTORCAT_BG,
    FLAGS,
    OBJECT_SPOT03_OBJECT,
    sizeof(BgSpot03Taki),
    (ActorFunc)BgSpot03Taki_Init,
    (ActorFunc)BgSpot03Taki_Destroy,
    (ActorFunc)BgSpot03Taki_Update,
    (ActorFunc)BgSpot03Taki_Draw,
    NULL,
};

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_STOP),
};

void BgSpot03Taki_ApplyOpeningAlpha(BgSpot03Taki* this, s32 bufferIndex) {
    s32 i;
    Vtx* vtx = (bufferIndex == 0) ? SEGMENTED_TO_VIRTUAL(object_spot03_object_Vtx_000800)
                                  : SEGMENTED_TO_VIRTUAL(object_spot03_object_Vtx_000990);

    vtx = ResourceMgr_LoadVtxByName(vtx);

    for (i = 0; i < 5; i++) {
        vtx[i + 10].v.cn[3] = this->openingAlpha;
    }
}

void BgSpot03Taki_Init(Actor* thisx, PlayState* play) {
    BgSpot03Taki* this = (BgSpot03Taki*)thisx;
    s16 pad;
    CollisionHeader* colHeader = NULL;

    DynaPolyActor_Init(&this->dyna, DPM_UNK);
    CollisionHeader_GetVirtual(&object_spot03_object_Col_000C98, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
    Actor_ProcessInitChain(&this->dyna.actor, sInitChain);
    this->bufferIndex = 0;
    this->openingAlpha = 0.0f;
    BgSpot03Taki_ApplyOpeningAlpha(this, 0);
    BgSpot03Taki_ApplyOpeningAlpha(this, 1);
    func_8003EBF8(play, &play->colCtx.dyna, this->dyna.bgId);
}

void BgSpot03Taki_Destroy(Actor* thisx, PlayState* play) {
    BgSpot03Taki* this = (BgSpot03Taki*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

void BgSpot03Taki_Update(Actor* thisx, PlayState* play) {
}

void BgSpot03Taki_Draw(Actor* thisx, PlayState* play) {
    BgSpot03Taki* this = (BgSpot03Taki*)thisx;
    s32 pad;
    u32 gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);

    gameplayFrames = play->gameplayFrames;

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, gameplayFrames * 5, 64, 64, 1, 0, gameplayFrames * 5, 64,
                                  64, 0, 5, 0, 5));

    gSPDisplayList(POLY_XLU_DISP++, object_spot03_object_DL_000B20);

    if (this->bufferIndex == 0) {
        gSPVertex(POLY_XLU_DISP++, object_spot03_object_Vtx_000800, 25, 0);
    } else {
        gSPVertex(POLY_XLU_DISP++, object_spot03_object_Vtx_000990, 25, 0);
    }

    gSPDisplayList(POLY_XLU_DISP++, object_spot03_object_DL_000BC0);

    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, gameplayFrames * 1, gameplayFrames * 3, 64, 64, 1,
                                  -gameplayFrames, gameplayFrames * 3, 64, 64, 1, 3, -1, 3));

    gSPDisplayList(POLY_XLU_DISP++, object_spot03_object_DL_001580);

    CLOSE_DISPS(play->state.gfxCtx);

    this->bufferIndex = this->bufferIndex == 0;

    Audio_PlaySoundWaterfall(&this->dyna.actor.projectedPos, 0.5f);
}
