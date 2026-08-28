#include "global.h"
#include <assert.h>
#include "soh/ResourceManagerHelpers.h"

void SkelCurve_Clear(SkelAnimeCurve* skelCurve) {
    skelCurve->limbCount = 0;
    skelCurve->limbList = NULL;
    skelCurve->transUpdIdx = NULL;
    skelCurve->animCurFrame = 0.0f;
    skelCurve->animSpeed = 0.0f;
    skelCurve->animFinalFrame = 0.0f;
    skelCurve->transforms = NULL;
    skelCurve->unk_0C = 0.0f;
}

s32 SkelCurve_Init(PlayState* play, SkelAnimeCurve* skelCurve, SkelCurveLimbList* limbListSeg,
                   TransformUpdateIndex* transUpdIdx) {

    if (ResourceMgr_OTRSigCheck(limbListSeg))
        limbListSeg = ResourceMgr_LoadSkeletonByName(limbListSeg, NULL);

    SkelCurveLimbList* limbList = SEGMENTED_TO_VIRTUAL(limbListSeg);

    skelCurve->limbCount = limbList->limbCount;
    skelCurve->limbList = SEGMENTED_TO_VIRTUAL(limbList->limbs);

    skelCurve->transforms = ZELDA_ARENA_MALLOC_DEBUG(sizeof(*skelCurve->transforms) * skelCurve->limbCount);
    assert(skelCurve->transforms != NULL);
    skelCurve->animCurFrame = 0.0f;
    return 1;
}

void SkelCurve_Destroy(PlayState* play, SkelAnimeCurve* skelCurve) {
    if (skelCurve->transforms != NULL) {
        ZELDA_ARENA_FREE_DEBUG(skelCurve->transforms);
    }
}

void SkelCurve_SetAnim(SkelAnimeCurve* skelCurve, TransformUpdateIndex* transUpdIdx, f32 arg2, f32 animFinalFrame,
                       f32 animCurFrame, f32 animSpeed) {
    skelCurve->unk_0C = arg2 - skelCurve->animSpeed;
    skelCurve->animFinalFrame = animFinalFrame;
    skelCurve->animCurFrame = animCurFrame;
    skelCurve->animSpeed = animSpeed;
    skelCurve->transUpdIdx = transUpdIdx;
}

s32 SkelCurve_Update(PlayState* play, SkelAnimeCurve* skelCurve) {
    f32 transformValue = { 0 };
    s16* transforms = { 0 };
    u8* transformRefIdx = { 0 };
    u16* transformCopyValues = { 0 };
    s32 i;
    s32 ret = 0;
    s32 k;
    TransformData* transData = { 0 };
    s32 j;

    TransformUpdateIndex* transformIndex = SEGMENTED_TO_VIRTUAL(skelCurve->transUpdIdx);

    if (ResourceMgr_OTRSigCheck(transformIndex))
        transformIndex = ResourceMgr_LoadAnimByName(transformIndex);

    transformRefIdx = SEGMENTED_TO_VIRTUAL(transformIndex->refIndex);
    transData = SEGMENTED_TO_VIRTUAL(transformIndex->transformData);
    transformCopyValues = SEGMENTED_TO_VIRTUAL(transformIndex->copyValues);
    transforms = (s16*)skelCurve->transforms;

    skelCurve->animCurFrame += skelCurve->animSpeed * R_UPDATE_RATE * 0.5f;

    if ((skelCurve->animSpeed >= 0.0f && skelCurve->animCurFrame > skelCurve->animFinalFrame) ||
        (skelCurve->animSpeed < 0.0f && skelCurve->animCurFrame < skelCurve->animFinalFrame)) {
        skelCurve->animCurFrame = skelCurve->animFinalFrame;
        ret = 1;
    }

    for (i = 0; i < skelCurve->limbCount; i++) {
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 3; k++, transformRefIdx++, transforms++) {
                if (*transformRefIdx == 0) {
                    transformValue = *transformCopyValues;
                    *transforms = transformValue;
                    transformCopyValues++;
                } else {
                    transformValue = func_8006C5A8(skelCurve->animCurFrame, transData, *transformRefIdx);
                    transData += *transformRefIdx;
                    if (j == 0) {
                        *transforms = transformValue * 1024.0f;
                    } else if (j == 1) {
                        *transforms = transformValue * (32768.0f / 180.0f);
                    } else {
                        *transforms = transformValue * 100.0f;
                    }
                }
            }
        }
    }

    return ret;
}

void SkelCurve_DrawLimb(PlayState* play, s32 limbIndex, SkelAnimeCurve* skelCurve,
                        OverrideCurveLimbDraw overrideLimbDraw, PostCurveLimbDraw postLimbDraw, s32 lod, void* data) {
    Gfx* dList = { 0 };
    SkelCurveLimb* limb = SEGMENTED_TO_VIRTUAL(skelCurve->limbList[limbIndex]);

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();

    if (overrideLimbDraw == NULL || (overrideLimbDraw != NULL && overrideLimbDraw(play, skelCurve, limbIndex, data))) {
        Vec3f scale;
        Vec3s rot;
        Vec3f pos;
        Vec3s* transform = (Vec3s*)&skelCurve->transforms[limbIndex];

        scale.x = transform->x / 1024.0f;
        scale.y = transform->y / 1024.0f;
        scale.z = transform->z / 1024.0f;
        transform++;
        rot.x = transform->x;
        rot.y = transform->y;
        rot.z = transform->z;
        transform++;
        pos.x = transform->x;
        pos.y = transform->y;
        pos.z = transform->z;

        Matrix_TranslateRotateZYX(&pos, &rot);
        Matrix_Scale(scale.x, scale.y, scale.z, MTXMODE_APPLY);

        if (lod == 0) {

            dList = limb->dList[0];
            if (dList != NULL) {
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_LOAD | G_MTX_NOPUSH | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, dList);
            }
        } else if (lod == 1) {

            dList = limb->dList[0];
            if (dList != NULL) {
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_LOAD | G_MTX_NOPUSH | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, dList);
            }
            dList = limb->dList[1];
            if (dList != NULL) {
                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_LOAD | G_MTX_NOPUSH | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, dList);
            }
        } else {
            // "FcSkeletonInfo_draw_child (): Not supported"
            osSyncPrintf("FcSkeletonInfo_draw_child():未対応\n");
        }
    }

    if (postLimbDraw != NULL) {
        postLimbDraw(play, skelCurve, limbIndex, data);
    }

    if (limb->firstChildIdx != LIMB_DONE) {
        SkelCurve_DrawLimb(play, limb->firstChildIdx, skelCurve, overrideLimbDraw, postLimbDraw, lod, data);
    }

    Matrix_Pop();

    if (limb->nextLimbIdx != LIMB_DONE) {
        SkelCurve_DrawLimb(play, limb->nextLimbIdx, skelCurve, overrideLimbDraw, postLimbDraw, lod, data);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void SkelCurve_Draw(Actor* actor, PlayState* play, SkelAnimeCurve* skelCurve, OverrideCurveLimbDraw overrideLimbDraw,
                    PostCurveLimbDraw postLimbDraw, s32 lod, void* data) {
    if (skelCurve->transforms != NULL) {
        SkelCurve_DrawLimb(play, 0, skelCurve, overrideLimbDraw, postLimbDraw, lod, data);
    }
}
