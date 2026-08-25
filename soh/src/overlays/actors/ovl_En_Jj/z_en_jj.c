/*
 * File: z_en_jj.c
 * Overlay: ovl_En_Jj
 * Description: Lord Jabu-Jabu
 */

#include "z_en_jj.h"
#include "objects/object_jj/object_jj.h"
#include "soh/ResourceManagerHelpers.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

typedef enum {
    /* 0 */ JABUJABU_EYE_OPEN,
    /* 1 */ JABUJABU_EYE_HALF,
    /* 2 */ JABUJABU_EYE_CLOSED,
    /* 3 */ JABUJABU_EYE_MAX
} EnJjEyeState;

void EnJj_Init(Actor* thisx, PlayState* play);
void EnJj_Destroy(Actor* thisx, PlayState* play);
void EnJj_Update(Actor* thisx, PlayState* play);
void EnJj_Draw(Actor* thisx, PlayState* play);

void EnJj_UpdateStaticCollision(Actor* thisx, PlayState* play);
void EnJj_OpenMouth(EnJj* this, PlayState* play);
void EnJj_WaitToOpenMouth(EnJj* this, PlayState* play);

const ActorInit En_Jj_InitVars = {
    ACTOR_EN_JJ,
    ACTORCAT_ITEMACTION,
    FLAGS,
    OBJECT_JJ,
    sizeof(EnJj),
    (ActorFunc)EnJj_Init,
    (ActorFunc)EnJj_Destroy,
    (ActorFunc)EnJj_Update,
    (ActorFunc)EnJj_Draw,
    NULL,
};

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 87, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 4000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 3300, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1100, ICHAIN_STOP),
};

void EnJj_SetupAction(EnJj* this, EnJjActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

void EnJj_Init(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    EnJj* this = (EnJj*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_ProcessInitChain(&this->dyna.actor, sInitChain);
    ActorShape_Init(&this->dyna.actor.shape, 0.0f, NULL, 0.0f);

    switch (this->dyna.actor.params) {
        case JABUJABU_MAIN:
            SkelAnime_InitFlex(play, &this->skelAnime, &gJabuJabuSkel, &gJabuJabuAnim, this->jointTable,
                               this->morphTable, 22);
            Animation_PlayLoop(&this->skelAnime, &gJabuJabuAnim);
            this->eyeIndex = 0;
            this->blinkTimer = 0;
            this->extraBlinkCounter = 0;
            this->extraBlinkTotal = 0;
            this->mouthOpenAngle = 0;
            EnJj_SetupAction(this, EnJj_WaitToOpenMouth);

            this->bodyCollisionActor = (DynaPolyActor*)Actor_SpawnAsChild(
                &play->actorCtx, &this->dyna.actor, play, ACTOR_EN_JJ, this->dyna.actor.world.pos.x - 10.0f,
                this->dyna.actor.world.pos.y, this->dyna.actor.world.pos.z, 0, this->dyna.actor.world.rot.y, 0,
                JABUJABU_COLLISION);
            DynaPolyActor_Init(&this->dyna, 0);
            CollisionHeader_GetVirtual(&gJabuJabuHeadCol, &colHeader);
            this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
            this->dyna.actor.colChkInfo.mass = MASS_IMMOVABLE;
            break;

        case JABUJABU_COLLISION:
            DynaPolyActor_Init(&this->dyna, 0);
            CollisionHeader_GetVirtual(&gJabuJabuBodyCol, &colHeader);
            this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
            func_8003ECA8(play, &play->colCtx.dyna, this->dyna.bgId);
            this->dyna.actor.update = EnJj_UpdateStaticCollision;
            this->dyna.actor.draw = NULL;
            Actor_SetScale(&this->dyna.actor, 0.087f);
            break;

        case JABUJABU_UNUSED_COLLISION:
            DynaPolyActor_Init(&this->dyna, 0);
            CollisionHeader_GetVirtual(&gJabuJabuUnusedCol, &colHeader);
            this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
            this->dyna.actor.update = EnJj_UpdateStaticCollision;
            this->dyna.actor.draw = NULL;
            Actor_SetScale(&this->dyna.actor, 0.087f);
            break;
    }
}

void EnJj_Destroy(Actor* thisx, PlayState* play) {
    EnJj* this = (EnJj*)thisx;

    switch (this->dyna.actor.params) {
        case JABUJABU_MAIN:
            DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);

            ResourceMgr_UnregisterSkeleton(&this->skelAnime);
            break;

        case JABUJABU_COLLISION:
        case JABUJABU_UNUSED_COLLISION:
            DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
            break;
    }
}

/**
 * Blink routine. Blinks at the end of each randomised blinkTimer cycle. If extraBlinkCounter is not zero, blink that
 * many more times before resuming random blinkTimer cycles. extraBlinkTotal can be set to a positive number to blink
 * that many extra times at the end of every blinkTimer cycle, but the actor always sets it to zero, so only one
 * multiblink happens when extraBlinkCounter is nonzero.
 */
void EnJj_Blink(EnJj* this) {
    if (this->blinkTimer > 0) {
        this->blinkTimer--;
    } else {
        this->eyeIndex++;
        if (this->eyeIndex >= JABUJABU_EYE_MAX) {
            this->eyeIndex = JABUJABU_EYE_OPEN;
            if (this->extraBlinkCounter > 0) {
                this->extraBlinkCounter--;
            } else {
                this->blinkTimer = Rand_S16Offset(20, 20);
                this->extraBlinkCounter = this->extraBlinkTotal;
            }
        }
    }
}

void EnJj_OpenMouth(EnJj* this, PlayState* play) {
    DynaPolyActor* bodyCollisionActor = this->bodyCollisionActor;

    if (this->mouthOpenAngle >= -5200) {
        this->mouthOpenAngle -= 102;

        if (this->mouthOpenAngle < -2600) {
            func_8003EBF8(play, &play->colCtx.dyna, bodyCollisionActor->bgId);
        }
    }
}

void EnJj_WaitToOpenMouth(EnJj* this, PlayState* play) {
    static Vec3f feedingSpot = { -1589.0f, 53.0f, -43.0f };

    if (Math_Vec3f_DistXZ(&feedingSpot, &GET_PLAYER(play)->actor.world.pos) < 300.0f) {
        Flags_SetEventChkInf(EVENTCHKINF_OFFERED_FISH_TO_JABU_JABU);
        EnJj_SetupAction(this, EnJj_OpenMouth);
    }
}

void EnJj_UpdateStaticCollision(Actor* thisx, PlayState* play) {
}

void EnJj_Update(Actor* thisx, PlayState* play) {
    EnJj* this = (EnJj*)thisx;

    this->actionFunc(this, play);

    if (this->skelAnime.curFrame == 41.0f) {
        Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_JABJAB_GROAN);
    }

    EnJj_Blink(this);
    SkelAnime_Update(&this->skelAnime);
    Actor_SetScale(&this->dyna.actor, 0.087f);

    // Head
    this->skelAnime.jointTable[10].z = this->mouthOpenAngle;
}

void EnJj_Draw(Actor* thisx, PlayState* play2) {
    static void* eyeTextures[] = { gJabuJabuEyeOpenTex, gJabuJabuEyeHalfTex, gJabuJabuEyeClosedTex };
    PlayState* play = play2;
    EnJj* this = (EnJj*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    Matrix_Translate(0.0f, (cosf(this->skelAnime.curFrame * (M_PI / 41.0f)) * 10.0f) - 10.0f, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(10.0f, 10.0f, 10.0f, MTXMODE_APPLY);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eyeTextures[this->eyeIndex]));
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, NULL, NULL, this);

    CLOSE_DISPS(play->state.gfxCtx);
}
