#include "global.h"
#include "engine/input/PCInput.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
#include "objects/object_triforce_spot/object_triforce_spot.h"
#include "objects/object_fish/object_fish.h"
#include "port/ResourceManagerHelpers.h"
#include "port/Enhancements/debugger/colViewer.h"

#include <stdlib.h>

typedef struct {
    /* 0x00 */ uint8_t flag;
    /* 0x02 */ uint16_t textId;
} TextTriggerEntry; // size = 0x04

typedef struct {
    /* 0x00 */ void* dList;
    /* 0x04 */ Vec3f pos;
} BowStringData; // size = 0x10

FlexSkeletonHeader* gPlayerSkelHeaders = &gLinkAdultSkel;

static const int16_t sMovementTuning[] = {
    200, 1000, 300, 700, 550, 270, 600, 350, 800, 600, -100, 600, 590, 750, 125, 200, 130,
};


TextTriggerEntry sTextTriggers[] = {
    { 1, 0x3040 },
    { 2, 0x401D },
    { 0, 0x0000 },
    { 2, 0x401D },
};

// Used to map model groups to model types for [animation, left hand, right hand, sheath, waist]
uint8_t gPlayerModelTypes[PLAYER_MODELGROUP_MAX][PLAYER_MODELGROUPENTRY_MAX] = {
    /* PLAYER_MODELGROUP_SWORD_AND_SHIELD */
    { PLAYER_ANIMTYPE_1, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_SHIELD, PLAYER_MODELTYPE_SHEATH_EMPTY,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_DEFAULT */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_SWORD_AND_SHIELD,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BGS */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_BGS, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_EMPTY_AND_SHIELD,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOW */
    { PLAYER_ANIMTYPE_4, PLAYER_MODELTYPE_LH_CLOSED, PLAYER_MODELTYPE_RH_BOW, PLAYER_MODELTYPE_SHEATH_SWORD_AND_SHIELD,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_FISHING */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_CLOSED, PLAYER_MODELTYPE_RH_CLOSED,
      PLAYER_MODELTYPE_SHEATH_SWORD_AND_SHIELD,
      PLAYER_MODELTYPE_WAIST },
};

Gfx* sPlayerRightHandShieldDLs[PLAYER_SHIELD_MAX * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightHandClosedFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultRightHandHoldingMirrorShieldNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandHoldingMirrorShieldFarDL,
    gLinkChildRightHandClosedFarDL,
};

Gfx* sSheathWithSwordDLs[PLAYER_SHIELD_MAX * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultMirrorShieldSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMirrorShieldSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
};

Gfx* sSheathWithoutSwordDLs[PLAYER_SHIELD_MAX * 4] = {
    // PLAYER_SHIELD_NONE
    gLinkAdultSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultSheathFarDL,
    gLinkChildSheathFarDL,
    // PLAYER_SHIELD_MIRROR
    gLinkAdultMirrorShieldAndSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultMirrorShieldAndSheathFarDL,
    gLinkChildSheathFarDL,
};

Gfx* gPlayerLeftHandBgsDLs[] = {
    // Biggoron Sword
    gLinkAdultLeftHandHoldingBgsNearDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    gLinkAdultLeftHandHoldingBgsFarDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    // Broken Giant's Knife
    gLinkAdultHandHoldingBrokenGiantsKnifeDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    gLinkAdultHandHoldingBrokenGiantsKnifeFarDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
};

Gfx* gPlayerLeftHandOpenDLs[] = {
    gLinkAdultLeftHandNearDL,
    gLinkChildLeftHandNearDL,
    gLinkAdultLeftHandFarDL,
    gLinkChildLeftHandFarDL,
};

Gfx* gPlayerLeftHandClosedDLs[] = {
    gLinkAdultLeftHandClosedNearDL,
    gLinkChildLeftFistNearDL,
    gLinkAdultLeftHandClosedFarDL,
    gLinkChildLeftFistFarDL,
};

Gfx* sPlayerLeftHandSwordDLs[] = {
    gLinkAdultLeftHandHoldingMasterSwordNearDL,
    gLinkAdultLeftHandHoldingMasterSwordNearDL,
    gLinkAdultLeftHandHoldingMasterSwordFarDL,
    gLinkAdultLeftHandHoldingMasterSwordFarDL,
};

Gfx* sPlayerRightHandOpenDLs[] = {
    gLinkAdultRightHandNearDL,
    gLinkChildRightHandNearDL,
    gLinkAdultRightHandFarDL,
    gLinkChildRightHandFarDL,
};

Gfx* sPlayerRightHandClosedDLs[] = {
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightHandClosedNearDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightHandClosedFarDL,
};

Gfx* sPlayerRightHandBowDLs[] = {
    gLinkAdultRightHandHoldingBowNearDL,
    gLinkAdultRightHandHoldingBowNearDL,
    gLinkAdultRightHandHoldingBowFarDL,
    gLinkAdultRightHandHoldingBowFarDL,
};

Gfx* sSwordAndSheathDLs[] = {
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
};

Gfx* sSheathDLs[] = {
    gLinkAdultSheathNearDL,
    gLinkChildSheathNearDL,
    gLinkAdultSheathFarDL,
    gLinkChildSheathFarDL,
};

Gfx* sPlayerWaistDLs[] = {
    gLinkAdultWaistNearDL,
    gLinkChildWaistNearDL,
    gLinkAdultWaistFarDL,
    gLinkChildWaistFarDL,
};

Gfx* sFirstPersonLeftForearmDLs[] = {
    gLinkAdultRightArmOutNearDL,
    NULL,
};

Gfx* sFirstPersonLeftHandDLs[] = {
    gLinkAdultRightHandOutNearDL,
    NULL,
};

Gfx* sFirstPersonRightShoulderDLs[] = {
    gLinkAdultRightShoulderNearDL,
    gLinkChildRightShoulderNearDL,
};

Gfx* sFirstPersonForearmDLs[] = {
    gLinkAdultLeftArmOutNearDL,
    NULL,
};

// Indexed by model types (left hand, right hand, sheath or waist)
Gfx** sPlayerDListGroups[PLAYER_MODELTYPE_MAX] = {
    gPlayerLeftHandOpenDLs,
    gPlayerLeftHandClosedDLs,
    sPlayerLeftHandSwordDLs,
    gPlayerLeftHandBgsDLs,
    sPlayerRightHandOpenDLs,
    sPlayerRightHandClosedDLs,
    sPlayerRightHandShieldDLs,
    sPlayerRightHandBowDLs,
    sSwordAndSheathDLs,
    sSheathDLs,
    sSheathWithSwordDLs,
    sSheathWithoutSwordDLs,
    sPlayerWaistDLs,
};

int32_t sLeftHandType;
int32_t sRightHandType;

/**
 * Selects adult Link's hand, sheath, and waist models for a synchronized
 * player. data points to two bytes: model group, then shield. Unlike the
 * gameplay callback this only selects display lists; it never touches local
 * player state, colliders, held actors, or weapon effects.
 */
int32_t Player_OverrideLimbDrawPresentation(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* data) {
    PlayerPresentationDrawData* presentation = data;
    uint8_t modelGroup = presentation->modelGroup;
    uint8_t shield = presentation->shield;
    int32_t type = { 0 };
    int32_t dListOffset = 0;

    (void)play;
    (void)pos;
    if (limbIndex == PLAYER_LIMB_HEAD) {
        rot->x += presentation->headLimbRot.z;
        rot->y -= presentation->headLimbRot.y;
        rot->z += presentation->headLimbRot.x;
    } else if (limbIndex == PLAYER_LIMB_UPPER) {
        Matrix_RotateY(presentation->upperLimbRot.y * (M_PI / 0x8000), MTXMODE_APPLY);
        Matrix_RotateX(presentation->upperLimbRot.x * (M_PI / 0x8000), MTXMODE_APPLY);
        Matrix_RotateZ(presentation->upperLimbRot.z * (M_PI / 0x8000), MTXMODE_APPLY);
    }

    if (modelGroup >= PLAYER_MODELGROUP_MAX) {
        modelGroup = PLAYER_MODELGROUP_DEFAULT;
    }
    if (shield >= PLAYER_SHIELD_MAX) {
        shield = PLAYER_SHIELD_MIRROR;
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_LEFT_HAND];
        sLeftHandType = type;
        // This project keeps the unbreakable Biggoron Sword only.
        dListOffset = 0;
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
        sRightHandType = type;
        if (type == PLAYER_MODELTYPE_RH_SHIELD) {
            dListOffset = shield * (int32_t)sizeof(uint32_t);
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_SHEATH];
        if ((type == PLAYER_MODELTYPE_SHEATH_SWORD_AND_SHIELD) ||
            (type == PLAYER_MODELTYPE_SHEATH_EMPTY_AND_SHIELD)) {
            dListOffset = shield * (int32_t)sizeof(uint32_t);
        }
    } else if (limbIndex == PLAYER_LIMB_WAIST) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_WAIST];
    } else {
        return 0;
    }

    *dList = sPlayerDListGroups[type][PLAYER_AGE + dListOffset];
    return 0;
}

void Player_PostLimbDrawPresentation(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3s* rot, void* data) {
    static Vec3f zero = { 0.0f, 0.0f, 0.0f };
    static float rodScales[22] = {
        1.0f, 1.0f, 1.0f, 0.9625f, 0.925f, 0.8875f, 0.85f, 0.8125f, 0.775f, 0.73749995f, 0.7f,
        0.6625f, 0.625f, 0.5875f, 0.54999995f, 0.5125f, 0.47499996f, 0.4375f, 0.39999998f,
        0.36249995f, 0.325f, 0.28749996f,
    };
    PlayerPresentationDrawData* presentation = data;
    int32_t i;

    if ((limbIndex > PLAYER_LIMB_NONE) && (limbIndex < PLAYER_LIMB_MAX)) {
        Matrix_MultVec3f(&zero, &presentation->limbOrigins[limbIndex]);
    }

    (void)dList;
    (void)rot;
    if ((limbIndex == PLAYER_LIMB_L_HAND) && presentation->bowReady &&
        (presentation->itemAction == PLAYER_IA_BOW)) {
        SkelAnime* arrowSkelAnime = presentation->bowArrowSkelAnime;

        if (arrowSkelAnime == NULL) {
            return;
        }
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        Matrix_Push();
        Matrix_Translate(398.0f, 1419.0f, 244.0f, MTXMODE_APPLY);
        Matrix_RotateZYX(0x69E8, -0x5708, 0x458E, MTXMODE_APPLY);
        SkelAnime_DrawLod(play, arrowSkelAnime->skeleton, arrowSkelAnime->jointTable, NULL, NULL,
                          arrowSkelAnime, 0);
        Matrix_Pop();
        CLOSE_DISPS(play->state.gfxCtx);
    }
    if ((limbIndex == PLAYER_LIMB_R_HAND) && (sRightHandType == PLAYER_MODELTYPE_RH_BOW)) {
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Push();
        Matrix_Translate(0.0f, -360.4f, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(1.0f, presentation->bowStringScale, 1.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gLinkAdultBowStringDL);
        Matrix_Pop();
        CLOSE_DISPS(play->state.gfxCtx);
    }
    // Fishing_DrawRod starts from Player.mf_9E0, captured from the left hand.
    if (limbIndex != PLAYER_LIMB_L_HAND || presentation->itemAction != PLAYER_IA_FISHING_POLE) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, gFishingRodMaterialDL);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 155, 0, 255);

    Matrix_Translate(0.0f, 400.0f, 0.0f, MTXMODE_APPLY);
    Matrix_RotateY((presentation->fishingState == 5 ? 0.56f : 0.41f) * M_PI, MTXMODE_APPLY);
    Matrix_RotateX(-M_PI / 5.0000003f, MTXMODE_APPLY);
    Matrix_RotateZ((presentation->fishingRodTwist * 0.5f) + (3.0f * M_PI / 20.0f), MTXMODE_APPLY);
    Matrix_RotateX((presentation->fishingRodCastX + 20.0f) * 0.01f * M_PI, MTXMODE_APPLY);
    Matrix_Scale(0.70000005f, 0.70000005f, 0.70000005f, MTXMODE_APPLY);
    Matrix_Translate(0.0f, 0.0f, -1300.0f, MTXMODE_APPLY);

    for (i = 0; i < 22; ++i) {
        static float rodBendRatios[22] = {
            0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.06f,   0.12f,   0.18f,   0.24f,   0.30f,   0.36f,
            0.42f, 0.48f, 0.54f, 0.60f, 0.60f, 0.5142f, 0.4285f, 0.3428f, 0.2571f, 0.1714f, 0.0857f,
        };
        Matrix_RotateY(rodBendRatios[i] * presentation->fishingRodBendY * 0.5f, MTXMODE_APPLY);
        Matrix_RotateX(rodBendRatios[i] * presentation->fishingRodBendX * 0.5f, MTXMODE_APPLY);
        Matrix_Push();
        Matrix_Scale(rodScales[i], rodScales[i], 0.52f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        if (i < 5) {
            gDPLoadTextureBlock(POLY_OPA_DISP++, gFishingRodSegmentBlackTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 8, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 4, 3, G_TX_NOLOD, G_TX_NOLOD);
        } else if ((i < 8) || ((i % 2) == 0)) {
            gDPLoadTextureBlock(POLY_OPA_DISP++, gFishingRodSegmentWhiteTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 8, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 4, 3, G_TX_NOLOD, G_TX_NOLOD);
        } else {
            gDPLoadTextureBlock(POLY_OPA_DISP++, gFishingRodSegmentStripTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 8, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 4, 3, G_TX_NOLOD, G_TX_NOLOD);
        }
        gSPDisplayList(POLY_OPA_DISP++, gFishingRodSegmentDL);
        Matrix_Pop();
        Matrix_Translate(0.0f, 0.0f, 500.0f, MTXMODE_APPLY);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

Gfx gCullBackDList[] = {
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};

Gfx gCullFrontDList[] = {
    gsSPSetGeometryMode(G_CULL_FRONT),
    gsSPEndDisplayList(),
};

Vec3f* D_80160000;
int32_t sDListsLodOffset;
Vec3f sGetItemRefPos;

void Player_ApplyMovementTuning(PlayState* play) {
    REG(27) = 2000;
    REG(48) = 370;

    REG(19) = sMovementTuning[0];
    REG(30) = sMovementTuning[1];
    REG(32) = sMovementTuning[2];
    REG(34) = sMovementTuning[3];
    REG(35) = sMovementTuning[4];
    REG(36) = sMovementTuning[5];
    REG(37) = sMovementTuning[6];
    REG(38) = sMovementTuning[7];
    REG(43) = sMovementTuning[8];
    REG(45) = sMovementTuning[9];
    REG(68) = sMovementTuning[10];
    REG(69) = sMovementTuning[11];
    IREG(66) = sMovementTuning[12];
    IREG(67) = sMovementTuning[13];
    IREG(68) = sMovementTuning[14];
    IREG(69) = sMovementTuning[15];
    MREG(95) = sMovementTuning[16];

    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        REG(45) = 500;
    }
}

int32_t Player_InBlockingCsMode(PlayState* play, Player* this) {
    return (this->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE)) || (this->csAction != 0) ||
           (play->transitionTrigger == TRANS_TRIGGER_START) || (this->stateFlags1 & PLAYER_STATE1_LOADING);
}

int32_t Player_InCsMode(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return Player_InBlockingCsMode(play, this) || (this->unk_6AD == 4);
}

/**
 * Checks if Player is currently locked onto a hostile actor.
 * `PLAYER_STATE1_HOSTILE_LOCK_ON` controls Player's "battle" response to hostile actors.
 *
 * Note that within Player, `Player_UpdateHostileLockOn` exists, which updates the flag and also returns the check.
 * Player can use this function instead if the flag should be checked, but not updated.
 */
int32_t Player_CheckHostileLockOn(Player* this) {
    return (this->stateFlags1 & PLAYER_STATE1_HOSTILE_LOCK_ON);
}

int32_t Player_ActionToModelGroup(int32_t actionParam) {
    switch (actionParam) {
        case PLAYER_IA_FISHING_POLE:
            return PLAYER_MODELGROUP_FISHING;
        case PLAYER_IA_SWORD_MASTER:
            return PLAYER_MODELGROUP_SWORD_AND_SHIELD;
        case PLAYER_IA_SWORD_BIGGORON:
            return PLAYER_MODELGROUP_BGS;
        case PLAYER_IA_BOW:
            return PLAYER_MODELGROUP_BOW;
        default:
            return PLAYER_MODELGROUP_DEFAULT;
    }
}

void Player_SetModelsForHoldingShield(Player* this) {
    if ((this->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
        ((this->itemAction < 0) || (this->itemAction == this->heldItemAction))) {
        if (!Player_HoldsTwoHandedWeapon(this)) {
            this->rightHandType = PLAYER_MODELTYPE_RH_SHIELD;
            this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][PLAYER_AGE];
            if (this->sheathType == PLAYER_MODELTYPE_SHEATH_SWORD_AND_SHIELD) {
                this->sheathType = PLAYER_MODELTYPE_SHEATH_SWORD;
            } else if (this->sheathType == PLAYER_MODELTYPE_SHEATH_EMPTY_AND_SHIELD) {
                this->sheathType = PLAYER_MODELTYPE_SHEATH_EMPTY;
            }
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][PLAYER_AGE];
            this->modelAnimType = PLAYER_ANIMTYPE_2;
            this->itemAction = -1;
        }
    }
}

void Player_SetModels(Player* this, int32_t modelGroup) {
    // Left hand
    this->leftHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_LEFT_HAND];
    this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][PLAYER_AGE];

    // Right hand
    this->rightHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
    this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][PLAYER_AGE];

    // Sheath
    this->sheathType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_SHEATH];
    this->sheathDLists = &sPlayerDListGroups[this->sheathType][PLAYER_AGE];

    // Waist
    this->waistDLists = &sPlayerDListGroups[gPlayerModelTypes[modelGroup][4]][PLAYER_AGE];

    Player_SetModelsForHoldingShield(this);
}

void Player_SetModelGroup(Player* this, int32_t modelGroup) {
    this->modelGroup = modelGroup;

    this->modelAnimType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_ANIM];

    if ((this->modelAnimType < PLAYER_ANIMTYPE_3) && (this->currentShield == PLAYER_SHIELD_NONE)) {
        this->modelAnimType = PLAYER_ANIMTYPE_0;
    }

    Player_SetModels(this, modelGroup);
}

void func_8008EC70(Player* this) {
    this->itemAction = this->heldItemAction;
    Player_SetModelGroup(this, Player_ActionToModelGroup(this->heldItemAction));
    this->unk_6AD = 0;
}

void Player_SetEquipmentData(PlayState* play, Player* this) {
    static bool mirrorShieldColorPatched = false;

    if (!mirrorShieldColorPatched) {
        static const char* mirrorShieldDLists[] = {
            gLinkAdultRightHandHoldingMirrorShieldNearDL,
            gLinkAdultRightHandHoldingMirrorShieldFarDL,
            gLinkAdultMirrorShieldAndSheathNearDL,
            gLinkAdultMirrorShieldAndSheathFarDL,
            gLinkAdultMirrorShieldSwordAndSheathNearDL,
            gLinkAdultMirrorShieldSwordAndSheathFarDL,
        };
        int32_t i;

        for (i = 0; i < ARRAY_COUNT(mirrorShieldDLists); i++) {
            ResourceMgr_ReplaceGfxPrimColorByName(mirrorShieldDLists[i], "PurpleMirrorShield", 215, 0, 0, 59, 0,
                                                  255);
        }
        mirrorShieldColorPatched = true;
    }

    if (this->csAction != 0x56) {
        this->currentShield = SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
        this->currentSwordItemId = ITEM_SWORD_MASTER;
        Player_SetModelGroup(this, Player_ActionToModelGroup(this->heldItemAction));
        Player_ApplyMovementTuning(play);
    }
}

void Player_ReleaseLockOn(Player* this) {
    this->focusActor = NULL;
    this->stateFlags2 &= ~PLAYER_STATE2_LOCK_ON_WITH_SWITCH;
}

/**
 * This function aims to clear Z-Target related state when it isn't in use.
 * It also handles setting a specific free fall related state that is interntwined with Z-Targeting.
 * TODO: Learn more about this and give a name to PLAYER_STATE1_19
 */
void Player_ClearZTargeting(Player* this) {
    if ((this->actor.bgCheckFlags & 1) ||
        (this->stateFlags1 & (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_IN_WATER)) ||
        (!(this->stateFlags1 & (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL)) &&
         ((this->actor.world.pos.y - this->actor.floorHeight) < 100.0f))) {
        this->stateFlags1 &=
            ~(PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS | PLAYER_STATE1_PARALLEL |
              PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    } else if (!(this->stateFlags1 &
                 (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_CLIMBING_LADDER))) {
        this->stateFlags1 |= PLAYER_STATE1_FREEFALL;
    }

    Player_ReleaseLockOn(this);
}

/**
 * Sets the "auto lock-on actor" to lock onto an actor without Player's input.
 * This function will first release any existing lock-on or (try to) release parallel.
 *
 * When using Switch Targeting, it is not possible to carry an auto lock-on actor into a normal
 * lock-on when the auto lock-on is finished.
 * This is because the `PLAYER_STATE2_LOCK_ON_WITH_SWITCH` flag is never set with an auto lock-on.
 * With Hold Targeting it is possible to keep the auto lock-on going by keeping the Z button held down.
 *
 * The auto lock-on is considered "friendly" even if the actor is actually hostile. If the auto lock-on is hostile,
 * Player's battle response will not occur (if he is actionable) and the camera behaves differently.
 * When transitioning from auto lock-on to normal lock-on (with Hold Targeting) there will be a noticeable change
 * when it switches from "friendly" mode to "hostile" mode.
 */
void Player_SetAutoLockOnActor(PlayState* play, Actor* actor) {
    Player* this = GET_PLAYER(play);

    Player_ClearZTargeting(this);
    this->focusActor = actor;
    this->autoLockOnActor = actor;
    this->stateFlags1 |= PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS;
    Camera_SetParam(Play_GetCamera(play, 0), 8, actor);
    Camera_ChangeMode(Play_GetCamera(play, 0), 2);
}

int32_t func_8008EF44(PlayState* play, int32_t ammo) {
    play->shootingGalleryStatus = ammo + 1;
    return 1;
}

int32_t Player_HasMirrorShieldEquipped(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->currentShield == PLAYER_SHIELD_MIRROR);
}

int32_t Player_HasMirrorShieldSetToDraw(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) && (this->currentShield == PLAYER_SHIELD_MIRROR);
}

int32_t Player_HoldsBow(Player* this) {
    return this->heldItemAction == PLAYER_IA_BOW;
}

int32_t Player_ActionToMeleeWeapon(int32_t actionParam) {
    switch (actionParam) {
        case PLAYER_IA_SWORD_MASTER:
            return 1;
        case PLAYER_IA_SWORD_BIGGORON:
            return 3;
        default:
            return 0;
    }
}

int32_t Player_GetMeleeWeaponHeld(Player* this) {
    return Player_ActionToMeleeWeapon(this->heldItemAction);
}

int32_t Player_HoldsTwoHandedWeapon(Player* this) {
    return this->heldItemAction == PLAYER_IA_SWORD_BIGGORON;
}

int32_t Player_HoldsBrokenKnife(Player* this) {
    return false;
}

int32_t func_8008F2BC(Player* this, int32_t actionParam) {
    switch (actionParam) {
        case PLAYER_IA_SWORD_MASTER:
            return 0;
        case PLAYER_IA_SWORD_BIGGORON:
            return 2;
        default:
            return -1;
    }
}

int32_t Player_GetEnvironmentalHazard(PlayState* play) {
    Player* this = GET_PLAYER(play);
    int32_t envHazard = { 0 };

    if (play->roomCtx.curRoom.behaviorType2 == ROOM_BEHAVIOR_TYPE2_3) { // Room is hot
        envHazard = PLAYER_ENV_HAZARD_HOTROOM - 1;
    } else if (this->underwaterTimer >= 300) { // Deep underwater
        envHazard = PLAYER_ENV_HAZARD_UNDERWATER_FREE - 1;
    } else if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) { // Swimming
        envHazard = PLAYER_ENV_HAZARD_SWIMMING - 1;
    } else {
        return PLAYER_ENV_HAZARD_NONE;
    }

    // Trigger general textboxes under certain conditions, like "It's so hot in here!"
    if (!Player_InCsMode(play)) {
        TextTriggerEntry* triggerEntry = &sTextTriggers[envHazard];

        if ((triggerEntry->flag != 0) && !(gSaveContext.textTriggerFlags & triggerEntry->flag) &&
            ((envHazard == (PLAYER_ENV_HAZARD_HOTROOM - 1)) ||
             (envHazard == (PLAYER_ENV_HAZARD_UNDERWATER_FREE - 1)))) {
            Message_StartTextbox(play, triggerEntry->textId, NULL);
            gSaveContext.textTriggerFlags |= triggerEntry->flag;
        }
    }

    return envHazard + 1;
}

uint8_t sEyeMouthIndexes[][2] = {
    { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 }, { 4, 0 }, { 5, 1 },
    { 7, 2 }, { 0, 2 }, { 3, 0 }, { 4, 0 }, { 2, 2 }, { 1, 1 }, { 0, 2 }, { 0, 0 },
};

/**
 * Link's eye and mouth textures are placed at the exact same place in adult and child Link's respective object files.
 * This allows the array to only contain the symbols for one file and have it apply to both. This is a problem for
 * shiftability, and changes will need to be made in the code to account for this in a modding scenario. The symbols
 * from adult Link's object are used here.
 */

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
// TODO: Formatting
void* sEyeTextures[2][8] = {
    { gLinkAdultEyesOpenTex, gLinkAdultEyesHalfTex, gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
      gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex, gLinkAdultEyesUnk2Tex },
    { gLinkChildEyesOpenTex, gLinkChildEyesHalfTex, gLinkChildEyesClosedfTex, gLinkChildEyesRollLeftTex,
      gLinkChildEyesRollRightTex, gLinkChildEyesShockTex, gLinkChildEyesUnk1Tex, gLinkChildEyesUnk2Tex },
};

#else
void* sEyeTextures[] = {
    gLinkAdultEyesOpenTex,      gLinkAdultEyesHalfTex,  gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
    gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex,    gLinkAdultEyesUnk2Tex,
};
#endif

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
void* sMouthTextures[2][4] = {
    {
        gLinkAdultMouth1Tex,
        gLinkAdultMouth2Tex,
        gLinkAdultMouth3Tex,
        gLinkAdultMouth4Tex,
    },
    {
        gLinkChildMouth1Tex,
        gLinkChildMouth2Tex,
        gLinkChildMouth3Tex,
        gLinkChildMouth4Tex,
    },
};
#else
void* sMouthTextures[] = {
    gLinkAdultMouth1Tex,
    gLinkAdultMouth2Tex,
    gLinkAdultMouth3Tex,
    gLinkAdultMouth4Tex,
};
#endif

Color_RGB8 sPlayerColor = { 59, 0, 255 };

void Player_DrawImpl(PlayState* play, void** skeleton, Vec3s* jointTable, int32_t dListCount, int32_t lod,
                     int32_t face, OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* data) {
    Color_RGB8* color = { 0 };
    int32_t eyeIndex = (jointTable[22].x & 0xF) - 1;
    int32_t mouthIndex = (jointTable[22].x >> 4) - 1;

    OPEN_DISPS(play->state.gfxCtx);

    if (eyeIndex < 0) {
        eyeIndex = sEyeMouthIndexes[face][0];
    }

    if (eyeIndex > 7)
        eyeIndex = 7;

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[PLAYER_AGE][eyeIndex]));
#else
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[eyeIndex]));
#endif
    if (mouthIndex < 0) {
        mouthIndex = sEyeMouthIndexes[face][1];
    }

    if (mouthIndex > 3)
        mouthIndex = 3;

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[PLAYER_AGE][mouthIndex]));
#else
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[eyeIndex]));
#endif

    color = &sPlayerColor;


        gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);


    lod = 0;

    sDListsLodOffset = lod * 2;

    SkelAnime_DrawFlexLod(play, skeleton, jointTable, dListCount, overrideLimbDraw, postLimbDraw, data, lod);

    CLOSE_DISPS(play->state.gfxCtx);
}

Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

Vec3f D_80126038[] = {
    { 1304.0f, 0.0f, 0.0f },
    { 695.0f, 0.0f, 0.0f },
};

float D_80126050[] = { 1265.0f, 826.0f };
float D_80126058[] = { SQ(13.04f), SQ(6.95f) };
float D_80126060[] = { 10.019104f, -19.925102f };
float D_80126068[] = { 5.0f, 3.0f };

Vec3f D_80126070 = { 0.0f, -300.0f, 0.0f };

void func_8008F87C(PlayState* play, Player* this, SkelAnime* skelAnime, Vec3f* pos, Vec3s* rot, int32_t thighLimbIndex,
                   int32_t shinLimbIndex, int32_t footLimbIndex) {
    Vec3f spA4;
    Vec3f sp98;
    Vec3f footprintPos;
    CollisionPoly* sp88;
    int32_t sp84;

    if ((this->actor.scale.y >= 0.0f) && !(this->stateFlags1 & PLAYER_STATE1_DEAD)) {

        float sp7C = D_80126058[PLAYER_AGE];
        float sp78 = D_80126060[PLAYER_AGE];
        float sp74 = D_80126068[PLAYER_AGE] - this->unk_6C4;

        Matrix_Push();
        Matrix_TranslateRotateZYX(pos, rot);
        Matrix_MultVec3f(&sZeroVec, &spA4);
        Matrix_TranslateRotateZYX(&D_80126038[PLAYER_AGE], &skelAnime->jointTable[shinLimbIndex]);
        Matrix_Translate(D_80126050[PLAYER_AGE], 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_MultVec3f(&sZeroVec, &sp98);
        Matrix_MultVec3f(&D_80126070, &footprintPos);
        Matrix_Pop();

        footprintPos.y += 15.0f;

        float sp80 = BgCheck_EntityRaycastFloor4(&play->colCtx, &sp88, &sp84, &this->actor, &footprintPos) + sp74;

        if (sp98.y < sp80) {
            float sp70 = sp98.x - spA4.x;
            float sp6C = sp98.y - spA4.y;
            float sp68 = sp98.z - spA4.z;

            float sp64 = sqrtf(SQ(sp70) + SQ(sp6C) + SQ(sp68));
            float sp60 = (SQ(sp64) + sp78) / (2.0f * sp64);

            float sp58 = sp7C - SQ(sp60);
            sp58 = (sp7C < SQ(sp60)) ? 0.0f : sqrtf(sp58);

            float sp54 = atan2f(sp58, sp60);

            sp6C = sp80 - spA4.y;

            sp64 = sqrtf(SQ(sp70) + SQ(sp6C) + SQ(sp68));
            sp60 = (SQ(sp64) + sp78) / (2.0f * sp64);
            float sp5C = sp64 - sp60;

            sp58 = sp7C - SQ(sp60);
            sp58 = (sp7C < SQ(sp60)) ? 0.0f : sqrtf(sp58);

            float sp50 = atan2f(sp58, sp60);

            int16_t temp1 = (M_PI - (atan2f(sp5C, sp58) + ((M_PI / 2) - sp50))) * (0x8000 / M_PI);
            temp1 = temp1 - skelAnime->jointTable[shinLimbIndex].z;

            if ((int16_t)(ABS(skelAnime->jointTable[shinLimbIndex].x) + ABS(skelAnime->jointTable[shinLimbIndex].y)) < 0) {
                temp1 += 0x8000;
            }

            int16_t temp2 = (sp50 - sp54) * (0x8000 / M_PI);
            rot->z -= temp2;

            skelAnime->jointTable[thighLimbIndex].z = skelAnime->jointTable[thighLimbIndex].z - temp2;
            skelAnime->jointTable[shinLimbIndex].z = skelAnime->jointTable[shinLimbIndex].z + temp1;
            skelAnime->jointTable[footLimbIndex].z = skelAnime->jointTable[footLimbIndex].z + temp2 - temp1;

            int32_t temp3 = func_80041D4C(&play->colCtx, sp88, sp84);

            if ((temp3 >= 2) && (temp3 < 4) && !SurfaceType_IsWallDamage(&play->colCtx, sp88, sp84)) {
                footprintPos.y = sp80;
            }
        }
    }
}

int32_t Player_OverrideLimbDrawGameplayCommon(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                          void* thisx) {
    Player* this = (Player*)thisx;

    if (limbIndex == PLAYER_LIMB_ROOT) {
        sLeftHandType = this->leftHandType;
        sRightHandType = this->rightHandType;
        D_80160000 = &this->meleeWeaponInfo[2].base;

        pos->y -= this->unk_6C4;

        if (this->unk_6C2 != 0) {
            Matrix_Translate(pos->x, ((Math_CosS(this->unk_6C2) - 1.0f) * 200.0f) + pos->y, pos->z, MTXMODE_APPLY);
            Matrix_RotateX(this->unk_6C2 * (M_PI / 0x8000), MTXMODE_APPLY);
            Matrix_RotateZYX(rot->x, rot->y, rot->z, MTXMODE_APPLY);
            pos->x = pos->y = pos->z = 0.0f;
            rot->x = rot->y = rot->z = 0;
        }
    } else {
        if (*dList != NULL) {
            D_80160000++;
        }

        if (limbIndex == PLAYER_LIMB_HEAD) {

            rot->x += this->headLimbRot.z;
            rot->y -= this->headLimbRot.y;
            rot->z += this->headLimbRot.x;
        } else if (limbIndex == PLAYER_LIMB_L_HAND) {

        } else if (limbIndex == PLAYER_LIMB_UPPER) {
            if (this->upperLimbYawSecondary != 0) {
                Matrix_RotateZ(0x44C * (M_PI / 0x8000), MTXMODE_APPLY);
                Matrix_RotateY(this->upperLimbYawSecondary * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.y != 0) {
                Matrix_RotateY(this->upperLimbRot.y * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.x != 0) {
                Matrix_RotateX(this->upperLimbRot.x * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.z != 0) {
                Matrix_RotateZ(this->upperLimbRot.z * (M_PI / 0x8000), MTXMODE_APPLY);
            }
        } else if (limbIndex == PLAYER_LIMB_L_THIGH) {
            func_8008F87C(play, this, &this->skelAnime, pos, rot, PLAYER_LIMB_L_THIGH, PLAYER_LIMB_L_SHIN,
                          PLAYER_LIMB_L_FOOT);
        } else if (limbIndex == PLAYER_LIMB_R_THIGH) {
            func_8008F87C(play, this, &this->skelAnime, pos, rot, PLAYER_LIMB_R_THIGH, PLAYER_LIMB_R_SHIN,
                          PLAYER_LIMB_R_FOOT);
            return false;
        } else {
            return false;
        }
    }

    return false;
}

int32_t Player_OverrideLimbDrawGameplayDefault(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                           void* thisx) {
    Player* this = (Player*)thisx;

    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        if (limbIndex == PLAYER_LIMB_L_HAND) {
            Gfx** dLists = this->leftHandDLists;

            if ((this->leftHandType == PLAYER_MODELTYPE_LH_OPEN) && (this->actor.speedXZ > 2.0f) &&
                       !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                dLists = &gPlayerLeftHandClosedDLs[PLAYER_AGE];
                sLeftHandType = PLAYER_MODELTYPE_LH_CLOSED;
            }

            *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
        } else if (limbIndex == PLAYER_LIMB_R_HAND) {
            Gfx** dLists = this->rightHandDLists;

            if (sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
                dLists += this->currentShield * 4;
            } else if ((this->rightHandType == PLAYER_MODELTYPE_RH_OPEN) && (this->actor.speedXZ > 2.0f) &&
                       !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                dLists = &sPlayerRightHandClosedDLs[PLAYER_AGE];
                sRightHandType = PLAYER_MODELTYPE_RH_CLOSED;
            }

            *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
        } else if (limbIndex == PLAYER_LIMB_SHEATH) {
            Gfx** dLists = this->sheathDLists;

            if ((this->sheathType == PLAYER_MODELTYPE_SHEATH_SWORD_AND_SHIELD) ||
                (this->sheathType == PLAYER_MODELTYPE_SHEATH_EMPTY_AND_SHIELD)) {
                dLists += this->currentShield * 4;

            } else {
            }

            if (dLists[sDListsLodOffset] != NULL) {
                *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
            } else {
                *dList = NULL;
            }

        } else if (limbIndex == PLAYER_LIMB_WAIST) {

            *dList = ResourceMgr_LoadGfxByName(this->waistDLists[sDListsLodOffset]);
        }
    }

    return false;
}

int32_t Player_OverrideLimbDrawGameplayFirstPerson(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                               void* thisx) {
    Player* this = (Player*)thisx;
    int32_t pcBowAim = (this == GET_PLAYER(play)) && PCInput_IsBowAimHeld() &&
                   (this->heldItemAction == PLAYER_IA_BOW);

    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        if ((this->unk_6AD != 2) && !pcBowAim) {
            *dList = NULL;
        } else if (limbIndex == PLAYER_LIMB_L_FOREARM) {
            *dList = sFirstPersonLeftForearmDLs[PLAYER_AGE];
        } else if (limbIndex == PLAYER_LIMB_L_HAND) {
            *dList = sFirstPersonLeftHandDLs[PLAYER_AGE];
        } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
            *dList = sFirstPersonRightShoulderDLs[PLAYER_AGE];
        } else if (limbIndex == PLAYER_LIMB_R_FOREARM) {
            *dList = sFirstPersonForearmDLs[PLAYER_AGE];
        } else if (limbIndex == PLAYER_LIMB_R_HAND) {
            *dList = gLinkAdultRightHandHoldingBowFirstPersonDL;
        } else {
            *dList = NULL;
        }
    }
    return false;
}

int32_t Player_OverrideLimbDrawGameplayCrawling(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* thisx) {
    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        *dList = NULL;
    }

    return false;
}

uint8_t func_80090480(PlayState* play, ColliderQuad* collider, WeaponInfo* weaponInfo, Vec3f* newTip, Vec3f* newBase) {
    if (weaponInfo->active == 0) {
        if (collider != NULL) {
            Collider_ResetQuadAT(play, &collider->base);
        }
        Math_Vec3f_Copy(&weaponInfo->tip, newTip);
        Math_Vec3f_Copy(&weaponInfo->base, newBase);
        weaponInfo->active = 1;
        return 1;
    } else if ((weaponInfo->tip.x == newTip->x) && (weaponInfo->tip.y == newTip->y) &&
               (weaponInfo->tip.z == newTip->z) && (weaponInfo->base.x == newBase->x) &&
               (weaponInfo->base.y == newBase->y) && (weaponInfo->base.z == newBase->z)) {
        if (collider != NULL) {
            Collider_ResetQuadAT(play, &collider->base);
        }
        return 0;
    } else {
        if (collider != NULL) {
            Collider_SetQuadVertices(collider, newBase, newTip, &weaponInfo->base, &weaponInfo->tip);
            CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
        }
        Math_Vec3f_Copy(&weaponInfo->base, newBase);
        Math_Vec3f_Copy(&weaponInfo->tip, newTip);
        weaponInfo->active = 1;
        return 1;
    }
}

void Player_UpdateShieldCollider(PlayState* play, Player* this, ColliderTris* collider, Vec3f* shieldSrc) {
    if (this->stateFlags1 & PLAYER_STATE1_SHIELDING) {
        Vec3f shieldDest[7];

        collider->base.colType = COLTYPE_METAL;

        for (int32_t i = 0; i < ARRAY_COUNT(shieldDest); ++i) {
            Matrix_MultVec3f(&shieldSrc[i], &shieldDest[i]);
        }
        for (int32_t i = 0; i < collider->count; ++i) {
            int32_t nextBoundary = 1 + ((i + 1) % collider->count);
            Collider_SetTrisVertices(collider, i, &shieldDest[0], &shieldDest[i + 1],
                                     &shieldDest[nextBoundary]);
        }

        CollisionCheck_SetAC(play, &play->colChkCtx, &collider->base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
    }
}

Vec3f D_80126080 = { 5000.0f, 400.0f, 0.0f };
Vec3f D_8012608C = { 5000.0f, -400.0f, 1000.0f };
Vec3f D_80126098 = { 5000.0f, 1400.0f, -1000.0f };

Vec3f D_801260A4[3] = {
    { 0.0f, 400.0f, 0.0f },
    { 0.0f, 1400.0f, -1000.0f },
    { 0.0f, -400.0f, 1000.0f },
};

void func_800906D4(PlayState* play, Player* this, Vec3f* newTipPos) {
    Vec3f newBasePos[3];

    Matrix_MultVec3f(&D_801260A4[0], &newBasePos[0]);
    Matrix_MultVec3f(&D_801260A4[1], &newBasePos[1]);
    Matrix_MultVec3f(&D_801260A4[2], &newBasePos[2]);

    func_80090480(play, NULL, &this->meleeWeaponInfo[0], &newTipPos[0], &newBasePos[0]);

    if ((this->meleeWeaponState > 0) &&
        ((this->meleeWeaponAnimation < 0x18) || (this->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING))) {
        func_80090480(play, &this->meleeWeaponQuads[0], &this->meleeWeaponInfo[1], &newTipPos[1], &newBasePos[1]);
        func_80090480(play, &this->meleeWeaponQuads[1], &this->meleeWeaponInfo[2], &newTipPos[2], &newBasePos[2]);
    }
}

void func_80090A28(Player* this, Vec3f* vecs) {
    D_8012608C.x = D_80126080.x;

    if (this->unk_845 >= 3) {
        this->unk_845 += 1;
        D_8012608C.x *= 1.0f + ((9 - this->unk_845) * 0.1f);
    }

    D_8012608C.x += 1200.0f;
    D_80126098.x = D_8012608C.x;

    Matrix_MultVec3f(&D_80126080, &vecs[0]);
    Matrix_MultVec3f(&D_8012608C, &vecs[1]);
    Matrix_MultVec3f(&D_80126098, &vecs[2]);
}

Vec3f D_801260D4 = { 1100.0f, -700.0f, 0.0f };

float sMeleeWeaponLengths[] = {
    0.0f, 4000.0f, 3000.0f, 5500.0f, 0.0f, 2500.0f,
};

Vec3f sLeftHandArrowVec3 = { 398.0f, 1419.0f, 244.0f };

BowStringData sBowStringData = { gLinkAdultBowStringDL, { 0.0f, -360.4f, 0.0f } };

// Exact six-point Mirror Shield silhouette from
// gLinkAdultRightHandHoldingMirrorShieldNearVtx1, with its recessed center.
Vec3f sRightHandLimbModelMirrorShieldVertices[] = {
    { -309.0f, 122.0f, -590.0f }, // center
    { -2098.0f, 122.0f, -312.0f },
    { -679.0f, 1375.0f, 66.0f },
    { 1030.0f, 1122.0f, -37.0f },
    { 1302.0f, 122.0f, -312.0f },
    { 1030.0f, -878.0f, -37.0f },
    { -679.0f, -1131.0f, 66.0f },
};

Vec3f sSheathLimbModelShieldOnBackPos = { 630.0f, 100.0f, -30.0f };
Vec3s sSheathLimbModelShieldOnBackZyxRot = { 0, 0, 0x7FFF };

Vec3f sLeftRightFootLimbModelFootPos[] = {
    { 200.0f, 300.0f, 0.0f },
    { 200.0f, 200.0f, 0.0f },
};

void Player_PostLimbDrawGameplay(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    Player* this = (Player*)thisx;

    if ((limbIndex > PLAYER_LIMB_NONE) && (limbIndex < PLAYER_LIMB_MAX)) {
        Vec3f limbOrigin;
        Matrix_MultVec3f(&sZeroVec, &limbOrigin);
        RecordLocalRenderedPlayerCollisionLimb(
            limbIndex, limbOrigin.x, limbOrigin.y, limbOrigin.z);
    }

    if (*dList != NULL) {
        Matrix_MultVec3f(&sZeroVec, D_80160000);
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        MtxF sp14C;
        Actor* hookedActor;

        Math_Vec3f_Copy(&this->leftHandPos, D_80160000);

        if ((this->actor.scale.y >= 0.0f) && (this->meleeWeaponState != 0)) {
            Vec3f spE4[3];

            if (Player_HoldsBrokenKnife(this)) {
                D_80126080.x = 1500.0f;
            } else {
                D_80126080.x = sMeleeWeaponLengths[Player_GetMeleeWeaponHeld(this)];
            }

            func_80090A28(this, spE4);
            func_800906D4(play, this, spE4);
        }

        if (this->actor.scale.y >= 0.0f) {
            if ((hookedActor = this->heldActor) != NULL) {
                if (this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) {
                    Matrix_MultVec3f(&sLeftHandArrowVec3, &hookedActor->world.pos);
                    Matrix_RotateZYX(0x69E8, -0x5708, 0x458E, MTXMODE_APPLY);
                    Matrix_Get(&sp14C);
                    Matrix_MtxFToYXZRotS(&sp14C, &hookedActor->world.rot, 0);
                    hookedActor->shape.rot = hookedActor->world.rot;
                } else if (this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
                    hookedActor->world.rot.y = hookedActor->shape.rot.y =
                        this->actor.shape.rot.y + this->unk_3BC.y;
                }
            } else {
                Matrix_Get(&this->mf_9E0);
                Matrix_MtxFToYXZRotS(&this->mf_9E0, &this->unk_3BC, 0);
            }
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        Actor* heldActor = this->heldActor;

        if (this->rightHandType == PLAYER_MODELTYPE_RH_BOW) {
            BowStringData* stringData = &sBowStringData;

            OPEN_DISPS(play->state.gfxCtx);

            Matrix_Push();
            Matrix_Translate(stringData->pos.x, stringData->pos.y, stringData->pos.z, MTXMODE_APPLY);

            if ((this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && (this->unk_860 >= 0) && (this->unk_834 <= 10)) {
                Vec3f sp90;

                Matrix_MultVec3f(&sZeroVec, &sp90);
                float distXYZ = Math_Vec3f_DistXYZ(D_80160000, &sp90);

                this->unk_858 = distXYZ - 3.0f;
                if (distXYZ < 3.0f) {
                    this->unk_858 = 0.0f;
                } else {
                    this->unk_858 *= 1.6f;
                    if (this->unk_858 > 1.0f) {
                        this->unk_858 = 1.0f;
                    }
                }

                this->unk_85C = -0.5f;
            }

            Matrix_Scale(1.0f, this->unk_858, 1.0f, MTXMODE_APPLY);


            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, stringData->dList);

            Matrix_Pop();

            CLOSE_DISPS(play->state.gfxCtx);
        } else if ((this->actor.scale.y >= 0.0f) && (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD)) {
            Matrix_Get(&this->shieldMf);
            Player_UpdateShieldCollider(play, this, &this->shieldCollider,
                                        sRightHandLimbModelMirrorShieldVertices);
        }

        if (this->actor.scale.y >= 0.0f) {
            if ((this->unk_862 != 0) || ((func_8002DD6C(this) == 0) && (heldActor != NULL))) {
                sGetItemRefPos.x = (this->bodyPartsPos[15].x + this->leftHandPos.x) * 0.5f;
                sGetItemRefPos.y = (this->bodyPartsPos[15].y + this->leftHandPos.y) * 0.5f;
                sGetItemRefPos.z = (this->bodyPartsPos[15].z + this->leftHandPos.z) * 0.5f;

                if (this->unk_862 == 0) {
                    Math_Vec3f_Copy(&heldActor->world.pos, &sGetItemRefPos);
                }
            }
        }
    } else if (this->actor.scale.y >= 0.0f) {
        if (limbIndex == PLAYER_LIMB_SHEATH) {
            if (this->rightHandType != PLAYER_MODELTYPE_RH_SHIELD) {
                Matrix_TranslateRotateZYX(&sSheathLimbModelShieldOnBackPos, &sSheathLimbModelShieldOnBackZyxRot);
                Matrix_Get(&this->shieldMf);
            }
        } else if (limbIndex == PLAYER_LIMB_HEAD) {
            Matrix_MultVec3f(&D_801260D4, &this->actor.focus.pos);
        } else {
            Vec3f* vec = &sLeftRightFootLimbModelFootPos[(PLAYER_AGE)];

            Actor_SetFeetPos(&this->actor, limbIndex, PLAYER_LIMB_L_FOOT, vec, PLAYER_LIMB_R_FOOT, vec);
        }
    }
}

uint32_t func_80091738(PlayState* play, uint8_t* segment, SkelAnime* skelAnime) {
    const int16_t linkObjectId = OBJECT_LINK_BOY;

    size_t size = gObjectTable[OBJECT_GAMEPLAY_KEEP].vromEnd - gObjectTable[OBJECT_GAMEPLAY_KEEP].vromStart;
    void* ptr = segment + 0x3800;
    DmaMgr_SendRequest1(ptr, gObjectTable[OBJECT_GAMEPLAY_KEEP].vromStart, size, __FILE__, __LINE__);

    size = gObjectTable[linkObjectId].vromEnd - gObjectTable[linkObjectId].vromStart;
    ptr = segment + 0x8800;
    DmaMgr_SendRequest1(ptr, gObjectTable[linkObjectId].vromStart, size, __FILE__, __LINE__);

    ptr = (void*)ALIGN16((intptr_t)ptr + size);

    gSegments[4] = VIRTUAL_TO_PHYSICAL(segment + 0x3800);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(segment + 0x8800);

    SkelAnime_InitLink(play, skelAnime, gPlayerSkelHeaders, &gPlayerAnim_link_normal_wait, 9, ptr,
                       ptr, PLAYER_LIMB_MAX);

    return size + 0x8800 + 0x90;
}
