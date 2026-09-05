/*
 * File: z_fishing.c
 * Overlay: ovl_Fishing
 * Description: Fishing Pond Elements (Owner, Fish, Props, Effects...)
 */

#include "z_fishing.h"
#include "platform/win32/PCInput.h"
#include "gameplay/FishingGameplay.h"
#include "gameplay/FishPresentation.h"
#include "gameplay/GameplayNotification.h"

#include "objects/object_fish/object_fish.h"
#include "message_data_fmt.h"
#include "vt.h"

#include "rendering/FrameInterpolation.h"
#define FLAGS ACTOR_FLAG_UPDATE_CULLING_DISABLED
#define WATER_SURFACE_Y(play) Fishing_GetWaterSurfaceY(play)
bool getShouldSpawnLoaches();
extern WaterBox zdWaterBox;

void Fishing_Init(Actor* thisx, PlayState* play);
void Fishing_Destroy(Actor* thisx, PlayState* play);
void Fishing_UpdateFish(Actor* thisx, PlayState* play);
void Fishing_UpdateOwner(Actor* thisx, PlayState* play);
void Fishing_DrawFish(Actor* thisx, PlayState* play);
void Fishing_DrawOwner(Actor* thisx, PlayState* play);
void Fishing_UpdatePortable(Actor* thisx, PlayState* play);
void Fishing_DrawPortable(Actor* thisx, PlayState* play);
void Fishing_Reset(void);
static void Fishing_SpawnWildFish(PlayState* play);

typedef struct {
    /* 0x00 */ uint8_t isLoach;
    /* 0x02 */ Vec3s pos;
    /* 0x08 */ uint8_t baseLength;
    /* 0x0C */ float perception;
} FishingFishInit; // size = 0x10

typedef enum {
    /* 0x00 */ FS_EFF_NONE,
    /* 0x01 */ FS_EFF_RIPPLE,
    /* 0x02 */ FS_EFF_DUST_SPLASH,
    /* 0x03 */ FS_EFF_WATER_DUST,
    /* 0x04 */ FS_EFF_BUBBLE,
    /* 0x05 */ FS_EFF_RAIN_DROP,
    /* 0x06 */ FS_EFF_OWNER_HAT,
    /* 0x07 */ FS_EFF_RAIN_RIPPLE,
    /* 0x08 */ FS_EFF_RAIN_SPLASH
} FishingEffectType;

#define FISHING_EFFECT_COUNT 130

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ Vec3f vel;
    /* 0x18 */ Vec3f accel;
    /* 0x24 */ uint8_t type;
    /* 0x25 */ uint8_t timer;
    /* 0x26 */ char unk_26[0x04];
    /* 0x2A */ int16_t alpha;
    /* 0x2C */ int16_t state;
    /* 0x2E */ int16_t alphaMax;
    /* 0x30 */ float scale;
    /* 0x34 */ Vec3f rot;
    uint32_t epoch;
} FishingEffect; // size = 0x40

#define POND_PROP_COUNT 140

typedef enum {
    /* 0x00 */ FS_PROP_NONE,
    /* 0x01 */ FS_PROP_REED,
    /* 0x02 */ FS_PROP_LILY_PAD,
    /* 0x03 */ FS_PROP_ROCK,
    /* 0x04 */ FS_PROP_WOOD_POST,
    /* 0x23 */ FS_PROP_INIT_STOP = 0x23
} FishingPropType;

typedef struct {
    /* 0x00 */ uint8_t type;
    /* 0x02 */ Vec3s pos;
} FishingPropInit; // size = 0x08

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ float rotX;
    /* 0x10 */ float rotY;
    /* 0x14 */ float reedAngle;
    /* 0x18 */ Vec3f projectedPos;
    /* 0x24 */ float scale;
    /* 0x28 */ int16_t lilyPadAngle;
    /* 0x2C */ float lilyPadOffset;
    /* 0x30 */ uint8_t type;
    /* 0x32 */ int16_t timer;
    /* 0x34 */ uint8_t shouldDraw;
    /* 0x38 */ float drawDistance;
    uint32_t epoch;
} FishingProp; // size = 0x3C

typedef enum {
    /* 0x00 */ FS_GROUP_FISH_NONE,
    /* 0x01 */ FS_GROUP_FISH_NORMAL
} FishingGroupFishType;

#define GROUP_FISH_COUNT 60

typedef struct {
    /* 0x00 */ uint8_t type;
    /* 0x02 */ int16_t timer;
    /* 0x04 */ Vec3f pos;
    /* 0x10 */ Vec3f homePos;
    /* 0x1C */ Vec3f projectedPos;
    /* 0x28 */ float velY;
    /* 0x2C */ float scaleX;
    /* 0x30 */ float unk_30;
    /* 0x34 */ float unk_34;
    /* 0x38 */ float unk_38;
    /* 0x3C */ int16_t unk_3C;
    /* 0x3E */ int16_t unk_3E;
    /* 0x40 */ int16_t unk_40;
    /* 0x42 */ int16_t unk_42;
    /* 0x44 */ uint8_t shouldDraw;
    uint32_t epoch;
} FishingGroupFish; // size = 0x48

typedef enum {
    /* 0x00 */ FS_LURE_STOCK,
    /* 0x01 */ FS_LURE_UNK, // hinted at with an "== 1"
    /* 0x02 */ FS_LURE_SINKING
} FishingLureTypes;

#define LINE_SEG_COUNT FISHING_LINE_SEG_COUNT
#define SINKING_LURE_SEG_COUNT 20

const ActorInit Fishing_InitVars = {
    ACTOR_FISHING,
    ACTORCAT_NPC,
    FLAGS,
    OBJECT_FISH,
    sizeof(Fishing),
    (ActorFunc)Fishing_Init,
    (ActorFunc)Fishing_Destroy,
    (ActorFunc)Fishing_UpdateFish,
    (ActorFunc)Fishing_DrawFish,
    (ActorResetFunc)Fishing_Reset,
};

static float sStormStrength = 0.0f;

static uint8_t sStormStrengthTarget = 0;

static float sFishingStormShade = 0.0f;

static Vec3f sFishingStormSfxPos = { 0.0f, 0.0f, 0.0f };

static float sStormSfxFreqScale = 0.0f;

static uint8_t sSinkingLureLocation = 0;

/// Weight of caught fish.
static float sFishOnHandLength = 0.0f;

static uint8_t sIsRodVisible = true;

static uint16_t sFishLengthToWeigh = 0;

static uint8_t sFishingCaughtTextDelay = 0;

static int32_t sFishingTimePlayed = 0;

static int16_t sOwnerTheftTimer = 0;

typedef enum {
    /* 0x00 */ FS_OWNER_BALD,
    /* 0x01 */ FS_OWNER_CAPPED,
    /* 0x02 */ FS_OWNER_HAIR
} FishingOwnerHair;

static uint8_t sOwnerHair = FS_OWNER_BALD;
static uint8_t sIsOwnersHatHooked = false; // hat is on fishing hook
static uint8_t sIsOwnersHatSunk = false;   // hat is sinking into pond.

static int16_t sRodCastState = 0;

static Vec3f sFishMouthOffset = { 500.0f, 500.0f, 0.0f };

static uint8_t D_80B7A6A4 = 0;

static float sRodBendRotY = 0.0f;
static float D_80B7A6AC = 0.0f;
static float D_80B7A6B0 = 0.0f;
static float D_80B7A6B4 = 0.0f;
static float D_80B7A6B8 = 0.0f;
static float D_80B7A6BC = 0.0f;
static float D_80B7A6C0 = 0.0f;

static int16_t sStickAdjXPrev = 0;
static int16_t sStickAdjYPrev = 0;

static uint8_t sFishingPlayerCinematicState = 0;
static uint8_t sFishingCinematicTimer = 0;
static uint8_t sSinkingLureFound = false;

static ColliderJntSphElementInit sJntSphElementsInit[12] = {
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
    {
        {
            ELEMTYPE_UNK0,
            { 0xFFCFFFFF, 0x00, 0x10 },
            { 0xFFCFFFFF, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_NONE,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 30 }, 100 },
    },
};

static ColliderJntSphInit sJntSphInit = {
    {
        COLTYPE_NONE,
        AT_TYPE_ENEMY,
        AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    12,
    sJntSphElementsInit,
};

static float sFishGroupVar = 0.0f;

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
static Vec3f sUnusedVec = { 0.0f, 0.0f, 2000.0f };

static Fishing* sFishingMain;
static uint8_t sReelLock;
static uint8_t sLinkAge;
static uint8_t sFishingFoggy;
static uint8_t sStormChanceTimer;
static float sFishingRecordLength;
static uint8_t sFishOnHandIsLoach;
static uint8_t sFishGameNumber; // increments for each purchased play. effects weather
static uint8_t sLureCaughtWith;
static int16_t sFishFightTime;
static uint8_t sPondOwnerTextIdIndex;
static uint16_t sFishesCaught;
static uint16_t sFishingCaughtTextId;
static int8_t sLureCameraZoomLevel;
static Vec3f sOwnerHeadPos;
static Vec3s sEffOwnersHatRot;
static uint8_t sLureMoveDelay; // a small delay between the lure hitting the water, and being able to reel.
static int16_t sFishingMusicDelay;
static Fishing* sFishingHookedFish;
static int16_t sFishingPlayingState;
static bool sPortableFishing;
static bool sPortableWaterFound;
static bool sPortableLineInitialized;
static float sPortableWaterSurfaceY;
static void* sPreviousSpecialEffects;

static void Fishing_StopCinematic(PlayState* play);

static int32_t Fishing_IsReelHeld(void) {
    return PCInput_IsFishingReelHeld();
}

static void Fishing_StartCatchNotification(PlayState* play) {
    char notification[96];
    const char* fishName = sFishingCaughtTextId == 0x4099 ? "loach" : "fish";

    snprintf(notification, sizeof(notification), "Caught a %s: %d lb  -  Left click to release", fishName,
             gSaveContext.minigameScore);
    GameplayNotification_Show(notification);
}
static int16_t sLureTimer; // AND'd for various effects/checks
static int16_t D_80B7E0B0;
static int16_t D_80B7E0B2;
static int16_t sRodCastTimer; // used for the inital line casting
static uint8_t sLureEquipped;
static Vec3f sLurePos;
static Vec3f sLureDrawPos;
static Vec3f sLureRot;
static Vec3f sLurePosDelta;
static Vec3f sLureCastDelta;
static float sLure1Rotate; // lure type 1 is programmed to change this.
static float sLurePosZOffset;
static float sLureRotXTarget;
static float sLureRotXStep;
static int8_t D_80B7E114;
static int16_t sRodPullback; // holding A+Down to keep the fish on line
static uint8_t D_80B7E118;
static float sRodReelingSpeed;
static uint8_t sWiggleAttraction;
static int16_t sLureBitTimer;
static uint8_t sLineHooked;
static Vec3f sLureLineSegPosDelta;
static float sLureWiggleRotYTarget;
static float sLureWigglePosY;
static int16_t sLureWiggleRotY;
static float sLureWiggleSign; // +/-1.0f
static float sRodLineSpooled; // 200 represents the full spool.
static float D_80B7E148;
static float sFishingLineScale;
static int16_t D_80B7E150;
static float sReelLinePosStep;
static Vec3f sRodTipPos;
static Vec3f sReelLinePos[LINE_SEG_COUNT];
static Vec3f sSinkingLurePos[SINKING_LURE_SEG_COUNT];
static int16_t sSinkingLureSegmentIndex;
static Vec3f sSinkingLureBasePos;
static Vec3f sPresentedLureHookPos[2];
static Vec3f sPresentedLureHookRot[2];

int32_t FishingGameplay_ReadLocalVisual(PlayState* play, FishingLocalVisual* visual) {
    Player* player;
    int32_t i;
    int32_t firstLinePoint;

    if (play == NULL || visual == NULL) {
        return false;
    }
    player = GET_PLAYER(play);
    if (player == NULL || player->heldItemAction != PLAYER_IA_FISHING_POLE || sFishingMain == NULL) {
        return false;
    }
    visual->state = (uint8_t)sRodCastState;
    visual->lureDrawOffset[0] = sLureDrawPos.x - player->actor.world.pos.x;
    visual->lureDrawOffset[1] = sLureDrawPos.y - player->actor.world.pos.y;
    visual->lureDrawOffset[2] = sLureDrawPos.z - player->actor.world.pos.z;
    firstLinePoint = CLAMP((int32_t)sRodLineSpooled, 0, LINE_SEG_COUNT - 1);
    visual->rodBendY = sRodBendRotY;
    visual->rodBendX =
        (D_80B7A6BC * (((player->unk_85C - 1.0f) * -0.25f) + 0.5f)) + (D_80B7A6AC + D_80B7A6B8);
    visual->rodTwist = player->unk_858;
    visual->rodCastX = D_80B7A6C0;
    visual->lureRotation[0] = sLureRot.x;
    visual->lureRotation[1] = sLureRot.y;
    visual->lureRotation[2] = sLureRot.z;
    visual->lureSpin = sLure1Rotate;
    visual->lureZOffset = sLurePosZOffset;
    for (i = 0; i < 2; i++) {
        visual->lureHookOffsets[i][0] = sPresentedLureHookPos[i].x - player->actor.world.pos.x;
        visual->lureHookOffsets[i][1] = sPresentedLureHookPos[i].y - player->actor.world.pos.y;
        visual->lureHookOffsets[i][2] = sPresentedLureHookPos[i].z - player->actor.world.pos.z;
        visual->lureHookRotations[i][0] = sPresentedLureHookRot[i].x;
        visual->lureHookRotations[i][1] = sPresentedLureHookRot[i].y;
    }
    visual->lineScale = sFishingLineScale;
    visual->lineGravity = D_80B7E148;
    visual->lineSpooled = (uint8_t)firstLinePoint;
    visual->sinkingLureSegmentIndex = (uint8_t)sSinkingLureSegmentIndex;
    // Fishing_DrawSinkingLure and Fishing_UpdateSinkingLure both classify the
    // active lure from sLurePos. The discovery/base position may be elsewhere
    // in the scene and must not control a remote player's lure material or
    // segment spacing.
    visual->sinkingLureUnderwater = sLurePos.y < WATER_SURFACE_Y(play);
    if (sFishingHookedFish != NULL && (sRodCastState == 4 || sRodCastState == 5)) {
        visual->fishRotation[0] = sFishingHookedFish->actor.shape.rot.x + sFishingHookedFish->unk_160;
        visual->fishRotation[1] = sFishingHookedFish->actor.shape.rot.y + sFishingHookedFish->unk_162;
        visual->fishRotation[2] = sFishingHookedFish->actor.shape.rot.z + sFishingHookedFish->unk_164;
        visual->fishLimbRotation[0] = sFishingHookedFish->fishLimbDRotZDelta;
        visual->fishLimbRotation[1] = sFishingHookedFish->fishLimbEFRotYDelta;
        visual->fishLimbRotation[2] = sFishingHookedFish->fishLimb23RotYDelta;
        visual->fishLimbRotation[3] = sFishingHookedFish->fishLimb89RotYDelta;
        visual->fishLimbRotation[4] = sFishingHookedFish->fishLimb4RotYDelta;
        visual->fishLimbRotation[5] = sFishingHookedFish->loachRotYDelta[0];
        visual->fishLimbRotation[6] = sFishingHookedFish->loachRotYDelta[1];
        visual->fishLimbRotation[7] = sFishingHookedFish->loachRotYDelta[2];
    } else {
        visual->fishRotation[0] = visual->fishRotation[1] = visual->fishRotation[2] = 0;
        for (i = 0; i < 8; i++) {
            visual->fishLimbRotation[i] = 0;
        }
    }
    return true;
}
static Vec3f sReelLineRot[LINE_SEG_COUNT];
static Vec3f sReelLineUnk[LINE_SEG_COUNT];
static Vec3f sLureHookRefPos[2];
static float sLureHookRotY[2];
static uint8_t sRodHitTimer; // brief timer for bending rod when line is snapped
static float sProjectedW;
static Vec3f sCameraEye;
static Vec3f sCameraAt;
static int16_t sSubCamId;

static float sCatchCamX;
static float sSubCamVelFactor;
static float D_80B7FED0;
static float sSinkingLureHeldY; // the lure going to its mark when being held up.
static int32_t sRandSeed0;
static int32_t sRandSeed1;
static int32_t sRandSeed2;
static FishingProp sPondProps[POND_PROP_COUNT];
static FishingGroupFish sGroupFishes[GROUP_FISH_COUNT];
static float sFishGroupAngle1;
static float sFishGroupAngle2;
static float sFishGroupAngle3;
static FishingEffect sFishingEffects[FISHING_EFFECT_COUNT];
static Vec3f sStreamSoundProjectedPos;
static int16_t sFishOnHandParams;

static float Fishing_GetWaterSurfaceY(PlayState* play) {
    if (sPortableFishing) {
        return sPortableWaterSurfaceY;
    }

    return play->colCtx.colHeader->waterBoxes->ySurface;
}

void Fishing_SetColliderElement(int32_t index, ColliderJntSph* collider, Vec3f* pos, float scale) {
    collider->elements[index].dim.worldSphere.center.x = pos->x;
    collider->elements[index].dim.worldSphere.center.y = pos->y;
    collider->elements[index].dim.worldSphere.center.z = pos->z;
    collider->elements[index].dim.worldSphere.radius =
        collider->elements[index].dim.modelSphere.radius * collider->elements[index].dim.scale * scale * 1.6f;
}

void Fishing_SeedRand(int32_t seed0, int32_t seed1, int32_t seed2) {
    sRandSeed0 = seed0;
    sRandSeed1 = seed1;
    sRandSeed2 = seed2;
}

float Fishing_RandZeroOne(void) {

    // Wichmann-Hill algorithm
    sRandSeed0 = (sRandSeed0 * 171) % 30269;
    sRandSeed1 = (sRandSeed1 * 172) % 30307;
    sRandSeed2 = (sRandSeed2 * 170) % 30323;

    float rand = (sRandSeed0 / 30269.0f) + (sRandSeed1 / 30307.0f) + (sRandSeed2 / 30323.0f);
    while (rand >= 1.0f) {
        rand -= 1.0f;
    }

    return fabsf(rand);
}

int16_t Fishing_SmoothStepToS(int16_t* pValue, int16_t target, int16_t scale, int16_t step) {

    int16_t diff = target - *pValue;
    int16_t stepSize = diff / scale;

    if (stepSize > step) {
        stepSize = step;
    }

    if (stepSize < -step) {
        stepSize = -step;
    }

    *pValue += stepSize;

    return stepSize;
}

void Fishing_SpawnRipple(Vec3f* projectedPos, FishingEffect* effect, Vec3f* pos, float scale, float rotX, int16_t alpha,
                         int16_t countLimit) {
    int16_t i;

    if ((projectedPos != NULL) && ((projectedPos->z > 500.0f) || (projectedPos->z < 0.0f))) {
        return;
    }

    for (i = 0; i < countLimit; i++) {
        if (effect->type == FS_EFF_NONE) {
            effect->type = FS_EFF_RIPPLE;
            effect->pos = *pos;
            effect->vel = sZeroVec;
            effect->accel = sZeroVec;
            effect->scale = scale * 0.0025f;
            effect->rot.x = rotX * 0.0025f;

            if (scale > 300.0f) {
                effect->alpha = 0;
                effect->alphaMax = alpha;
                effect->state = 0;
                effect->rot.y = (effect->rot.x - effect->scale) * 0.05f;
            } else {
                effect->alpha = alpha;
                effect->state = 1;
                effect->rot.y = (effect->rot.x - effect->scale) * 0.1f;
            }

            effect->epoch++;
            break;
        }

        effect++;
    }
}

void Fishing_SpawnDustSplash(Vec3f* projectedPos, FishingEffect* effect, Vec3f* pos, Vec3f* vel, float scale) {
    int16_t i;
    Vec3f accel = { 0.0f, -1.0f, 0.0f };

    if ((projectedPos != NULL) && ((projectedPos->z > 500.0f) || (projectedPos->z < 0.0f))) {
        return;
    }

    for (i = 0; i < 100; i++) {
        if ((effect->type == FS_EFF_NONE) || (effect->type == FS_EFF_RAIN_DROP) ||
            (effect->type == FS_EFF_RAIN_RIPPLE) || (effect->type == FS_EFF_RAIN_SPLASH)) {
            effect->type = FS_EFF_DUST_SPLASH;
            effect->pos = *pos;
            effect->vel = *vel;
            effect->accel = accel;
            effect->alpha = 100 + (int16_t)Rand_ZeroFloat(100.0f);
            effect->scale = scale;
            effect->epoch++;
            break;
        }

        effect++;
    }
}

void Fishing_SpawnWaterDust(Vec3f* projectedPos, FishingEffect* effect, Vec3f* pos, float scale) {
    int16_t i;
    Vec3f accel = { 0.0f, 0.05f, 0.0f };

    if ((projectedPos != NULL) && ((projectedPos->z > 500.0f) || (projectedPos->z < 0.0f))) {
        return;
    }

    for (i = 0; i < 90; i++) {
        if (effect->type == FS_EFF_NONE) {
            effect->type = FS_EFF_WATER_DUST;
            effect->pos = *pos;
            effect->vel = sZeroVec;
            effect->accel = accel;
            effect->alpha = 255;
            effect->timer = (int16_t)Rand_ZeroFloat(100.0f);
            effect->scale = scale;
            effect->rot.x = 2.0f * scale;
            effect->epoch++;
            break;
        }

        effect++;
    }
}

void Fishing_SpawnBubble(Vec3f* projectedPos, FishingEffect* effect, Vec3f* pos, float scale, uint8_t state) {
    int16_t i;
    Vec3f vel = { 0.0f, 1.0f, 0.0f };

    if ((projectedPos != NULL) && ((projectedPos->z > 500.0f) || (projectedPos->z < 0.0f))) {
        return;
    }

    for (i = 0; i < 90; i++) {
        if (effect->type == FS_EFF_NONE) {
            effect->type = FS_EFF_BUBBLE;
            effect->pos = *pos;
            effect->vel = vel;
            effect->accel = sZeroVec;
            effect->timer = (int16_t)Rand_ZeroFloat(100.0f);
            effect->scale = scale;
            effect->state = state;
            effect->epoch++;
            break;
        }

        effect++;
    }
}

void Fishing_SpawnRainDrop(FishingEffect* effect, Vec3f* pos, Vec3f* rot) {
    int16_t i;
    Vec3f velSrc;

    velSrc.x = 0.0f;
    velSrc.y = 0.0f;
    velSrc.z = 300.0f;

    effect += 30;

    for (i = 30; i < FISHING_EFFECT_COUNT; i++) {
        if (effect->type == FS_EFF_NONE) {
            effect->type = FS_EFF_RAIN_DROP;
            effect->pos = *pos;
            effect->accel = sZeroVec;
            effect->rot.x = rot->x;
            effect->rot.y = rot->y;
            effect->rot.z = rot->z;
            Matrix_RotateY(rot->y, MTXMODE_NEW);
            Matrix_RotateX(rot->x, MTXMODE_APPLY);
            Matrix_MultVec3f(&velSrc, &effect->vel);
            effect->epoch++;
            break;
        }

        effect++;
    }
}

static FishingPropInit sPondPropInits[POND_PROP_COUNT + 1] = {
    { FS_PROP_ROCK, { 529, -53, -498 } },
    { FS_PROP_ROCK, { 461, -66, -480 } },
    { FS_PROP_ROCK, { 398, -73, -474 } },
    { FS_PROP_ROCK, { -226, -52, -691 } },
    { FS_PROP_ROCK, { -300, -41, -710 } },
    { FS_PROP_ROCK, { -333, -50, -643 } },
    { FS_PROP_ROCK, { -387, -46, -632 } },
    { FS_PROP_ROCK, { -484, -43, -596 } },
    { FS_PROP_ROCK, { -409, -57, -560 } },
    { FS_PROP_WOOD_POST, { 444, -87, -322 } },
    { FS_PROP_WOOD_POST, { 447, -91, -274 } },
    { FS_PROP_WOOD_POST, { 395, -109, -189 } },
    { FS_PROP_REED, { 617, -29, 646 } },
    { FS_PROP_REED, { 698, -26, 584 } },
    { FS_PROP_REED, { 711, -29, 501 } },
    { FS_PROP_REED, { 757, -28, 457 } },
    { FS_PROP_REED, { 812, -29, 341 } },
    { FS_PROP_REED, { 856, -30, 235 } },
    { FS_PROP_REED, { 847, -31, 83 } },
    { FS_PROP_REED, { 900, -26, 119 } },
    { FS_PROP_LILY_PAD, { 861, -22, 137 } },
    { FS_PROP_LILY_PAD, { 836, -22, 150 } },
    { FS_PROP_LILY_PAD, { 829, -22, 200 } },
    { FS_PROP_LILY_PAD, { 788, -22, 232 } },
    { FS_PROP_LILY_PAD, { 803, -22, 319 } },
    { FS_PROP_LILY_PAD, { 756, -22, 348 } },
    { FS_PROP_LILY_PAD, { 731, -22, 377 } },
    { FS_PROP_LILY_PAD, { 700, -22, 392 } },
    { FS_PROP_LILY_PAD, { 706, -22, 351 } },
    { FS_PROP_LILY_PAD, { 677, -22, 286 } },
    { FS_PROP_LILY_PAD, { 691, -22, 250 } },
    { FS_PROP_LILY_PAD, { 744, -22, 290 } },
    { FS_PROP_LILY_PAD, { 766, -22, 201 } },
    { FS_PROP_LILY_PAD, { 781, -22, 128 } },
    { FS_PROP_LILY_PAD, { 817, -22, 46 } },
    { FS_PROP_LILY_PAD, { 857, -22, -50 } },
    { FS_PROP_LILY_PAD, { 724, -22, 110 } },
    { FS_PROP_LILY_PAD, { 723, -22, 145 } },
    { FS_PROP_LILY_PAD, { 728, -22, 202 } },
    { FS_PROP_LILY_PAD, { 721, -22, 237 } },
    { FS_PROP_LILY_PAD, { 698, -22, 312 } },
    { FS_PROP_LILY_PAD, { 660, -22, 349 } },
    { FS_PROP_LILY_PAD, { 662, -22, 388 } },
    { FS_PROP_LILY_PAD, { 667, -22, 432 } },
    { FS_PROP_LILY_PAD, { 732, -22, 429 } },
    { FS_PROP_LILY_PAD, { 606, -22, 366 } },
    { FS_PROP_LILY_PAD, { 604, -22, 286 } },
    { FS_PROP_LILY_PAD, { 620, -22, 217 } },
    { FS_PROP_LILY_PAD, { 663, -22, 159 } },
    { FS_PROP_LILY_PAD, { 682, -22, 73 } },
    { FS_PROP_LILY_PAD, { 777, -22, 83 } },
    { FS_PROP_LILY_PAD, { 766, -22, 158 } },
    { FS_PROP_REED, { 1073, 0, -876 } },
    { FS_PROP_REED, { 970, 0, -853 } },
    { FS_PROP_REED, { 896, 0, -886 } },
    { FS_PROP_REED, { 646, -27, -651 } },
    { FS_PROP_REED, { 597, -29, -657 } },
    { FS_PROP_REED, { 547, -32, -651 } },
    { FS_PROP_REED, { 690, -29, -546 } },
    { FS_PROP_REED, { 720, -29, -490 } },
    { FS_PROP_REED, { -756, -30, -409 } },
    { FS_PROP_REED, { -688, -34, -458 } },
    { FS_PROP_REED, { -613, -34, -581 } },
    { FS_PROP_LILY_PAD, { -593, -22, -479 } },
    { FS_PROP_LILY_PAD, { -602, -22, -421 } },
    { FS_PROP_LILY_PAD, { -664, -22, -371 } },
    { FS_PROP_LILY_PAD, { -708, -22, -316 } },
    { FS_PROP_LILY_PAD, { -718, -22, -237 } },
    { FS_PROP_REED, { -807, -36, -183 } },
    { FS_PROP_REED, { -856, -29, -259 } },
    { FS_PROP_LILY_PAD, { -814, -22, -317 } },
    { FS_PROP_LILY_PAD, { -759, -22, -384 } },
    { FS_PROP_LILY_PAD, { -718, -22, -441 } },
    { FS_PROP_LILY_PAD, { -474, -22, -567 } },
    { FS_PROP_LILY_PAD, { -519, -22, -517 } },
    { FS_PROP_LILY_PAD, { -539, -22, -487 } },
    { FS_PROP_LILY_PAD, { -575, -22, -442 } },
    { FS_PROP_LILY_PAD, { -594, -22, -525 } },
    { FS_PROP_LILY_PAD, { -669, -22, -514 } },
    { FS_PROP_LILY_PAD, { -653, -22, -456 } },
    { FS_PROP_REED, { -663, -28, -606 } },
    { FS_PROP_REED, { -708, -26, -567 } },
    { FS_PROP_REED, { -739, -27, -506 } },
    { FS_PROP_REED, { -752, -28, -464 } },
    { FS_PROP_REED, { -709, -29, -513 } },
    { FS_PROP_LILY_PAD, { -544, -22, -436 } },
    { FS_PROP_LILY_PAD, { -559, -22, -397 } },
    { FS_PROP_LILY_PAD, { -616, -22, -353 } },
    { FS_PROP_LILY_PAD, { -712, -22, -368 } },
    { FS_PROP_LILY_PAD, { -678, -22, -403 } },
    { FS_PROP_LILY_PAD, { -664, -22, -273 } },
    { FS_PROP_LILY_PAD, { -630, -22, -276 } },
    { FS_PROP_LILY_PAD, { -579, -22, -311 } },
    { FS_PROP_LILY_PAD, { -588, -22, -351 } },
    { FS_PROP_LILY_PAD, { -555, -22, -534 } },
    { FS_PROP_LILY_PAD, { -547, -22, -567 } },
    { FS_PROP_LILY_PAD, { -592, -22, -571 } },
    { FS_PROP_LILY_PAD, { -541, -22, -610 } },
    { FS_PROP_LILY_PAD, { -476, -22, -629 } },
    { FS_PROP_LILY_PAD, { -439, -22, -598 } },
    { FS_PROP_LILY_PAD, { -412, -22, -550 } },
    { FS_PROP_LILY_PAD, { -411, -22, -606 } },
    { FS_PROP_LILY_PAD, { -370, -22, -634 } },
    { FS_PROP_LILY_PAD, { -352, -22, -662 } },
    { FS_PROP_LILY_PAD, { -413, -22, -641 } },
    { FS_PROP_LILY_PAD, { -488, -22, -666 } },
    { FS_PROP_LILY_PAD, { -578, -22, -656 } },
    { FS_PROP_LILY_PAD, { -560, -22, -640 } },
    { FS_PROP_LILY_PAD, { -531, -22, -654 } },
    { FS_PROP_LILY_PAD, { -451, -22, -669 } },
    { FS_PROP_LILY_PAD, { -439, -22, -699 } },
    { FS_PROP_LILY_PAD, { -482, -22, -719 } },
    { FS_PROP_LILY_PAD, { -524, -22, -720 } },
    { FS_PROP_LILY_PAD, { -569, -22, -714 } },
    { FS_PROP_REED, { -520, -27, -727 } },
    { FS_PROP_REED, { -572, -28, -686 } },
    { FS_PROP_REED, { -588, -32, -631 } },
    { FS_PROP_REED, { -622, -34, -571 } },
    { FS_PROP_REED, { -628, -36, -510 } },
    { FS_PROP_REED, { -655, -36, -466 } },
    { FS_PROP_REED, { -655, -41, -393 } },
    { FS_PROP_REED, { -661, -47, -328 } },
    { FS_PROP_REED, { -723, -40, -287 } },
    { FS_PROP_REED, { -756, -33, -349 } },
    { FS_PROP_REED, { -755, -43, -210 } },
    { FS_PROP_LILY_PAD, { -770, -22, -281 } },
    { FS_PROP_LILY_PAD, { -750, -22, -313 } },
    { FS_PROP_LILY_PAD, { -736, -22, -341 } },
    { FS_PROP_LILY_PAD, { -620, -22, -418 } },
    { FS_PROP_LILY_PAD, { -601, -22, -371 } },
    { FS_PROP_LILY_PAD, { -635, -22, -383 } },
    { FS_PROP_LILY_PAD, { -627, -22, -311 } },
    { FS_PROP_LILY_PAD, { -665, -22, -327 } },
    { FS_PROP_LILY_PAD, { -524, -22, -537 } },
    { FS_PROP_LILY_PAD, { -514, -22, -579 } },
    { FS_PROP_LILY_PAD, { -512, -22, -623 } },
    { FS_PROP_LILY_PAD, { -576, -22, -582 } },
    { FS_PROP_LILY_PAD, { -600, -22, -608 } },
    { FS_PROP_LILY_PAD, { -657, -22, -531 } },
    { FS_PROP_LILY_PAD, { -641, -22, -547 } },
    { FS_PROP_INIT_STOP, { 0 } },
};

void Fishing_InitPondProps(Fishing* this, PlayState* play) {
    FishingProp* prop = &sPondProps[0];
    int16_t i;

    Fishing_SeedRand(1, 29100, 9786);

    for (i = 0; i < POND_PROP_COUNT; i++) {
        if (sPondPropInits[i].type == FS_PROP_INIT_STOP) {
            break;
        }

        prop->epoch++;
        prop->type = sPondPropInits[i].type;
        prop->pos.x = sPondPropInits[i].pos.x;
        prop->pos.y = sPondPropInits[i].pos.y;
        prop->pos.z = sPondPropInits[i].pos.z;
        prop->rotX = 0.0f;
        prop->reedAngle = 0.0f;

        prop->timer = Rand_ZeroFloat(100.0f);
        prop->drawDistance = 800.0f;

        if (prop->type == FS_PROP_REED) {
            prop->scale = (Fishing_RandZeroOne() * 0.25f) + 0.75f;
            prop->reedAngle = Rand_ZeroFloat(2 * M_PI);
            if (sLinkAge == LINK_AGE_CHILD) {
                prop->scale *= 0.6f;
            }
            prop->drawDistance = 1200.0f;
        } else if (prop->type == FS_PROP_WOOD_POST) {
            prop->scale = 0.08f;
            prop->drawDistance = 1200.0f;
            Vec3f colliderPos = prop->pos;
            colliderPos.y += 50.0f;
            Fishing_SetColliderElement(i, &sFishingMain->collider, &colliderPos, prop->scale * 3.5f);
        } else if (prop->type == FS_PROP_LILY_PAD) {
            prop->scale = (Fishing_RandZeroOne() * 0.3f) + 0.5f;
            prop->rotY = Rand_ZeroFloat(2 * M_PI);
            if (sLinkAge == LINK_AGE_CHILD) {
                if ((i % 4) != 0) {
                    prop->scale *= 0.6f;
                } else {
                    prop->type = FS_PROP_NONE;
                }
            }
        } else {
            prop->scale = (Fishing_RandZeroOne() * 0.1f) + 0.3f;
            prop->rotY = Rand_ZeroFloat(2 * M_PI);
            prop->drawDistance = 1000.0f;
            Fishing_SetColliderElement(i, &sFishingMain->collider, &prop->pos, prop->scale);
        }

        prop++;
    }
}

static FishingFishInit sFishInits[] = {
    { 0, { 666, -45, 354 }, 38, 0.1f },    { 0, { 681, -45, 240 }, 36, 0.1f },   { 0, { 670, -45, 90 }, 41, 0.05f },
    { 0, { 615, -45, -450 }, 35, 0.2f },   { 0, { 500, -45, -420 }, 39, 0.1f },  { 0, { 420, -45, -550 }, 44, 0.05f },
    { 0, { -264, -45, -640 }, 40, 0.1f },  { 0, { -470, -45, -540 }, 34, 0.2f }, { 0, { -557, -45, -430 }, 54, 0.01f },
    { 0, { -260, -60, -330 }, 47, 0.05f }, { 0, { -500, -60, 330 }, 42, 0.06f }, { 0, { 428, -40, -283 }, 33, 0.2f },
    { 0, { 409, -70, -230 }, 57, 0.0f },   { 0, { 450, -67, -300 }, 63, 0.0f },  { 0, { -136, -65, -196 }, 71, 0.0f },
    { 1, { -561, -35, -547 }, 45, 0.0f },  { 1, { 667, -35, 317 }, 43, 0.0f },
};

static float Fishing_GetFishPerception(Fishing* this) {
    if (this->isWild) {
        return this->perception;
    }

    return sFishInits[this->actor.params - EN_FISH_PARAM].perception;
}

static float Fishing_GetRandomFishTargetX(Fishing* this, float range) {
    float target = Rand_CenteredFloat(range) + (this->isWild ? this->actor.home.pos.x : 0.0f);

    return this->isWild ? CLAMP(target, this->wildMinX, this->wildMaxX) : target;
}

static float Fishing_GetRandomFishTargetZ(Fishing* this, float range) {
    float target = Rand_CenteredFloat(range) + (this->isWild ? this->actor.home.pos.z : 0.0f);

    return this->isWild ? CLAMP(target, this->wildMinZ, this->wildMaxZ) : target;
}

static uint32_t Fishing_WildHash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352D;
    value ^= value >> 15;
    value *= 0x846CA68B;
    return value ^ (value >> 16);
}

static float Fishing_WildRandom01(uint32_t seed) {
    return (Fishing_WildHash(seed) & 0x00FFFFFF) / 16777216.0f;
}

static float Fishing_GetDeterministicPondLength(PlayState* play, Actor* actor, float baseLength) {
    uint32_t seed = ((uint32_t)play->sceneNum * 0x9E3779B9U) ^ ((uint32_t)(int32_t)actor->params * 0x85EBCA6BU) ^
               ((uint32_t)(int32_t)actor->home.pos.x * 0xC2B2AE35U) ^
               ((uint32_t)(int32_t)actor->home.pos.z * 0x27D4EB2FU);
    float length = baseLength + Fishing_WildRandom01(seed) * 4.99999f;

    if ((length >= 65.0f) && (Fishing_WildRandom01(seed ^ 0x63D83595U) < 0.05f)) {
        length += Fishing_WildRandom01(seed ^ 0xA511E9B3U) * 7.99999f;
    }
    return length;
}

static int32_t Fishing_FindWildFishSpawn(PlayState* play, WaterBox* waterBox, uint32_t seed, bool loach,
                                     float minX, float minZ, float width, float depth, float marginX, float marginZ,
                                     Vec3f* spawnPos) {
    CollisionPoly* floorPoly;
    int32_t floorBgId;
    int32_t attempt;

    spawnPos->y = waterBox->ySurface - (loach ? 45.0f : 25.0f);
    for (attempt = 0; attempt < 32; attempt++) {
        uint32_t candidateSeed = seed ^ ((uint32_t)attempt * 0xD3A2646CU);
        Vec3f floorCheckPos;

        spawnPos->x = minX + marginX + Fishing_WildRandom01(candidateSeed) * (width - (marginX * 2.0f));
        spawnPos->z = minZ + marginZ + Fishing_WildRandom01(candidateSeed ^ 0xA511E9B3U) *
                                              (depth - (marginZ * 2.0f));
        floorCheckPos.x = spawnPos->x;
        floorCheckPos.y = waterBox->ySurface + 10.0f;
        floorCheckPos.z = spawnPos->z;
        float floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &floorPoly, &floorBgId, &floorCheckPos);
        if ((floorY != BGCHECK_Y_MIN) && (floorY <= (spawnPos->y - 20.0f))) {
            return true;
        }
    }
    return false;
}

static void Fishing_SpawnWildFishInWaterBox(PlayState* play, WaterBox* waterBox, int32_t waterIndex, int32_t fishIndex,
                                            bool loach) {
    float width = fabsf(waterBox->xLength);
    float depth = fabsf(waterBox->zLength);
    float minX = (waterBox->xLength >= 0) ? waterBox->xMin : waterBox->xMin + waterBox->xLength;
    float minZ = (waterBox->zLength >= 0) ? waterBox->zMin : waterBox->zMin + waterBox->zLength;
    float marginX = (width > 80.0f) ? 40.0f : width * 0.2f;
    float marginZ = (depth > 80.0f) ? 40.0f : depth * 0.2f;
    uint32_t seed = ((uint32_t)play->sceneNum * 0x9E3779B9U) ^ ((uint32_t)(waterIndex + 1) * 0x85EBCA6BU) ^
               ((uint32_t)(fishIndex + 1) * 0x165667B1U) ^ (loach ? 0xC2B2AE35U : 0x27D4EB2FU);
    Vec3f spawnPos;
    Fishing* fish;

    if (!Fishing_FindWildFishSpawn(play, waterBox, seed, loach, minX, minZ, width, depth, marginX, marginZ,
                                   &spawnPos)) {
        return;
    }
    fish = (Fishing*)Actor_Spawn(&play->actorCtx, play, ACTOR_FISHING, spawnPos.x, spawnPos.y, spawnPos.z, 0,
                                          (int16_t)(Fishing_WildHash(seed ^ 0x63D83595U) & 0xFFFF), 0,
                                          loach ? EN_LOACH_WILD : EN_FISH_WILD);

    if (fish != NULL) {
        fish->wildWaterBox = waterBox;
        fish->wildWaterSurfaceY = waterBox->ySurface;
        fish->wildMinX = minX + marginX;
        fish->wildMaxX = minX + width - marginX;
        fish->wildMinZ = minZ + marginZ;
        fish->wildMaxZ = minZ + depth - marginZ;
        fish->actor.home.pos = fish->actor.world.pos;
    }
}

static void Fishing_SpawnWildFish(PlayState* play) {
    const int16_t normalFishCount = 12;
    const int16_t loachCount = 4;
    CollisionHeader* colHeader = play->colCtx.colHeader;
    int16_t i;
    int16_t fishIndex;

    if ((colHeader == NULL) || (colHeader->waterBoxes == NULL)) {
        return;
    }

    for (i = 0; i < colHeader->numWaterBoxes; i++) {
        WaterBox* waterBox = &colHeader->waterBoxes[i];

        if ((waterBox->xLength == 0) || (waterBox->zLength == 0)) {
            continue;
        }

        for (fishIndex = 0; fishIndex < normalFishCount; fishIndex++) {
            Fishing_SpawnWildFishInWaterBox(play, waterBox, i, fishIndex, false);
        }
        for (fishIndex = 0; fishIndex < loachCount; fishIndex++) {
            Fishing_SpawnWildFishInWaterBox(play, waterBox, i, fishIndex, true);
        }
    }

    if (play->sceneNum == SCENE_ZORAS_DOMAIN) {
        for (fishIndex = 0; fishIndex < normalFishCount; fishIndex++) {
            Fishing_SpawnWildFishInWaterBox(play, &zdWaterBox, colHeader->numWaterBoxes, fishIndex, false);
        }
        for (fishIndex = 0; fishIndex < loachCount; fishIndex++) {
            Fishing_SpawnWildFishInWaterBox(play, &zdWaterBox, colHeader->numWaterBoxes, fishIndex, true);
        }
    }
}

static InitChainEntry sInitChain[] = {
    ICHAIN_U8(targetMode, 5, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 0, ICHAIN_STOP),
};

int32_t Fishing_EnsurePresentedPopulation(PlayState* play) {
    Actor* actor = { 0 };
    Player* player = { 0 };

    if (play == NULL) {
        return false;
    }
    actor = play->actorCtx.actorLists[ACTORCAT_NPC].head;
    while (actor != NULL) {
        if ((actor->id == ACTOR_FISHING) &&
            ((actor->params == EN_FISH_OWNER) || (actor->params == EN_FISH_PORTABLE))) {
            return true;
        }
        actor = actor->next;
    }

    player = GET_PLAYER(play);
    if (player == NULL) {
        return false;
    }
    return Actor_Spawn(&play->actorCtx, play, ACTOR_FISHING, player->actor.world.pos.x,
                       player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0,
                       EN_FISH_PORTABLE) != NULL;
}

void Fishing_Init(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    Fishing* this = (Fishing*)thisx;
    uint16_t fishCount = { 0 };
    int16_t i;

    Actor_ProcessInitChain(thisx, sInitChain);
    ActorShape_Init(&thisx->shape, 0.0f, NULL, 0.0f);
    this->isWild = false;
    this->remotePresentationActive = false;

    // The pond uses its adult rules even though the surrounding world is
    // intentionally loaded with child-age scene variants.
    sLinkAge = LINK_AGE_ADULT;

    if (thisx->params == EN_FISH_PORTABLE) {
        sFishingMain = this;
        sPortableFishing = true;
        sPortableWaterFound = false;
        sPortableLineInitialized = false;
        sPortableWaterSurfaceY = -32000.0f;
        sPreviousSpecialEffects = play->specialEffects;
        play->specialEffects = sFishingEffects;

        for (i = 0; i < FISHING_EFFECT_COUNT; i++) {
            sFishingEffects[i].type = FS_EFF_NONE;
        }

        sFishingHookedFish = NULL;
        sLineHooked = false;
        sIsOwnersHatHooked = false;
        sIsRodVisible = true;
        sFishingPlayingState = 1;
        sFishingPlayerCinematicState = 0;
        thisx->room = -1;
        thisx->flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED;
        thisx->update = Fishing_UpdatePortable;
        thisx->draw = Fishing_DrawPortable;
        Fishing_SpawnWildFish(play);
    } else if (thisx->params < EN_FISH_PARAM) {
        sPortableFishing = false;
        sReelLock = 0;
        sFishingMain = this;
        Collider_InitJntSph(play, &sFishingMain->collider);
        Collider_SetJntSph(play, &sFishingMain->collider, thisx, &sJntSphInit, sFishingMain->colliderElements);

        thisx->params = EN_FISH_OWNER;

        SkelAnime_InitFlex(play, &this->skelAnime, &gFishingOwnerSkel, &gFishingOwnerAnim, NULL, NULL, 0);
        Animation_MorphToLoop(&this->skelAnime, &gFishingOwnerAnim, 0.0f);

        thisx->update = Fishing_UpdateOwner;
        thisx->draw = Fishing_DrawOwner;

        thisx->shape.rot.y = -0x6000;
        thisx->world.pos.x = 160.0f;
        thisx->world.pos.y = -2.0f;
        thisx->world.pos.z = 1208.0f;

        Actor_SetScale(thisx, 0.011f);

        thisx->focus.pos = thisx->world.pos;
        thisx->focus.pos.y += 75.0f;
        thisx->flags |= ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY;

        if (sLinkAge != LINK_AGE_CHILD) {
            if (HIGH_SCORE(HS_FISHING) & HS_FISH_STOLE_HAT) {
                sOwnerHair = FS_OWNER_BALD;
            } else {
                sOwnerHair = FS_OWNER_CAPPED;
            }
        } else {
            sOwnerHair = FS_OWNER_HAIR;
        }

        sOwnerTheftTimer = 20;
        play->specialEffects = sFishingEffects;
        gTimeIncrement = 1;
        sFishingPlayingState = 0;
        sFishingMusicDelay = 10;

        Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x0100FF);

        if (sLinkAge == LINK_AGE_CHILD) {
            if ((HIGH_SCORE(HS_FISHING) & HS_FISH_LENGTH_CHILD) != 0) {
                sFishingRecordLength = HIGH_SCORE(HS_FISHING) & HS_FISH_LENGTH_CHILD;
            } else {
                sFishingRecordLength = 40.0f; // 6 lbs
            }
        } else {
            if ((HIGH_SCORE(HS_FISHING) & HS_FISH_LENGTH_ADULT) != 0) {
                sFishingRecordLength = (HIGH_SCORE(HS_FISHING) & HS_FISH_LENGTH_ADULT) >> 0x18;
            } else {
                sFishingRecordLength = 45.0f; // 7 lbs
            }
        }

        sFishGameNumber = (HIGH_SCORE(HS_FISHING) & (HS_FISH_PLAYED * 255)) >> 0x10;
        if ((sFishGameNumber & 7) == 7) {
            play->roomCtx.unk_74[0] = 90;
            sFishingFoggy = 1;
        } else {
            play->roomCtx.unk_74[0] = 40;
            sFishingFoggy = 0;
        }

        if (((sFishGameNumber & 7) == 6) || (KREG(3) != 0)) {
            sStormChanceTimer = 100;
            if (KREG(3) != 0) {
                KREG(3) = 0;
                HIGH_SCORE(HS_FISHING) &= ~(HS_FISH_PLAYED * 255);
                HIGH_SCORE(HS_FISHING) |= (HS_FISH_PLAYED * 6);
            }
        } else {
            sStormChanceTimer = 0;
        }

        for (i = 0; i < FISHING_EFFECT_COUNT; i++) {
            sFishingEffects[i].type = FS_EFF_NONE;
        }

        for (i = 0; i < POND_PROP_COUNT; i++) {
            sPondProps[i].type = FS_PROP_NONE;
        }

        sFishGroupAngle1 = 0.7f;
        sFishGroupAngle2 = 2.3f;
        sFishGroupAngle3 = 4.6f;

        for (i = 0; i < GROUP_FISH_COUNT; i++) {
            FishingGroupFish* fish = &sGroupFishes[i];

            fish->epoch++;

            fish->type = FS_GROUP_FISH_NORMAL;

            if (i <= 20) {
                fish->homePos.x = fish->pos.x = sinf(sFishGroupAngle1) * 720.0f;
                fish->homePos.z = fish->pos.z = cosf(sFishGroupAngle1) * 720.0f;
            } else if (i <= 40) {
                fish->homePos.x = fish->pos.x = sinf(sFishGroupAngle2) * 720.0f;
                fish->homePos.z = fish->pos.z = cosf(sFishGroupAngle2) * 720.0f;
            } else {
                fish->homePos.x = fish->pos.x = sinf(sFishGroupAngle3) * 720.0f;
                fish->homePos.z = fish->pos.z = cosf(sFishGroupAngle3) * 720.0f;
            }

            fish->homePos.y = fish->pos.y = -35.0f;

            fish->timer = Rand_ZeroFloat(100.0f);

            fish->unk_3C = 0;
            fish->unk_3E = 0;
            fish->unk_40 = 0;

            if (sLinkAge != LINK_AGE_CHILD) {
                if (((i >= 15) && (i < 20)) || ((i >= 35) && (i < 40)) || ((i >= 55) && (i < 60))) {
                    fish->type = FS_GROUP_FISH_NONE;
                }
            }
        }

        Fishing_InitPondProps(this, play);
        Actor_Spawn(&play->actorCtx, play, ACTOR_FISHING, 0.0f, 0.0f, 0.0f, 0, 0, 0, 200);

        // Loach(es) will spawn every fourth game, or if "Loaches Always Appear" is enabled
        if (getShouldSpawnLoaches()) {
            // Fishes 16 and 17 are loaches. Only 16 is spawned as adult; child also spawns 17.
            if (sLinkAge != LINK_AGE_CHILD) {
                fishCount = 16;
            } else {
                fishCount = 17;
            }
        } else {
            fishCount = 15;
        }

        for (i = 0; i < fishCount; i++) {
            Actor_Spawn(&play->actorCtx, play, ACTOR_FISHING, sFishInits[i].pos.x, sFishInits[i].pos.y,
                        sFishInits[i].pos.z, 0, Rand_ZeroFloat(0x10000), 0, 100 + i);
        }
    } else {
        if ((thisx->params < (EN_FISH_PARAM + 15)) || (thisx->params == EN_FISH_AQUARIUM) ||
            (thisx->params == EN_FISH_WILD)) {
            SkelAnime_InitFlex(play, &this->skelAnime, &gFishingFishSkel, &gFishingFishAnim, NULL, NULL, 0);
            Animation_MorphToLoop(&this->skelAnime, &gFishingFishAnim, 0.0f);
        } else {
            SkelAnime_InitFlex(play, &this->skelAnime, &gFishingLoachSkel, &gFishingLoachAnim, NULL, NULL, 0);
            Animation_MorphToLoop(&this->skelAnime, &gFishingLoachAnim, 0.0f);
        }

        SkelAnime_Update(&this->skelAnime);

        if (thisx->params == EN_FISH_AQUARIUM) {
            this->fishState = 100;
            Actor_ChangeCategory(play, &play->actorCtx, thisx, ACTORCAT_PROP);
            thisx->targetMode = 0;
            thisx->flags |= ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY;
            this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
        } else if ((thisx->params == EN_FISH_WILD) || (thisx->params == EN_LOACH_WILD)) {
            this->fishState = 10;
            this->fishStateNext = 10;
            this->isWild = true;
            this->isLoach = thisx->params == EN_LOACH_WILD;
            this->perception = this->isLoach ? 0.20f : 0.35f;
            {
                uint32_t lengthSeed = ((uint32_t)play->sceneNum * 0x9E3779B9U) ^
                                 ((uint32_t)(int32_t)thisx->home.pos.x * 0x85EBCA6BU) ^
                                 ((uint32_t)(int32_t)thisx->home.pos.z * 0xC2B2AE35U) ^
                                 (this->isLoach ? 0xA511E9B3U : 0x63D83595U);
                this->fishLength = (this->isLoach ? 42.0f : 34.0f) + Fishing_WildRandom01(lengthSeed) * 18.0f;
            }
            this->fishTargetPos = thisx->home.pos;
            thisx->room = -1;
            thisx->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        } else {
            this->fishState = 10;
            this->fishStateNext = 10;

            this->isLoach = sFishInits[thisx->params - EN_FISH_PARAM].isLoach;
            this->perception = sFishInits[thisx->params - EN_FISH_PARAM].perception;
            // Every client and the dedicated server derive the same fish size
            // from immutable actor identity. Global RNG order differs between
            // clients and previously produced conflicting sizes and weights.
            this->fishLength = Fishing_GetDeterministicPondLength(
                play, thisx, sFishInits[thisx->params - EN_FISH_PARAM].baseLength);

            if (KREG(6) != 0) {
                this->fishLength = KREG(6) + 80.0f;
            }

            // "Come back when you get older! The fish will be bigger, too!"
            if (sLinkAge == LINK_AGE_CHILD) {
                this->fishLength *= 0.73f;
            }
        }
    }
}

void Fishing_Destroy(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    Fishing* this = (Fishing*)thisx;

    if (sFishingHookedFish == this) {
        FishingGameplay_SubmitAction(FISHING_GAMEPLAY_ACTION_RELEASE);
        sFishingHookedFish = NULL;
    }

    if (thisx->params == EN_FISH_PORTABLE) {
        if (play->specialEffects == sFishingEffects) {
            play->specialEffects = sPreviousSpecialEffects;
        }
        sPreviousSpecialEffects = NULL;
        sFishingMain = NULL;
        sPortableFishing = false;
        sFishingPlayingState = 0;
        return;
    }

    SkelAnime_Free(&this->skelAnime, play);

    if (thisx->params == EN_FISH_AQUARIUM) {
        LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
    } else if (thisx->params == EN_FISH_OWNER) {
        Collider_DestroyJntSph(play, &this->collider);
    }
}

void Fishing_UpdateEffects(FishingEffect* effect, PlayState* play) {
    float rippleY = { 0 };
    int16_t i;

    for (i = 0; i < FISHING_EFFECT_COUNT; i++) {
        if (effect->type) {
            effect->timer++;
            effect->pos.x += effect->vel.x;
            effect->pos.y += effect->vel.y;
            effect->pos.z += effect->vel.z;
            effect->vel.y += effect->accel.y;

            if (effect->type == FS_EFF_RIPPLE) {
                Math_ApproachF(&effect->scale, effect->rot.x, 0.2f, effect->rot.y);

                if (effect->state == 0) {
                    effect->alpha += 20;

                    if (effect->alpha >= effect->alphaMax) {
                        effect->alpha = effect->alphaMax;
                        effect->state++;
                    }
                } else {
                    effect->alpha -= 8;

                    if (effect->alpha <= 0) {
                        effect->type = FS_EFF_NONE;
                    }
                }
            } else if (effect->type == FS_EFF_WATER_DUST) {
                Math_ApproachF(&effect->scale, effect->rot.x, 0.1f, 0.1f);
                effect->alpha -= 10;

                if (effect->pos.y > (WATER_SURFACE_Y(play) - 5.0f)) {
                    effect->accel.y = 0.0f;
                    effect->vel.y = 0.0f;
                    effect->alpha -= 5;
                }

                if (effect->alpha <= 0) {
                    effect->type = FS_EFF_NONE;
                }
            } else if (effect->type == FS_EFF_BUBBLE) {
                if (effect->state == 0) {
                    rippleY = WATER_SURFACE_Y(play);
                } else {
                    rippleY = 69.0f;
                }

                if (effect->pos.y >= rippleY) {
                    effect->type = FS_EFF_NONE;

                    if (Rand_ZeroOne() < 0.3f) {
                        Vec3f pos = effect->pos;
                        pos.y = rippleY;
                        Fishing_SpawnRipple(NULL, play->specialEffects, &pos, 20.0f, 60.0f, 150, 90);
                    }
                }
            } else if (effect->type == FS_EFF_DUST_SPLASH) {
                if (effect->vel.y < -20.0f) {
                    effect->vel.y = -20.0f;
                    effect->accel.y = 0.0f;
                }

                if (effect->pos.y <= WATER_SURFACE_Y(play)) {
                    effect->type = FS_EFF_NONE;
                    if (Rand_ZeroOne() < 0.5f) {
                        Vec3f pos = effect->pos;
                        pos.y = WATER_SURFACE_Y(play);
                        Fishing_SpawnRipple(NULL, play->specialEffects, &pos, 40.0f, 110.0f, 150, 90);
                    }
                }
            } else if (effect->type == FS_EFF_RAIN_DROP) {
                if (effect->pos.y < WATER_SURFACE_Y(play)) {
                    float sqDistXZ = SQ(effect->pos.x) + SQ(effect->pos.z);

                    if (sqDistXZ > SQ(920.0f)) {
                        effect->pos.y = WATER_SURFACE_Y(play) + ((sqrtf(sqDistXZ) - 920.0f) * 0.11f);
                        effect->timer = KREG(17) + 2;
                        effect->type = FS_EFF_RAIN_SPLASH;
                        effect->scale = (KREG(18) + 30) * 0.001f;
                    } else {
                        effect->pos.y = WATER_SURFACE_Y(play) + 3.0f;
                        effect->timer = 0;
                        if (Rand_ZeroOne() < 0.75f) {
                            effect->type = FS_EFF_RAIN_RIPPLE;
                            effect->vel = sZeroVec;
                            effect->scale = (KREG(18) + 30) * 0.001f;
                        } else {
                            effect->type = FS_EFF_NONE;
                        }
                    }

                    effect->vel = sZeroVec;
                }
            } else if (effect->type >= FS_EFF_RAIN_RIPPLE) {
                effect->scale += (KREG(18) + 30) * 0.001f;

                if (effect->timer >= 6) {
                    effect->type = FS_EFF_NONE;
                }
            } else if (effect->type == FS_EFF_OWNER_HAT) {

                effect->scale = 10 * .001f;

                Math_ApproachS(&sEffOwnersHatRot.y, 0, 20, 100);
                Math_ApproachS(&sEffOwnersHatRot.x, 0, 20, 100);
                Math_ApproachS(&sEffOwnersHatRot.z, -0x4000, 20, 100);

                float sqDistXZ = SQ(effect->pos.x) + SQ(effect->pos.z);
                float bottomY = WATER_SURFACE_Y(play) + ((sqrtf(sqDistXZ) - 920.0f) * 0.147f);

                if (effect->pos.y > (bottomY - 10.0f)) {
                    effect->pos.y -= 0.1f;
                }

                if ((effect->timer % 16) == 0) {
                    Vec3f pos = effect->pos;
                    pos.y = WATER_SURFACE_Y(play);
                    Fishing_SpawnRipple(NULL, play->specialEffects, &pos, 30.0f, 300.0f, 150, 90);
                }

                if (effect->state >= 0) {
                    effect->state++;
                }

                if (effect->state == 30) {
                    Message_StartTextbox(play, 0x40B3, NULL);
                }

                if ((effect->state >= 100) && (Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT)) {
                    if (Message_ShouldAdvance(play) || (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                        Message_CloseTextbox(play);
                        Rupees_ChangeBy(-50);
                        effect->state = -1;
                    }
                }
            }
        }

        effect++;
    }
}

void Fishing_DrawEffects(FishingEffect* effect, PlayState* play) {
    uint8_t flag = 0;
    float rotY = { 0 };
    int16_t i;
    FishingEffect* firstEffect = effect;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();

    gDPPipeSync(POLY_XLU_DISP++);

    for (i = 0; i < 100; i++) {
        if (effect->type == FS_EFF_RIPPLE) {
            FrameInterpolation_RecordOpenChild(effect, effect->epoch);
            if (flag == 0) {
                gSPDisplayList(POLY_XLU_DISP++, gFishingRippleMaterialDL);
                gDPSetEnvColor(POLY_XLU_DISP++, 155, 155, 155, 0);
                flag++;
            }

            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, effect->alpha);

            Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
            Matrix_Scale(effect->scale, 1.0f, effect->scale, MTXMODE_APPLY);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_XLU_DISP++, gFishingRippleModelDL);
            FrameInterpolation_RecordCloseChild();
        }
        effect++;
    }

    effect = firstEffect;
    flag = 0;
    for (i = 0; i < 100; i++) {
        if (effect->type == FS_EFF_DUST_SPLASH) {
            FrameInterpolation_RecordOpenChild(effect, effect->epoch);
            if (flag == 0) {
                gSPDisplayList(POLY_XLU_DISP++, gFishingDustSplashMaterialDL);
                gDPSetEnvColor(POLY_XLU_DISP++, 200, 200, 200, 0);
                flag++;
            }

            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 180, 180, 180, effect->alpha);

            Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(effect->scale, effect->scale, 1.0f, MTXMODE_APPLY);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_XLU_DISP++, gFishingDustSplashModelDL);
            FrameInterpolation_RecordCloseChild();
        }
        effect++;
    }

    effect = firstEffect;
    flag = 0;
    for (i = 0; i < 100; i++) {
        if (effect->type == FS_EFF_WATER_DUST) {
            FrameInterpolation_RecordOpenChild(effect, effect->epoch);
            if (flag == 0) {
                gSPDisplayList(POLY_OPA_DISP++, gFishingWaterDustMaterialDL);
                gDPSetEnvColor(POLY_OPA_DISP++, 40, 90, 80, 128);
                flag++;
            }

            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 40, 90, 80, effect->alpha);

            gSPSegment(POLY_OPA_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, effect->timer + (i * 3), (effect->timer + (i * 3)) * 5,
                                          32, 64, 1, 0, 0, 32, 32, 1, 5, 0, 0));

            Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(effect->scale, effect->scale, 1.0f, MTXMODE_APPLY);

            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_OPA_DISP++, gFishingWaterDustModelDL);
            FrameInterpolation_RecordCloseChild();
        }
        effect++;
    }

    effect = firstEffect;
    flag = 0;
    for (i = 0; i < 100; i++) {
        if (effect->type == FS_EFF_BUBBLE) {
            FrameInterpolation_RecordOpenChild(effect, effect->epoch);
            if (flag == 0) {
                gSPDisplayList(POLY_XLU_DISP++, gFishingBubbleMaterialDL);
                gDPSetEnvColor(POLY_XLU_DISP++, 150, 150, 150, 0);
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 255);
                flag++;
            }

            Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_Scale(effect->scale, effect->scale, 1.0f, MTXMODE_APPLY);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_XLU_DISP++, gFishingBubbleModelDL);
            FrameInterpolation_RecordCloseChild();
        }
        effect++;
    }

    effect = firstEffect + 30;
    flag = 0;
    for (i = 30; i < FISHING_EFFECT_COUNT; i++) {
        if (effect->type == FS_EFF_RAIN_DROP) {
            FrameInterpolation_RecordOpenChild(effect, effect->epoch);
            if (flag == 0) {
                POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 0x14);
                gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 150, 255, 255, 30);
                flag++;
            }

            Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
            Matrix_RotateY(effect->rot.y, MTXMODE_APPLY);
            Matrix_RotateX(effect->rot.x, MTXMODE_APPLY);
            Matrix_RotateZ(effect->rot.z, MTXMODE_APPLY);
            Matrix_Scale(0.002f, 1.0f, 0.1f, MTXMODE_APPLY);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_XLU_DISP++, gFishingRainDropModelDL);
            FrameInterpolation_RecordCloseChild();
        }
        effect++;
    }

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    effect = firstEffect + 30;
    flag = 0;
    for (i = 30; i < FISHING_EFFECT_COUNT; i++) {
        if (effect->type == FS_EFF_RAIN_RIPPLE) {
            FrameInterpolation_RecordOpenChild(effect, effect->epoch);
            if (flag == 0) {
                gSPDisplayList(POLY_XLU_DISP++, gFishingRippleMaterialDL);
                gDPSetEnvColor(POLY_XLU_DISP++, 155, 155, 155, 0);
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 130);
                flag++;
            }

            Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
            Matrix_Scale(effect->scale, 1.0f, effect->scale, MTXMODE_APPLY);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_XLU_DISP++, gFishingRippleModelDL);
            FrameInterpolation_RecordCloseChild();
        }
        effect++;
    }

    effect = firstEffect + 30;
    flag = 0;
    for (i = 30; i < FISHING_EFFECT_COUNT; i++) {
        if (effect->type == FS_EFF_RAIN_SPLASH) {
            FrameInterpolation_RecordOpenChild(effect, effect->epoch);
            if (flag == 0) {
                gSPDisplayList(POLY_XLU_DISP++, gFishingRainSplashMaterialDL);
                gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, KREG(19) + 80);
                flag++;
            }

            if (Rand_ZeroOne() < 0.5f) {
                rotY = 0.0f;
            } else {
                rotY = M_PI;
            }

            Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            Matrix_RotateY(rotY, MTXMODE_APPLY);
            Matrix_Scale(effect->scale, effect->scale, 1.0f, MTXMODE_APPLY);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            gSPDisplayList(POLY_XLU_DISP++, gFishingRainSplashModelDL);
            FrameInterpolation_RecordCloseChild();
        }
        effect++;
    }

    effect = firstEffect;
    if (effect->type == FS_EFF_OWNER_HAT) {
        FrameInterpolation_RecordOpenChild(effect, effect->epoch);
        Matrix_Translate(effect->pos.x, effect->pos.y, effect->pos.z, MTXMODE_NEW);
        Matrix_RotateY((sEffOwnersHatRot.y * M_PI) / 32768, MTXMODE_APPLY);
        Matrix_RotateX((sEffOwnersHatRot.x * M_PI) / 32768, MTXMODE_APPLY);
        Matrix_RotateZ((sEffOwnersHatRot.z * M_PI) / 32768, MTXMODE_APPLY);
        Matrix_Scale(effect->scale, effect->scale, effect->scale, MTXMODE_APPLY);
        Matrix_Translate(-1250.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateX(M_PI / 2, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

        gSPDisplayList(POLY_OPA_DISP++, gFishingOwnerHatDL);
        FrameInterpolation_RecordCloseChild();
    }

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

void Fishing_DrawStreamSplash(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    gSPSegment(POLY_XLU_DISP++, 0x09,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, play->gameplayFrames * 1, play->gameplayFrames * 8, 32, 64, 1,
                                  -(play->gameplayFrames * 2), 0, 16, 16, 1, 8, -2, 0));

    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 195, 225, 235, 50);

    Matrix_Translate(670.0f, -24.0f, -600.0f, MTXMODE_NEW);
    Matrix_Scale(0.02f, 1.0f, 0.02f, MTXMODE_APPLY);

    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, SEGMENTED_TO_VIRTUAL(gFishingStreamSplashDL));

    CLOSE_DISPS(play->state.gfxCtx);
}

int32_t Fishing_IsAboveCounter(Vec3f* vec) {
    if (((vec->x >= 110.0f) && (vec->x <= 150.0f) && (vec->z <= 1400.0f) && (vec->z >= 1160.0f)) ||
        ((vec->x >= 110.0f) && (vec->x <= 210.0f) && (vec->z <= 1200.0f) && (vec->z >= 1160.0f))) {
        if (vec->y <= 42.0f) {
            return true;
        }
    }

    return false;
}

static void Fishing_UpdateLineInternal(PlayState* play, Actor* collisionActor, Vec3f* basePos, Vec3f* pos,
                                       Vec3f* rot, Vec3f* unk, float* lineSpooled, uint8_t lineStraightenTimer,
                                       float lineGravity, bool portableFishing, bool portableWaterFound,
                                       uint8_t lureType, float waterSurfaceY) {
    int16_t i;
    int16_t k;
    int16_t spooled = { 0 };
    Vec3f posSrc = { 0.0f, 0.0f, 0.0f };
    Vec3f posStep;
    Vec3f tempPos = { 0 };
    float sp94 = { 0 };
    float sp90 = { 0 };
    float sp8C = { 0 };
    float phi_f12 = { 0 };
    float phi_f2 = { 0 };
    CollisionPoly* floorPoly;
    int32_t floorBgId;
    float floorY;

    if (lineStraightenTimer != 0) {
        tempPos = *basePos;
        Vec3f segPos = pos[LINE_SEG_COUNT - 1];

        sp94 = segPos.x - tempPos.x;
        sp90 = segPos.y - tempPos.y;
        sp8C = segPos.z - tempPos.z;

        float lineLength = sqrtf(SQ(sp94) + SQ(sp90) + SQ(sp8C)) * 0.97f;
        if (lineLength > 1000.0f) {
            lineLength = 1000.0f;
        }

        *lineSpooled = 200.0f - (lineLength * 200.0f * 0.001f);
    }

    spooled = *lineSpooled;
    posSrc.z = 5.0f;

    for (i = 0; i < LINE_SEG_COUNT; i++) {
        if (i <= spooled) {
            pos[i] = *basePos;
        } else if (lineStraightenTimer != 0) {
            float temp_f20 = (float)(i - spooled) / (float)(LINE_SEG_COUNT - spooled + 1);
            Math_ApproachF(&pos[i].x, (sp94 * temp_f20) + tempPos.x, 1.0f, 20.0f);
            Math_ApproachF(&pos[i].y, (sp90 * temp_f20) + tempPos.y, 1.0f, 20.0f);
            Math_ApproachF(&pos[i].z, (sp8C * temp_f20) + tempPos.z, 1.0f, 20.0f);
        }
    }

    for (i = spooled + 1, k = 0; i < LINE_SEG_COUNT; i++, k++) {
        float temp_f18 = 2.0f * lineGravity;
        float segmentStartY = pos[i].y;

        float dx = (pos + i)->x - (pos + i - 1)->x;
        float spD8 = (pos + i)->y;

        float sqDistXZ = SQ((pos + i)->x) + SQ((pos + i)->z);

        if (portableFishing) {
            phi_f12 = waterSurfaceY;
        } else if (sqDistXZ > SQ(920.0f)) {
            phi_f12 = ((sqrtf(sqDistXZ) - 920.0f) * 0.11f) + waterSurfaceY;
        } else {
            phi_f12 = waterSurfaceY;
        }

        if (portableFishing && !portableWaterFound) {
            spD8 -= temp_f18;
        } else if (lureType == FS_LURE_SINKING) {
            if (spD8 < phi_f12) {
                if (!portableFishing) {
                    phi_f12 = ((sqrtf(sqDistXZ) - 920.0f) * 0.147f) + waterSurfaceY;
                }
                if (spD8 > phi_f12) {
                    phi_f2 = (spD8 - phi_f12) * 0.05f;
                    if (phi_f2 > 0.29999998f) {
                        phi_f2 = 0.29999998f;
                    }
                    if (i >= 100) {
                        phi_f2 *= (i - 100) * 0.02f;
                        spD8 -= phi_f2;
                    }
                }
            } else {
                spD8 -= temp_f18;
            }
        } else if (i > LINE_SEG_COUNT - 10) {
            if (spD8 > phi_f12) {
                phi_f2 = (spD8 - phi_f12) * 0.2f;
                if (phi_f2 > temp_f18) {
                    phi_f2 = temp_f18;
                }
                spD8 -= phi_f2;
            }
        } else {
            if (spD8 > phi_f12) {
                spD8 -= temp_f18;
            }
        }

        if (!portableFishing && Fishing_IsAboveCounter(&pos[i])) {
            spD8 = 42.0f;
        }

        float dy = spD8 - (pos + i - 1)->y;
        float dz = (pos + i)->z - (pos + i - 1)->z;

        float ry = atan2f(dx, dz);
        float dist = sqrtf(SQ(dx) + SQ(dz));
        float rx = -atan2f(dy, dist);

        (rot + i - 1)->y = ry;
        (rot + i - 1)->x = rx;

        Matrix_RotateY(ry, MTXMODE_NEW);
        Matrix_RotateX(rx, MTXMODE_APPLY);
        Matrix_MultVec3f(&posSrc, &posStep);

        (pos + i)->x = (pos + i - 1)->x + posStep.x;
        (pos + i)->y = (pos + i - 1)->y + posStep.y;
        (pos + i)->z = (pos + i - 1)->z + posStep.z;

        if (portableFishing && !portableWaterFound) {
            Vec3f floorCheckPos = pos[i];
            floorCheckPos.y = segmentStartY;
            if (floorCheckPos.y < pos[i].y) {
                floorCheckPos.y = pos[i].y;
            }
            if (floorCheckPos.y < pos[i - 1].y) {
                floorCheckPos.y = pos[i - 1].y;
            }
            floorCheckPos.y += 5.0f;
            floorY = BgCheck_EntityRaycastFloor4(&play->colCtx, &floorPoly, &floorBgId, collisionActor,
                                                 &floorCheckPos);
            if ((floorY != BGCHECK_Y_MIN) && (pos[i].y < (floorY + 2.0f))) {
                pos[i].y = floorY + 2.0f;
                dx = pos[i].x - pos[i - 1].x;
                dy = pos[i].y - pos[i - 1].y;
                dz = pos[i].z - pos[i - 1].z;
                rot[i - 1].y = atan2f(dx, dz);
                rot[i - 1].x = -atan2f(dy, sqrtf(SQ(dx) + SQ(dz)));
            }
        }
    }
}

void Fishing_UpdateLine(PlayState* play, Vec3f* basePos, Vec3f* pos, Vec3f* rot, Vec3f* unk) {
    Fishing_UpdateLineInternal(play, &sFishingMain->actor, basePos, pos, rot, unk, &sRodLineSpooled, D_80B7A6A4,
                               D_80B7E148, sPortableFishing, sPortableWaterFound, sLureEquipped,
                               WATER_SURFACE_Y(play));
}

static void Fishing_UpdateLinePosInternal(Vec3f* pos, int16_t spooled) {
    int16_t i;
    Vec3f posSrc = { 0.0f, 0.0f, 0.0f };
    Vec3f posStep;

    posSrc.z = 5.0f;

    for (i = LINE_SEG_COUNT - 2; i > spooled; i--) {
        float dx = (pos + i)->x - (pos + i + 1)->x;
        float dy = (pos + i)->y - (pos + i + 1)->y;
        float dz = (pos + i)->z - (pos + i + 1)->z;

        float ry = atan2f(dx, dz);
        float dist = sqrtf(SQ(dx) + SQ(dz));
        float rx = -atan2f(dy, dist);

        Matrix_RotateY(ry, MTXMODE_NEW);
        Matrix_RotateX(rx, MTXMODE_APPLY);
        Matrix_MultVec3f(&posSrc, &posStep);

        (pos + i)->x = (pos + i + 1)->x + posStep.x;
        (pos + i)->y = (pos + i + 1)->y + posStep.y;
        (pos + i)->z = (pos + i + 1)->z + posStep.z;
    }
}

void Fishing_UpdateLinePos(Vec3f* pos) {
    Fishing_UpdateLinePosInternal(pos, sRodLineSpooled);
}

void Fishing_UpdatePresentedLine(PlayState* play, Actor* collisionActor, Vec3f* rodTip, Vec3f* lurePos,
                               Vec3f linePos[LINE_SEG_COUNT], Vec3f lineRot[LINE_SEG_COUNT],
                               Vec3f lineUnk[LINE_SEG_COUNT], int16_t lineSpooled, uint8_t lureType, float lineGravity) {
    float remoteLineSpooled = { 0 };
    bool waterFound;
    WaterBox* waterBox;
    float waterSurfaceY = 32000.0f;

    if (play == NULL || collisionActor == NULL || rodTip == NULL || lurePos == NULL || linePos == NULL ||
        lineRot == NULL || lineUnk == NULL) {
        return;
    }

    remoteLineSpooled = CLAMP(lineSpooled, 0, LINE_SEG_COUNT - 1);
    lineGravity = CLAMP(lineGravity, 0.0f, 520.0f);
    waterFound =
        WaterBox_GetSurface1(play, &play->colCtx, lurePos->x, lurePos->z, &waterSurfaceY, &waterBox) != 0;
    if (!waterFound) {
        waterSurfaceY = -32000.0f;
    }

    linePos[LINE_SEG_COUNT - 1] = *lurePos;
    Fishing_UpdateLinePosInternal(linePos, remoteLineSpooled);
    Fishing_UpdateLineInternal(play, collisionActor, rodTip, linePos, lineRot, lineUnk, &remoteLineSpooled, 0,
                               lineGravity, true, waterFound, lureType, waterSurfaceY);
}

void Fishing_DrawLureHook(PlayState* play, Vec3f* pos, Vec3f* refPos, uint8_t hookIndex) {
    float dx = { 0 };
    float dy = { 0 };
    float dz = { 0 };
    float rx = { 0 };
    float ry = { 0 };
    float dist = { 0 };
    float offsetY = { 0 };
    Vec3f posSrc = { 0.0f, 0.0f, 1.0f };
    Vec3f posStep;
    Player* player = GET_PLAYER(play);

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();

    if ((sRodCastState == 3) && ((pos->y > WATER_SURFACE_Y(play)) || (sIsOwnersHatHooked && hookIndex))) {
        offsetY = 0.0f;
    } else if (pos->y < WATER_SURFACE_Y(play)) {
        offsetY = -1.0f;
    } else {
        offsetY = -3.0f;
    }

    dx = refPos->x - pos->x;
    dy = refPos->y - pos->y + offsetY;
    dz = refPos->z - pos->z;

    ry = atan2f(dx, dz);
    dist = sqrtf(SQ(dx) + SQ(dz));
    rx = -atan2f(dy, dist);

    Matrix_RotateY(ry, MTXMODE_NEW);
    Matrix_RotateX(rx, MTXMODE_APPLY);
    Matrix_MultVec3f(&posSrc, &posStep);

    refPos->x = pos->x + posStep.x;
    refPos->y = pos->y + posStep.y;
    refPos->z = pos->z + posStep.z;

    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_NEW);

    if ((player->actor.speedXZ == 0.0f) && (sLureWigglePosY == 0.0f)) {
        Math_ApproachF(&sLureHookRotY[hookIndex], ry, 0.1f, 0.3f);
    } else {
        sLureHookRotY[hookIndex] = ry;
    }

    sPresentedLureHookPos[hookIndex] = *pos;
    sPresentedLureHookRot[hookIndex] = (Vec3f){ rx, sLureHookRotY[hookIndex], 0.0f };

    Matrix_RotateY(sLureHookRotY[hookIndex], MTXMODE_APPLY);
    Matrix_RotateX(rx, MTXMODE_APPLY);
    Matrix_Scale(0.0039999997f, 0.0039999997f, 0.005f, MTXMODE_APPLY);
    Matrix_RotateY(M_PI, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, gFishingLureHookDL);

    Matrix_RotateZ(M_PI / 2, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, gFishingLureHookDL);

    if ((hookIndex == 1) && (sIsOwnersHatHooked)) {
        Matrix_Scale(2.0f, 2.0f, 2.0f, MTXMODE_APPLY);
        Matrix_Translate(250.0f, 0.0f, -1400.0f, MTXMODE_APPLY);
        Matrix_Push();

        if (sIsOwnersHatSunk) {
            FishingEffect* effect = play->specialEffects;
            MtxF mf;

            Matrix_MultVec3f(&sZeroVec, &effect->pos);
            Matrix_Get(&mf);
            Matrix_MtxFToYXZRotS(&mf, &sEffOwnersHatRot, 0);

            sIsOwnersHatSunk = false;
            sIsOwnersHatHooked = false;

            effect->type = FS_EFF_OWNER_HAT;
            effect->state = 0;
            effect->vel = sZeroVec;
            effect->accel = sZeroVec;
        }

        Matrix_Pop();
        Matrix_Translate(-1250.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateX(M_PI / 2, MTXMODE_APPLY);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gFishingOwnerHatDL);
    }

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

static void Fishing_UpdateSinkingLureInternal(Vec3f positions[SINKING_LURE_SEG_COUNT], Vec3f* lurePos,
                                              int16_t playerYaw, uint8_t castState, bool sinkingLureFound,
                                              bool underwater) {
    int16_t i;
    float offsetY = { 0 };
    Vec3f posSrc = { 0.0f, 0.0f, 0.0f };
    Vec3f posStep;
    Vec3f sp94;
    Vec3f sp88;
    float offsetX = { 0 };
    float offsetZ = { 0 };

    posSrc.z = 0.85f;

    positions[0] = *lurePos;

    if (sinkingLureFound) {
        offsetY = -1.0f;
    } else if (underwater) {
        offsetY = 0.5f;
    } else {
        offsetY = -5.0f;
    }

    if (castState == 5) {
        Matrix_RotateY(playerYaw * (M_PI / 32768), MTXMODE_NEW);
        sp94.x = 5.0f;
        sp94.y = 0.0f;
        sp94.z = 3.0f;
        Matrix_MultVec3f(&sp94, &sp88);
    }

    for (i = 1; i < SINKING_LURE_SEG_COUNT; i++) {
        Vec3f* pos = positions;

        if ((i < 10) && (castState == 5)) {
            offsetX = (10 - i) * sp88.x * 0.1f;
            offsetZ = (10 - i) * sp88.z * 0.1f;
        } else {
            offsetX = offsetZ = 0.0f;
        }

        float dx = (pos + i)->x - (pos + i - 1)->x + offsetX;
        float dy = (pos + i)->y - (pos + i - 1)->y + offsetY;
        float dz = (pos + i)->z - (pos + i - 1)->z + offsetZ;

        float ry = atan2f(dx, dz);
        float dist = sqrtf(SQ(dx) + SQ(dz));
        float rx = -atan2f(dy, dist);

        Matrix_RotateY(ry, MTXMODE_NEW);
        Matrix_RotateX(rx, MTXMODE_APPLY);
        Matrix_MultVec3f(&posSrc, &posStep);

        (pos + i)->x = (pos + i - 1)->x + posStep.x;
        (pos + i)->y = (pos + i - 1)->y + posStep.y;
        (pos + i)->z = (pos + i - 1)->z + posStep.z;
    }
}

void Fishing_UpdateSinkingLure(PlayState* play) {
    Player* player = GET_PLAYER(play);

    Fishing_UpdateSinkingLureInternal(sSinkingLurePos, &sLurePos, player->actor.shape.rot.y, sRodCastState,
                                      sSinkingLureFound, sLurePos.y < WATER_SURFACE_Y(play));
}

void Fishing_UpdatePresentedSinkingLure(Vec3f* lurePos, Vec3f positions[SINKING_LURE_SEG_COUNT], int16_t playerYaw,
                                      uint8_t castState, uint8_t underwater) {
    if (lurePos == NULL || positions == NULL) {
        return;
    }

    Fishing_UpdateSinkingLureInternal(positions, lurePos, playerYaw, castState, false, underwater != 0);
}

static float sSinkingLureSizes[] = {
    1.0f, 1.5f,  1.8f, 2.0f, 1.8f, 1.6f, 1.4f, 1.2f, 1.0f, 1.0f,
    0.9f, 0.85f, 0.8f, 0.7f, 0.8f, 1.0f, 1.2f, 1.1f, 1.0f, 0.8f,
};

void Fishing_DrawSinkingLure(PlayState* play) {
    int16_t i;
    float scale = { 0 };

    OPEN_DISPS(play->state.gfxCtx);

    Fishing_UpdateSinkingLure(play);

    if (sLurePos.y < WATER_SURFACE_Y(play)) {
        // Submit submerged lure segments after the translucent water surface.
        // Drawing them in the opaque pass lets the later water pass completely
        // hide the small segments when viewed from above.
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);

        gSPDisplayList(POLY_XLU_DISP++, gFishingSinkingLureSegmentMaterialDL);

        for (i = SINKING_LURE_SEG_COUNT - 1; i >= 0; i--) {
            if ((i + sSinkingLureSegmentIndex) < SINKING_LURE_SEG_COUNT) {
                FrameInterpolation_RecordOpenChild("Fishing Lures 1", i);
                Matrix_Translate(sSinkingLurePos[i].x, sSinkingLurePos[i].y, sSinkingLurePos[i].z, MTXMODE_NEW);
                scale = sSinkingLureSizes[i + sSinkingLureSegmentIndex] * 0.04f;
                Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                Matrix_ReplaceRotation(&play->billboardMtxF);

                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, gFishingSinkingLureSegmentModelDL);
                FrameInterpolation_RecordCloseChild();
            }
        }
    } else {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);

        gSPDisplayList(POLY_XLU_DISP++, gFishingSinkingLureSegmentMaterialDL);

        for (i = SINKING_LURE_SEG_COUNT - 1; i >= 0; i--) {
            if ((i + sSinkingLureSegmentIndex) < SINKING_LURE_SEG_COUNT) {
                FrameInterpolation_RecordOpenChild("Fishing Lures 2", i);
                Matrix_Translate(sSinkingLurePos[i].x, sSinkingLurePos[i].y, sSinkingLurePos[i].z, MTXMODE_NEW);
                scale = sSinkingLureSizes[i + sSinkingLureSegmentIndex] * 0.04f;
                Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                Matrix_ReplaceRotation(&play->billboardMtxF);

                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, gFishingSinkingLureSegmentModelDL);
                FrameInterpolation_RecordCloseChild();
            }
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Fishing_DrawLureAndLine(PlayState* play, Vec3f* linePos, Vec3f* lineRot) {
    Vec3f posSrc;
    Vec3f posStep;
    Vec3f hookPos[2];
    int16_t i;
    int16_t spooled = sRodLineSpooled;
    Player* player = GET_PLAYER(play);

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Push();

    if (sSinkingLureFound) {
        Vec3f posTemp = sLurePos;
        sLurePos = sSinkingLureBasePos;
        Fishing_DrawSinkingLure(play);
        sLurePos = posTemp;
    }

    if ((sRodCastState == 4) || (sRodCastState == 5)) {
        sLurePos = sFishingHookedFish->fishMouthPos;

        if ((sRodCastState == 5) && (sLureEquipped == FS_LURE_SINKING)) {
            Matrix_RotateY(player->actor.shape.rot.y * (M_PI / 32768), MTXMODE_NEW);
            posSrc.x = 2.0f;
            posSrc.y = 0.0f;
            posSrc.z = 0.0f;
            Matrix_MultVec3f(&posSrc, &posStep);
            sLurePos.x += posStep.x;
            sLurePos.z += posStep.z;
        }
    } else if (sRodCastState == 0) {
        sLurePos = sReelLinePos[LINE_SEG_COUNT - 1];
        sLureRot.x = sReelLineRot[LINE_SEG_COUNT - 2].x + M_PI;

        if ((player->actor.speedXZ == 0.0f) && (D_80B7E0B0 == 0)) {
            Math_ApproachF(&sLureRot.y, sReelLineRot[LINE_SEG_COUNT - 2].y, 0.1f, 0.2f);
        } else {
            sLureRot.y = sReelLineRot[LINE_SEG_COUNT - 2].y;
        }
    }

    if (sLureEquipped != FS_LURE_SINKING) {
        Matrix_Translate(sLurePos.x, sLurePos.y, sLurePos.z, MTXMODE_NEW);
        Matrix_RotateY(sLureRot.y + sLure1Rotate, MTXMODE_APPLY);
        Matrix_RotateX(sLureRot.x, MTXMODE_APPLY);
        Matrix_Scale(0.0039999997f, 0.0039999997f, 0.0039999997f, MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, sLurePosZOffset, MTXMODE_APPLY);
        Matrix_RotateZ(M_PI / 2, MTXMODE_APPLY);
        Matrix_RotateY(M_PI / 2, MTXMODE_APPLY);

        Gfx_SetupDL_25Opa(play->state.gfxCtx);

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, gFishingLureFloatDL);

        posSrc.x = -850.0f;
        posSrc.y = 0.0f;
        posSrc.z = 0.0f;
        Matrix_MultVec3f(&posSrc, &sLureDrawPos);

        posSrc.x = 500.0f;
        posSrc.z = -300.0f;
        Matrix_MultVec3f(&posSrc, &hookPos[0]);
        Fishing_DrawLureHook(play, &hookPos[0], &sLureHookRefPos[0], 0);

        posSrc.x = 2100.0f;
        posSrc.z = -50.0f;
        Matrix_MultVec3f(&posSrc, &hookPos[1]);
        Fishing_DrawLureHook(play, &hookPos[1], &sLureHookRefPos[1], 1);
    }

    POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 0x14);

    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 55);

    if ((sRodCastState == 4) && ((sLineHooked != 0) || (sLureEquipped != 2))) {

        float dx = sLurePos.x - sRodTipPos.x;
        float dy = sLurePos.y - sRodTipPos.y;
        float dz = sLurePos.z - sRodTipPos.z;

        float ry = atan2f(dx, dz);
        float dist = sqrtf(SQ(dx) + SQ(dz));
        float rx = -atan2f(dy, dist);

        dist = sqrtf(SQ(dx) + SQ(dy) + SQ(dz)) * 0.001f;

        Matrix_Translate(sRodTipPos.x, sRodTipPos.y, sRodTipPos.z, MTXMODE_NEW);
        Matrix_RotateY(ry, MTXMODE_APPLY);
        Matrix_RotateX(rx, MTXMODE_APPLY);
        Matrix_Scale(sFishingLineScale, 1.0f, dist, MTXMODE_APPLY);

        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, gFishingLineModelDL);
    } else {
        for (i = spooled; i < LINE_SEG_COUNT - 1; i++) {
            if ((i == LINE_SEG_COUNT - 3) && (sLureEquipped == FS_LURE_STOCK) && (sRodCastState == 3)) {

                float dx = sLureDrawPos.x - (linePos + i)->x;
                float dy = sLureDrawPos.y - (linePos + i)->y;
                float dz = sLureDrawPos.z - (linePos + i)->z;

                float ry = atan2f(dx, dz);
                float dist = sqrtf(SQ(dx) + SQ(dz));
                float rx = -atan2f(dy, dist);

                dist = sqrtf(SQ(dx) + SQ(dy) + SQ(dz)) * 0.001f;

                Matrix_Translate((linePos + i)->x, (linePos + i)->y, (linePos + i)->z, MTXMODE_NEW);
                Matrix_RotateY(ry, MTXMODE_APPLY);
                Matrix_RotateX(rx, MTXMODE_APPLY);
                Matrix_Scale(sFishingLineScale, 1.0f, dist, MTXMODE_APPLY);

                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, gFishingLineModelDL);
                break;
            }

            Matrix_Translate((linePos + i)->x, (linePos + i)->y, (linePos + i)->z, MTXMODE_NEW);
            Matrix_RotateY((lineRot + i)->y, MTXMODE_APPLY);
            Matrix_RotateX((lineRot + i)->x, MTXMODE_APPLY);
            Matrix_Scale(sFishingLineScale, 1.0f, 0.005f, MTXMODE_APPLY);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, gFishingLineModelDL);
        }
    }

    Matrix_Pop();
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    CLOSE_DISPS(play->state.gfxCtx);
}

static float sRodScales[22] = {
    1.0f,        1.0f,        1.0f,        0.9625f,     0.925f, 0.8875f,     0.85f,       0.8125f,
    0.775f,      0.73749995f, 0.7f,        0.6625f,     0.625f, 0.5875f,     0.54999995f, 0.5125f,
    0.47499996f, 0.4375f,     0.39999998f, 0.36249995f, 0.325f, 0.28749996f,
};

static float sRodBendRatios[22] = {
    0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.06f,   0.12f,   0.18f,   0.24f,   0.30f,   0.36f,
    0.42f, 0.48f, 0.54f, 0.60f, 0.60f, 0.5142f, 0.4285f, 0.3428f, 0.2571f, 0.1714f, 0.0857f,
};

static Vec3f sRodTipOffset = { 0.0f, 0.0f, 0.0f };

void Fishing_DrawRod(PlayState* play) {
    int16_t i;
    float spC0 = { 0 };
    MovementInput* input = &play->state.input;
    Player* player = GET_PLAYER(play);

    OPEN_DISPS(play->state.gfxCtx);

    if (sRodHitTimer != 0) {
        sRodHitTimer--;

        Math_ApproachF(&D_80B7A6C0, 35.0f, 1.0f, 100.0f);
        Math_ApproachF(&D_80B7A6BC, -0.8f, 1.0f, 0.4f);
        Math_ApproachS(&player->actor.shape.rot.x, -4000, 2, 15000);
    } else {
        int16_t target = 0;

        if ((sRodCastState == 4) && sLineHooked) {
            target = Math_SinS(sLureTimer * 25600) * 1500.0f;
        } else {
            Math_ApproachZeroF(&D_80B7A6C0, 0.1f, 10.0f);
            Math_ApproachZeroF(&D_80B7A6BC, 1.0f, 0.05f);
        }

        Math_ApproachS(&player->actor.shape.rot.x, target, 5, 1000);
    }

    if ((sRodCastState == 3) || (sRodCastState == 4)) {
        if ((input->x == 0) && (sStickAdjXPrev != 0)) {
            D_80B7A6B0 = 0.0f;
        }
        if ((input->y == 0) && (sStickAdjYPrev != 0)) {
            D_80B7A6B4 = 0.0f;
        }

        float lureXZLen = player->unk_85C;
        Math_SmoothStepToF(&player->unk_85C, input->y * 0.02f, 0.3f, 5.0f, 0.0f);
        lureXZLen = player->unk_85C - lureXZLen;

        float spC4 = player->unk_858;
        Math_SmoothStepToF(&player->unk_858, input->x * 0.02f, 0.3f, 5.0f, 0.0f);
        spC4 = player->unk_858 - spC4;

        if (player->unk_858 > 1.0f) {
            player->unk_858 = 1.0f;
        }
        if (player->unk_85C > 1.0f) {
            player->unk_85C = 1.0f;
        }
        if (player->unk_858 < -1.0f) {
            player->unk_858 = -1.0f;
        }
        if (player->unk_85C < -1.0f) {
            player->unk_85C = -1.0f;
        }

        Math_ApproachF(&sRodBendRotY, spC4 * 70.0f * -0.01f, 1.0f, D_80B7A6B0);
        Math_ApproachF(&D_80B7A6B0, 1.0f, 1.0f, 0.1f);
        Math_ApproachF(&D_80B7A6AC, lureXZLen * 70.0f * 0.01f, 1.0f, D_80B7A6B4);
        Math_ApproachF(&D_80B7A6B4, 1.0f, 1.0f, 0.1f);
        Math_ApproachZeroF(&D_80B7A6B8, 1.0f, 0.05f);
    } else {
        Math_ApproachZeroF(&player->unk_85C, 1.0f, 0.1f);
        Math_ApproachZeroF(&player->unk_858, 1.0f, 0.1f);
        Math_ApproachF(&D_80B7A6AC, (Math_SinS(sLureTimer * 3000) * 0.025f) + -0.03f, 1.0f, 0.05f);
        Math_ApproachZeroF(&sRodBendRotY, 1.0f, 0.05f);

        if ((sRodCastTimer > 18) && (sRodCastTimer < 25)) {
            Math_ApproachF(&D_80B7A6B8, 0.8f, 1.0f, 0.2f);
        } else {
            Math_ApproachF(&D_80B7A6B8, 0.0f, 1.0f, 0.4f);
        }
    }

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gSPDisplayList(POLY_OPA_DISP++, gFishingRodMaterialDL);

    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 155, 0, 255);

    Matrix_Mult(&player->mf_9E0, MTXMODE_NEW);

    if (sLinkAge != LINK_AGE_CHILD) {
        Matrix_Translate(0.0f, 400.0f, 0.0f, MTXMODE_APPLY);
    } else {
        Matrix_Translate(0.0f, 230.0f, 0.0f, MTXMODE_APPLY);
    }

    if (sRodCastState == 5) {
        Matrix_RotateY(0.56f * M_PI, MTXMODE_APPLY);
    } else {
        Matrix_RotateY(0.41f * M_PI, MTXMODE_APPLY);
    }

    Matrix_RotateX(-M_PI / 5.0000003f, MTXMODE_APPLY);
    Matrix_RotateZ((player->unk_858 * 0.5f) + 3.0f * M_PI / 20.0f, MTXMODE_APPLY);
    Matrix_RotateX((D_80B7A6C0 + 20.0f) * 0.01f * M_PI, MTXMODE_APPLY);
    Matrix_Scale(0.70000005f, 0.70000005f, 0.70000005f, MTXMODE_APPLY);

    spC0 = (D_80B7A6BC * (((player->unk_85C - 1.0f) * -0.25f) + 0.5f)) + (D_80B7A6AC + D_80B7A6B8);

    Matrix_Translate(0.0f, 0.0f, -1300.0f, MTXMODE_APPLY);

    for (i = 0; i < 22; i++) {
        Matrix_RotateY(sRodBendRatios[i] * sRodBendRotY * 0.5f, MTXMODE_APPLY);
        Matrix_RotateX(sRodBendRatios[i] * spC0 * 0.5f, MTXMODE_APPLY);

        Matrix_Push();
        Matrix_Scale(sRodScales[i], sRodScales[i], 0.52f, MTXMODE_APPLY);

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

        if (i == 21) {
            Matrix_MultVec3f(&sRodTipOffset, &sRodTipPos);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static Vec3f sSoundPos = { 0.0f, 0.0f, 0.0f };

void Fishing_UpdateLure(Fishing* this, PlayState* play) {
    float spE4 = { 0 };
    float spE0 = { 0 };
    int16_t i;
    Player* player = GET_PLAYER(play);
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    Vec3f spA8 = { 0 };
    Vec3f sp9C;
    Vec3f sp90;
    MovementInput* input = &play->state.input;
    float phi_f0 = { 0 };
    Vec3f sp64;

    sLureTimer++;

    if (D_80B7E0B0 != 0) {
        D_80B7E0B0--;
    }

    if (D_80B7E0B2 != 0) {
        D_80B7E0B2--;
    }

    if (sRodCastTimer != 0) {
        sRodCastTimer--;
    }

    if (sLureBitTimer != 0) {
        sLureBitTimer--;
    }

    if (D_80B7E150 != 0) {
        D_80B7E150--;
    }

    if (D_80B7A6A4 != 0) {
        D_80B7A6A4--;
    }

    if (D_80B7E114 != 0) {
        D_80B7E114--;
    }

    if (sFishingPlayingState == 1) {
        sFishingPlayingState = 2;
        sFishesCaught = 0;
        sPondOwnerTextIdIndex = 0;
        sLureEquipped = sPortableFishing ? FS_LURE_SINKING : FS_LURE_STOCK;

        // if prize item won as child or adult, set the sinking lure location.
        if (((sLinkAge == LINK_AGE_CHILD) && (HIGH_SCORE(HS_FISHING) & HS_FISH_PRIZE_CHILD)) ||
            ((sLinkAge != LINK_AGE_CHILD) && (HIGH_SCORE(HS_FISHING) & HS_FISH_PRIZE_ADULT))) {
            sSinkingLureLocation = (uint8_t)Rand_ZeroFloat(3.999f) + 1;
        }

        D_80B7E148 = 520.0f;
        sRodLineSpooled = 195.0f;

        sRodCastState = sLureTimer = D_80B7E0B0 = D_80B7E0B2 = sRodCastTimer = sWiggleAttraction = D_80B7E114 =
            D_80B7E150 = 0;
        sLure1Rotate = sReelLinePosStep = sLurePosZOffset = 0.0f;

        sLureLineSegPosDelta = zeroVec;

        for (i = 0; i < LINE_SEG_COUNT; i++) {
            sReelLinePos[i] = zeroVec;
            sReelLineRot[i] = zeroVec;
            sReelLineUnk[i] = zeroVec;
        }
    }

    SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &sLurePos, &sSoundPos, &sProjectedW);

    if (sRodCastState == 0) {
        Math_ApproachF(&sLurePosZOffset, -800.0f, 1.0f, 20.0f);
    } else {
        Math_ApproachF(&sLurePosZOffset, 300.0f, 1.0f, 20.0f);
    }

    switch (sRodCastState) {
        case 0:
            sSinkingLureSegmentIndex = 0;

            if (KREG(14) != 0) {
                KREG(14) = 0;
                sLureEquipped = FS_LURE_SINKING - sLureEquipped;
                if (sLureEquipped != FS_LURE_STOCK) {
                    sPondOwnerTextIdIndex = 0;
                }
            }

            Math_ApproachF(&sRodLineSpooled, 195.0f, 1.0f, 1.0f);

            if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
                sRodCastTimer = 0;
                player->unk_860 = 0;
            }

            if (sRodCastTimer == 0) {
                if ((D_80B7E0B0 == 0) && (player->unk_860 == 1)) {
                    sRodCastTimer = 37;
                    Message_CloseTextbox(play);
                }
            } else {
                sLureRot.x = sReelLineRot[LINE_SEG_COUNT - 2].x + M_PI;
                sLureRot.y = sReelLineRot[LINE_SEG_COUNT - 2].y;

                if (sRodCastTimer == 18) {
                    sRodCastState = 1;
                    sLurePos = sRodTipPos;
                    Matrix_RotateY((player->actor.shape.rot.y / 32768.0f) * M_PI, MTXMODE_NEW);
                    sp90.x = 0.0f;
                    sp90.y = 0.0f;
                    sp90.z = sPortableFishing ? 30.0f : 25.0f;
                    Matrix_MultVec3f(&sp90, &sLurePosDelta);
                    sLurePosDelta.y = 15.0f;
                    sLureCastDelta.x = sLureCastDelta.z = 0.0f;
                    sLureCastDelta.y = -1.0f;
                    D_80B7E148 = 0.0f;
                    D_80B7E0B2 = 5;
                    sRodReelingSpeed = 0.5f;
                    D_80B7E118 = Rand_ZeroFloat(1.9f);
                    sFishMouthOffset.y = 500.0f;
                    Sfx_PlaySfxAtPos(&sSoundPos, NA_SE_IT_SWORD_SWING_HARD);
                }
            }
            break;

        case 1: // casting the line
            spE0 = sLurePos.y;

            sLurePos.x += sLurePosDelta.x;
            sLurePos.y += sLurePosDelta.y;
            sLurePos.z += sLurePosDelta.z;

            sLurePosDelta.x += sLureCastDelta.x;
            sLurePosDelta.y += sLureCastDelta.y;
            sLurePosDelta.z += sLureCastDelta.z;
            // air drag by hat or reeling during cast.
            if (Fishing_IsReelHeld() || sIsOwnersHatHooked) {
                sLurePosDelta.x *= 0.9f;
                sLurePosDelta.z *= 0.9f;
                if (!sIsOwnersHatHooked) {
                    Sfx_PlaySfxCentered(NA_SE_IT_FISHING_REEL_HIGH - SFX_FLAG);
                }
            }

            float spD8 = sLurePos.x - sRodTipPos.x;
            float spD4 = sLurePos.y - sRodTipPos.y;
            float spD0 = sLurePos.z - sRodTipPos.z;

            if (D_80B7E0B2 != 0) {
                sLureRot.x = sReelLineRot[LINE_SEG_COUNT - 2].x + M_PI;
                sLureRot.y = sReelLineRot[LINE_SEG_COUNT - 2].y;
            } else {
                sLureRot.x = 0.0f;
                sLureRot.y = atan2f(spD8, spD0) + M_PI;
            }

            float lengthCasted = sqrtf(SQ(spD8) + SQ(spD4) + SQ(spD0));
            if (lengthCasted > 1000.0f) {
                lengthCasted = 1000.0f;
            }
            sRodLineSpooled = 200.0f - (lengthCasted * 200.0f * 0.001f);

            float lureXZLen = sPortableFishing ? 0.0f : SQ(sLurePos.x) + SQ(sLurePos.z);
            if (lureXZLen > SQ(920.0f)) {
                if ((KREG(56) != 0) || (sLurePos.y > 160.0f) || (sLurePos.x < 80.0f) || (sLurePos.x > 180.0f) ||
                    (sLurePos.z > 1350.0f) || (sLurePos.z < 1100.0f) || (sLurePos.y < 45.0f)) {
                    Vec3f sp80 = this->actor.world.pos;
                    this->actor.prevPos = this->actor.world.pos = sLurePos;
                    Actor_UpdateBgCheckInfo(play, &this->actor, 15.0f, 30.0f, 30.0f, 0x43);
                    this->actor.world.pos = sp80;

                    if (this->actor.bgCheckFlags & 0x10) {
                        sLurePosDelta.y = -0.5f;
                    }
                    if (this->actor.bgCheckFlags & 8) {
                        if (sLurePosDelta.y > 0.0f) {
                            sLurePosDelta.y = 0.0f;
                        }
                        sLurePosDelta.x = sLurePosDelta.z = 0.0f;
                    }
                } else {
                    if (Fishing_IsAboveCounter(&sLurePos)) {
                        sRodCastState = 3;
                        sReelLinePosStep = 0.0f;
                    }
                }

                spE4 = ((sqrtf(lureXZLen) - 920.0f) * 0.11f) + WATER_SURFACE_Y(play);
                if (sLurePos.y <= spE4) {
                    sLurePos.y = spE4;
                    sLurePosDelta.x = sLurePosDelta.y = sLurePosDelta.z = 0.0f;
                    sRodCastState = 3;
                    sReelLinePosStep = 0.0;
                } else {
                    Math_ApproachF(&D_80B7E148, 0.0f, 1.0f, 0.05f);
                }
            } else {
                spE4 = WATER_SURFACE_Y(play);

                if (sLurePos.y <= spE4) {
                    sRodCastState = 2;
                    sReelLinePosStep = 0.0f;
                    sLurePosDelta.x = sLurePosDelta.z = 0.0f;

                    if (sLureEquipped == FS_LURE_SINKING) {
                        sLureMoveDelay = 0;
                    } else {
                        sLureMoveDelay = 10;
                    }

                    if ((sLurePos.y <= spE4) && (spE4 < spE0) && (spE4 == WATER_SURFACE_Y(play))) {
                        D_80B7E114 = 10;
                        Sfx_PlaySfxAtPos(&sSoundPos, NA_SE_EV_BOMB_DROP_WATER);
                        sLureCastDelta.y = 0.0f;
                        sLurePosDelta.y *= 0.2f;

                        for (i = 0; i < 50; i++) {
                            float sp7C = Rand_ZeroFloat(1.5f) + 0.5f;
                            float sp78 = Rand_ZeroFloat(6.28f);

                            sp9C.x = sinf(sp78) * sp7C;
                            sp9C.z = cosf(sp78) * sp7C;
                            sp9C.y = Rand_ZeroFloat(3.0f) + 3.0f;

                            spA8 = sLurePos;
                            spA8.x += (sp9C.x * 3.0f);
                            spA8.y = WATER_SURFACE_Y(play);
                            spA8.z += (sp9C.z * 3.0f);
                            Fishing_SpawnDustSplash(NULL, play->specialEffects, &spA8, &sp9C,
                                                    Rand_ZeroFloat(0.02f) + 0.025f);
                        }

                        spA8 = sLurePos;
                        spA8.y = WATER_SURFACE_Y(play);
                        Fishing_SpawnRipple(NULL, play->specialEffects, &spA8, 100.0f, 800.0f, 150, 90);
                    }
                } else {
                    Math_ApproachZeroF(&D_80B7E148, 1.0f, 0.05f);
                }
            }

            sReelLinePos[LINE_SEG_COUNT - 1].x = sLurePos.x;
            sReelLinePos[LINE_SEG_COUNT - 1].y = sLurePos.y;
            sReelLinePos[LINE_SEG_COUNT - 1].z = sLurePos.z;

            sLureWiggleSign = 1.0f;
            sLureRotXTarget = 0.5f;
            break;

        case 2:
            if (sLurePos.y <= WATER_SURFACE_Y(play)) {
                sLurePos.y += sLurePosDelta.y;

                Math_ApproachZeroF(&sLurePosDelta.y, 1.0f, 1.0f);

                if (sLureEquipped != FS_LURE_SINKING) {
                    Math_ApproachF(&sLurePos.y, WATER_SURFACE_Y(play), 0.5f, 1.0f);
                }
            }

            Math_ApproachF(&D_80B7E148, 2.0f, 1.0f, 0.1f);

            if (sLureMoveDelay == 0) {
                sRodCastState = 3;
            } else {
                sLureMoveDelay--;
            }
            break;

        case 3:
            sSinkingLureSegmentIndex = 0;

            if (sIsOwnersHatHooked && ((SQ(sLurePos.x) + SQ(sLurePos.z)) < SQ(500.0f))) {
                sIsOwnersHatSunk = true;
            }

            player->unk_860 = 2;

            if (sLureWigglePosY < 3.0f) {
                spD0 = sLureRotXTarget * Math_SinS(sLureTimer * 0x1060);
                Math_ApproachF(&sLureRot.x, -M_PI / 6.0f + spD0, 0.3f, sLureRotXStep);
                Math_ApproachF(&sLureRotXStep, 0.5f, 1.0f, 0.02f);
                Math_ApproachZeroF(&sLureRotXTarget, 1.0f, 0.02f);
            } else {
                sLureRotXStep = 0.0f;
            }

            int16_t spDC = 0x4000;
            spE4 = WATER_SURFACE_Y(play);

            lureXZLen = sPortableFishing ? 0.0f : SQ(sLurePos.x) + SQ(sLurePos.z);
            if (lureXZLen < SQ(920.0f)) {
                if (sLurePos.y <= (spE4 + 4.0f)) {
                    float wiggle = 0.0f;

                    if (D_80B7E150 == 0) {
                        if (fabsf(input->x) > 30.0f) {
                            wiggle = fabsf((input->x - sStickAdjXPrev) * (1.0f / 60.0f));
                        } else if (fabsf(input->y) > 30.0f) {
                            wiggle = fabsf((input->y - sStickAdjYPrev) * (1.0f / 60.0f));
                        }
                    }

                    if (wiggle > 1.0f) {
                        wiggle = 1.0f;
                    }
                    if (sIsOwnersHatHooked) {
                        if (wiggle > 0.3f) {
                            wiggle = 0.3f;
                        }
                    }

                    if ((wiggle > 0.2f) && (sLureWigglePosY < 4.0f)) {
                        D_80B7E150 = 5;

                        if (wiggle > 0.8f) {
                            sWiggleAttraction = 2;
                        } else {
                            sWiggleAttraction = 1;
                        }

                        sp90.x = player->actor.world.pos.x - sLurePos.x;
                        sp90.z = player->actor.world.pos.z - sLurePos.z;
                        sp90.y = atan2f(sp90.x, sp90.z);

                        sLureWiggleRotYTarget = (wiggle * sLureWiggleSign) + sp90.y;
                        sLureWiggleSign *= -1.0f;
                        sLureWigglePosY = fabsf(wiggle) * 6.0f;
                        sLureRot.x = 0.0f;
                        sLureRotXTarget = 0.5f;
                        sRodLineSpooled += (fabsf(wiggle) * (7.5f + (KREG(25) * 0.1f)));

                        func_800F436C(&sSoundPos, NA_SE_EV_LURE_MOVE_W, (wiggle * 1.999f * 0.25f) + 0.75f);

                        if (sLureEquipped == FS_LURE_SINKING) {
                            sLureLineSegPosDelta.y = 5.0f * wiggle;
                            sReelLinePos[LINE_SEG_COUNT - 1].y += sLureLineSegPosDelta.y;
                            sLurePos.y += sLureLineSegPosDelta.y;
                        }
                    } else if (Fishing_IsReelHeld()) {
                        spDC = 0x500;
                        sLureWiggleRotYTarget = sReelLineRot[LINE_SEG_COUNT - 2].y + M_PI;
                        sLureRot.x = 0.0f;
                        sLureRotXTarget = 0.5f;
                        if (sLureEquipped == FS_LURE_SINKING) {
                            sLureLineSegPosDelta.y = 0.2f;
                            sReelLinePos[LINE_SEG_COUNT - 1].y += sLureLineSegPosDelta.y;
                            sLurePos.y += sLureLineSegPosDelta.y;
                        }
                    }
                } else {
                    if (sRodLineSpooled > 150.0f) {
                        sLureRot.x = sReelLineRot[LINE_SEG_COUNT - 2].x + M_PI;
                        sLureWiggleRotYTarget = sReelLineRot[LINE_SEG_COUNT - 2].y + M_PI;
                        sRodLineSpooled += 2.0f;
                    }
                }
            } else {
                spE4 = ((sqrtf(lureXZLen) - 920.0f) * 0.11f) + WATER_SURFACE_Y(play);
                if (sLurePos.y <= spE4) {
                    sLurePos.y = spE4;
                    spDC = 0x500;
                    sLureWiggleRotYTarget = sReelLineRot[LINE_SEG_COUNT - 2].y + M_PI;
                    sLureRot.x = 0.0f;
                } else {
                    if (sRodLineSpooled > 150.0f) {
                        sLureRot.x = sReelLineRot[LINE_SEG_COUNT - 2].x + M_PI;
                        sLureWiggleRotYTarget = sReelLineRot[LINE_SEG_COUNT - 2].y + M_PI;
                        sRodLineSpooled += 2.0f;
                    }
                }
            }

            Math_ApproachZeroF(&sLureWigglePosY, 1.0f, 0.3f);
            Math_ApproachS(&sLureWiggleRotY, (sLureWiggleRotYTarget * 32768.0f) / M_PI, 3, spDC);

            sLureRot.y = (sLureWiggleRotY / 32768.0f) * M_PI;

            sp90.x = 0.0f;
            sp90.y = 0.0f;
            sp90.z = sLureWigglePosY;

            Matrix_RotateY(sLureRot.y, MTXMODE_NEW);

            if (sLureEquipped == FS_LURE_SINKING) {
                Matrix_MultVec3f(&sp90, &sp64);
                sLureLineSegPosDelta.x = sp64.x;
                sLureLineSegPosDelta.z = sp64.z;
                phi_f0 = 10.0f;
            } else {
                Matrix_MultVec3f(&sp90, &sLureLineSegPosDelta);
                phi_f0 = 0.0f;
            }

            sLure1Rotate = 0.0f;

            if ((sLureEquipped == FS_LURE_UNK) && Fishing_IsReelHeld()) {
                sLureLineSegPosDelta.y = -2.0f;

                if ((sLureTimer & 1) != 0) {
                    sLure1Rotate = 0.5f;
                } else {
                    sLure1Rotate = -0.5f;
                }
            } else if (sReelLinePos[LINE_SEG_COUNT - 1].y < (WATER_SURFACE_Y(play) + phi_f0)) {
                if (sLureEquipped == FS_LURE_SINKING) {
                    Vec3f sp58 = this->actor.world.pos;
                    this->actor.prevPos = this->actor.world.pos = sLurePos;
                    Actor_UpdateBgCheckInfo(play, &this->actor, 15.0f, 30.0f, 30.0f, 0x44);
                    this->actor.world.pos = sp58;

                    sLureLineSegPosDelta.y += -0.5f;
                    if (sLureLineSegPosDelta.y < -1.0f) {
                        sLureLineSegPosDelta.y = -1.0f;
                    }

                    if (sLurePos.y < (this->actor.floorHeight + 5.0f)) {
                        sReelLinePos[LINE_SEG_COUNT - 1].y = sLurePos.y = this->actor.floorHeight + 5.0f;
                        sLureLineSegPosDelta.y = 0.0f;
                    } else {
                        sWiggleAttraction = 1;
                    }
                } else {
                    sLureLineSegPosDelta.y = fabsf(sReelLinePos[LINE_SEG_COUNT - 1].y - WATER_SURFACE_Y(play)) * 0.2f;
                    if (sLureLineSegPosDelta.y > 1.5f) {
                        sLureLineSegPosDelta.y = 1.5f;
                    }
                }
            }

            sReelLinePos[LINE_SEG_COUNT - 1].x += sLureLineSegPosDelta.x;
            sReelLinePos[LINE_SEG_COUNT - 1].y += sLureLineSegPosDelta.y;
            sReelLinePos[LINE_SEG_COUNT - 1].z += sLureLineSegPosDelta.z;

            if (sReelLinePos[LINE_SEG_COUNT - 1].y > (spE4 + 6.0f)) {
                sReelLinePos[LINE_SEG_COUNT - 1].y -= 5.0f;
            }

            sLurePosDelta.x = sLurePosDelta.y = sLurePosDelta.z = sLureCastDelta.y = 0.0f;

            if (Fishing_IsReelHeld()) {
                sRodLineSpooled += sRodReelingSpeed;
                Sfx_PlaySfxCentered(NA_SE_IT_FISHING_REEL_SLOW - SFX_FLAG);
                Math_ApproachF(&sReelLinePosStep, 1000.0f, 1.0f, 0.2f);

                if (sReelLinePos[LINE_SEG_COUNT - 1].y > (WATER_SURFACE_Y(play) + 4.0f)) {
                    Math_ApproachF(&D_80B7E148, 3.0f, 1.0f, 0.2f);
                } else {
                    Math_ApproachF(&D_80B7E148, 1.0f, 1.0f, 0.2f);
                }
            } else {
                Math_ApproachF(&D_80B7E148, 2.0f, 1.0f, 0.2f);
            }

            Math_ApproachF(&sLurePos.x, sReelLinePos[LINE_SEG_COUNT - 1].x, 1.0f, sReelLinePosStep);
            Math_ApproachF(&sLurePos.y, sReelLinePos[LINE_SEG_COUNT - 1].y, 1.0f, sReelLinePosStep);
            Math_ApproachF(&sLurePos.z, sReelLinePos[LINE_SEG_COUNT - 1].z, 1.0f, sReelLinePosStep);

            if (sLureWigglePosY > 1.0f) {
                Math_ApproachF(&sReelLinePosStep, 1000.0f, 1.0f, 1.0f);
            }

            Math_ApproachF(&sReelLinePosStep, 1000.0f, 1.0f, 0.1f);

            if (sRodLineSpooled >= 195.0f) {
                sRodLineSpooled = 195.0f;
                sRodCastState = 0;
                D_80B7E148 = 520.0f;
                sFishingPlayerCinematicState = 3;
            }

            if ((sLurePos.y <= (WATER_SURFACE_Y(play) + 4.0f)) && (sLurePos.y >= (WATER_SURFACE_Y(play) - 4.0f))) {

                int16_t timer = 63;
                if (Fishing_IsReelHeld() || (sLureWigglePosY > 1.0f)) {
                    timer = 1;
                }

                if ((sLureTimer & timer) == 0) {
                    spA8 = sLurePos;
                    spA8.y = WATER_SURFACE_Y(play);
                    Fishing_SpawnRipple(NULL, play->specialEffects, &spA8, 30.0f, 300.0f, 150, 90);
                }
            }
            break;

        case 4:
            if (this->unk_157 != 0) {
                this->unk_157--;
                sRodLineSpooled += sRodReelingSpeed;
            }

            if (Fishing_IsReelHeld()) {
                if (!sPortableFishing && ((SQ(sLurePos.x) + SQ(sLurePos.z)) > SQ(920.0f))) {
                    sRodLineSpooled += (1.0f + (KREG(65) * 0.1f));
                } else {
                    sRodLineSpooled += sRodReelingSpeed;
                }
                Sfx_PlaySfxCentered(NA_SE_IT_FISHING_REEL_SLOW - SFX_FLAG);
            }

            if ((sLureTimer & 0x1F) == 0) {
                if (sLineHooked || (sLureEquipped != FS_LURE_SINKING)) {
                    D_80B7A6A4 = 5;
                }
            }

            Math_ApproachF(&D_80B7E148, 0.0f, 1.0f, 0.2f);
            break;

        case 5:
            sFishingLineScale = 0.0005000001f;
            sReelLinePos[LINE_SEG_COUNT - 1].x = sLurePos.x;
            sReelLinePos[LINE_SEG_COUNT - 1].y = sLurePos.y;
            sReelLinePos[LINE_SEG_COUNT - 1].z = sLurePos.z;
            D_80B7E148 = 2.0f;
            break;
    }
}

int32_t Fishing_SplashBySize(Fishing* this, PlayState* play, uint8_t ignorePosCheck) {
    int16_t i;
    int16_t count = { 0 };
    float scale = { 0 };
    Vec3f pos;
    Vec3f vel;

    if ((this->actor.world.pos.y < (WATER_SURFACE_Y(play) - 10.0f)) && !ignorePosCheck) {
        return false;
    }

    if (this->fishLength >= 40.0f) {
        count = 40;
        scale = 1.2f;
    } else {
        count = 30;
        scale = 1.0f;
    }

    for (i = 0; i < count; i++) {
        float speedXZ = (Rand_ZeroFloat(1.5f) + 0.5f) * scale;
        float angle = Rand_ZeroFloat(6.28f);

        vel.x = sinf(angle) * speedXZ;
        vel.z = cosf(angle) * speedXZ;
        vel.y = (Rand_ZeroFloat(3.0f) + 3.0f) * scale;

        pos = this->actor.world.pos;
        pos.x += vel.x * 3.0f;
        pos.y = WATER_SURFACE_Y(play);
        pos.z += vel.z * 3.0f;

        Fishing_SpawnDustSplash(&this->actor.projectedPos, play->specialEffects, &pos, &vel,
                                (Rand_ZeroFloat(0.02f) + 0.025f) * scale);
    }

    pos = this->actor.world.pos;
    pos.y = WATER_SURFACE_Y(play);

    Fishing_SpawnRipple(&this->actor.projectedPos, play->specialEffects, &pos, 100.0f, 800.0f, 150, 90);

    this->lilyTimer = 30;

    return true;
}

void Fishing_SplashBySize2(Fishing* this, PlayState* play) {
    int16_t count = { 0 };
    int16_t i;
    float scale = { 0 };
    Vec3f vel;

    if (this->fishLength >= 45.0f) {
        count = 30;
        scale = 0.5f;
    } else {
        count = 20;
        scale = 0.3f;
    }

    for (i = 0; i < count; i++) {
        float speedXZ = (Rand_ZeroFloat(1.5f) + 0.5f) * scale;
        float angle = Rand_ZeroFloat(6.28f);

        vel.x = sinf(angle) * speedXZ;
        vel.z = cosf(angle) * speedXZ;
        vel.y = Rand_ZeroFloat(2.0f) + 2.0f;

        Vec3f pos = this->actor.world.pos;
        pos.x += (vel.x * 3.0f);
        pos.y += (vel.y * 3.0f);
        pos.z += (vel.z * 3.0f);

        Fishing_SpawnDustSplash(&this->actor.projectedPos, play->specialEffects, &pos, &vel,
                                (Rand_ZeroFloat(0.02f) + 0.025f) * scale);
    }
}

void func_80B70ED4(Fishing* this, MovementInput* input) {
    Vec3f lineVec;
    Vec3f sp28;

    lineVec.x = sLurePos.x - this->actor.world.pos.x;
    lineVec.y = sLurePos.y - this->actor.world.pos.y;
    lineVec.z = sLurePos.z - this->actor.world.pos.z;

    float lineLengthSQ = SQ(lineVec.x) + SQ(lineVec.y) + SQ(lineVec.z);

    if ((sRodCastState == 3) && (this->unk_1A2 == 0) && (sIsOwnersHatHooked == 0)) {
        Matrix_RotateY((-this->actor.shape.rot.y / 32768.0f) * M_PI, MTXMODE_NEW);
        Matrix_MultVec3f(&lineVec, &sp28);

        if ((sp28.z > 0.0f) || (this->fishLength < 40.0f) || this->isWild) {
            if ((this->fishState == 7) && (lineLengthSQ < SQ(200.0f))) {
                this->fishState = 4;
                this->fishTargetPos = sLurePos;
                this->rotationStep = 28672.0f;
                this->speedTarget = 5.0f;
            } else {
                if ((Fishing_IsReelHeld() || (sLureWigglePosY > 1.0f)) &&
                    (lineLengthSQ < SQ(this->isWild ? 500.0f : 120.0f))) {
                    this->fishState = 2;
                    this->unk_15E = 0;
                    this->timerArray[0] = 0;
                    this->timerArray[2] = (int16_t)Rand_ZeroFloat(100.0f) + 100;
                    this->perception = Fishing_GetFishPerception(this);
                    this->rotationStep = 0.0f;
                }

                if ((this->timerArray[1] == 0) && (lineLengthSQ < SQ(this->isWild ? 350.0f : 70.0f))) {
                    this->fishState = 2;
                    this->unk_15E = 0;
                    this->timerArray[0] = 0;
                    this->timerArray[2] = (int16_t)Rand_ZeroFloat(100.0f) + 100;
                    this->perception = Fishing_GetFishPerception(this);
                    this->rotationStep = 0.0f;
                }
            }
        }
    } else if ((sRodCastState == 4) && sLineHooked && (lineLengthSQ < SQ(100.0f)) && (this->fishState >= 10)) {
        this->fishStateNext = 0;
        this->fishState = 1;
        this->unk_1A4 = 1000;
        this->unk_1A2 = 100;
        this->timerArray[1] = 50;
    }

    if ((sLureEquipped != FS_LURE_SINKING) && (D_80B7E114 != 0) && (this->fishLength > 60.0f) &&
        (lineLengthSQ < SQ(30.0f)) && (this->fishState >= 10)) {
        this->fishStateNext = 0;
        this->fishState = 1;
        this->unk_1A4 = 1000;
        this->unk_1A2 = 100;
        this->timerArray[1] = 50;
    }
}

void Fishing_FishLeapSfx(Fishing* this, uint8_t outOfWater) {
    int16_t sfxId = { 0 };
    uint8_t length = { 0 };

    if (this->isLoach == 0) {
        length = this->fishLength;
    } else {
        length = 2.0f * this->fishLength;
    }

    if (outOfWater == false) {
        if (length >= 50) {
            sfxId = NA_SE_EV_DIVE_INTO_WATER;
        } else if (length >= 40) {
            sfxId = NA_SE_EV_BOMB_DROP_WATER;
        } else {
            sfxId = NA_SE_EV_BOMB_DROP_WATER;
        }
    } else {
        if (length >= 50) {
            sfxId = NA_SE_EV_JUMP_OUT_WATER;
        } else if (length >= 40) {
            sfxId = NA_SE_EV_OUT_OF_WATER;
        } else {
            sfxId = NA_SE_EV_OUT_OF_WATER;
        }
    }

    Audio_PlayActorSound2(&this->actor, sfxId);
}

void Fishing_HandleAquariumDialog(Fishing* this, PlayState* play) {
    if (sLinkAge == LINK_AGE_CHILD) {
        if ((HIGH_SCORE(HS_FISHING) & HS_FISH_LENGTH_CHILD) != 0) {
            if (HIGH_SCORE(HS_FISHING) & HS_FISH_CHEAT_CHILD) {
                this->actor.textId = 0x40B1;
            } else {
                this->actor.textId = 0x4089;
            }
        } else {
            this->actor.textId = 0x40AE;
        }
    } else {
        if ((HIGH_SCORE(HS_FISHING) & HS_FISH_LENGTH_ADULT) != 0) {
            if (HIGH_SCORE(HS_FISHING) & HS_FISH_CHEAT_ADULT) {
                this->actor.textId = 0x40B1;
            } else {
                this->actor.textId = 0x4089;
            }
        } else {
            this->actor.textId = 0x40AE;
        }
    }

    if (!this->isAquariumMessage) {
        if (this->aquariumWaitTimer == 0) {
            this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;

            if (Actor_ProcessTalkRequest(&this->actor, play)) {
                sFishLengthToWeigh = sFishingRecordLength;
                this->isAquariumMessage = true;
            } else {
                func_8002F2F4(&this->actor, play);
            }
        } else {
            this->aquariumWaitTimer--;
            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        }
    } else if (Actor_TextboxIsClosing(&this->actor, play)) {
        this->isAquariumMessage = false;
        this->aquariumWaitTimer = 20;
    }
}

float Fishing_GetMinimumRequiredScore() {
    int32_t weight = sLinkAge == 1 ? 10 : 13;

    return sqrt(((float)weight - 0.5f) / 0.0036f);
}

bool getShouldSpawnLoaches() {
    return true;
}

void Fishing_UpdateFish(Actor* thisx, PlayState* play2) {
    float temp = { 0 };
    int16_t i;
    int16_t rotXYScale = 10;
    float distX = { 0 };
    float distY = { 0 };
    float distZ = { 0 };
    float distToTarget = { 0 };
    float multiplier = { 0 };
    float playerSpeedMod = { 0 };
    Vec3f multiVecSrc;
    Vec3f targetPosOffset;
    int16_t rotXTarget = { 0 };
    int16_t rotYTarget = { 0 };
    int16_t rotZScale;
    int16_t timer = { 0 };
    int16_t rotXScale = { 0 };
    int16_t rotXStep = { 0 };
    int16_t rotYScale;
    int16_t rotYStep;
    Fishing* this = (Fishing*)thisx;
    PlayState* play = play2;
    Player* player = GET_PLAYER(play);
    MovementInput* input = &play->state.input;
    Vec3f bubblePos;
    uint8_t phi_v0_2 = { 0 };
    uint16_t attempts;
    float savedPortableWaterSurfaceY = sPortableWaterSurfaceY;
    bool savedPortableWaterFound = sPortableWaterFound;
    FishPresentationIdentity presentedIdentity;
    FishPresentationState presentedState;

    if (this->isWild) {
        if (this->wildWaterBox != NULL) {
            this->wildWaterSurfaceY = this->wildWaterBox->ySurface;
        }
        sPortableWaterSurfaceY = this->wildWaterSurfaceY;
        sPortableWaterFound = true;
    }

    presentedIdentity.sceneId = play->sceneNum;
    presentedIdentity.spawnKey = FishPresentation_MakeSpawnKey(
        play->sceneNum, this->actor.room,
        (int32_t)lroundf(this->actor.home.pos.x),
        (int32_t)lroundf(this->actor.home.pos.y),
        (int32_t)lroundf(this->actor.home.pos.z));
    if (FishPresentation_Read(&presentedIdentity, &presentedState)) {
        if (!this->remotePresentationActive) {
            this->remotePresentationActive = true;
            this->fishState = this->fishStateNext = 5;
            // A hooked fish can be moved from its original water box to a
            // remote player's hands in a single update. Keep it drawable while
            // externally presented instead of testing its stale water volume.
            this->actor.flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED;
        }
        this->actor.prevPos = this->actor.world.pos;
        this->actor.world.pos =
            (Vec3f){ presentedState.position[0], presentedState.position[1], presentedState.position[2] };
        this->actor.world.rot = this->actor.shape.rot =
            (Vec3s){ presentedState.rotation[0], presentedState.rotation[1], presentedState.rotation[2] };
        this->actor.speedXZ = 0.0f;
        this->actor.velocity = (Vec3f){ 0.0f, 0.0f, 0.0f };
        this->fishLength = presentedState.length;
        if (presentedState.isLoach == 0) {
            Actor_SetScale(&this->actor, this->fishLength * 15.0f * 0.00001f);
        } else {
            Actor_SetScale(&this->actor, this->fishLength * 65.0f * 0.000001f);
            this->actor.scale.x = this->actor.scale.z * 1.1f;
            this->actor.scale.y = this->actor.scale.z * 1.1f;
        }
        this->unk_160 = this->unk_162 = this->unk_164 = 0;
        this->stateAndTimer++;
        this->fishLimbDRotZDelta = presentedState.limbRotation[0];
        this->fishLimbEFRotYDelta = presentedState.limbRotation[1];
        this->fishLimb23RotYDelta = presentedState.limbRotation[2];
        this->fishLimb89RotYDelta = presentedState.limbRotation[3];
        this->fishLimb4RotYDelta = presentedState.limbRotation[4];
        this->loachRotYDelta[0] = presentedState.limbRotation[5];
        this->loachRotYDelta[1] = presentedState.limbRotation[6];
        this->loachRotYDelta[2] = presentedState.limbRotation[7];
        if (this->isWild) {
            sPortableWaterSurfaceY = savedPortableWaterSurfaceY;
            sPortableWaterFound = savedPortableWaterFound;
        }
        return;
    }
    if (this->remotePresentationActive) {
        this->remotePresentationActive = false;
        this->fishState = this->fishStateNext = 10;
        this->fishTargetPos = this->actor.home.pos;
        this->actor.flags &= ~ACTOR_FLAG_DRAW_CULLING_DISABLED;
    }

    this->actor.uncullZoneForward = 700.0f;
    this->actor.uncullZoneScale = 50.0f;

    if (this->isLoach == 0) {
        playerSpeedMod = (player->actor.speedXZ * 0.15f) + 0.25f;
    } else {
        playerSpeedMod = (player->actor.speedXZ * 0.3f) + 0.25f;
    }

    if ((D_80B7E0B0 != 0) || (sSubCamId != 0) ||
        (!this->isWild && (player->actor.world.pos.z > 1150.0f) && (this->fishState != 100))) {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    } else {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        if (sRodCastState != 0) {
            if (D_80B7E0B2 == 0) {
                this->actor.focus.pos = sLurePos;
            } else if (D_80B7E0B2 == 1) {
                sFishingPlayerCinematicState = 1;
                D_80B7FED0 = 0.0f;
                sLureCameraZoomLevel = 2;
            }
        }
        this->actor.focus.pos = this->actor.world.pos;
    }

    this->stateAndTimer++;

    for (i = 0; i < 4; i++) {
        if (this->timerArray[i] != 0) {
            this->timerArray[i]--;
        }
    }

    if (this->unk_1A4 != 0) {
        this->unk_1A4--;
    }

    if (this->unk_1A2 != 0) {
        this->unk_1A2--;
    }

    if (this->bumpTimer != 0) {
        this->bumpTimer--;
    }

    if (this->lilyTimer != 0) {
        this->lilyTimer--;
    }

    Math_ApproachF(&this->fishLimbRotPhaseStep, this->unk_190, 1.0f, 0.2f);

    if (this->fishState == 6) {
        Math_ApproachF(&this->fishLimbRotPhaseMag, this->unk_194, 0.2f, 200.0f);
    } else {
        float phi_f0 = 1.0f;
        float phi_f2 = 1.0f;
        if (this->actor.world.pos.y > WATER_SURFACE_Y(play)) {
            phi_f0 = (KREG(64) * 0.1f) + 1.5f;
            phi_f2 = 3.0f;
        }
        Math_ApproachF(&this->fishLimbRotPhaseMag, this->unk_194 * phi_f0, 1.0f, 500.0f * phi_f2);
    }

    Math_ApproachS(&this->fishLimbDRotZDelta, 0, 5, 0x1F4);

    if (this->isLoach == 0) {
        Actor_SetScale(&this->actor, this->fishLength * 15.0f * 0.00001f);

        this->fishLimbRotPhase += this->fishLimbRotPhaseStep;

        temp = cosf(this->fishLimbRotPhase);
        this->fishLimb23RotYDelta = this->unk_16E + (int16_t)(temp * this->fishLimbRotPhaseMag);

        temp = cosf(this->fishLimbRotPhase + -1.2f);
        this->fishLimb4RotYDelta = this->unk_16E + (int16_t)(temp * this->fishLimbRotPhaseMag * 1.6f);
    } else {
        Actor_SetScale(&this->actor, this->fishLength * 65.0f * 0.000001f);

        this->actor.scale.x = this->actor.scale.z * 1.1f;
        this->actor.scale.y = this->actor.scale.z * 1.1f;

        this->fishLimbRotPhase += this->fishLimbRotPhaseStep * 0.8f;

        for (i = 0; i < 3; i++) {
            temp = cosf(this->fishLimbRotPhase + (i * 2.1f));
            this->loachRotYDelta[i] = this->unk_16E + (int16_t)(temp * this->fishLimbRotPhaseMag * 2.0f);
        }

        temp = cosf(this->fishLimbRotPhase + 0.4f);
        this->fishLimb23RotYDelta = (this->fishLimbRotPhaseMag * temp * 2.0f) * 0.6f;
    }

    distX = this->fishTargetPos.x - this->actor.world.pos.x;
    distY = this->fishTargetPos.y - this->actor.world.pos.y;
    distZ = this->fishTargetPos.z - this->actor.world.pos.z;

    rotYTarget = RADF_TO_BINANG(atan2f(distX, distZ));
    distToTarget = sqrtf(SQ(distX) + SQ(distZ));

    rotXTarget = RADF_TO_BINANG(atan2f(distY, distToTarget));
    distToTarget = sqrtf(SQ(distX) + SQ(distZ) + SQ(distY));

    if ((this->bumpTimer != 0) && (this->fishState != 2) && (this->fishState != 3) && (this->fishState != 4)) {
        if ((this->stateAndTimer & 0x40) != 0) {
            rotYTarget += 0x4000;
        } else {
            rotYTarget -= 0x4000;
        }
        if (((this->stateAndTimer + 0x20) & 0x40) != 0) {
            rotXTarget += 0x2000;
        } else {
            rotXTarget -= 0x2000;
        }
    }

    switch (this->fishState) {
        case 100:
            Fishing_HandleAquariumDialog(this, play);

            this->actor.uncullZoneForward = 500.0f;
            this->actor.uncullZoneScale = 300.0f;

            Lights_PointNoGlowSetInfo(&this->lightInfo, (int16_t)this->actor.world.pos.x,
                                      (int16_t)this->actor.world.pos.y + 20.0f, (int16_t)this->actor.world.pos.z - 50.0f, 255,
                                      255, 255, 255);

            this->fishLength = sFishingRecordLength;
            targetPosOffset.y = (float)Math_SinS(play->gameplayFrames * 300) * 1;
            targetPosOffset.z = (float)Math_SinS(play->gameplayFrames * 230) * 2;
            this->actor.world.pos.x = 130.0f;
            this->actor.world.pos.y = 55.0f + targetPosOffset.y;
            this->actor.world.pos.z = 1300.0f + targetPosOffset.z;
            this->actor.shape.rot.y = -0x8000;

            if ((this->actor.projectedPos.z < 200.0f) && (this->actor.projectedPos.z > 0.0f)) {
                bubblePos.x = Rand_CenteredFloat(5.0f) + 130.0f;
                bubblePos.y = 40.0f;
                bubblePos.z = Rand_CenteredFloat(5.0f) + 1280.0f;
                Fishing_SpawnBubble(NULL, play->specialEffects, &bubblePos, Rand_ZeroFloat(0.02f) + 0.03f, 1);
            }

            Math_ApproachS(&this->fishLimbEFRotYDelta, (Math_SinS(this->stateAndTimer * 0x800) * 2500.0f) + 2500.0f, 2,
                           0x7D0);
            Math_ApproachS(&this->fishLimb89RotYDelta, Math_SinS(this->stateAndTimer * 0xA00) * 1500.0f, 2, 0x7D0);

            this->unk_190 = 0.3f;
            this->unk_194 = 1000.0f / 3.0f;
            return;

        case 10:
            if (!this->isWild) {
                this->fishTargetPos = this->actor.home.pos;
            }

            Math_ApproachF(&this->actor.speedXZ, this->isWild ? 3.0f : 2.0f, 1.0f, 0.5f);
            Math_ApproachF(&this->rotationStep, 4096.0f, 1.0f, 256.0f);

            if (distToTarget < 40.0f) {
                if (this->isWild) {
                    this->fishTargetPos.x = Fishing_GetRandomFishTargetX(this, 500.0f);
                    this->fishTargetPos.y = (WATER_SURFACE_Y(play) - 50.0f) - Rand_ZeroFloat(80.0f);
                    this->fishTargetPos.z = Fishing_GetRandomFishTargetZ(this, 500.0f);
                    this->unk_190 = 1.0f;
                    this->unk_194 = 2000.0f;
                } else {
                    this->fishState = 11;
                    this->unk_190 = 0.4f;
                    this->unk_194 = 500.0f;
                }
            }

            func_80B70ED4(this, input);

            if (this->actor.xzDistToPlayer < (250.0f * playerSpeedMod)) {
                this->fishStateNext = this->fishState = 0;
                this->unk_1A4 = 1000;
                this->unk_1A2 = 200;
                this->timerArray[1] = 50;
            }
            break;

        case 11:
            this->fishTargetPos = this->actor.home.pos;

            Math_ApproachF(&this->actor.speedXZ, 0.0f, 1.0f, 0.05f);
            Math_ApproachF(&this->rotationStep, 0.0f, 1.0f, 256.0f);

            if (distToTarget >= 40.0f) {
                this->fishState = 10;
                this->unk_190 = 1.0f;
                this->unk_194 = 2000.0f;
            }
            func_80B70ED4(this, input);

            if (this->actor.xzDistToPlayer < (250.0f * playerSpeedMod)) {
                this->fishStateNext = this->fishState = 0;
                this->unk_1A4 = 1000;
                this->unk_1A2 = 200;
                this->timerArray[1] = 50;
            }

            if (!this->isWild && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                if ((gSaveContext.dayTime >= 0xC000) && (gSaveContext.dayTime <= 0xC01B)) {
                    this->fishState = 7;
                    this->timerArray[3] = (int16_t)Rand_ZeroFloat(150.0f) + 200;
                }
                if ((gSaveContext.dayTime >= 0x3AAA) && (gSaveContext.dayTime <= 0x3AC5)) {
                    this->fishState = 7;
                    this->timerArray[3] = (int16_t)Rand_ZeroFloat(150.0f) + 200;
                }
            }

            if (!this->isWild && (KREG(15) != 0)) {
                KREG(15) = 0;
                this->fishState = 7;
                this->timerArray[3] = (int16_t)Rand_ZeroFloat(150.0f) + 2000;
            }
            break;

        case 0:
            Math_ApproachF(&this->actor.speedXZ, 1.0f, 1.0f, 0.05f);
            Math_ApproachF(&this->rotationStep, 0.0f, 1.0f, 256.0f);

            if (this->timerArray[0] == 0) {
                if (this->unk_1A4 == 0) {
                    this->fishState = this->fishStateNext = 10;
                } else {
                    this->fishState = 1;
                    this->timerArray[0] = (int16_t)Rand_ZeroFloat(30.0f) + 10;
                    this->fishTargetPos.x = Fishing_GetRandomFishTargetX(this, 300.0f);
                    this->fishTargetPos.y = (WATER_SURFACE_Y(play) - 50.0f) - Rand_ZeroFloat(50.0f);
                    this->fishTargetPos.z = Fishing_GetRandomFishTargetZ(this, 300.0f);
                    this->unk_190 = 1.0f;
                    this->unk_194 = 2000.0f;
                }
            }

            if (sLureEquipped == FS_LURE_SINKING) {
                func_80B70ED4(this, input);
            } else {
                this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            }
            break;

        case 1:
            if (this->isLoach == 1) {
                this->fishState = -1;
                this->unk_1A4 = 20000;
                this->unk_1A2 = 20000;
                this->fishTargetPos.x = this->isWild ? this->actor.home.pos.x : 0.0f;
                this->fishTargetPos.y = this->isWild ? (WATER_SURFACE_Y(play) - 80.0f) : -140.0f;
                this->fishTargetPos.z = this->isWild ? this->actor.home.pos.z : 0.0f;
            } else {
                Math_ApproachF(&this->rotationStep, 4096.0f, 1.0f, 256.0f);

                if ((this->actor.xzDistToPlayer < (250.0f * playerSpeedMod)) || (this->timerArray[1] != 0)) {
                    Math_ApproachF(&this->rotationStep, 8192.0f, 1.0f, 768.0f);
                    Math_ApproachF(&this->actor.speedXZ, 4.2f, 1.0f, 0.75);
                    this->unk_190 = 1.2f;
                    this->unk_194 = 4000.0f;
                    this->timerArray[0] = 20;
                } else {
                    this->unk_190 = 1.0f;
                    this->unk_194 = 2000.0f;
                    Math_ApproachF(&this->actor.speedXZ, 1.5f, 1.0f, 0.1f);
                }

                if ((this->timerArray[0] == 0) || (distToTarget < 50.0f)) {
                    this->fishState = 0;
                    this->timerArray[0] = (int16_t)Rand_ZeroFloat(30.0f) + 3;
                    this->unk_190 = 1.0f;
                    this->unk_194 = 500.0f;
                }

                if (sLureEquipped == FS_LURE_SINKING) {
                    func_80B70ED4(this, input);
                } else {
                    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
                }
            }
            break;

        case -1: // loach state 1
            Math_ApproachS(&this->rotationTarget.x, 0, 0x14, 0x20);

            if ((this->actor.xzDistToPlayer < (250.0f * playerSpeedMod)) || (this->timerArray[1] != 0)) {
                Math_ApproachF(&this->actor.speedXZ, 3.0f, 1.0f, 0.75);
                this->unk_190 = 1.0f;
                this->timerArray[0] = 20;
                this->unk_194 = 4000.0f;
                Math_ApproachF(&this->rotationStep, 4096.0f, 1.0f, 256.0f);

                if ((play->gameplayFrames % 32) == 0) {
                    this->fishTargetPos.x = Fishing_GetRandomFishTargetX(this, 600.0f);
                    this->fishTargetPos.z = Fishing_GetRandomFishTargetZ(this, 600.0f);
                    this->fishTargetPos.y = this->isWild ? (WATER_SURFACE_Y(play) - 70.0f) : -120.0f;
                }
            } else if (distToTarget > 50.0f) {
                this->unk_190 = 0.8f;
                this->unk_194 = 1500.0f;
                Math_ApproachF(&this->actor.speedXZ, 1.0f, 1.0f, 0.1f);
                Math_ApproachF(&this->rotationStep, 2048.0f, 1.0f, 128.0f);
            } else {
                this->unk_190 = 0.4f;
                this->unk_194 = 500.0f;
                Math_ApproachZeroF(&this->actor.speedXZ, 1.0f, 0.02f);
                Math_ApproachF(&this->rotationStep, 0.0f, 1.0f, 256.0f);
            }

            if (this->unk_1A4 == 0) {
                this->fishState = 10;
                this->fishStateNext = 10;
            } else if ((KREG(2) != 0) || (((this->unk_1A4 & 0x7FF) == 0) && (this->unk_1A4 < 15000))) {
                KREG(2) = 0;
                this->fishState = -2;
                this->actor.world.rot.x = this->actor.shape.rot.x = 0;
                this->fishTargetPos.y = WATER_SURFACE_Y(play) + 10.0f;
                this->fishTargetPos.x = Fishing_GetRandomFishTargetX(this, 50.0f);
                this->fishTargetPos.z = Fishing_GetRandomFishTargetZ(this, 50.0f);
            }

            this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            break;

        case -2:
            if ((this->actor.xzDistToPlayer < (250.0f * playerSpeedMod)) || (this->timerArray[1] != 0)) {
                this->fishState = -1;
                this->fishTargetPos.y = -120.0f;
            } else {
                this->unk_190 = 0.6f;
                this->unk_194 = 1000.0f;

                Math_ApproachS(&this->rotationTarget.x, -0x1000, 0x14, 0x100);

                if (this->actor.world.pos.y < (WATER_SURFACE_Y(play) - 20.0f)) {
                    Math_ApproachF(&this->actor.speedXZ, 0.5f, 1.0f, 0.1f);
                } else {
                    Math_ApproachZeroF(&this->actor.speedXZ, 1.0f, 0.01f);

                    if ((this->actor.speedXZ == 0.0f) || (this->actor.world.pos.y > (WATER_SURFACE_Y(play) - 5.0f))) {
                        this->fishTargetPos.x = Fishing_GetRandomFishTargetX(this, 300.0f);
                        this->fishTargetPos.z = Fishing_GetRandomFishTargetZ(this, 300.0f);
                        this->fishTargetPos.y = this->actor.floorHeight + 10.0f;
                        this->fishState = -25;
                        this->rotationStep = 0.0f;

                        Vec3f spB8 = this->fishMouthPos;
                        spB8.y = WATER_SURFACE_Y(play);
                        Fishing_SpawnRipple(&this->actor.projectedPos, play->specialEffects, &spB8, 10.0f, 300.0f, 150,
                                            90);
                        Fishing_SpawnRipple(&this->actor.projectedPos, play->specialEffects, &spB8, 30.0f, 400.0f, 150,
                                            90);

                        Audio_PlayActorSound2(&this->actor, NA_SE_PL_CATCH_BOOMERANG);
                        break;
                    }
                }

                Math_ApproachF(&this->rotationStep, 2048.0f, 1.0f, 128.0f);
                this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            }
            break;

        case -25:
            if ((this->actor.xzDistToPlayer < (250.0f * playerSpeedMod)) || (this->timerArray[1] != 0)) {
                this->fishState = -1;
                this->fishTargetPos.y = -120.0f;
            } else {
                Math_ApproachS(&this->rotationTarget.x, 0x1000, 0x14, 0x6A);

                if (distToTarget > 40.0f) {
                    this->unk_190 = 0.7f;
                    this->unk_194 = 1200.0f;
                    Math_ApproachF(&this->actor.speedXZ, 0.5f, 1.0f, 0.01f);
                    Math_ApproachF(&this->rotationStep, 2048.0f, 1.0f, 128.0f);
                } else {
                    this->fishState = -1;
                }
            }
            break;

        case 2:
            if (((this->actor.params + D_80B7E118) & 1) != 0) {
                multiVecSrc.x = 10.0f;
            } else {
                multiVecSrc.x = -10.0f;
            }
            multiVecSrc.y = 0.0f;
            multiVecSrc.z = 0.0f;
            Matrix_RotateY(sLureRot.y, MTXMODE_NEW);
            Matrix_MultVec3f(&multiVecSrc, &targetPosOffset);

            this->fishTargetPos.x = sLurePos.x + targetPosOffset.x;
            this->fishTargetPos.z = sLurePos.z + targetPosOffset.z;

            if (sLureEquipped == FS_LURE_SINKING) {
                this->fishTargetPos.y = sLurePos.y;
            } else if (this->isLoach == 0) {
                this->fishTargetPos.y = sLurePos.y - 15.0f;
            } else {
                this->fishTargetPos.y = sLurePos.y - 5.0f;
            }

            if (this->fishTargetPos.y <= this->actor.floorHeight) {
                this->fishTargetPos.y = this->actor.floorHeight + 3.0f;
            }

            if ((sLureEquipped != FS_LURE_SINKING) && (this->fishTargetPos.y < this->actor.world.pos.y)) {
                Math_ApproachF(&this->actor.world.pos.y, this->fishTargetPos.y, 0.1f,
                               (this->actor.world.pos.y - this->fishTargetPos.y) * 0.1f);
            }

            Math_ApproachF(&this->rotationStep, 8192.0f, 1.0f, (KREG(16) * 128) + 384.0f);
            if (PCInput_IsFishingReelPressed()) {
                this->perception += 0.005f;
            }

            if (sWiggleAttraction != 0) {
                if (sWiggleAttraction == 1) {
                    this->perception += 0.01f;
                } else {
                    this->perception += 0.05f;
                }
                sWiggleAttraction = 0;
            }

            if (distToTarget < ((this->fishLength * 0.5f) + 20.0f)) {
                if (this->unk_15E == 0) {
                    this->unk_190 = 1.0f;
                    this->unk_194 = 500.0f;
                    this->timerArray[0] = (int16_t)Rand_ZeroFloat(10.0f) + 2;
                }
                Math_ApproachF(&this->actor.speedXZ, -0.2f, 1.0f, 0.1f);
                this->unk_15E = 1;
            } else {
                if (this->unk_15E != 0) {
                    this->unk_190 = 1.0f;
                    this->rotationStep = 0.0f;
                    this->unk_194 = 3000.0f;
                }
                Math_ApproachF(&this->actor.speedXZ, 3.0f, 1.0f, 0.15f);
                this->unk_15E = 0;
            }

            if (this->fishLength >= 60.0f) {
                multiplier = 0.3f;
            } else if (this->fishLength >= 45.0f) {
                multiplier = 0.6f;
            } else {
                multiplier = 1.0f;
            }

            if ((gSaveContext.dayTime >= 0xB555) && (gSaveContext.dayTime <= 0xCAAA)) {
                multiplier *= 1.75f;
            } else if ((gSaveContext.dayTime >= 0x3555) && (gSaveContext.dayTime <= 0x4AAA)) {
                multiplier *= 1.5f;
            } else if (sFishingFoggy != 0) {
                multiplier *= 1.5f;
            } else if ((uint8_t)sStormStrength != 0) {
                multiplier *= 3.0f;
            }

            float chance = 0.03f * multiplier;
            if (sLureEquipped == FS_LURE_SINKING) {
                chance *= 5.0f;
            }
            if (((this->timerArray[0] == 1) || (Rand_ZeroOne() < chance)) &&
                ((Rand_ZeroOne() < (this->perception * multiplier)) || ((this->isLoach + 1) == KREG(69)))) {
                if (this->isLoach == 0) {
                    this->fishState = 3;
                    this->unk_190 = 1.2f;
                    this->unk_194 = 5000.0f;
                    this->timerArray[0] = Rand_ZeroFloat(10.0f);
                } else {
                    this->fishState = -3;
                    this->unk_190 = 1.0f;
                    this->unk_194 = 3000.0f;
                    this->timerArray[0] = 40;
                }
                if (sLureEquipped == FS_LURE_SINKING) {
                    this->speedTarget = Rand_ZeroFloat(1.5f) + 3.0f;
                } else {
                    this->speedTarget = Rand_ZeroFloat(1.5f) + 4.5f;
                }
            }

            if ((sRodCastState != 3) || (this->timerArray[2] == 0) ||
                (!this->isWild &&
                 (sqrtf(SQ(this->actor.world.pos.x) + SQ(this->actor.world.pos.z)) > 800.0f))) {
                this->fishState = this->fishStateNext;
                this->timerArray[1] = (int16_t)Rand_ZeroFloat(30.0f) + 50;
                this->timerArray[0] = (int16_t)Rand_ZeroFloat(10.0f) + 5;
                this->unk_190 = 1.0f;
                this->rotationStep = 0.0f;
                this->unk_194 = 2000.0f;
            }

            if (this->actor.xzDistToPlayer < (100.0f * playerSpeedMod)) {
                this->fishStateNext = this->fishState = 0;
                this->unk_1A4 = 1000;
                this->unk_1A2 = 200;
                this->timerArray[1] = 50;
            }
            break;

        case 3:
            this->lilyTimer = 6;
            rotXYScale = 2;

            if ((((int16_t)player->actor.world.pos.x + D_80B7E118) & 1) != 0) {
                multiVecSrc.x = 30.0f;
            } else {
                multiVecSrc.x = -30.0f;
            }
            multiVecSrc.y = 0.0f;
            multiVecSrc.z = 30.0f;

            Matrix_RotateY(sLureRot.y, MTXMODE_NEW);
            Matrix_MultVec3f(&multiVecSrc, &targetPosOffset);

            this->fishTargetPos.x = sLurePos.x + targetPosOffset.x;
            this->fishTargetPos.z = sLurePos.z + targetPosOffset.z;
            this->fishTargetPos.y = sLurePos.y - 10.0f;
            this->rotationStep = 4096.0f;
            Math_ApproachF(&this->actor.speedXZ, this->speedTarget * 0.8f, 1.0f, 1.0f);

            if ((sRodCastState != 3) || (sLurePos.y > (WATER_SURFACE_Y(play) + 5.0f)) ||
                (!this->isWild && (sqrtf(SQ(sLurePos.x) + SQ(sLurePos.z)) > 800.0f))) {
                this->fishState = this->fishStateNext;
                this->timerArray[0] = 0;
                this->unk_190 = 1.0f;
                this->unk_194 = 2000.0f;
            } else if ((this->timerArray[0] == 0) || (distToTarget < 30.0f)) {
                this->fishState = 4;
                this->fishTargetPos = sLurePos;
                this->rotationStep = 16384.0f;
                this->unk_190 = 1.2f;
                this->unk_194 = 5000.0f;
                this->timerArray[0] = 20;
            }
            break;

        case 4:
            Math_ApproachF(&this->rotationStep, 16384.0f, 1.0f, 4096.0f);
            Math_ApproachS(&this->fishLimbDRotZDelta, 0x4E20, 4, 0x1388);

            this->lilyTimer = 50;
            rotXYScale = 2;
            this->fishTargetPos = sLurePos;
            Math_ApproachF(&this->actor.speedXZ, this->speedTarget, 1.0f, 1.0f);

            if ((sRodCastState != 3) || (this->timerArray[0] == 0) || (sLurePos.y > (WATER_SURFACE_Y(play) + 5.0f)) ||
                (!this->isWild && (sqrtf(SQ(sLurePos.x) + SQ(sLurePos.z)) > 800.0f))) {

                this->timerArray[0] = 0;
                this->fishState = this->fishStateNext;
                this->unk_190 = 1.0f;
                this->unk_194 = 2000.0f;
            } else if (distToTarget < 10.0f) {
                if (Fishing_SplashBySize(this, play, false)) {
                    Fishing_FishLeapSfx(this, false);
                }

                this->fishState = 5;
                this->unk_190 = 1.2f;
                this->unk_194 = 5000.0f;
                this->timerArray[1] = 150;
                this->timerArray[0] = 0;
                this->timerArray[2] = 0;
                this->timerArray[3] = 120;

                sRodCastState = 4;
                sFishingHookedFish = this;
                FishingGameplay_SubmitAction(FISHING_GAMEPLAY_ACTION_HOOK);
                sFishMouthOffset.y = 500.0f - Rand_ZeroFloat(400.0f);

                if (sLureEquipped == FS_LURE_SINKING) {
                    if (this->fishLength > 70.0f) {
                        timer = (int16_t)Rand_ZeroFloat(20.0f) + 10;
                    } else if (this->fishLength > 60.0f) {
                        timer = (int16_t)Rand_ZeroFloat(30.0f) + 20;
                    } else if (this->fishLength > 50.0f) {
                        timer = (int16_t)Rand_ZeroFloat(30.0f) + 30;
                    } else {
                        timer = (int16_t)Rand_ZeroFloat(40.0f) + 40;
                    }
                    sLureBitTimer = timer;
                } else {
                    if (this->fishLength > 70.0f) {
                        timer = (int16_t)Rand_ZeroFloat(5.0f) + 10;
                    } else if (this->fishLength > 60.0f) {
                        timer = (int16_t)Rand_ZeroFloat(5.0f) + 15;
                    } else if (this->fishLength > 50.0f) {
                        timer = (int16_t)Rand_ZeroFloat(5.0f) + 17;
                    } else {
                        timer = (int16_t)Rand_ZeroFloat(5.0f) + 25;
                    }
                    sLureBitTimer = timer;
                }

                sLineHooked = 0;
                sRodPullback = 100;
                sFishFightTime = 0;
            }
            break;

        case -3:
            this->lilyTimer = 50;
            this->fishTargetPos = sLurePos;
            Math_ApproachF(&this->actor.speedXZ, 2.0f, 1.0f, 1.0f);

            if ((sRodCastState != 3) || (this->timerArray[0] == 0) || (sLurePos.y > (WATER_SURFACE_Y(play) + 5.0f)) ||
                (!this->isWild && (sqrtf(SQ(sLurePos.x) + SQ(sLurePos.z)) > 800.0f))) {

                this->timerArray[0] = 0;
                this->unk_190 = 1.0f;
                this->fishState = this->fishStateNext;
                this->unk_194 = 2000.0f;
            } else if (distToTarget < 10.0f) {
                if (sLurePos.y > (WATER_SURFACE_Y(play) - 10.0f)) {
                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_JUMP_OUT_WATER);
                    Sfx_PlaySfxCentered(NA_SE_PL_CATCH_BOOMERANG);
                }

                Fishing_SplashBySize(this, play, false);
                this->fishState = 5;
                this->unk_190 = 1.2f;
                this->unk_194 = 5000.0f;
                this->timerArray[1] = 150;
                this->timerArray[0] = 0;
                this->timerArray[2] = 0;
                this->timerArray[3] = 120;

                sRodCastState = 4;
                sFishingHookedFish = this;
                FishingGameplay_SubmitAction(FISHING_GAMEPLAY_ACTION_HOOK);

                if (sLureEquipped == FS_LURE_SINKING) {
                    sLureBitTimer = 30;
                } else {
                    sLureBitTimer = 30;
                }

                sLineHooked = false;
                sRodPullback = 100;
                sFishFightTime = 0;
            }
            break;

        case 5:
            this->actor.uncullZoneForward = 1200.0f;
            this->actor.uncullZoneScale = 200.0f;

            sFishFightTime++;
            osSyncPrintf("HIT FISH %dcm\n", (uint8_t)this->fishLength);

            Math_ApproachS(&this->fishLimbDRotZDelta, 0x2AF8, 4, 0xBB8);
            sFishingHookedFish = this;
            Math_ApproachS(&player->actor.shape.rot.y, this->actor.yawTowardsPlayer + 0x8000, 5, 0x500);

            if (sLineHooked == 0) {
                if ((sSinkingLureSegmentIndex < 20) && ((sLureTimer & 3) == 0)) {
                    sSinkingLureSegmentIndex++;
                }
            }

            if ((sLureBitTimer != 0) && (sLineHooked == 0)) { // pull the line to hook it
                if (((input->y < -50) && (sStickAdjYPrev > -40)) ||
                    PCInput_IsFishingReelPressed()) {
                    if (input->y < -50) {
                        float temp_f0 = 40.0f - ((this->fishLength - 30.0f) * 1.333333f);
                        if (temp_f0 > 0.0f) {
                            this->unk_152 = temp_f0;
                            this->unk_154 = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
                            this->unk_156 = 1;
                        }
                    }

                    this->fishLimbRotPhaseStep = 1.7f;
                    this->fishLimbRotPhaseMag = 7000.0f;
                    sLineHooked = 1;
                    Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_ENEMY | 0x800);
                    sFishingMusicDelay = 0;

                    sRodHitTimer = 10;
                    Sfx_PlaySfxCentered(NA_SE_IT_FISHING_HIT);
                }
            }

            if (this->actor.world.pos.y < WATER_SURFACE_Y(play)) {
                if (this->timerArray[1] > 30) {
                    phi_v0_2 = 7;
                } else {
                    phi_v0_2 = 0xF;
                }

                if (this->timerArray[1] > 30) {
                    if (this->timerArray[0] == 0) {
                        multiVecSrc.x = 0.0f;
                        multiVecSrc.y = 0.0f;
                        multiVecSrc.z = 200.0f;

                        for (attempts = 0; attempts < 100; attempts++) {
                            Matrix_RotateY(Rand_CenteredFloat(3.0f * M_PI / 4.0f) +
                                               (((this->actor.yawTowardsPlayer + 0x8000) / 32768.0f) * M_PI),
                                           MTXMODE_NEW);
                            Matrix_MultVec3f(&multiVecSrc, &targetPosOffset);

                            this->fishTargetPos.x = this->actor.world.pos.x + targetPosOffset.x;
                            this->fishTargetPos.z = this->actor.world.pos.z + targetPosOffset.z;

                            if ((SQ(this->fishTargetPos.x) + SQ(this->fishTargetPos.z)) < SQ(750.0f)) {
                                break;
                            }
                        }

                        if ((Rand_ZeroOne() < 0.1f) && (this->timerArray[3] == 0)) {
                            this->timerArray[0] = 20;
                            this->timerArray[1] = 100;
                            this->timerArray[2] = 20;
                            this->timerArray[3] = 100;
                            this->fishTargetPos.y = 300.0f;
                            sRodPullback = (int16_t)Rand_ZeroFloat(30.0f) + 20;
                        } else {
                            this->timerArray[0] = (int16_t)Rand_ZeroFloat(10.0f) + 3;
                            this->timerArray[2] = 0;
                            this->fishTargetPos.y = -70.0f - Rand_ZeroFloat(150.0f);
                        }
                    }

                    if (this->timerArray[2] != 0) {
                        sRodReelingSpeed = 0.0f;
                        this->unk_190 = 1.6f;
                        this->unk_194 = 6000.0f;
                        Math_ApproachF(&this->actor.speedXZ, 7.5f, 1.0f, 1.0f);
                        Math_ApproachS(&this->fishLimbDRotZDelta, 0x4E20, 2, 0xFA0);
                    } else {
                        if ((sLineHooked == 0) && (sLureEquipped == FS_LURE_SINKING)) {
                            this->unk_190 = 1.0f;
                            this->unk_194 = 2000.0f;
                            Math_ApproachF(&this->actor.speedXZ, 3.0f, 1.0f, 0.2f);
                        } else {
                            this->unk_190 = 1.4f;
                            this->unk_194 = 5000.0f;
                            Math_ApproachF(&this->actor.speedXZ, 5.0f, 1.0f, 0.5f);
                        }

                        if (this->isLoach == 0) {
                            sRodReelingSpeed = 1.0f - (this->fishLength * 0.00899f);
                        } else {
                            sRodReelingSpeed = 1.0f - (this->fishLength * 0.00899f * 1.4f);
                        }
                    }
                } else {
                    if (((this->timerArray[1] & 0xF) == 0) && Fishing_IsReelHeld() &&
                        (!(this->fishLength >= 60.0f) || (sFishFightTime >= 2000))) {
                        this->unk_152 = (int16_t)Rand_ZeroFloat(30.0f) + 15;
                        this->unk_154 = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
                    }

                    this->unk_190 = 1.0f;
                    this->unk_194 = 4500.0f;

                    if (this->isLoach == 0) {
                        sRodReelingSpeed = 1.3f - (this->fishLength * 0.00899f);
                    } else {
                        sRodReelingSpeed = 1.3f - (this->fishLength * 0.00899f * 1.4f);
                    }

                    Math_ApproachF(&this->actor.speedXZ, 2.0f, 1.0f, 0.5f);

                    if (this->timerArray[1] == 0) {
                        this->unk_152 = 0;

                        if (sFishFightTime < 2000) {
                            this->timerArray[1] = (int16_t)Rand_ZeroFloat(50.0f) + 50;
                        } else if (sFishFightTime < 3000) {
                            this->timerArray[1] = (int16_t)Rand_ZeroFloat(20.0f) + 30;
                        } else {
                            this->timerArray[1] = (int16_t)Rand_ZeroFloat(10.0f) + 25;
                        }
                    }
                }
            }

            if (sReelLock != 0) {
                sRodReelingSpeed = 0.0f;
            }

            if (sLineHooked || (sLureEquipped != FS_LURE_SINKING)) {
                if (this->actor.speedXZ < 3.0f) {
                    if ((sLureTimer & 8) != 0) {
                        targetPosOffset.x = -0.8f;
                    } else {
                        targetPosOffset.x = -0.75f;
                    }
                } else {
                    if ((sLureTimer & 4) != 0) {
                        targetPosOffset.x = -0.9f;
                    } else {
                        targetPosOffset.x = -0.85f;
                    }
                }

                Math_ApproachF(&D_80B7A6C0, 35.0f, 0.1f, 3.5f);
                Math_ApproachF(&D_80B7A6BC, targetPosOffset.x, 0.3f, 0.1f);
            }

            sReelLinePos[LINE_SEG_COUNT - 1] = this->fishMouthPos;
            multiVecSrc.x = sReelLinePos[LINE_SEG_COUNT - 1].x - sReelLinePos[LINE_SEG_COUNT - 2].x;
            multiVecSrc.y = sReelLinePos[LINE_SEG_COUNT - 1].y - sReelLinePos[LINE_SEG_COUNT - 2].y;
            multiVecSrc.z = sReelLinePos[LINE_SEG_COUNT - 1].z - sReelLinePos[LINE_SEG_COUNT - 2].z;

            if ((SQ(multiVecSrc.x) + SQ(multiVecSrc.y) + SQ(multiVecSrc.z)) > SQ(20.0f)) {
                Math_ApproachF(&this->actor.world.pos.x, sReelLinePos[LINE_SEG_COUNT - 2].x, 0.2f,
                               2.0f * (this->actor.speedXZ * 1.5f));
                Math_ApproachF(&this->actor.world.pos.y, sReelLinePos[LINE_SEG_COUNT - 2].y, 0.2f,
                               2.0f * (this->actor.speedXZ * 1.5f) * 5.0f * 0.1f);
                Math_ApproachF(&this->actor.world.pos.z, sReelLinePos[LINE_SEG_COUNT - 2].z, 0.2f,
                               2.0f * (this->actor.speedXZ * 1.5f));
            }

            if (Fishing_IsReelHeld() || (input->y < -30)) {
                if (sRodPullback < 100) {
                    sRodPullback++;
                }
            } else {
                if (sRodPullback != 0) {
                    sRodPullback--;
                }
            }

            if ((sRodCastState < 3) || ((sReelLock != 0) && (sFishFightTime > 50)) || (sFishFightTime >= 6000) ||
                ((sLureBitTimer == 0) && (sLineHooked == 0)) || (sRodPullback == 0) ||
                (((sLureTimer & 0x7F) == 0) && (Rand_ZeroOne() < 0.05f) && (sLureEquipped != FS_LURE_SINKING) &&
                 (KREG(69) == 0))) {
                // Losing a hooked fish should immediately return control; the
                // pond-owner failure textbox is unnecessary in portable play.
                sFishingCaughtTextDelay = 0;
                FishingGameplay_SubmitAction(FISHING_GAMEPLAY_ACTION_RELEASE);

                if (!((sLureBitTimer == 0) && (sLineHooked == 0))) {
                    Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x0A00FF);
                }

                this->fishState = this->fishStateNext = 0;
                this->unk_1A4 = 10000;
                this->unk_1A2 = 500;
                this->timerArray[1] = 50;
                this->timerArray[0] = 0;
                this->unk_190 = 1.0f;
                this->unk_194 = 3000.0f;

                if (sRodCastState == 4) {
                    sRodCastState = 3;
                }

                sFishingMusicDelay = 50;
                sRodReelingSpeed = 0.5f;
                this->unk_152 = 0;
            } else if (this->actor.xzDistToPlayer < (KREG(59) + 50.0f)) {
                this->fishState = 6;
                this->timerArray[0] = 100;
                player->unk_860 = 3;
                sFishesCaught++;
                sFishingPlayerCinematicState = 100;
                sCatchCamX = 45.0f;
                sRodCastState = 5;
                this->unk_190 = 1.0f;
                this->unk_194 = 500.0f;
                this->fishLimbRotPhaseMag = 5000.0f;

                if (this->actor.world.pos.y <= WATER_SURFACE_Y(play)) {
                    Fishing_FishLeapSfx(this, true);
                    Fishing_SplashBySize(this, play, true);
                }
                goto hoistCatch;
            }
            break;

        hoistCatch:
        case 6:
            Math_ApproachS(&this->fishLimbDRotZDelta, 0x2AF8, 2, 0xFA0);
            Math_ApproachF(&sCatchCamX, 15.0f, 0.05f, 0.75f);

            multiVecSrc.x = sCatchCamX;
            if (sLinkAge != LINK_AGE_CHILD) {
                multiVecSrc.y = 30.0f;
                multiVecSrc.z = 55.0f;
            } else {
                multiVecSrc.y = 10.0f;
                multiVecSrc.z = 50.0f;
            }
            Matrix_RotateY((player->actor.shape.rot.y / 32768.0f) * M_PI, MTXMODE_NEW);
            Matrix_MultVec3f(&multiVecSrc, &sCameraEye);

            sCameraEye.x += player->actor.world.pos.x;
            sCameraEye.y += player->actor.world.pos.y;
            sCameraEye.z += player->actor.world.pos.z;

            sCameraAt = player->actor.world.pos;
            if (sLinkAge != LINK_AGE_CHILD) {
                sCameraAt.y += 40.0f;
            } else {
                sCameraAt.y += 25.0f;
            }

            if (this->timerArray[0] == 90) {
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_HEART_GET | 0x900);
                sFishingCaughtTextDelay = 40;

                if (this->isLoach == 0) {
                    sFishLengthToWeigh = this->fishLength;

                    if (sFishLengthToWeigh >= 75) {
                        sFishingCaughtTextId = 0x409F;
                    } else if (sFishLengthToWeigh >= 50) {
                        sFishingCaughtTextId = 0x4091;
                    } else {
                        sFishingCaughtTextId = 0x4083;
                    }
                } else {
                    sFishLengthToWeigh = 2.0f * this->fishLength;
                    sFishingCaughtTextId = 0x4099;
                }

                // The catch text reads this score as its displayed weight.
                // The pond owner also refreshes it while drawing, but portable
                // fishing has no owner actor to perform that calculation.
                gSaveContext.minigameScore = (SQ((float)sFishLengthToWeigh) * 0.0036f) + 0.5f;
            }

            this->unk_160 = -0x4000;
            this->actor.shape.rot.y = player->actor.shape.rot.y + 0x5000;
            this->actor.shape.rot.x = this->actor.shape.rot.z = this->unk_162 = this->unk_164 = this->unk_16E = 0;

            multiVecSrc.x = 4.0f;
            multiVecSrc.y = -10.0f;
            multiVecSrc.z = 5.0f;
            Matrix_MultVec3f(&multiVecSrc, &targetPosOffset);
            Math_ApproachF(&this->actor.world.pos.x, player->bodyPartsPos[15].x + targetPosOffset.x, 1.0f, 6.0f);
            Math_ApproachF(&this->actor.world.pos.y, player->bodyPartsPos[15].y + targetPosOffset.y, 1.0f, 6.0f);
            Math_ApproachF(&this->actor.world.pos.z, player->bodyPartsPos[15].z + targetPosOffset.z, 1.0f, 6.0f);
            sRodLineSpooled = 188.0f;

            if (this->timerArray[0] <= 50) {
                // Dismissing the weight notification always releases the fish.
                if (((Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) ||
                     (Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) ||
                     (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) &&
                    PCInput_PrimaryPressed()) {
                    Message_CloseTextbox(play);
                    PCInput_DiscardActionIntents();
                    sRodCastState = 0;
                    sRodCastTimer = 0;
                    sLineHooked = false;
                    FishingGameplay_SubmitAction(FISHING_GAMEPLAY_ACTION_RELEASE);
                    sFishingHookedFish = NULL;
                    sLureBitTimer = 0;
                    sFishFightTime = 0;
                    sRodPullback = 0;
                    sReelLock = 0;
                    sReelLinePosStep = 0.0f;
                    sRodReelingSpeed = 0.5f;

                    Fishing_StopCinematic(play);
                    Player_FinishFishingCatch(play);
                    D_80B7E0B0 = 5;
                }
            }

            if (sRodCastState == 0) {
                if (this->actor.update != NULL) {
                    this->fishState = this->fishStateNext = 0;
                    this->unk_1A4 = 10000;
                    this->unk_1A2 = 500;
                    this->timerArray[1] = 50;
                    this->timerArray[0] = 0;
                    this->unk_190 = 1.0f;
                    this->unk_194 = 2000.0f;
                    SkelAnime_Free(&this->skelAnime, play);

                    if (this->isLoach == 0) {
                        SkelAnime_InitFlex(play, &this->skelAnime, &gFishingFishSkel, &gFishingFishAnim, 0, 0, 0);
                        Animation_MorphToLoop(&this->skelAnime, &gFishingFishAnim, 0.0f);
                    } else {
                        SkelAnime_InitFlex(play, &this->skelAnime, &gFishingLoachSkel, &gFishingLoachAnim, 0, 0, 0);
                        Animation_MorphToLoop(&this->skelAnime, &gFishingLoachAnim, 0.0f);
                    }
                }

                D_80B7E148 = 520.0f;
                sRodLineSpooled = 195.0f;
                Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0xA00FF);
                sFishingMusicDelay = 20;
                sFishingPlayerCinematicState = 3;
            }
            break;

        case 7:
            if (this->isWild) {
                this->fishState = 10;
                this->fishStateNext = 10;
                break;
            }

            this->lilyTimer = 50;
            rotXYScale = 5;
            this->rotationStep = 12288.0f;

            if (this->actor.params < (EN_FISH_PARAM + 4)) {
                this->fishTargetPos = sGroupFishes[this->actor.params - EN_FISH_PARAM].pos;
                sFishGroupVar = 1;
            } else if (this->actor.params < (EN_FISH_PARAM + 8)) {
                this->fishTargetPos = sGroupFishes[this->actor.params - EN_FISH_PARAM + 16].pos;
                sFishGroupVar = 2;
            } else {
                this->fishTargetPos = sGroupFishes[this->actor.params - EN_FISH_PARAM + 32].pos;
                sFishGroupVar = 3;
            }

            Math_ApproachF(&this->actor.speedXZ, 5.0f, 1.0f, 1.0f);

            if (distToTarget < 20.0f) {
                Math_ApproachS(&this->fishLimbDRotZDelta, 0x4E20, 2, 0xFA0);

                if ((this->timerArray[2] == 0) && Fishing_SplashBySize(this, play, false)) {
                    Fishing_FishLeapSfx(this, Rand_ZeroFloat(1.99f));
                    this->timerArray[2] = (int16_t)Rand_ZeroFloat(20.0f) + 20;
                }
            }

            if (this->timerArray[3] == 0) {
                this->fishState = 10;
                this->fishStateNext = 10;
            } else {
                func_80B70ED4(this, input);
                if (this->actor.xzDistToPlayer < (100.0f * playerSpeedMod)) {
                    this->fishStateNext = this->fishState = 0;
                    this->unk_1A4 = 500;
                    this->unk_1A2 = 200;
                    this->timerArray[1] = 50;
                }
            }
            break;
    }

    Math_ApproachS(&this->fishLimbEFRotYDelta, (Math_SinS(this->stateAndTimer * 0x1000) * 5000.0f) + 5000.0f, 2, 0x7D0);

    if (this->fishState != 6) {
        if (this->actor.world.pos.y > WATER_SURFACE_Y(play)) {
            this->unk_190 = 1.5f;
            this->unk_194 = 5000.0f;

            Math_ApproachS(&this->unk_16E, 0, 5, 0x7D0);

            rotXScale = rotYScale = rotZScale = 3;
            rotXStep = rotYStep = 0x2000;

            this->timerArray[2] = 0;
            this->unk_184 -= 1.0f;
        } else {
            Math_ApproachZeroF(&this->unk_184, 1.0f, 2.0f);
            if ((this->fishState != -1) && (this->fishState != -2) && (this->fishState != -25)) {
                this->rotationTarget.x = 0;
            }

            this->rotationTarget.y = this->rotationTarget.z = 0;
            rotXScale = rotYScale = rotZScale = 4;
            rotXStep = rotYStep = 0x2000;

            int16_t spF6 = Fishing_SmoothStepToS(&this->actor.world.rot.y, rotYTarget, rotXYScale, this->rotationStep) * 3.0f;
            Math_ApproachS(&this->actor.world.rot.x, rotXTarget, rotXYScale, this->rotationStep * 0.5f);

            if (spF6 > 0x1F40) {
                spF6 = 0x1F40;
            } else if (spF6 < -0x1F40) {
                spF6 = -0x1F40;
            }

            if (this->actor.speedXZ >= 3.2f) {
                Math_ApproachS(&this->unk_16E, spF6, 2, 0x4E20);
            } else {
                Math_ApproachS(&this->unk_16E, spF6, 3, 0xBB8);
            }

            Actor_UpdateVelocityXYZ(&this->actor);
        }

        Actor_UpdatePos(&this->actor);

        this->actor.world.pos.y += (this->unk_184 * 1.5f);

        if (this->unk_152 != 0) {
            this->rotationTarget.y = this->unk_154;
            this->unk_152--;
            if (this->unk_156 != 0) {
                rotYScale = 5;
                rotYStep = 0x4000;
            } else {
                rotYScale = 10;
                rotYStep = 0x800;
            }
            this->rotationTarget.x = -0x500 - this->actor.shape.rot.x;
            rotXScale = 5;
            rotXStep = 0x4000;
        } else {
            this->unk_156 = 0;
        }

        Math_ApproachS(&this->unk_160, this->rotationTarget.x, rotXScale, rotXStep);
        Math_ApproachS(&this->unk_162, this->rotationTarget.y, rotYScale, rotYStep);
        Math_ApproachS(&this->unk_164, this->rotationTarget.z, rotZScale, 0x2000);

        if (this->actor.speedXZ <= 0.5f) {
            Math_ApproachS(&this->actor.shape.rot.x, 0, 10, this->unk_178);
            Math_ApproachS(&this->unk_178, 0x500, 1, 0x20);
        } else {
            Math_ApproachS(&this->actor.shape.rot.x, -this->actor.world.rot.x, 10, 0x1000);
            this->unk_178 = 0;
        }

        this->actor.shape.rot.y = this->actor.world.rot.y;

        if ((this->fishState != -1) && (this->fishState != -2) && (this->fishState != -25)) {
            if ((this->actor.world.pos.y > WATER_SURFACE_Y(play)) && (this->actor.prevPos.y <= WATER_SURFACE_Y(play))) {
                Fishing_SplashBySize(this, play, true);
                Fishing_FishLeapSfx(this, true);
                this->unk_184 = this->actor.velocity.y;
                this->actor.velocity.y = 0.0f;
                this->rotationTarget.z = Rand_CenteredFloat(32768.0f);
            } else if ((this->actor.world.pos.y < WATER_SURFACE_Y(play)) &&
                       (this->actor.prevPos.y >= WATER_SURFACE_Y(play))) {
                if (this->unk_184 < -5.0f) {
                    this->unk_184 = -5.0f;
                }
                this->actor.world.rot.x = -0xFA0;
                Fishing_SplashBySize(this, play, true);
                this->bubbleTime = 20;
                Fishing_FishLeapSfx(this, 0);
            }
        }

        if ((this->actor.world.pos.y < WATER_SURFACE_Y(play)) &&
            (this->actor.world.pos.y > (WATER_SURFACE_Y(play) - 10.0f)) && ((this->stateAndTimer & 1) == 0) &&
            (this->actor.speedXZ > 0.0f)) {
            Vec3f pos = this->actor.world.pos;
            pos.y = WATER_SURFACE_Y(play);
            Fishing_SpawnRipple(&this->actor.projectedPos, play->specialEffects, &pos, 80.0f, 500.0f, 150, 90);
        }

        if ((this->actor.speedXZ > 0.0f) || (this->fishState == 5)) {
            float velocityY = this->actor.velocity.y;

            float spD8 = this->fishLength * 0.1f;

            this->actor.world.pos.y -= spD8;
            this->actor.prevPos.y -= spD8;
            this->actor.velocity.y = -1.0f;
            Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 30.0f, 100.0f, 0x45);
            this->actor.world.pos.y += spD8;
            this->actor.prevPos.y += spD8;

            this->actor.velocity.y = velocityY;

            if (this->actor.bgCheckFlags & 8) {
                this->bumpTimer = 20;
            }

            if (this->actor.bgCheckFlags & 1) {
                if (this->actor.world.pos.y > WATER_SURFACE_Y(play)) {
                    this->unk_184 = Rand_ZeroFloat(3.0f) + 3.0f;
                    this->actor.velocity.x =
                        (this->actor.world.pos.x - (this->isWild ? this->actor.home.pos.x : 0.0f)) * -0.003f;
                    this->actor.velocity.z =
                        (this->actor.world.pos.z - (this->isWild ? this->actor.home.pos.z : 0.0f)) * -0.003f;

                    Audio_PlayActorSound2(&this->actor, NA_SE_EV_FISH_LEAP);
                    Fishing_SplashBySize2(this, play);

                    if (Rand_ZeroOne() < 0.5f) {
                        this->rotationTarget.z = 0x4000;
                    } else {
                        this->rotationTarget.z = -0x4000;
                    }

                    if (Rand_ZeroOne() < 0.5f) {
                        this->rotationTarget.x = 0;
                    } else {
                        this->rotationTarget.x = (int16_t)Rand_CenteredFloat(32.0f) + 0x8000;
                    }

                    this->rotationTarget.y = (int16_t)Rand_CenteredFloat(16384.0f);
                    this->unk_190 = 1.0f;
                    this->unk_194 = 5000.0f;
                    this->fishLimbRotPhaseMag = 5000.0f;
                } else {
                    this->unk_184 = 0.0f;

                    if ((this->fishState == 5) && ((this->stateAndTimer & 1) == 0)) {
                        Vec3f pos;

                        pos.x = Rand_CenteredFloat(10.0f) + this->actor.world.pos.x;
                        pos.z = Rand_CenteredFloat(10.0f) + this->actor.world.pos.z;
                        pos.y = this->actor.floorHeight + 5.0f;
                        Fishing_SpawnWaterDust(&this->actor.projectedPos, play->specialEffects, &pos,
                                               (this->fishLength * 0.005f) + 0.15f);
                    }
                }
            }
        }
    }

    // A released wild fish gets the same short flop/drop period as a pond
    // fish before its water-box bounds pull it safely back into the water.
    if (this->isWild && (this->timerArray[1] == 0) && ((this->fishState < 2) || (this->fishState > 6))) {
        if ((this->actor.world.pos.x < this->wildMinX) || (this->actor.world.pos.x > this->wildMaxX) ||
            (this->actor.world.pos.z < this->wildMinZ) || (this->actor.world.pos.z > this->wildMaxZ)) {
            this->actor.world.pos.x = CLAMP(this->actor.world.pos.x, this->wildMinX, this->wildMaxX);
            this->actor.world.pos.z = CLAMP(this->actor.world.pos.z, this->wildMinZ, this->wildMaxZ);
            this->fishTargetPos = this->actor.home.pos;
            this->fishState = 10;
            this->fishStateNext = 10;
        }
    }

    if (this->bubbleTime != 0) {
        int16_t i;
        Vec3f pos;
        float range = (this->fishLength * 0.075f) + 10.0f;

        this->bubbleTime--;

        for (i = 0; i < 2; i++) {
            pos.x = Rand_CenteredFloat(range) + this->actor.world.pos.x;
            pos.y = Rand_CenteredFloat(range) + this->actor.world.pos.y;
            pos.z = Rand_CenteredFloat(range) + this->actor.world.pos.z;
            Fishing_SpawnBubble(&this->actor.projectedPos, play->specialEffects, &pos, Rand_ZeroFloat(0.035f) + 0.04f,
                                0);
        }
    }

    if (this->isWild) {
        sPortableWaterSurfaceY = savedPortableWaterSurfaceY;
        sPortableWaterFound = savedPortableWaterFound;
    }
}

int32_t Fishing_FishOverrideLimbDraw(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    Fishing* this = (Fishing*)thisx;

    if (limbIndex == 0xD) {
        rot->z -= this->fishLimbDRotZDelta - 11000;
    } else if ((limbIndex == 2) || (limbIndex == 3)) {
        rot->y += this->fishLimb23RotYDelta;
    } else if (limbIndex == 4) {
        rot->y += this->fishLimb4RotYDelta;
    } else if (limbIndex == 0xE) {
        rot->y -= this->fishLimbEFRotYDelta;
    } else if (limbIndex == 0xF) {
        rot->y += this->fishLimbEFRotYDelta;
    } else if (limbIndex == 8) {
        rot->y += this->fishLimb89RotYDelta;
    } else if (limbIndex == 9) {
        rot->y -= this->fishLimb89RotYDelta;
    }

    return 0;
}

void Fishing_FishPostLimbDraw(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    Fishing* this = (Fishing*)thisx;

    if (limbIndex == 0xD) {
        Matrix_MultVec3f(&sFishMouthOffset, &this->fishMouthPos);
    }
}

int32_t Fishing_LoachOverrideLimbDraw(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    Fishing* this = (Fishing*)thisx;

    if (limbIndex == 3) {
        rot->y += this->loachRotYDelta[0];
    } else if (limbIndex == 4) {
        rot->y += this->loachRotYDelta[1];
    } else if (limbIndex == 5) {
        rot->y += this->loachRotYDelta[2];
    }

    return 0;
}

void Fishing_LoachPostLimbDraw(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sLoachMouthOffset = { 500.0f, 500.0f, 0.0f };
    Fishing* this = (Fishing*)thisx;

    if (limbIndex == 0xB) {
        Matrix_MultVec3f(&sLoachMouthOffset, &this->fishMouthPos);
    }
}

void Fishing_DrawFish(Actor* thisx, PlayState* play) {
    Fishing* this = (Fishing*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(((this->unk_162 + this->actor.shape.rot.y) / 32768.0f) * M_PI, MTXMODE_APPLY);
    Matrix_RotateX(((this->unk_160 + this->actor.shape.rot.x) / 32768.0f) * M_PI, MTXMODE_APPLY);
    Matrix_RotateZ(((this->unk_164 + this->actor.shape.rot.z) / 32768.0f) * M_PI, MTXMODE_APPLY);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);

    if (this->isLoach == 0) {
        Matrix_RotateY((this->fishLimb23RotYDelta * (M_PI / 32768)) - (M_PI / 2), MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, this->fishLimb23RotYDelta * 10.0f * 0.01f, MTXMODE_APPLY);

        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, Fishing_FishOverrideLimbDraw, Fishing_FishPostLimbDraw, this);
    } else {
        Matrix_Translate(0.0f, 0.0f, 3000.0f, MTXMODE_APPLY);
        Matrix_RotateY(this->fishLimb23RotYDelta * (M_PI / 32768), MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, -3000.0f, MTXMODE_APPLY);
        Matrix_RotateY(-(M_PI / 2), MTXMODE_APPLY);

        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, Fishing_LoachOverrideLimbDraw, Fishing_LoachPostLimbDraw,
                                  this);
    }
}

void Fishing_HandleReedContact(FishingProp* prop, Vec3f* entityPos) {
    float dx = prop->pos.x - entityPos->x;
    float dz = prop->pos.z - entityPos->z;
    float distXZ = sqrtf(SQ(dx) + SQ(dz));

    if (distXZ <= 20.0f) {
        prop->rotY = atan2f(dx, dz);

        Math_ApproachF(&prop->rotX, (20.0f - distXZ) * 0.03f, 0.2f, 0.2f);
    }
}

void Fishing_HandleLilyPadContact(FishingProp* prop, Vec3f* entityPos, uint8_t fishTimer) {
    float dx = prop->pos.x - entityPos->x;
    float dz = prop->pos.z - entityPos->z;
    float distXZ = sqrtf(SQ(dx) + SQ(dz));

    if (distXZ <= 40.0f) {
        Math_ApproachS(&prop->lilyPadAngle, RADF_TO_BINANG(atan2f(dx, dz)), 10, 0x300);
    }

    if (fishTimer && (distXZ <= 60.0f)) {
        float heightTarget = 1.0f;

        if (fishTimer >= 21) {
            heightTarget = 1.5f;
        }

        Math_ApproachF(&prop->lilyPadOffset, heightTarget, 0.1f, 0.2f);
    }
}

void Fishing_UpdatePondProps(PlayState* play) {
    FishingProp* prop = &sPondProps[0];
    Player* player = GET_PLAYER(play);
    Actor* actor;
    int16_t i;

    for (i = 0; i < POND_PROP_COUNT; i++) {
        if (prop->type != FS_PROP_NONE) {
            prop->shouldDraw = false;
            prop->timer++;

            SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &prop->pos, &prop->projectedPos, &sProjectedW);

            if ((prop->projectedPos.z < prop->drawDistance) &&
                (fabsf(prop->projectedPos.x) < (100.0f + prop->projectedPos.z))) {
                prop->shouldDraw = true;
            }

            if ((prop->projectedPos.z < 500.0f) && (fabsf(prop->projectedPos.x) < (100.0f + prop->projectedPos.z))) {
                if (prop->type == FS_PROP_REED) {
                    Fishing_HandleReedContact(prop, &player->actor.world.pos);

                    actor = play->actorCtx.actorLists[ACTORCAT_NPC].head;
                    while (actor != NULL) {
                        if (!((actor->id == ACTOR_FISHING) && (actor->params >= EN_FISH_PARAM))) {
                            actor = actor->next;
                        } else {
                            Fishing_HandleReedContact(prop, &actor->world.pos);
                            actor = actor->next;
                        }
                    }

                    Math_ApproachZeroF(&prop->rotX, 0.05f, 0.05f);
                } else if (prop->type == FS_PROP_LILY_PAD) {
                    Fishing_HandleLilyPadContact(prop, &player->actor.world.pos, 0);

                    actor = play->actorCtx.actorLists[ACTORCAT_NPC].head;
                    while (actor != NULL) {
                        if (!((actor->id == ACTOR_FISHING) && (actor->params >= EN_FISH_PARAM))) {
                            actor = actor->next;
                        } else {
                            Fishing_HandleLilyPadContact(prop, &actor->world.pos, ((Fishing*)actor)->lilyTimer);
                            actor = actor->next;
                        }
                    }

                    Math_ApproachS(&prop->lilyPadAngle, 0, 20, 80);
                    prop->pos.y =
                        (Math_SinS(prop->timer * 0x1000) * prop->lilyPadOffset) + (WATER_SURFACE_Y(play) + 2.0f);
                    Math_ApproachZeroF(&prop->lilyPadOffset, 0.1f, 0.02f);
                }
            }
        }

        prop++;
    }

    if (sSubCamId == 0) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &sFishingMain->collider.base);
    }
}

void Fishing_DrawPondProps(PlayState* play) {
    uint8_t materialFlag = 0;
    FishingProp* prop = &sPondProps[0];
    int16_t i;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();

    for (i = 0; i < POND_PROP_COUNT; i++) {
        if (prop->type == FS_PROP_REED) {
            if (materialFlag == 0) {
                gSPDisplayList(POLY_XLU_DISP++, gFishingReedMaterialDL);
                materialFlag++;
            }

            if (prop->shouldDraw) {
                FrameInterpolation_RecordOpenChild(prop, prop->epoch);
                Matrix_Translate(prop->pos.x, prop->pos.y, prop->pos.z, MTXMODE_NEW);
                Matrix_Scale(prop->scale, prop->scale, prop->scale, MTXMODE_APPLY);
                Matrix_RotateY(prop->rotY, MTXMODE_APPLY);
                Matrix_RotateX(prop->rotX, MTXMODE_APPLY);
                Matrix_RotateY(prop->reedAngle, MTXMODE_APPLY);

                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, gFishingReedModelDL);
                FrameInterpolation_RecordCloseChild();
            }
        }

        prop++;
    }

    prop = &sPondProps[0];
    materialFlag = 0;
    for (i = 0; i < POND_PROP_COUNT; i++) {
        if (prop->type == FS_PROP_WOOD_POST) {
            if (materialFlag == 0) {
                gSPDisplayList(POLY_OPA_DISP++, gFishingWoodPostMaterialDL);
                materialFlag++;
            }

            if (prop->shouldDraw) {
                FrameInterpolation_RecordOpenChild(prop, prop->epoch);
                Matrix_Translate(prop->pos.x, prop->pos.y, prop->pos.z, MTXMODE_NEW);
                Matrix_Scale(prop->scale, prop->scale, prop->scale, MTXMODE_APPLY);

                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, gFishingWoodPostModelDL);
                FrameInterpolation_RecordCloseChild();
            }
        }

        prop++;
    }

    prop = &sPondProps[0];
    materialFlag = 0;
    for (i = 0; i < POND_PROP_COUNT; i++) {
        if (prop->type == FS_PROP_LILY_PAD) {
            if (materialFlag == 0) {
                gSPDisplayList(POLY_XLU_DISP++, gFishingLilyPadMaterialDL);
                materialFlag++;
            }

            if (prop->shouldDraw) {
                FrameInterpolation_RecordOpenChild(prop, prop->epoch);
                Matrix_Translate(prop->pos.x, prop->pos.y, prop->pos.z, MTXMODE_NEW);
                Matrix_Scale(prop->scale, 1.0f, prop->scale, MTXMODE_APPLY);
                Matrix_RotateY(prop->lilyPadAngle * (M_PI / 32768), MTXMODE_APPLY);
                Matrix_Translate(0.0f, 0.0f, 20.0f, MTXMODE_APPLY);
                Matrix_RotateY(prop->rotY, MTXMODE_APPLY);

                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_XLU_DISP++, gFishingLilyPadModelDL);
                FrameInterpolation_RecordCloseChild();
            }
        }

        prop++;
    }

    prop = &sPondProps[0];
    materialFlag = 0;
    for (i = 0; i < POND_PROP_COUNT; i++) {
        if (prop->type == FS_PROP_ROCK) {
            if (materialFlag == 0) {
                gSPDisplayList(POLY_OPA_DISP++, gFishingRockMaterialDL);
                materialFlag++;
            }

            if (prop->shouldDraw) {
                FrameInterpolation_RecordOpenChild(prop, prop->epoch);
                Matrix_Translate(prop->pos.x, prop->pos.y, prop->pos.z, MTXMODE_NEW);
                Matrix_Scale(prop->scale, prop->scale, prop->scale, MTXMODE_APPLY);
                Matrix_RotateY(prop->rotY, MTXMODE_APPLY);

                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, gFishingRockModelDL);
                FrameInterpolation_RecordCloseChild();
            }
        }

        prop++;
    }

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

void Fishing_UpdateGroupFishes(PlayState* play) {
    int16_t groupContactFlags = 0;
    Player* player = GET_PLAYER(play);
    FishingGroupFish* fish = &sGroupFishes[0];
    int16_t groupIndex = { 0 };
    int16_t groupFlag = { 0 };
    float spD8 = { 0 };
    int16_t i;
    Vec3f basePos[3];
    Vec3f* refPos = { 0 };
    float temp1 = { 0 };
    float temp2 = { 0 };

    if ((D_80B7E114 != 0) || (sRodCastState == 4)) {
        refPos = &sLurePos;
    } else {
        refPos = &player->actor.world.pos;
    }

    basePos[0].x = sinf(sFishGroupAngle1) * 720.0f;
    basePos[0].y = -35.0f;
    basePos[0].z = cosf(sFishGroupAngle1) * 720.0f;

    temp1 = refPos->x - basePos[0].x;
    temp2 = refPos->z - basePos[0].z;

    if ((SQ(temp1) + SQ(temp2)) < SQ(50.0f)) {
        sFishGroupAngle1 += 0.3f;
        groupContactFlags |= 1;
    } else if (sFishGroupVar != 0.0f) {
        sFishGroupAngle1 += 0.05f;
        basePos[0].y = WATER_SURFACE_Y(play) - 5.0f;
    } else {
        Math_ApproachF(&sFishGroupAngle1, 0.7f, 1.0f, 0.001f);
    }

    basePos[1].x = sinf(sFishGroupAngle2) * 720.0f;
    basePos[1].y = -35.0f;
    basePos[1].z = cosf(sFishGroupAngle2) * 720.0f;

    temp1 = refPos->x - basePos[1].x;
    temp2 = refPos->z - basePos[1].z;

    if ((SQ(temp1) + SQ(temp2)) < SQ(50.0f)) {
        sFishGroupAngle2 -= 0.3f;
        groupContactFlags |= 2;
    } else if (sFishGroupVar != 0.0f) {
        sFishGroupAngle2 -= 0.05f;
        basePos[1].y = WATER_SURFACE_Y(play) - 5.0f;
    } else {
        Math_ApproachF(&sFishGroupAngle2, 2.3f, 1.0f, 0.001f);
    }

    basePos[2].x = sinf(sFishGroupAngle3) * 720.0f;
    basePos[2].y = -35.0f;
    basePos[2].z = cosf(sFishGroupAngle3) * 720.0f;

    temp1 = refPos->x - basePos[2].x;
    temp2 = refPos->z - basePos[2].z;

    if ((SQ(temp1) + SQ(temp2)) < SQ(50.0f)) {
        sFishGroupAngle3 -= 0.3f;
        groupContactFlags |= 4;
    } else if (sFishGroupVar != 0.0f) {
        sFishGroupAngle3 -= 0.05f;
        basePos[2].y = WATER_SURFACE_Y(play) - 5.0f;
    } else {
        Math_ApproachF(&sFishGroupAngle3, 4.6f, 1.0f, 0.001f);
    }

    if (sLinkAge == LINK_AGE_CHILD) {
        spD8 = 0.8f;
    } else {
        spD8 = 1.0f;
    }

    for (i = 0; i < GROUP_FISH_COUNT; i++) {
        if (fish->type != FS_GROUP_FISH_NONE) {
            fish->timer++;

            SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &fish->pos, &fish->projectedPos, &sProjectedW);

            if ((fish->projectedPos.z < 400.0f) && (fabsf(fish->projectedPos.x) < (100.0f + fish->projectedPos.z))) {
                fish->shouldDraw = true;
            } else {
                fish->shouldDraw = false;
            }

            if (i <= 20) {
                groupIndex = 0;
                groupFlag = 1;
            } else if (i <= 40) {
                groupIndex = 1;
                groupFlag = 2;
            } else {
                groupIndex = 2;
                groupFlag = 4;
            }

            float dx = fish->homePos.x - fish->pos.x;
            float dy = fish->homePos.y - fish->pos.y;
            float dz = fish->homePos.z - fish->pos.z;
            int16_t spD4 = RADF_TO_BINANG(atan2f(dx, dz));
            float dist = sqrtf(SQ(dx) + SQ(dz));
            int16_t spD6 = RADF_TO_BINANG(atan2f(dy, dist));

            if ((dist < 10.0f) || (((fish->timer % 32) == 0) && (Rand_ZeroOne() > 0.5f))) {
                fish->homePos.y = basePos[groupIndex].y + Rand_CenteredFloat(10.0f);

                if (sFishGroupVar != 0.0f) {
                    fish->homePos.x = basePos[groupIndex].x + Rand_CenteredFloat(200.0f);
                    fish->homePos.z = basePos[groupIndex].z + Rand_CenteredFloat(200.0f);
                } else {
                    fish->homePos.x = basePos[groupIndex].x + Rand_CenteredFloat(100.0f);
                    fish->homePos.z = basePos[groupIndex].z + Rand_CenteredFloat(100.0f);
                }

                Vec3f ripplePos = fish->pos;
                ripplePos.y = WATER_SURFACE_Y(play);
                Fishing_SpawnRipple(&fish->projectedPos, play->specialEffects, &ripplePos, 20.0f,
                                    Rand_ZeroFloat(50.0f) + 100.0f, 150, 90);

                if (fish->velY < 1.5f) {
                    fish->velY = 1.5f;
                }

                fish->unk_34 = 1.5f;
                fish->unk_38 = 1.0f;
            }

            int16_t target = Fishing_SmoothStepToS(&fish->unk_3E, spD4, 5, 0x4000) * 3.0f;
            if (target > 0x1F40) {
                target = 0x1F40;
            } else if (target < -0x1F40) {
                target = -0x1F40;
            }

            Math_ApproachS(&fish->unk_42, target, 3, 0x1388);

            float offset = fish->unk_42 * -0.0001f;
            Math_ApproachS(&fish->unk_3C, spD6, 5, 0x4000);

            if (groupContactFlags & groupFlag) {
                fish->unk_38 = 1.0f;
                fish->velY = 6.0f;
                fish->unk_34 = 2.0f;
            }

            if (sFishGroupVar != 0.0f) {
                fish->unk_38 = 1.0f;
                fish->velY = 4.0f;
                fish->unk_34 = 2.0f;
            }

            Math_ApproachF(&fish->velY, 0.75f, 1.0f, 0.05f);

            temp1 = fish->velY * spD8;
            temp2 = Math_CosS(fish->unk_3C) * temp1;

            fish->pos.x += temp2 * Math_SinS(fish->unk_3E);
            fish->pos.y += temp1 * Math_SinS(fish->unk_3C);
            fish->pos.z += temp2 * Math_CosS(fish->unk_3E);

            if (fish->shouldDraw) {
                Math_ApproachF(&fish->unk_34, 1.0f, 1.0f, 0.1f);
                Math_ApproachF(&fish->unk_38, 0.4f, 1.0f, 0.04f);
                fish->unk_30 += fish->unk_34;
                fish->scaleX = (cosf(fish->unk_30) * fish->unk_38) + offset;
            }
        }

        fish++;
    }

    sFishGroupVar = 0.0f;
}

void Fishing_DrawGroupFishes(PlayState* play) {
    uint8_t materialFlag = 0;
    FishingGroupFish* fish = &sGroupFishes[0];
    float scale = { 0 };
    int16_t i;

    if (sLinkAge == LINK_AGE_CHILD) {
        scale = 0.003325f;
    } else {
        scale = 0.00475f;
    }

    OPEN_DISPS(play->state.gfxCtx);

    for (i = 0; i < GROUP_FISH_COUNT; i++) {
        if (fish->type != FS_GROUP_FISH_NONE) {
            if (!materialFlag) {
                gSPDisplayList(POLY_OPA_DISP++, gFishingGroupFishMaterialDL);
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 155, 155, 155, 255);
                materialFlag++;
            }

            if (fish->shouldDraw) {
                FrameInterpolation_RecordOpenChild(fish, fish->epoch);
                Matrix_Translate(fish->pos.x, fish->pos.y, fish->pos.z, MTXMODE_NEW);
                Matrix_RotateY(((float)fish->unk_3E * M_PI) / 32768.0f, MTXMODE_APPLY);
                Matrix_RotateX((-(float)fish->unk_3C * M_PI) / 32768.0f, MTXMODE_APPLY);
                Matrix_Scale(fish->scaleX * scale, scale, scale, MTXMODE_APPLY);

                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, gFishingGroupFishModelDL);
                FrameInterpolation_RecordCloseChild();
            }
        }
        fish++;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static uint16_t sPondOwnerTextIds[] = { 0x4096, 0x408D, 0x408E, 0x408F, 0x4094, 0x4095 };

static void Fishing_StartPlaying(PlayState* play) {
    // The pole is a normal C-Up item now; do not replace or disable the
    // player's normal B/C loadout while fishing.
    play->interfaceCtx.unk_260 = 0;
    sFishingPlayingState = 1;
    sOwnerTheftTimer = 20;
}

static void Fishing_StopCinematic(PlayState* play) {
    if (sSubCamId != 0) {
        Camera* mainCam = Play_GetCamera(play, MAIN_CAM);

        mainCam->eye = sCameraEye;
        mainCam->eyeNext = sCameraEye;
        mainCam->at = sCameraAt;
        func_800C08AC(play, sSubCamId);
        sSubCamId = 0;
    }

    sFishingPlayerCinematicState = 0;
    Environment_EnableUnderwaterLights(play, 0);
    play->envCtx.adjFogNear = 0;
}

void Fishing_HandleOwnerDialog(Fishing* this, PlayState* play) {
    switch (this->stateAndTimer) {
        case 0:
            if (sFishingPlayingState == 0) {
                if (sLinkAge != LINK_AGE_CHILD) {
                    if ((HIGH_SCORE(HS_FISHING) & HS_FISH_PLAYED_CHILD) &&
                        !(HIGH_SCORE(HS_FISHING) & HS_FISH_PLAYED_ADULT)) {
                        this->actor.textId = 0x4093;
                    } else {
                        this->actor.textId = 0x407B;
                    }
                } else {
                    this->actor.textId = 0x407B;
                }
            } else if (!sIsOwnersHatHooked) {
                this->actor.textId = 0x4084;
            } else {
                this->actor.textId = 0x4097;
            }

            if (Actor_ProcessTalkRequest(&this->actor, play)) {
                if (sFishingPlayingState == 0) {
                    this->stateAndTimer = 1;
                    if (sLinkAge != LINK_AGE_CHILD) {
                        HIGH_SCORE(HS_FISHING) |= HS_FISH_PLAYED_ADULT;
                    } else {
                        HIGH_SCORE(HS_FISHING) |= HS_FISH_PLAYED_CHILD;
                    }
                } else {
                    this->stateAndTimer = 10;
                }
            } else {
                func_8002F2CC(&this->actor, play, 100.0f);
            }
            break;

        case 1:
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);

                switch (play->msgCtx.choiceIndex) {
                    case 0:
                        if (gSaveContext.rupees >= 20) {
                            Rupees_ChangeBy(-20);
                            this->actor.textId = 0x407C;
                            Message_ContinueTextbox(play, this->actor.textId);
                            this->stateAndTimer = 2;
                        } else {
                            Message_ContinueTextbox(play, 0x407E);
                            this->stateAndTimer = 3;
                        }
                        break;
                    case 1:
                        Message_ContinueTextbox(play, 0x2D);
                        this->stateAndTimer = 3;
                        break;
                }
                // Fix for owner getting stuck when the pond is closed (Fishing Pole Shuffle)
            } else if (Message_GetState(&play->msgCtx) == TEXT_STATE_DONE && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);
                this->stateAndTimer = 0;
            }
            break;

        case 2:
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);
                Message_ContinueTextbox(play, 0x407F);
                this->stateAndTimer = 4;
            }
            break;

        case 3:
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);
                this->stateAndTimer = 0;
            }
            if (Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) {
                this->stateAndTimer = 0;
            }
            break;

        case 4:
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);

                switch (play->msgCtx.choiceIndex) {
                    case 0:
                        sFishLengthToWeigh = sFishingRecordLength;
                        Message_ContinueTextbox(play, 0x4080);
                        this->stateAndTimer = 5;
                        break;
                    case 1:
                        Message_ContinueTextbox(play, 0x407F);
                        break;
                }
            }
            break;

        case 5:
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);

                play->startPlayerFishing(play);
                Fishing_StartPlaying(play);
                this->stateAndTimer = 0;

                if ((HIGH_SCORE(HS_FISHING) & 0xFF0000) < 0xFF0000) {
                    HIGH_SCORE(HS_FISHING) += HS_FISH_PLAYED;
                }
            }
            break;

        case 10:
            if (sIsOwnersHatHooked) { // owner asks for hat back
                if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                    Message_CloseTextbox(play);

                    switch (play->msgCtx.choiceIndex) {
                        case 0:
                            Message_ContinueTextbox(play, 0x40B2);
                            sOwnerHair = FS_OWNER_CAPPED;
                            sIsOwnersHatHooked = false;
                            this->stateAndTimer = 20;
                            break;
                        case 1:
                            this->stateAndTimer = 0;
                            break;
                    }
                }
            } else {
                if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                    Message_CloseTextbox(play);

                    switch (play->msgCtx.choiceIndex) {
                        case 0:
                            if (sFishOnHandLength == 0.0f) {
                                this->actor.textId = 0x408C;
                                this->stateAndTimer = 20;
                            } else if (sFishOnHandIsLoach == 0) {
                                sFishLengthToWeigh = sFishOnHandLength;
                                // if we set the record or get the reward
                                if (((int16_t)sFishingRecordLength < (int16_t)sFishOnHandLength)) {
                                    if (sLureCaughtWith == FS_LURE_SINKING) {
                                        this->actor.textId = 0x40B0;
                                    } else {
                                        this->actor.textId = 0x4086;
                                    }
                                    this->stateAndTimer = 11;
                                } else {
                                    this->actor.textId = 0x408B;
                                    this->stateAndTimer = 20;
                                }
                            } else {
                                this->actor.textId = 0x409B;
                                this->stateAndTimer = 11;
                            }
                            Message_ContinueTextbox(play, this->actor.textId);
                            break;
                        case 1:
                            if (sFishingTimePlayed > 36000) {
                                sFishingTimePlayed = 30000;
                                Message_ContinueTextbox(play, 0x4088);
                            } else {
                                if (sFishingFoggy == 0) {
                                    if (sPondOwnerTextIdIndex == 0) {
                                        sPondOwnerTextIdIndex++;
                                    }
                                }

                                if ((sLureEquipped == FS_LURE_SINKING) &&
                                    (sPondOwnerTextIds[sPondOwnerTextIdIndex] == 0x408D)) {
                                    Message_ContinueTextbox(play, 0x40AF);
                                } else {
                                    Message_ContinueTextbox(play, sPondOwnerTextIds[sPondOwnerTextIdIndex]);
                                }

                                sPondOwnerTextIdIndex++;

                                if (sLinkAge != LINK_AGE_CHILD) {
                                    if (sPondOwnerTextIdIndex >= 6) {
                                        sPondOwnerTextIdIndex = 0;
                                    }
                                } else {
                                    if (sPondOwnerTextIdIndex >= 4) {
                                        sPondOwnerTextIdIndex = 0;
                                    }
                                }
                            }
                            this->stateAndTimer = 0;
                            break;
                        case 2:
                            if (sFishesCaught == 0) {
                                Message_ContinueTextbox(play, 0x4085);
                            } else if (sLinkAge == LINK_AGE_CHILD) {
                                Message_ContinueTextbox(play, 0x4092);
                            }
                            this->stateAndTimer = 22;
                            break;
                    }
                }
            }
            break;

        case 11:
            if (((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) ||
                 (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) &&
                Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);

                if (sFishOnHandIsLoach == 0) {
                    sFishingRecordLength = sFishOnHandLength;
                    HIGH_SCORE(HS_FISHING) &= 0x00FFFFFF;
                    HIGH_SCORE(HS_FISHING) |= ((int32_t)sFishingRecordLength & HS_FISH_LENGTH_CHILD) << 24;
                }

                sFishOnHandLength = 0.0f;
                this->actor.parent = NULL;
                this->stateAndTimer = 0;
                sIsRodVisible = true;
            }
            break;
        case 20:
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);
                this->stateAndTimer = 0;
            }
            break;

        case 21:
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);

                switch (play->msgCtx.choiceIndex) {
                    case 0:
                        this->stateAndTimer = 0;
                        break;
                    case 1:
                        if (sFishesCaught == 0) {
                            Message_ContinueTextbox(play, 0x4085);
                        } else if (sLinkAge == LINK_AGE_CHILD) {
                            Message_ContinueTextbox(play, 0x4092);
                        }
                        this->stateAndTimer = 22;
                        break;
                }
            }
            break;

        case 22:
            if (play) {}

            if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                this->stateAndTimer = 0;
                if (sIsOwnersHatHooked) {
                    sOwnerHair = FS_OWNER_CAPPED;
                    sIsOwnersHatHooked = false;
                }
                sFishingPlayingState = 0;
                play->interfaceCtx.unk_260 = 0;
            }
            break;

    }
}

static int16_t sOwnerBlinkIndex[] = { 0, 1, 2, 2, 1 };

static Vec3f sStreamSoundPos = { 670.0f, 0.0f, -600.0f };

static Vec3s sSinkingLureLocationPos[] = {
    { -364, -30, -269 }, // in the pond, log past the lilies.
    { 1129, 3, -855 },   // rock next to stream
    { -480, 0, -1055 },  // wall opposite of entrance
    { 553, -48, -508 },  // tip of log beside 3 posts
};

void Fishing_UpdateOwner(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    Fishing* this = (Fishing*)thisx;
    Vec3f multiVecSrc;
    Vec3f eyeTarget;
    Vec3f lureDist;
    int16_t headRotTarget = { 0 };
    float target = { 0 };
    float subCamAtMaxVelFrac = { 0 };
    Player* player = GET_PLAYER(play);
    MovementInput* input = &play->state.input;



    int16_t playerShadowAlpha = player->actor.shape.shadowAlpha;

    if ((SQ(player->actor.world.pos.x) + SQ(player->actor.world.pos.z)) < SQ(920.0f)) {
        Math_ApproachS(&playerShadowAlpha, 0, 1, 40);
    } else {
        Math_ApproachS(&playerShadowAlpha, 200, 1, 40);
    }

    player->actor.shape.shadowAlpha = playerShadowAlpha;

    SkelAnime_Update(&this->skelAnime);

    if ((sOwnerTheftTimer != 0) || (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE)) {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    } else {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    }

    if ((this->actor.xzDistToPlayer < 120.0f) || (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE)) {
        headRotTarget = this->actor.shape.rot.y - this->actor.yawTowardsPlayer;
    } else {
        headRotTarget = 0;
    }

    if (headRotTarget > 0x2710) {
        headRotTarget = 0x2710;
    } else if (headRotTarget < -0x2710) {
        headRotTarget = -0x2710;
    }

    Math_ApproachS(&this->unk_164, headRotTarget, 3, 0x1388);

    if (((play->gameplayFrames % 32) == 0) && (Rand_ZeroOne() < 0.3f)) {
        this->unk_162 = 4;
    }

    this->unk_160 = sOwnerBlinkIndex[this->unk_162];

    if (this->unk_162 != 0) {
        this->unk_162--;
    }

    if (sOwnerTheftTimer != 0) {
        sOwnerTheftTimer--;
    }

    // steal the owner's hat
    if (!sIsOwnersHatHooked && (sLureEquipped != FS_LURE_SINKING) && (sRodCastState > 0) &&
        (sOwnerHair == FS_OWNER_CAPPED) && (sOwnerTheftTimer == 0)) {
        float dx = sOwnerHeadPos.x - sLurePos.x;
        float dy = sOwnerHeadPos.y - sLurePos.y;
        float dz = sOwnerHeadPos.z - sLurePos.z;

        if ((sqrtf(SQ(dx) + SQ(dy) + SQ(dz)) < 25.0f) || (KREG(77) > 0)) {
            KREG(77) = 0;
            sOwnerHair = FS_OWNER_BALD;
            sIsOwnersHatHooked = true;
            Message_StartTextbox(play, 0x4087, NULL);
        }
    }

    // update hat flag.
    if (sOwnerHair == FS_OWNER_BALD) {
        HIGH_SCORE(HS_FISHING) |= HS_FISH_STOLE_HAT;
    } else if (sOwnerHair == FS_OWNER_CAPPED) {
        HIGH_SCORE(HS_FISHING) &= ~HS_FISH_STOLE_HAT;
    }

    if (KREG(77) < 0) {
        KREG(77) = 0;
        sIsOwnersHatSunk = true;
    }

    if (sFishingCaughtTextDelay != 0) {
        sFishingCaughtTextDelay--;
        if (sFishingCaughtTextDelay == 0) {
            Fishing_StartCatchNotification(play);
        }
    }

    // C-Up equips the fishing pole directly in this port. A valid cast puts
    // Link into fishing state before the pond owner has started the minigame,
    // so start the pond-side rod, lure, and line state here as well.
    if ((sFishingPlayingState == 0) && (player->heldItemAction == PLAYER_IA_FISHING_POLE) &&
        (player->unk_860 == 1)) {
        Fishing_StartPlaying(play);
    }

    Fishing_HandleOwnerDialog(this, play);

    sFishingLineScale = 0.0015f;
    sFishingTimePlayed++;

    if ((sFishingPlayingState != 0) && (player->heldItemAction != PLAYER_IA_FISHING_POLE)) {
        sRodCastState = 0;
        sRodCastTimer = 0;
        sLineHooked = false;
        FishingGameplay_SubmitAction(FISHING_GAMEPLAY_ACTION_RELEASE);
        sFishingHookedFish = NULL;
        Fishing_StopCinematic(play);
    }

    if ((sFishingPlayingState != 0) && sIsRodVisible &&
        (player->heldItemAction == PLAYER_IA_FISHING_POLE)) {
        Fishing_UpdateLure(this, play);
    }

    Fishing_UpdateEffects(play->specialEffects, play);
    Fishing_UpdatePondProps(play);
    Fishing_UpdateGroupFishes(play);
    if ((sSinkingLureLocation != 0) &&
        (fabsf(player->actor.world.pos.x - sSinkingLureLocationPos[sSinkingLureLocation - 1].x) < 25.0f) &&
        (fabsf(player->actor.world.pos.y - sSinkingLureLocationPos[sSinkingLureLocation - 1].y) < 10.0f) &&
        (fabsf(player->actor.world.pos.z - sSinkingLureLocationPos[sSinkingLureLocation - 1].z) < 25.0f)) {
        sSinkingLureLocation = 0;
        sFishingPlayerCinematicState = 20;
        Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
        Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x1400FF);
    }

    if (KREG(0) != 0) {
        KREG(0) = 0;
        sLureEquipped = FS_LURE_STOCK;
        sFishingPlayerCinematicState = 20;
        Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
        Audio_QueueSeqCmd(0x1 << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0x1400FF);
    }

    if (sFishingCinematicTimer != 0) {
        sFishingCinematicTimer--;
    }

    switch (sFishingPlayerCinematicState) {
        case 0:
            break;

        case 1: {

            sSubCamId = Play_CreateSubCamera(play);
            Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, sSubCamId, CAM_STAT_ACTIVE);
            Camera* mainCam = Play_GetCamera(play, MAIN_CAM);
            sCameraEye.x = mainCam->eye.x;
            sCameraEye.y = mainCam->eye.y;
            sCameraEye.z = mainCam->eye.z;
            sCameraAt.x = mainCam->at.x;
            sCameraAt.y = mainCam->at.y;
            sCameraAt.z = mainCam->at.z;
            sFishingPlayerCinematicState = 2;
            Interface_ChangeAlpha(12);
            sSubCamVelFactor = 0.0f;
            // fallthrough
        }

        case 2:
            ShrinkWindow_SetVal(0x1B);

            lureDist.x = sLurePos.x - player->actor.world.pos.x;
            lureDist.z = sLurePos.z - player->actor.world.pos.z;
            float lureDistXZ = sqrtf(SQ(lureDist.x) + SQ(lureDist.z));
            Matrix_RotateY(atan2f(lureDist.x, lureDist.z), MTXMODE_NEW);

            multiVecSrc.x = 0.0f;
            multiVecSrc.y = 0.0f;
            multiVecSrc.z = 100.0f;
            Matrix_MultVec3f(&multiVecSrc, &lureDist);

            if (sRodCastState == 1) {
                subCamAtMaxVelFrac = 0.2f;
            } else {
                subCamAtMaxVelFrac = 0.1f;
            }

            Math_ApproachF(&sCameraAt.x, sLurePos.x, subCamAtMaxVelFrac, fabsf(lureDist.x) * sSubCamVelFactor);
            Math_ApproachF(&sCameraAt.y, sLurePos.y, subCamAtMaxVelFrac, 50.0f * sSubCamVelFactor);
            Math_ApproachF(&sCameraAt.z, sLurePos.z, subCamAtMaxVelFrac, fabsf(lureDist.z) * sSubCamVelFactor);

            multiVecSrc.x = 0.0f - D_80B7FED0;
            if (sLinkAge != LINK_AGE_CHILD) {
                multiVecSrc.y = 80.0f;
            } else {
                multiVecSrc.y = 55.0f;
            }
            multiVecSrc.z = -80.0f;

            Matrix_MultVec3f(&multiVecSrc, &eyeTarget);
            eyeTarget.x += player->actor.world.pos.x;
            eyeTarget.y += player->actor.world.pos.y;
            eyeTarget.z += player->actor.world.pos.z;

            Math_ApproachF(&D_80B7FED0, 30.0f, 0.1f, 0.4f);

            if (sRodCastState >= 3) {
                if (lureDistXZ < 110.0f) {
                    sLureCameraZoomLevel = -1;
                } else if ((lureDistXZ > 300.0f) && (sLureCameraZoomLevel < 0)) {
                    sLureCameraZoomLevel = 0;
                }
            }

            if (sLureCameraZoomLevel > 0) {
                float offset = { 0 };
                float factor = { 0 };

                float dist = sqrtf(SQ(lureDist.x) + SQ(lureDist.z)) * 0.001f;
                if (dist > 1.0f) {
                    dist = 1.0f;
                }
                if (sLureCameraZoomLevel == 2) {
                    offset = 0.3f;
                } else {
                    offset = 0.1f;
                }
                factor = 0.4f + offset + (dist * 0.4f);

                eyeTarget.x += (sLurePos.x - eyeTarget.x) * factor;
                eyeTarget.y += ((sLurePos.y - eyeTarget.y) * factor) + 20.0f;
                eyeTarget.z += (sLurePos.z - eyeTarget.z) * factor;
                sFishingLineScale = 0.0005000001f;
            }

            multiVecSrc.x = 0.0f;
            multiVecSrc.y = 0.0f;
            multiVecSrc.z = 100.0f;
            Matrix_MultVec3f(&multiVecSrc, &lureDist);

            Math_ApproachF(&sCameraEye.x, eyeTarget.x, 0.3f, fabsf(lureDist.x) * sSubCamVelFactor);
            Math_ApproachF(&sCameraEye.y, eyeTarget.y, 0.3f, 20.0f * sSubCamVelFactor);
            Math_ApproachF(&sCameraEye.z, eyeTarget.z, 0.3f, fabsf(lureDist.z) * sSubCamVelFactor);
            break;

        case 3: {
            Camera* mainCam = Play_GetCamera(play, MAIN_CAM);

            mainCam->eye = sCameraEye;
            mainCam->eyeNext = sCameraEye;
            mainCam->at = sCameraAt;
            func_800C08AC(play, sSubCamId);
            PlayerAction_Reset(play);
            sFishingPlayerCinematicState = 0;
            sSubCamId = 0;
            Environment_EnableUnderwaterLights(play, 0);
            play->envCtx.adjFogNear = 0;
            player->unk_860 = -5;
            D_80B7E0B0 = 5;
            break;
        }

        case 10: { // owner tells you to return the rod.

            PlayerAction_Reset(play);
            sSubCamId = Play_CreateSubCamera(play);
            Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, sSubCamId, CAM_STAT_ACTIVE);
            Player_SetCsActionWithHaltedActors(play, &this->actor, 5);
            Camera* mainCam = Play_GetCamera(play, MAIN_CAM);
            sCameraEye.x = mainCam->eye.x;
            sCameraEye.y = mainCam->eye.y;
            sCameraEye.z = mainCam->eye.z;
            sCameraAt.x = mainCam->at.x;
            sCameraAt.y = mainCam->at.y;
            sCameraAt.z = mainCam->at.z;
            Message_StartTextbox(play, 0x409E, NULL);
            sFishingPlayerCinematicState = 11;
            // fallthrough
        }

        case 11:
            player->actor.world.pos.z = 1360.0f;
            player->actor.speedXZ = 0.0f;

            if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                Camera* mainCam = Play_GetCamera(play, MAIN_CAM);

                mainCam->eye = sCameraEye;
                mainCam->eyeNext = sCameraEye;
                mainCam->at = sCameraAt;
                func_800C08AC(play, sSubCamId);
                PlayerAction_Reset(play);
                Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
                sFishingPlayerCinematicState = 0;
                sSubCamId = 0;
                sFishingCinematicTimer = 30;
                Environment_EnableUnderwaterLights(play, 0);
                play->envCtx.adjFogNear = 0;
            }
            break;

        case 20: { // found the sinking lure

            PlayerAction_Reset(play);
            sSubCamId = Play_CreateSubCamera(play);
            Play_ChangeCameraStatus(play, MAIN_CAM, CAM_STAT_WAIT);
            Play_ChangeCameraStatus(play, sSubCamId, CAM_STAT_ACTIVE);
            Player_SetCsActionWithHaltedActors(play, &this->actor, 5);
            Camera* mainCam = Play_GetCamera(play, MAIN_CAM);
            sCameraEye.x = mainCam->eye.x;
            sCameraEye.y = mainCam->eye.y;
            sCameraEye.z = mainCam->eye.z;
            sCameraAt.x = mainCam->at.x;
            sCameraAt.y = mainCam->at.y;
            sCameraAt.z = mainCam->at.z;
            Message_StartTextbox(play, 0x409A, NULL);
            sFishingPlayerCinematicState = 21;
            sCatchCamX = 45.0f;
            sFishingCinematicTimer = 10;
            // fallthrough
        }

        case 21:
            if ((sFishingCinematicTimer == 0) && Message_ShouldAdvance(play)) {
                sFishingPlayerCinematicState = 22;
                sFishingCinematicTimer = 40;
                Player_SetCsActionWithHaltedActors(play, &this->actor, 0x1C);
                sSinkingLureHeldY = 0.0f;
            }
            break;

        case 22:
            if (sFishingCinematicTimer == 30) {
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_ITEM_GET | 0x900);
            }

            sSinkingLureFound = 1;

            Math_ApproachF(&sSinkingLureHeldY, 71.0f, 0.5f, 3.0f);
            Matrix_RotateY((player->actor.shape.rot.y / 32768.0f) * M_PI, MTXMODE_NEW);

            multiVecSrc.x = Math_SinS(play->gameplayFrames * 0x1000);
            multiVecSrc.y = sSinkingLureHeldY;
            multiVecSrc.z = -5.0f;
            if (sLinkAge == LINK_AGE_CHILD) {
                multiVecSrc.y -= 20.0f;
            }

            Matrix_MultVec3f(&multiVecSrc, &eyeTarget);

            sSinkingLureBasePos.x = player->actor.world.pos.x + eyeTarget.x;
            sSinkingLureBasePos.y = player->actor.world.pos.y + eyeTarget.y;
            sSinkingLureBasePos.z = player->actor.world.pos.z + eyeTarget.z;

            Math_ApproachF(&sCatchCamX, 15.0f, 0.1f, 0.75f);

            multiVecSrc.x = sCatchCamX - 15.0f;

            if (sLinkAge != LINK_AGE_CHILD) {
                multiVecSrc.y = 60.0f;
                multiVecSrc.z = -30.0f;
            } else {
                multiVecSrc.y = 40.0f;
                multiVecSrc.z = -35.0f;
            }

            Matrix_MultVec3f(&multiVecSrc, &sCameraEye);
            sCameraEye.x += player->actor.world.pos.x;
            sCameraEye.y += player->actor.world.pos.y;
            sCameraEye.z += player->actor.world.pos.z;

            sCameraAt = player->actor.world.pos;
            if (sLinkAge != LINK_AGE_CHILD) {
                sCameraAt.y += 62.0f;
            } else {
                sCameraAt.y += 40.0f;
            }

            if (sFishingCinematicTimer == 0) {
                if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) ||
                    (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
                    if (Message_ShouldAdvance(play)) {
                        Camera* mainCam = Play_GetCamera(play, MAIN_CAM);

                        Message_CloseTextbox(play);
                        if (play->msgCtx.choiceIndex == 0) {
                            sLureEquipped = FS_LURE_SINKING;
                            sPondOwnerTextIdIndex = 0;
                        }

                        mainCam->eye = sCameraEye;
                        mainCam->eyeNext = sCameraEye;
                        mainCam->at = sCameraAt;
                        func_800C08AC(play, sSubCamId);
                        PlayerAction_Reset(play);
                        Player_SetCsActionWithHaltedActors(play, &this->actor, 7);
                        sFishingPlayerCinematicState = 0;
                        sSubCamId = 0;
                        player->unk_860 = -5;
                        D_80B7E0B0 = 5;
                        sSinkingLureFound = false;
                        sFishingMusicDelay = 20;
                        Environment_EnableUnderwaterLights(play, 0);
                        play->envCtx.adjFogNear = 0;
                    }
                }
            }
            break;

        case 100:
            break;
    }

    if (sSubCamId != 0) {
        Play_CameraSetAtEye(play, sSubCamId, &sCameraAt, &sCameraEye);
        Math_ApproachF(&sSubCamVelFactor, 1.0f, 1.0f, 0.02f);

        if (sCameraEye.y <= (WATER_SURFACE_Y(play) + 1.0f)) {
            Environment_EnableUnderwaterLights(play, 1);
            if (sFishingFoggy != 0) {
                play->envCtx.adjFogNear = -0xB2;
            } else {
                play->envCtx.adjFogNear = -0x2E;
            }
        } else {
            Environment_EnableUnderwaterLights(play, 0);
            play->envCtx.adjFogNear = 0;
        }
    }

    if ((player->actor.floorHeight < (WATER_SURFACE_Y(play) - 3.0f)) &&
        (player->actor.world.pos.y < (player->actor.floorHeight + 3.0f)) && (player->actor.speedXZ > 1.0f) &&
        ((play->gameplayFrames % 2) == 0)) {
        Vec3f pos;

        pos.x = Rand_CenteredFloat(20.0f) + player->actor.world.pos.x;
        pos.z = Rand_CenteredFloat(20.0f) + player->actor.world.pos.z;
        pos.y = player->actor.floorHeight + 5.0f;
        Fishing_SpawnWaterDust(NULL, play->specialEffects, &pos, 0.5f);
    }

    if ((player->actor.floorHeight < WATER_SURFACE_Y(play)) &&
        (player->actor.floorHeight > (WATER_SURFACE_Y(play) - 10.0f)) && (player->actor.speedXZ >= 4.0f) &&
        ((play->gameplayFrames % 4) == 0)) {
        int16_t i;

        for (i = 0; i < 10; i++) {
            Vec3f vel;

            float speedXZ = Rand_ZeroFloat(1.5f) + 1.5f;
            float angle = Rand_ZeroFloat(6.28f);

            vel.x = sinf(angle) * speedXZ;
            vel.z = cosf(angle) * speedXZ;
            vel.y = Rand_ZeroFloat(3.0f) + 2.0f;

            Vec3f pos = player->actor.world.pos;
            pos.x += 2.0f * vel.x;
            pos.y = WATER_SURFACE_Y(play);
            pos.z += 2.0f * vel.z;
            Fishing_SpawnDustSplash(NULL, play->specialEffects, &pos, &vel, Rand_ZeroFloat(0.01f) + 0.020000001f);
        }
    }

    if (sREG(15) != 0) {
        if (sStormStrengthTarget != (sREG(15) - 1)) {
            if (sStormStrengthTarget == 0) {
                play->envCtx.gloomySkyMode = 1;
            } else {
                play->envCtx.gloomySkyMode = 2;
            }
        }

        sStormStrengthTarget = sREG(15) - 1;
    }

    if (sREG(14) == 1) {
        play->envCtx.gloomySkyMode = 1;
    }
    if (sREG(14) == -1) {
        play->envCtx.gloomySkyMode = 2;
    }

    sREG(14) = 0;

    osSyncPrintf(VT_FGCOL(GREEN));
    osSyncPrintf("zelda_time %x\n", ((void)0, gSaveContext.dayTime));
    osSyncPrintf(VT_RST);

    if (sStormChanceTimer >= 2) {
        sStormChanceTimer--;
    }

    if ((sStormChanceTimer == 1) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) &&
        ((sFishingTimePlayed & 0xFFF) == 0xFFF)) {
        sStormChanceTimer = 200;

        if (Rand_ZeroOne() < 0.5f) {
            sStormStrengthTarget = (uint8_t)Rand_ZeroFloat(10.0f) + 5;
            play->envCtx.gloomySkyMode = 1;
        } else {
            sStormStrengthTarget = 0;
            play->envCtx.gloomySkyMode = 2;
        }
    }

    Math_ApproachF(&sStormStrength, sStormStrengthTarget, 1.0f, 0.05f);

    if (sStormStrength > 0.0f) {
        target = (sStormStrength * 0.03f) + 0.8f;
        if (target > 1.2f) {
            target = 1.2f;
        }
        Math_ApproachF(&sStormSfxFreqScale, target, 1.0f, 0.01f);
    }

    target = (10.0f - sStormStrength) * 150.1f;
    if (target < 0.0f) {
        target = 0.0f;
    }
    Math_ApproachF(&sFishingStormSfxPos.z, target, 1.0f, 5.0f);

    if (sFishingStormSfxPos.z < 1500.0f) {
        func_800F436C(&sFishingStormSfxPos, NA_SE_EV_RAIN - SFX_FLAG, sStormSfxFreqScale);
    }

    if (sStormStrengthTarget != 0) {
        Math_ApproachF(&sFishingStormShade, -200.0f, 1.0f, 2.0f);
    } else {
        Math_ApproachZeroF(&sFishingStormShade, 1.0f, 2.0f);
    }

    play->envCtx.adjLight1Color[0] = play->envCtx.adjLight1Color[1] = play->envCtx.adjLight1Color[2] =
        sFishingStormShade;

    if ((uint8_t)sStormStrength > 0) {
        Camera* mainCam = Play_GetCamera(play, MAIN_CAM);
        int16_t i;
        Vec3f pos;
        Vec3f rot;
        Vec3f projectedPos;

        rot.x = M_PI / 2.0f + 0.1f;
        rot.y = 1.0f;
        rot.z = (Camera_GetInputDirYaw(mainCam) * -(M_PI / 32768)) + rot.y;

        for (i = 0; i < (uint8_t)sStormStrength; i++) {
            pos.x = Rand_CenteredFloat(700.0f) + play->view.eye.x;
            pos.y = (Rand_ZeroFloat(100.0f) + 150.0f) - 170.0f;
            pos.z = Rand_CenteredFloat(700.0f) + play->view.eye.z;

            if (pos.z < 1160.0f) {
                SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &pos, &projectedPos, &sProjectedW);

                if (projectedPos.z < 0.0f) {
                    i--;
                } else {
                    Fishing_SpawnRainDrop(play->specialEffects, &pos, &rot);
                }
            }
        }
    }

    SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &sStreamSoundPos, &sStreamSoundProjectedPos, &sProjectedW);

    Sfx_PlaySfxAtPos(&sStreamSoundProjectedPos, NA_SE_EV_WATER_WALL - SFX_FLAG);

    gSaveContext.minigameScore = (SQ((float)sFishLengthToWeigh) * 0.0036f) + 0.5f;

    if (BREG(26) != 0) {
        BREG(26) = 0;
        Message_StartTextbox(play, 0x407B + BREG(27), NULL);
    }

    osSyncPrintf("HI_SCORE = %x\n", HIGH_SCORE(HS_FISHING));
}

int32_t Fishing_OwnerOverrideLimbDraw(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    Fishing* this = (Fishing*)thisx;

    if (limbIndex == 8) { // Head
        rot->x -= this->unk_164;
    }

    return 0;
}

void Fishing_OwnerPostLimbDraw(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    if (limbIndex == 8) { // Head
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_MultVec3f(&sZeroVec, &sOwnerHeadPos);

        if (sOwnerHair == FS_OWNER_CAPPED) {
            gSPDisplayList(POLY_OPA_DISP++, SEGMENTED_TO_VIRTUAL(gFishingOwnerHatDL));
        } else if (sOwnerHair == FS_OWNER_HAIR) {
            gSPDisplayList(POLY_OPA_DISP++, SEGMENTED_TO_VIRTUAL(gFishingOwnerHairDL));
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

static void Fishing_UpdatePortableSurface(Fishing* this, PlayState* play) {
    WaterBox* waterBox;
    CollisionPoly* floorPoly;
    int32_t floorBgId;
    float waterY = 32000.0f;

    sPortableWaterFound =
        WaterBox_GetSurface1(play, &play->colCtx, sLurePos.x, sLurePos.z, &waterY, &waterBox) != 0;
    sPortableWaterSurfaceY = sPortableWaterFound ? waterY : -32000.0f;

    if (!sPortableWaterFound && (sRodCastState != 0)) {
        Vec3f floorCheckPos = sLurePos;

        floorCheckPos.y += 1000.0f;
        float floorY = BgCheck_EntityRaycastFloor4(&play->colCtx, &floorPoly, &floorBgId, &this->actor, &floorCheckPos);
        if ((floorY != BGCHECK_Y_MIN) && (sLurePos.y <= (floorY + 3.0f))) {
            sLurePos.y = floorY + 3.0f;
            if (sRodCastState != 3) {
                sLurePosDelta.x = sLurePosDelta.y = sLurePosDelta.z = 0.0f;
                sRodCastState = 3;
                sReelLinePosStep = 0.0f;
            }
        }
    }
}

static void Fishing_CollidePortableLure(PlayState* play, Vec3f* previousPos) {
    CollisionPoly* hitPoly;
    Vec3f hitPos;
    int32_t hitBgId;
    float moveX = sLurePos.x - previousPos->x;
    float moveY = sLurePos.y - previousPos->y;
    float moveZ = sLurePos.z - previousPos->z;
    float moveLength = sqrtf(SQ(moveX) + SQ(moveY) + SQ(moveZ));

    if ((sRodCastState == 0) || (moveLength < 0.001f)) {
        return;
    }

    if (BgCheck_EntityLineTest1(&play->colCtx, previousPos, &sLurePos, &hitPos, &hitPoly, true, false, true, false,
                                &hitBgId)) {
        float backoff = 2.0f / moveLength;

        sLurePos.x = hitPos.x - (moveX * backoff);
        sLurePos.y = hitPos.y - (moveY * backoff);
        sLurePos.z = hitPos.z - (moveZ * backoff);
        sLurePosDelta.x = sLurePosDelta.y = sLurePosDelta.z = 0.0f;
        sRodCastState = 3;
        sReelLinePosStep = 0.0f;
    }
}

void Fishing_UpdatePortable(Actor* thisx, PlayState* play) {
    Fishing* this = (Fishing*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f previousLurePos = { 0 };

    // This actor renders equipment attached to Link, so it must follow Link
    // instead of remaining at the location of the first cast.
    thisx->world.pos = player->actor.world.pos;
    thisx->prevPos = player->actor.prevPos;

    if (player->heldItemAction != PLAYER_IA_FISHING_POLE) {
        sRodCastState = 0;
        sRodCastTimer = 0;
        sLineHooked = false;
        FishingGameplay_SubmitAction(FISHING_GAMEPLAY_ACTION_RELEASE);
        sFishingHookedFish = NULL;
        sPortableLineInitialized = false;
        Fishing_StopCinematic(play);
        return;
    }

    Fishing_UpdatePortableSurface(this, play);

    sFishingLineScale = 0.0015f;
    sFishingTimePlayed++;

    // The pond owner normally starts this message. Portable fishing has no
    // owner actor, so present the same measured catch text from its controller.
    if (sFishingCaughtTextDelay != 0) {
        sFishingCaughtTextDelay--;
        if (sFishingCaughtTextDelay == 0) {
            Fishing_StartCatchNotification(play);
        }
    }

    previousLurePos = sLurePos;

    Fishing_UpdateLure(this, play);

    Fishing_CollidePortableLure(play, &previousLurePos);

    // The lure can cross a floor between frames, especially during the
    // faster portable cast. Check again after both cast and reel movement.
    Fishing_UpdatePortableSurface(this, play);

    if (sFishingPlayerCinematicState == 3) {
        Fishing_StopCinematic(play);
        player->unk_860 = 0;
        D_80B7E0B0 = 5;
    }

    Fishing_UpdateEffects(sFishingEffects, play);
}

void Fishing_DrawPortable(Actor* thisx, PlayState* play) {
    MovementInput* input = &play->state.input;
    Player* player = GET_PLAYER(play);
    int16_t i;

    // A dead owner is hidden while its retained corpse owns presentation.
    // Living owners always render their native rod, line, and lure; only
    // observers use the reconstructed multiplayer fishing presentation.
    if (player->authoritativeBodyHidden ||
        player->heldItemAction != PLAYER_IA_FISHING_POLE) {
        return;
    }

    Fishing_DrawEffects(sFishingEffects, play);
    Fishing_DrawRod(play);

    if (!sPortableLineInitialized) {
        sRodLineSpooled = 194.0f;
        for (i = 0; i < LINE_SEG_COUNT; i++) {
            sReelLinePos[i] = sRodTipPos;
            if (i > (int16_t)sRodLineSpooled) {
                sReelLinePos[i].y -= (i - (int16_t)sRodLineSpooled) * 5.0f;
            }
        }
        sLurePos = sReelLinePos[LINE_SEG_COUNT - 1];
        sPortableLineInitialized = true;
    }

    Fishing_UpdateLinePos(sReelLinePos);
    Fishing_UpdateLine(play, &sRodTipPos, sReelLinePos, sReelLineRot, sReelLineUnk);
    Fishing_DrawLureAndLine(play, sReelLinePos, sReelLineRot);
    if (sLureEquipped == FS_LURE_SINKING) {
        Fishing_DrawSinkingLure(play);
    }

    sStickAdjXPrev = input->x;
    sStickAdjYPrev = input->y;
}

static void* sFishingOwnerEyeTexs[] = {
    gFishingOwnerEyeOpenTex,
    gFishingOwnerEyeHalfTex,
    gFishingOwnerEyeClosedTex,
};

void Fishing_DrawOwner(Actor* thisx, PlayState* play) {
    Fishing* this = (Fishing*)thisx;
    MovementInput* input = &play->state.input;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    if ((this->actor.projectedPos.z < 1500.0f) &&
        (fabsf(this->actor.projectedPos.x) < (100.0f + this->actor.projectedPos.z))) {
        gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sFishingOwnerEyeTexs[this->unk_160]));

        SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, Fishing_OwnerOverrideLimbDraw, Fishing_OwnerPostLimbDraw,
                                  this);
    }

    Fishing_DrawPondProps(play);
    Fishing_DrawEffects(play->specialEffects, play);
    Fishing_DrawGroupFishes(play);
    Fishing_DrawStreamSplash(play);

    if (sFishingMusicDelay != 0) {
        sFishingMusicDelay--;

        if (sFishingMusicDelay == 0) {
            if (sLinkAge != LINK_AGE_CHILD) {
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_KAKARIKO_ADULT);
            } else {
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_KAKARIKO_KID);
            }

            if (sLinkAge != LINK_AGE_CHILD) {
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_KAKARIKO_ADULT);
            } else {
                Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_KAKARIKO_KID);
            }
        }
    }

    if ((sFishingPlayingState != 0) && sIsRodVisible &&
        (GET_PLAYER(play)->heldItemAction == PLAYER_IA_FISHING_POLE)) {
        Fishing_DrawRod(play);
        Fishing_UpdateLinePos(sReelLinePos);
        Fishing_UpdateLine(play, &sRodTipPos, sReelLinePos, sReelLineRot, sReelLineUnk);
        Fishing_DrawLureAndLine(play, sReelLinePos, sReelLineRot);

        sStickAdjXPrev = input->x;
        sStickAdjYPrev = input->y;
    }

    sIsRodVisible = true;

    Matrix_Translate(130.0f, 40.0f, 1300.0f, MTXMODE_NEW);
    Matrix_Scale(0.08f, 0.12f, 0.14f, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    gSPDisplayList(POLY_OPA_DISP++, gFishingAquariumBottomDL);
    gSPDisplayList(POLY_XLU_DISP++, gFishingAquariumContainerDL);

    if ((sFishingPlayingState != 0) && (sLureEquipped == 2)) {
        Fishing_DrawSinkingLure(play);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Fishing_Reset(void) {
    // Reset static variables for fishing camera and cinematic state to prevent crashing when dying
    // or re-entering the scene while the fishing rod was cast
    sSubCamId = 0;
    sFishingPlayerCinematicState = 0;
    FishingGameplay_SubmitAction(FISHING_GAMEPLAY_ACTION_RELEASE);
    sFishingHookedFish = NULL;
}
