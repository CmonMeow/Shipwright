#ifndef Z_FISHING_H
#define Z_FISHING_H

#include <runtime/libultra.h>
#include "global.h"

#define FISHING_LINE_SEG_COUNT 200

struct Fishing;

typedef struct Fishing {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ char unk_14C[0x004];
    /* 0x0150 */ uint8_t isLoach;
    /* 0x0151 */ uint8_t lilyTimer; // if near lily and >0, lily moves. Move more if >20
    /* 0x0152 */ uint8_t unk_152;
    /* 0x0154 */ int16_t unk_154;
    /* 0x0156 */ uint8_t unk_156;
    /* 0x0157 */ uint8_t unk_157;
    /* 0x0158 */ int16_t fishState;  // negative index for loach behavior
    /* 0x015A */ int16_t fishStateNext;
    /* 0x015C */ int16_t stateAndTimer; // fish use as timer that's AND'd, owner as talking state
    /* 0x015E */ int16_t unk_15E;
    /* 0x0160 */ int16_t unk_160; // fish use as rotateX, owner as index of eye texture
    /* 0x0162 */ int16_t unk_162; // fish use as rotateY, owner as index of eye texture
    /* 0x0164 */ int16_t unk_164; // fish use as rotateZ, owner as rotation of head
    /* 0x0166 */ Vec3s rotationTarget;
    /* 0x016C */ int16_t fishLimb23RotYDelta;
    /* 0x016E */ int16_t unk_16E;
    /* 0x0170 */ int16_t fishLimbDRotZDelta;
    /* 0x0172 */ int16_t fishLimbEFRotYDelta;
    /* 0x0174 */ int16_t fishLimb89RotYDelta;
    /* 0x0176 */ int16_t fishLimb4RotYDelta;
    /* 0x0178 */ int16_t unk_178;
    /* 0x017A */ int16_t timerArray[4];
    /* 0x0184 */ float unk_184;
    /* 0x0188 */ float speedTarget;
    /* 0x018C */ float fishLimbRotPhase;
    /* 0x0190 */ float unk_190; // fishLimbRotPhaseStep target
    /* 0x0194 */ float unk_194; // fishLimbRotPhaseMag target
    /* 0x0198 */ float fishLimbRotPhaseStep;
    /* 0x019C */ float fishLimbRotPhaseMag;
    /* 0x01A0 */ int16_t bumpTimer; // set when hitting a wall.
    /* 0x01A2 */ int16_t unk_1A2; // "scared" timer?
    /* 0x01A4 */ int16_t unk_1A4; // "scared" timer? set at same time as above
    /* 0x01A8 */ float perception; // how easily they are drawn to the lure.
    /* 0x01AC */ float fishLength; // fish are (x^2*.0036+.5) lbs, loach double that.
    /* 0x01B0 */ float rotationStep;
    /* 0x01B4 */ Vec3f fishTargetPos;
    /* 0x01C0 */ Vec3f fishMouthPos;
    /* 0x01CC */ int16_t loachRotYDelta[3]; // adds rotation to the loach limb 3-5.
    /* 0x01D2 */ uint8_t bubbleTime; // spawn bubbles while >0
    /* 0x01D3 */ uint8_t isAquariumMessage;
    /* 0x01D4 */ uint8_t aquariumWaitTimer;
    /* 0x01D8 */ SkelAnime skelAnime;
    /* 0x021C */ LightNode* lightNode;
    /* 0x0220 */ LightInfo lightInfo;
    /* 0x0230 */ ColliderJntSph collider;
    /* 0x0250 */ ColliderJntSphElement colliderElements[12];
    uint8_t isWild;
    WaterBox* wildWaterBox;
    float wildWaterSurfaceY;
    float wildMinX;
    float wildMaxX;
    float wildMinZ;
    float wildMaxZ;
    uint8_t remotePresentationActive;
} Fishing;

#define EN_FISH_OWNER 1      // param for owner of pond. default if params<100
#define EN_FISH_PARAM 100    // param base for fish in pond.
#define EN_FISH_AQUARIUM 200 // param for record fish in tank.
#define EN_FISH_PORTABLE 300 // rod/lure controller used outside the fishing pond
#define EN_FISH_WILD 400     // fish spawned in ordinary scene water boxes
#define EN_LOACH_WILD 401    // loach spawned in ordinary scene water boxes

struct VBFishingData {
    Fishing* actor;
    uint8_t* sFishOnHandIsLoach;
    uint8_t* sSinkingLureLocation;
    float* sFishOnHandLength;
    float fishWeight;
    float sFishingRecordLength;
};

int32_t Fishing_EnsurePresentedPopulation(PlayState* play);
void Fishing_UpdatePresentedLine(PlayState* play, Actor* collisionActor, Vec3f* rodTip, Vec3f* lurePos,
                               Vec3f linePos[FISHING_LINE_SEG_COUNT], Vec3f lineRot[FISHING_LINE_SEG_COUNT],
                               Vec3f lineUnk[FISHING_LINE_SEG_COUNT], int16_t lineSpooled, uint8_t lureType, float lineGravity);
void Fishing_UpdatePresentedSinkingLure(Vec3f* lurePos, Vec3f positions[20], int16_t playerYaw, uint8_t castState,
                                      uint8_t underwater);

#endif
