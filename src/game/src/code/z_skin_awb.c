#include "global.h"
#include <assert.h>
#include "port/ResourceManagerHelpers.h"

/**
 * Initialises the Vtx buffers used for limb at index `limbIndex`
 */
void Skin_InitAnimatedLimb(PlayState* play, Skin* skin, int32_t limbIndex) {
    int32_t i;
    SkinLimb** skeleton = SEGMENTED_TO_VIRTUAL(skin->skeletonHeader->segment);
    SkinAnimatedLimbData* animatedLimbData =
        SEGMENTED_TO_VIRTUAL(((SkinLimb*)SEGMENTED_TO_VIRTUAL(skeleton[limbIndex]))->segment);
    SkinLimbModif* limbModifications = SEGMENTED_TO_VIRTUAL(animatedLimbData->limbModifications);
    SkinLimbModif* modifEntry;
    SkinVertex* skinVtxEntry;

    for (i = 0; i < ARRAY_COUNT(skin->vtxTable->buf); i++) {
        Vtx* vtxBuf = skin->vtxTable[limbIndex].buf[i];

        for (modifEntry = limbModifications; modifEntry < limbModifications + animatedLimbData->limbModifCount;
             modifEntry++) {
            SkinVertex* skinVertices = SEGMENTED_TO_VIRTUAL(modifEntry->skinVertices);

            for (skinVtxEntry = skinVertices; skinVtxEntry < &skinVertices[modifEntry->vtxCount]; skinVtxEntry++) {
                Vtx* vtx = &vtxBuf[skinVtxEntry->index];

                vtx->n.flag = 0;
                vtx->n.tc[0] = skinVtxEntry->s;
                vtx->n.tc[1] = skinVtxEntry->t;
                vtx->n.a = skinVtxEntry->alpha;
            }
        }
    }
}

/**
 * Initializes a skin skeleton to looping animation, dynamically allocating the frame tables,
 * and dynamically allocating and initializing the Vtx and SkinLimbVtx buffers for its animated limbs
 */
void Skin_Init(PlayState* play, Skin* skin, SkeletonHeader* skeletonHeader, AnimationHeader* animationHeader) {
    if (ResourceMgr_OTRSigCheck(skeletonHeader))
        skeletonHeader = ResourceMgr_LoadSkeletonByName(skeletonHeader, NULL);

    int32_t limbCount;
    int32_t i;
    SkeletonHeader* virtSkelHeader = SEGMENTED_TO_VIRTUAL(skeletonHeader);

    skin->limbCount = virtSkelHeader->limbCount;
    skin->skeletonHeader = virtSkelHeader;

    SkinLimb** skeleton = SEGMENTED_TO_VIRTUAL(skin->skeletonHeader->segment);
    limbCount = skin->skeletonHeader->limbCount;

    skin->vtxTable = ZELDA_ARENA_MALLOC_DEBUG(limbCount * sizeof(SkinLimbVtx));

    assert(skin->vtxTable != NULL);

    for (i = 0; i < limbCount; i++) {
        SkinLimbVtx* vtxEntry = &skin->vtxTable[i];
        SkinLimb* limb = SEGMENTED_TO_VIRTUAL(skeleton[i]);

        if ((limb->segmentType != SKIN_LIMB_TYPE_ANIMATED) || (limb->segment == NULL)) {
            vtxEntry->index = 0;

            vtxEntry->buf[0] = NULL;
            vtxEntry->buf[1] = NULL;
        } else {
            SkinAnimatedLimbData* animatedLimbData = SEGMENTED_TO_VIRTUAL(((void)0, limb->segment));

            vtxEntry->index = 0;

            vtxEntry->buf[0] = ZELDA_ARENA_MALLOC_DEBUG(animatedLimbData->totalVtxCount * sizeof(Vtx));
            assert(vtxEntry->buf[0] != NULL);

            vtxEntry->buf[1] = ZELDA_ARENA_MALLOC_DEBUG(animatedLimbData->totalVtxCount * sizeof(Vtx));
            assert(vtxEntry->buf[1] != NULL);

            Skin_InitAnimatedLimb(play, skin, i);
        }
    }

    SkelAnime_InitSkin(play, &skin->skelAnime, skeletonHeader, animationHeader);
}

/**
 * Frees the dynamically allocated Vtx and SkinLimbVtx buffers and tables
 */
void Skin_Free(PlayState* play, Skin* skin) {
    if (skin->vtxTable != NULL) {
        int32_t i;

        for (i = 0; i < skin->limbCount; i++) {
            if (skin->vtxTable[i].buf[0] != NULL) {
                ZELDA_ARENA_FREE_DEBUG(skin->vtxTable[i].buf[0]);
                skin->vtxTable[i].buf[0] = NULL;
            }
            if (skin->vtxTable[i].buf[1] != NULL) {
                ZELDA_ARENA_FREE_DEBUG(skin->vtxTable[i].buf[1]);
                skin->vtxTable[i].buf[1] = NULL;
            }
        }

        if (skin->vtxTable != NULL) {
            ZELDA_ARENA_FREE_DEBUG(skin->vtxTable);
        }

        SkelAnime_Free(&skin->skelAnime, play);
    }
}

int32_t func_800A698C(Skin* skin, SkinLimb** skeleton, MtxF* limbMatrices, uint8_t parentIndex, uint8_t limbIndex) {
    SkinLimb* limb = SEGMENTED_TO_VIRTUAL(skeleton[limbIndex]);
    MtxF* mtx;
    int32_t ret = { 0 };
    MtxF sp28;

    if (parentIndex == LIMB_DONE) {
        SkinMatrix_GetClear(&mtx);
    } else {
        mtx = &limbMatrices[(int32_t)parentIndex];
    }

    SkinMatrix_MtxFMtxFMult(mtx, &limbMatrices[limbIndex], &sp28);
    SkinMatrix_MtxFCopy(&sp28, &limbMatrices[limbIndex]);

    if (limb->child != LIMB_DONE) {
        ret = func_800A698C(skin, skeleton, limbMatrices, limbIndex, limb->child);
        if (ret) {
            return ret;
        }
    }

    if (limb->sibling != LIMB_DONE) {
        ret = func_800A698C(skin, skeleton, limbMatrices, parentIndex, limb->sibling);
        if (ret) {
            return ret;
        }
    }

    return false;
}

/**
 * Recursively applies matrix tranformations to each limb
 */
int32_t Skin_ApplyAnimTransformations(Skin* skin, MtxF* limbMatrices, Actor* actor, int32_t setTranslation) {
    int32_t i;
    int32_t ret = { 0 };
    float yTransl = { 0 };
    float xTransl = { 0 };
    float zTransl = { 0 };
    SkinLimb** skeleton = SEGMENTED_TO_VIRTUAL(skin->skeletonHeader->segment);
    Vec3s* jointRot = &skin->skelAnime.jointTable[0];

    jointRot++;

    float xRot = jointRot->x;
    float yRot = jointRot->y;
    float zRot = jointRot->z;

    if (setTranslation) {
        jointRot--; // access joint table entry 0 for translation data

        xTransl = jointRot->x;
        yTransl = jointRot->y;
        zTransl = jointRot->z;

        jointRot++;

        SkinMatrix_SetTranslateRotateZYX(&limbMatrices[0], xRot, yRot, zRot, xTransl, yTransl, zTransl);
    } else {
        SkinMatrix_SetTranslateRotateZYX(&limbMatrices[0], xRot, yRot, zRot, 0.0f, 0.0f, 0.0f);
    }

    jointRot++;

    for (i = 1; i < skin->skeletonHeader->limbCount; i++) {
        SkinLimb* limb = SEGMENTED_TO_VIRTUAL(skeleton[i]);

        xTransl = limb->jointPos.x;
        yTransl = limb->jointPos.y;
        zTransl = limb->jointPos.z;
        xRot = jointRot->x;
        yRot = jointRot->y;
        zRot = jointRot->z;
        jointRot++;
        SkinMatrix_SetTranslateRotateZYX(&limbMatrices[i], xRot, yRot, zRot, xTransl, yTransl, zTransl);
    }

    SkinMatrix_SetTranslateRotateYXZScale(
        &skin->mtx, actor->scale.x, actor->scale.y, actor->scale.z, actor->shape.rot.x, actor->shape.rot.y,
        actor->shape.rot.z, actor->world.pos.x, actor->world.pos.y + (actor->shape.yOffset * actor->scale.y),
        actor->world.pos.z);

    ret = func_800A698C(skin, SEGMENTED_TO_VIRTUAL(skin->skeletonHeader->segment), limbMatrices, LIMB_DONE, 0);
    if (!ret) {
        return ret;
    }

    return false;
}
