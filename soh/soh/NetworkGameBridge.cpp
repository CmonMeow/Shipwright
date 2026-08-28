#include "NetworkGameBridge.h"

#include "ActorDB.h"
#include "ResourceManagerHelpers.h"
#include "Network/ShipwrightNetworkRuntime.h"
#include "ship/window/PathEngineOverlay.h"
#include "PathEngineMultiplayerUI.h"
#include "frame_interpolation.h"

#include "assets/objects/gameplay_keep/gameplay_keep.h"
#include "assets/objects/object_fish/object_fish.h"
#include "global.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

#include <libultraship/log/PathEngineLog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#ifndef NDEBUG
#define NETWORK_OPEN_DISPS(gfxContext)                                                                            \
    {                                                                                                             \
        FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);                                                   \
        GraphicsContext* __gfxCtx = (gfxContext);                                                                 \
        Gfx* networkDispRefs[4];                                                                                  \
        Graph_OpenDisps(networkDispRefs, __gfxCtx, __FILE__, __LINE__)
#define NETWORK_CLOSE_DISPS()                                                                                     \
        FrameInterpolation_RecordCloseChild();                                                                    \
        Graph_CloseDisps(networkDispRefs, __gfxCtx, __FILE__, __LINE__);                                          \
    }
#else
#define NETWORK_OPEN_DISPS(gfxContext)                                                                            \
    {                                                                                                             \
        FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);                                                   \
        GraphicsContext* __gfxCtx = (gfxContext);                                                                 \
        (void)__gfxCtx;
#define NETWORK_CLOSE_DISPS()                                                                                     \
        FrameInterpolation_RecordCloseChild();                                                                    \
    }
#endif

namespace {

using SoH::Network::ShipwrightNetworkRuntime;

constexpr const char* kRemotePlayerActorName = "NETWORK_REMOTE_PLAYER";
using DynamicObjectKey = std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t>;
struct NetworkRemotePlayer {
    Actor actor;
    ColliderCylinder bodyCollider;
    SkelAnime skelAnime;
    SkelAnime bowArrowSkelAnime;
    Vec3s jointTable[PLAYER_LIMB_BUF_COUNT];
    Vec3s morphTable[PLAYER_LIMB_BUF_COUNT];
    int32_t playerId;
    uint8_t modelGroup;
    uint8_t itemAction;
    Vec3s upperLimbRot;
    Vec3s headLimbRot;
    Vec3f smokeInitialPos;
    bool smokeMotionObserved;
    bool bodyColliderInitialized;
};

struct RemotePlayerRecord {
    NetworkPlayerStatePacket state{};
    NetworkPlayerStatePacket previousState{};
    NetworkRemotePlayer* actor = nullptr;
    uint64_t lastPacketMilliseconds = 0;
    bool hasState = false;
    bool hasPreviousState = false;
    bool fishingLineInitialized = false;
    bool fishingSinkingLureInitialized = false;
    Vec3f fishingLinePos[NETWORK_FISHING_LINE_POINT_COUNT]{};
    Vec3f fishingLineRot[NETWORK_FISHING_LINE_POINT_COUNT]{};
    Vec3f fishingLineUnk[NETWORK_FISHING_LINE_POINT_COUNT]{};
    Vec3f fishingSinkingLurePos[20]{};
};

struct NetworkRemoteProjectile {
    Actor actor;
    SkelAnime skelAnime;
    int32_t ownerPlayerId;
    int32_t projectileId;
    uint8_t lastPhase;
};

struct RemoteProjectileRecord {
    NetworkProjectileStatePacket state{};
    NetworkRemoteProjectile* actor = nullptr;
    bool hitWorld = false;
    bool impactReported = false;
    Vec3f worldHitPos{};
};

struct LocalProjectileRecord {
    int32_t projectileId = 0;
    bool launched = false;
    bool retired = false;
    uint8_t lastPhase = 0xFF;
};

struct NetworkGameState {
    std::unique_ptr<ShipwrightNetworkRuntime> runtime;
    std::unique_ptr<PathEngineMultiplayerUI> multiplayerUI;
    std::map<int32_t, RemotePlayerRecord> remotes;
    std::map<std::pair<int32_t, int32_t>, RemoteProjectileRecord> remoteProjectiles;
    std::map<Actor*, LocalProjectileRecord> localProjectiles;
    std::set<DynamicObjectKey> destroyedObjects;
    std::multimap<std::pair<DynamicObjectKey, uint8_t>, int32_t> actorEvents;
    uint32_t nextSequence = 1;
    uint64_t lastFrameMilliseconds = 0;
    int16_t remoteActorId = -1;
    int16_t remoteProjectileActorId = -1;
    int32_t nextProjectileId = 1;
    int32_t nextActorEventId = 1;
    bool autoStartTest = false;
    bool smokeSpawnAdjusted = false;
    uint32_t smokeMotionFrames = 0;
    uint32_t smokePostMotionFrames = 0;
    bool smokeDeathTriggered = false;
    bool suppressDeathDuringRespawn = false;
};

ColliderCylinderInit sRemotePlayerBodyColliderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_NONE,
        OCELEM_ON,
    },
    { 12, 60, 0, { 0, 0, 0 } },
};

CollisionCheckInfoInit2 sRemotePlayerCollisionInfo = { 0, 0, 0, 0, MASS_IMMOVABLE };

NetworkGameState gNetworkGame;

extern "C" s32 Fishing_GetNetworkVisualState(PlayState* play, u8* castState, Vec3f* rodTipOffset,
                                               Vec3f* lureOffset, Vec3f* lureDrawOffset,
                                               f32* rodBendY, f32* rodBendX, f32* rodTwist, f32* rodCastX, Vec3f* lureRot,
                                               f32* lureSpin, f32* lureZOffset,
                                               Vec3f lureHookOffsets[2], Vec3f lureHookRot[2], f32* lineScale,
                                               f32* lineGravity,
                                               u8* lureType,
                                               u8* lineSpooled, u8* lineHooked, u8* fishActive,
                                               u8* fishIsLoach, Vec3f* fishOffset,
                                               Vec3s* fishRot, s16 fishLimbRot[8], f32* fishLength,
                                               s32* fishRoomId, s32* fishActorParams, s32* fishHomeX,
                                               s32* fishHomeY, s32* fishHomeZ,
                                               u8* sinkingLureSegmentIndex, u8* sinkingLureUnderwater);
extern "C" void Fishing_UpdateNetworkLine(PlayState* play, Actor* collisionActor, Vec3f* rodTip, Vec3f* lurePos,
                                            Vec3f linePos[NETWORK_FISHING_LINE_POINT_COUNT],
                                            Vec3f lineRot[NETWORK_FISHING_LINE_POINT_COUNT],
                                            Vec3f lineUnk[NETWORK_FISHING_LINE_POINT_COUNT], s16 lineSpooled,
                                            u8 lureType, f32 lineGravity);
extern "C" void Fishing_UpdateNetworkSinkingLure(Vec3f* lurePos, Vec3f positions[20], s16 playerYaw,
                                                   u8 castState, u8 underwater);
extern "C" s32 Ship_IsBowAimHeld(void);
extern "C" s32 Player_BuildPCBowJointTable(Player* player, Vec3s jointTable[PLAYER_LIMB_BUF_COUNT],
                                             s32 freezeLowerBody);

uint64_t NowMilliseconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}

bool SequenceIsNewer(uint32_t candidate, uint32_t current) {
    return static_cast<int32_t>(candidate - current) > 0;
}

void CopyPacketToRemoteActor(NetworkRemotePlayer* remote, const NetworkPlayerStatePacket& state, bool snapPosition) {
    if (!remote) {
        return;
    }
    if (snapPosition) {
        remote->actor.world.pos = { state.x, state.y, state.z };
        remote->actor.prevPos = remote->actor.world.pos;
        remote->actor.shape.rot = { state.rotationX, state.rotationY, state.rotationZ };
    }
    remote->actor.world.rot = { state.rotationX, state.rotationY, state.rotationZ };
    remote->actor.speedXZ = state.speed;
    remote->modelGroup = state.modelGroup;
    remote->itemAction = state.itemAction;
    remote->upperLimbRot = { state.upperLimbRot[0], state.upperLimbRot[1], state.upperLimbRot[2] };
    remote->headLimbRot = { state.headLimbRot[0], state.headLimbRot[1], state.headLimbRot[2] };
    for (size_t limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb) {
        remote->jointTable[limb] = { state.jointTable[limb][0], state.jointTable[limb][1],
                                     state.jointTable[limb][2] };
    }
    remote->jointTable[PLAYER_LIMB_MAX] = { 0, 0, 0 };
}

void NetworkRemotePlayer_Init(Actor* thisx, PlayState* play) {
    auto* remote = reinterpret_cast<NetworkRemotePlayer*>(thisx);
    remote->bodyColliderInitialized = false;
    remote->playerId = thisx->params;
    thisx->room = -1;
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 18.0f);
    SkelAnime_InitLink(play, &remote->skelAnime, gPlayerSkelHeaders,
                       reinterpret_cast<LinkAnimationHeader*>(const_cast<char*>(gPlayerAnim_link_normal_wait)), 9,
                       remote->jointTable, remote->morphTable, PLAYER_LIMB_MAX);
    SkelAnime_Init(play, &remote->bowArrowSkelAnime,
                   reinterpret_cast<SkeletonHeader*>(const_cast<char*>(gArrowSkel)),
                   reinterpret_cast<AnimationHeader*>(const_cast<char*>(gArrow2Anim)), nullptr, nullptr, 0);
    const auto found = gNetworkGame.remotes.find(remote->playerId);
    if (found == gNetworkGame.remotes.end() || !found->second.hasState) {
        Actor_Kill(thisx);
        return;
    }
    found->second.actor = remote;
    found->second.fishingLineInitialized = false;
    found->second.fishingSinkingLureInitialized = false;
    CopyPacketToRemoteActor(remote, found->second.state, true);
    Collider_InitCylinder(play, &remote->bodyCollider);
    Collider_SetCylinder(play, &remote->bodyCollider, thisx, &sRemotePlayerBodyColliderInit);
    CollisionCheck_SetInfo2(&thisx->colChkInfo, nullptr, &sRemotePlayerCollisionInfo);
    remote->bodyColliderInitialized = true;
    remote->smokeInitialPos = remote->actor.world.pos;
    remote->smokeMotionObserved = false;
    Error("Network game: spawned remote player %d in scene %d room %d", remote->playerId,
          found->second.state.sceneId, found->second.state.roomId);
}

void NetworkRemotePlayer_Destroy(Actor* thisx, PlayState* play) {
    auto* remote = reinterpret_cast<NetworkRemotePlayer*>(thisx);
    const auto found = gNetworkGame.remotes.find(remote->playerId);
    if (found != gNetworkGame.remotes.end() && found->second.actor == remote) {
        found->second.actor = nullptr;
        found->second.fishingLineInitialized = false;
        found->second.fishingSinkingLureInitialized = false;
    }
    if (remote->bodyColliderInitialized) {
        Collider_DestroyCylinder(play, &remote->bodyCollider);
    }
    // SkelAnime_InitLink uses the actor-owned joint/morph arrays, so it must
    // only unregister the skeleton resource (Player_Destroy follows the same
    // ownership rule). SkelAnime_Free would incorrectly free those arrays.
    ResourceMgr_UnregisterSkeleton(&remote->skelAnime);
    SkelAnime_Free(&remote->bowArrowSkelAnime, play);
}

void NetworkRemotePlayer_Update(Actor* thisx, PlayState* play) {
    auto* remote = reinterpret_cast<NetworkRemotePlayer*>(thisx);
    const auto found = gNetworkGame.remotes.find(remote->playerId);
    if (found == gNetworkGame.remotes.end() || !found->second.hasState) {
        Actor_Kill(thisx);
        return;
    }
    const NetworkPlayerStatePacket& state = found->second.state;
    constexpr float positionBlend = 0.45f;
    thisx->prevPos = thisx->world.pos;
    thisx->world.pos.x += (state.x - thisx->world.pos.x) * positionBlend;
    thisx->world.pos.y += (state.y - thisx->world.pos.y) * positionBlend;
    thisx->world.pos.z += (state.z - thisx->world.pos.z) * positionBlend;
    thisx->shape.rot.x += static_cast<s16>(static_cast<s16>(state.rotationX - thisx->shape.rot.x) * positionBlend);
    thisx->shape.rot.y += static_cast<s16>(static_cast<s16>(state.rotationY - thisx->shape.rot.y) * positionBlend);
    thisx->shape.rot.z += static_cast<s16>(static_cast<s16>(state.rotationZ - thisx->shape.rot.z) * positionBlend);
    thisx->world.rot = thisx->shape.rot;
    if ((state.stateFlags & (NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_DEAD)) == NETWORK_PLAYER_VISIBLE) {
        Collider_UpdateCylinder(thisx, &remote->bodyCollider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &remote->bodyCollider.base);
    }
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 55.0f;
    CopyPacketToRemoteActor(remote, state, false);
    if (gNetworkGame.autoStartTest && !remote->smokeMotionObserved) {
        const float dx = state.x - remote->smokeInitialPos.x;
        const float dy = state.y - remote->smokeInitialPos.y;
        const float dz = state.z - remote->smokeInitialPos.z;
        if (dx * dx + dy * dy + dz * dz >= 25.0f * 25.0f) {
            remote->smokeMotionObserved = true;
            Error("Network smoke test: remote player %d motion observed at %.1f %.1f %.1f", remote->playerId,
                  state.x, state.y, state.z);
        }
    }
}

void NetworkRemotePlayer_Draw(Actor* thisx, PlayState* play) {
    auto* remote = reinterpret_cast<NetworkRemotePlayer*>(thisx);
    const auto stateRecord = gNetworkGame.remotes.find(remote->playerId);
    const uint8_t fishingState = stateRecord == gNetworkGame.remotes.end() ? 0 : stateRecord->second.state.fishingState;
    const float fishingBlend = stateRecord == gNetworkGame.remotes.end() || !stateRecord->second.hasPreviousState
                                   ? 1.0f
                                   : std::clamp(static_cast<float>(NowMilliseconds() -
                                                                   stateRecord->second.lastPacketMilliseconds) /
                                                    50.0f,
                                                0.0f, 1.0f);
    const auto fishingValue = [&](float previous, float current) {
        return previous + (current - previous) * fishingBlend;
    };
    PlayerNetworkDrawData drawData = { remote->modelGroup, PLAYER_SHIELD_MIRROR, remote->itemAction, fishingState,
                                       static_cast<u8>((stateRecord != gNetworkGame.remotes.end() &&
                                                        (stateRecord->second.state.stateFlags & NETWORK_PLAYER_READY_TO_FIRE)) != 0),
                                       { 0, 0, 0 }, remote->upperLimbRot, remote->headLimbRot,
                                       stateRecord == gNetworkGame.remotes.end() ? 0.0f :
                                           fishingValue(stateRecord->second.previousState.fishingRodBendY,
                                                        stateRecord->second.state.fishingRodBendY),
                                       stateRecord == gNetworkGame.remotes.end() ? 0.0f :
                                           fishingValue(stateRecord->second.previousState.fishingRodBendX,
                                                        stateRecord->second.state.fishingRodBendX),
                                       stateRecord == gNetworkGame.remotes.end() ? 0.0f :
                                           fishingValue(stateRecord->second.previousState.fishingRodTwist,
                                                        stateRecord->second.state.fishingRodTwist),
                                        stateRecord == gNetworkGame.remotes.end() ? 0.0f :
                                            fishingValue(stateRecord->second.previousState.fishingRodCastX,
                                                        stateRecord->second.state.fishingRodCastX),
                                       stateRecord == gNetworkGame.remotes.end() ? 0.0f :
                                           stateRecord->second.state.bowStringScale,
                                       &remote->bowArrowSkelAnime };
    NETWORK_OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, reinterpret_cast<uintptr_t>(gCullBackDList));
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
    Player_DrawImpl(play, remote->skelAnime.skeleton, remote->jointTable, remote->skelAnime.dListCount, 0,
                    PLAYER_TUNIC_KOKIRI, PLAYER_BOOTS_KOKIRI, 0, Player_OverrideLimbDrawNetwork,
                    Player_PostLimbDrawNetwork,
                    &drawData);

    if (remote->itemAction == PLAYER_IA_FISHING_POLE) {
        const auto found = gNetworkGame.remotes.find(remote->playerId);
        if (found != gNetworkGame.remotes.end()) {
            RemotePlayerRecord& record = found->second;
            const NetworkPlayerStatePacket& state = found->second.state;
            const NetworkPlayerStatePacket& previous = found->second.hasPreviousState ? found->second.previousState
                                                                                       : found->second.state;
            Vec3f lure = { thisx->world.pos.x + fishingValue(previous.fishingLureOffset[0],
                                                              state.fishingLureOffset[0]),
                           thisx->world.pos.y + fishingValue(previous.fishingLureOffset[1],
                                                              state.fishingLureOffset[1]),
                           thisx->world.pos.z + fishingValue(previous.fishingLureOffset[2],
                                                              state.fishingLureOffset[2]) };
            POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 0x14);
            gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 55);
            const size_t spooled = std::min<size_t>(state.fishingLineSpooled,
                                                    NETWORK_FISHING_LINE_POINT_COUNT - 1);
            const float lineScale = fishingValue(previous.fishingLineScale, state.fishingLineScale);
            Vec3f rodTip = { thisx->world.pos.x +
                                       fishingValue(previous.fishingRodTipOffset[0], state.fishingRodTipOffset[0]),
                                   thisx->world.pos.y +
                                       fishingValue(previous.fishingRodTipOffset[1], state.fishingRodTipOffset[1]),
                                   thisx->world.pos.z +
                                       fishingValue(previous.fishingRodTipOffset[2], state.fishingRodTipOffset[2]) };
            const Vec3f lureDraw = { thisx->world.pos.x +
                                        fishingValue(previous.fishingLureDrawOffset[0],
                                                     state.fishingLureDrawOffset[0]),
                                    thisx->world.pos.y +
                                        fishingValue(previous.fishingLureDrawOffset[1],
                                                     state.fishingLureDrawOffset[1]),
                                     thisx->world.pos.z +
                                         fishingValue(previous.fishingLureDrawOffset[2],
                                                      state.fishingLureDrawOffset[2]) };
            if (!record.fishingLineInitialized) {
                const size_t activePointCount = NETWORK_FISHING_LINE_POINT_COUNT - spooled;
                for (size_t point = 0; point < NETWORK_FISHING_LINE_POINT_COUNT; ++point) {
                    if (point <= spooled || activePointCount <= 1) {
                        record.fishingLinePos[point] = rodTip;
                    } else {
                        const float amount = static_cast<float>(point - spooled) /
                                             static_cast<float>(activePointCount - 1);
                        record.fishingLinePos[point] = {
                            rodTip.x + (lure.x - rodTip.x) * amount,
                            rodTip.y + (lure.y - rodTip.y) * amount,
                            rodTip.z + (lure.z - rodTip.z) * amount,
                        };
                    }
                    record.fishingLineRot[point] = { 0.0f, 0.0f, 0.0f };
                    record.fishingLineUnk[point] = { 0.0f, 0.0f, 0.0f };
                }
                record.fishingLineInitialized = true;
            }
            const float lineGravity = fishingValue(previous.fishingLineGravity, state.fishingLineGravity);
            Fishing_UpdateNetworkLine(play, thisx, &rodTip, &lure, record.fishingLinePos,
                                      record.fishingLineRot, record.fishingLineUnk, static_cast<s16>(spooled),
                                      state.fishingLureType, lineGravity);
            const auto linePoint = [&](size_t point) { return record.fishingLinePos[point]; };
            const bool drawTautLine = state.fishingState == 4 &&
                                      (state.fishingLineHooked != 0 || state.fishingLureType != 2);
            const size_t firstSegment = drawTautLine ? 0 : spooled;
            const size_t segmentLimit = drawTautLine ? 1 : NETWORK_FISHING_LINE_POINT_COUNT - 1;
            for (size_t point = firstSegment; point < segmentLimit; ++point) {
                const bool stockLureAttachment = !drawTautLine && point == NETWORK_FISHING_LINE_POINT_COUNT - 3 &&
                                                 state.fishingLureType == 0 && state.fishingState == 3;
                const Vec3f lineStart = drawTautLine ? rodTip : linePoint(point);
                if (drawTautLine || stockLureAttachment) {
                    const Vec3f lineEnd = drawTautLine ? lure : lureDraw;
                    const float dx = lineEnd.x - lineStart.x;
                    const float dy = lineEnd.y - lineStart.y;
                    const float dz = lineEnd.z - lineStart.z;
                    const float horizontal = std::sqrt(dx * dx + dz * dz);
                    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (distance > 0.01f) {
                        Matrix_Translate(lineStart.x, lineStart.y, lineStart.z, MTXMODE_NEW);
                        Matrix_RotateY(Math_FAtan2F(dx, dz), MTXMODE_APPLY);
                        Matrix_RotateX(-Math_FAtan2F(dy, horizontal), MTXMODE_APPLY);
                        Matrix_Scale(lineScale, 1.0f, distance * 0.001f, MTXMODE_APPLY);
                        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                        gSPDisplayList(POLY_XLU_DISP++,
                                       reinterpret_cast<Gfx*>(const_cast<char*>(gFishingLineModelDL)));
                    }
                } else {
                    // Match Fishing_DrawLureAndLine exactly: the native line
                    // solver stores each segment's rotation and every ordinary
                    // segment is rendered at its fixed five-unit length.
                    Matrix_Translate(lineStart.x, lineStart.y, lineStart.z, MTXMODE_NEW);
                    Matrix_RotateY(record.fishingLineRot[point].y, MTXMODE_APPLY);
                    Matrix_RotateX(record.fishingLineRot[point].x, MTXMODE_APPLY);
                    Matrix_Scale(lineScale, 1.0f, 0.005f, MTXMODE_APPLY);
                    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPDisplayList(POLY_XLU_DISP++, reinterpret_cast<Gfx*>(const_cast<char*>(gFishingLineModelDL)));
                }
                if (stockLureAttachment) {
                    break;
                }
            }

            if (state.fishingLureType == 2) {
                static const float sinkingSizes[20] = { 1.0f, 1.5f, 1.8f, 2.0f, 1.8f, 1.6f, 1.4f, 1.2f, 1.0f, 1.0f,
                                                        0.9f, 0.85f, 0.8f, 0.7f, 0.8f, 1.0f, 1.2f, 1.1f, 1.0f, 0.8f };
                const bool sinkingLureUnderwater = state.fishingSinkingLureUnderwater != 0;
                if (!record.fishingSinkingLureInitialized) {
                    std::fill_n(record.fishingSinkingLurePos, 20, lure);
                    record.fishingSinkingLureInitialized = true;
                }
                Fishing_UpdateNetworkSinkingLure(&lure, record.fishingSinkingLurePos, thisx->shape.rot.y,
                                                 state.fishingState, state.fishingSinkingLureUnderwater);
                if (sinkingLureUnderwater) {
                    Gfx_SetupDL_25Opa(play->state.gfxCtx);
                    gSPDisplayList(POLY_OPA_DISP++, reinterpret_cast<Gfx*>(
                        const_cast<char*>(gFishingSinkingLureSegmentMaterialDL)));
                } else {
                    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
                    gSPDisplayList(POLY_XLU_DISP++, reinterpret_cast<Gfx*>(
                        const_cast<char*>(gFishingSinkingLureSegmentMaterialDL)));
                }
                for (int point = 19; point >= 0; --point) {
                    const int sizeIndex = point + state.fishingSinkingLureSegmentIndex;
                    if (sizeIndex >= 20) {
                        continue;
                    }
                    Matrix_Translate(record.fishingSinkingLurePos[point].x,
                                     record.fishingSinkingLurePos[point].y,
                                     record.fishingSinkingLurePos[point].z, MTXMODE_NEW);
                    const float scale = sinkingSizes[sizeIndex] * 0.04f;
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    Matrix_ReplaceRotation(&play->billboardMtxF);
                    if (sinkingLureUnderwater) {
                        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                        gSPDisplayList(POLY_OPA_DISP++, reinterpret_cast<Gfx*>(
                            const_cast<char*>(gFishingSinkingLureSegmentModelDL)));
                    } else {
                        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                        gSPDisplayList(POLY_XLU_DISP++, reinterpret_cast<Gfx*>(
                            const_cast<char*>(gFishingSinkingLureSegmentModelDL)));
                    }
                }
            } else {
                Matrix_Translate(lure.x, lure.y, lure.z, MTXMODE_NEW);
                Matrix_RotateY(fishingValue(previous.fishingLureRot[1] + previous.fishingLureSpin,
                                            state.fishingLureRot[1] + state.fishingLureSpin),
                               MTXMODE_APPLY);
                Matrix_RotateX(fishingValue(previous.fishingLureRot[0], state.fishingLureRot[0]), MTXMODE_APPLY);
                Matrix_Scale(0.004f, 0.004f, 0.004f, MTXMODE_APPLY);
                Matrix_Translate(0.0f, 0.0f,
                                 fishingValue(previous.fishingLureZOffset, state.fishingLureZOffset),
                                 MTXMODE_APPLY);
                Matrix_RotateZ(M_PI / 2.0f, MTXMODE_APPLY);
                Matrix_RotateY(M_PI / 2.0f, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, reinterpret_cast<Gfx*>(const_cast<char*>(gFishingLureFloatDL)));
                for (size_t hook = 0; hook < 2; ++hook) {
                    Matrix_Translate(thisx->world.pos.x +
                                         fishingValue(previous.fishingLureHookOffsets[hook][0],
                                                      state.fishingLureHookOffsets[hook][0]),
                                     thisx->world.pos.y +
                                         fishingValue(previous.fishingLureHookOffsets[hook][1],
                                                      state.fishingLureHookOffsets[hook][1]),
                                     thisx->world.pos.z +
                                         fishingValue(previous.fishingLureHookOffsets[hook][2],
                                                      state.fishingLureHookOffsets[hook][2]),
                                     MTXMODE_NEW);
                    Matrix_RotateY(fishingValue(previous.fishingLureHookRot[hook][1],
                                                state.fishingLureHookRot[hook][1]),
                                   MTXMODE_APPLY);
                    Matrix_RotateX(fishingValue(previous.fishingLureHookRot[hook][0],
                                                state.fishingLureHookRot[hook][0]),
                                   MTXMODE_APPLY);
                    Matrix_Scale(0.004f, 0.004f, 0.005f, MTXMODE_APPLY);
                    Matrix_RotateY(M_PI, MTXMODE_APPLY);
                    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPDisplayList(POLY_OPA_DISP++,
                                   reinterpret_cast<Gfx*>(const_cast<char*>(gFishingLureHookDL)));
                    Matrix_RotateZ(M_PI / 2.0f, MTXMODE_APPLY);
                    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPDisplayList(POLY_OPA_DISP++,
                                   reinterpret_cast<Gfx*>(const_cast<char*>(gFishingLureHookDL)));
                }
            }
        }
    }
    NETWORK_CLOSE_DISPS();
}

void NetworkRemoteProjectile_Init(Actor* thisx, PlayState* play) {
    auto* projectile = reinterpret_cast<NetworkRemoteProjectile*>(thisx);
    projectile->ownerPlayerId = -1;
    projectile->projectileId = -1;
    projectile->lastPhase = 0xFF;
    thisx->room = -1;
    ActorShape_Init(&thisx->shape, 0.0f, nullptr, 0.0f);
    SkelAnime_Init(play, &projectile->skelAnime,
                   reinterpret_cast<SkeletonHeader*>(const_cast<char*>(gArrowSkel)),
                   reinterpret_cast<AnimationHeader*>(const_cast<char*>(gArrow2Anim)), nullptr, nullptr, 0);
}

void NetworkRemoteProjectile_Destroy(Actor* thisx, PlayState* play) {
    auto* projectile = reinterpret_cast<NetworkRemoteProjectile*>(thisx);
    const auto key = std::make_pair(projectile->ownerPlayerId, projectile->projectileId);
    const auto found = gNetworkGame.remoteProjectiles.find(key);
    if (found != gNetworkGame.remoteProjectiles.end() && found->second.actor == projectile) {
        found->second.actor = nullptr;
    }
    SkelAnime_Free(&projectile->skelAnime, play);
}

void NetworkRemoteProjectile_Update(Actor* thisx, PlayState* play) {
    auto* projectile = reinterpret_cast<NetworkRemoteProjectile*>(thisx);
    const auto found = gNetworkGame.remoteProjectiles.find(
        std::make_pair(projectile->ownerPlayerId, projectile->projectileId));
    if (found == gNetworkGame.remoteProjectiles.end() || !found->second.state.active) {
        Actor_Kill(thisx);
        return;
    }
    RemoteProjectileRecord& record = found->second;
    const NetworkProjectileStatePacket& state = record.state;
    if (state.projectileKind == NETWORK_PROJECTILE_ARROW && state.phase == NETWORK_ARROW_STUCK &&
        projectile->lastPhase != NETWORK_ARROW_STUCK) {
        Audio_PlayActorSound2(thisx, NA_SE_IT_ARROW_STICK_CRE);
    }
    projectile->lastPhase = state.phase;
    thisx->prevPos = thisx->world.pos;
    if (state.projectileKind == NETWORK_PROJECTILE_ARROW && state.phase == NETWORK_ARROW_STUCK) {
        record.hitWorld = true;
        record.worldHitPos = { state.x, state.y, state.z };
        thisx->world.pos = record.worldHitPos;
    } else if (record.hitWorld) {
        thisx->world.pos = record.worldHitPos;
    } else {
        thisx->world.pos.x += (state.x - thisx->world.pos.x) * 0.7f;
        thisx->world.pos.y += (state.y - thisx->world.pos.y) * 0.7f;
        thisx->world.pos.z += (state.z - thisx->world.pos.z) * 0.7f;
        if (state.projectileKind == NETWORK_PROJECTILE_ARROW && state.phase == NETWORK_ARROW_FLYING) {
            CollisionPoly* hitPoly = nullptr;
            s32 bgId = BGCHECK_SCENE;
            Vec3f hitPoint{};
            if (BgCheck_ProjectileLineTest(&play->colCtx, &thisx->prevPos, &thisx->world.pos, &hitPoint,
                                           &hitPoly, true, true, true, true, &bgId)) {
                record.hitWorld = true;
                record.worldHitPos = hitPoint;
                thisx->world.pos = hitPoint;
                Audio_PlayActorSound2(thisx, NA_SE_IT_ARROW_STICK_CRE);
                if (!record.impactReported && gNetworkGame.runtime) {
                    NetworkProjectileImpactPacket impact{ state.playerId, state.projectileId, state.sceneId,
                                                          hitPoint.x, hitPoint.y, hitPoint.z };
                    record.impactReported = gNetworkGame.runtime->SendProjectileImpact(impact);
                }
            }
        }
    }
    thisx->world.rot = thisx->shape.rot = { state.rotationX, state.rotationY, state.rotationZ };
}

void NetworkRemoteProjectile_Draw(Actor* thisx, PlayState* play) {
    auto* projectile = reinterpret_cast<NetworkRemoteProjectile*>(thisx);
    const auto found = gNetworkGame.remoteProjectiles.find(
        std::make_pair(projectile->ownerPlayerId, projectile->projectileId));
    if (found == gNetworkGame.remoteProjectiles.end()) {
        return;
    }
    if (found->second.state.projectileKind == NETWORK_PROJECTILE_BOMB) {
        if (found->second.state.phase != NETWORK_BOMB_EXPLODING) {
            Gfx_DrawDListOpa(play, reinterpret_cast<Gfx*>(const_cast<char*>(gBombBodyDL)));
        }
    } else {
        SkelAnime_DrawLod(play, projectile->skelAnime.skeleton, projectile->skelAnime.jointTable, nullptr, nullptr,
                          projectile, 0);
    }
}

DynamicObjectKey DynamicObjectKeyFor(int32_t sceneId, const Actor* actor) {
    return std::make_tuple(sceneId, actor->room, actor->id, actor->params,
                           static_cast<int32_t>(std::lround(actor->home.pos.x)),
                           static_cast<int32_t>(std::lround(actor->home.pos.y)),
                           static_cast<int32_t>(std::lround(actor->home.pos.z)));
}

void SpawnOrCullRemoteActors(PlayState* play) {
    for (auto it = gNetworkGame.remotes.begin(); it != gNetworkGame.remotes.end();) {
        RemotePlayerRecord& record = it->second;
        const bool sameScene = record.state.sceneId == play->sceneNum;
        const bool sameRoom = record.state.roomId == -1 || record.state.roomId == play->roomCtx.curRoom.num;
        const bool visible = (record.state.stateFlags & NETWORK_PLAYER_VISIBLE) != 0;
        if (!sameScene || !sameRoom || !visible) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            ++it;
            continue;
        }
        if (!record.actor && gNetworkGame.remoteActorId >= 0 && it->first >= INT16_MIN && it->first <= INT16_MAX) {
            Actor_Spawn(&play->actorCtx, play, gNetworkGame.remoteActorId, record.state.x, record.state.y,
                        record.state.z, record.state.rotationX, record.state.rotationY, record.state.rotationZ,
                        static_cast<s16>(it->first));
        }
        ++it;
    }
    for (auto it = gNetworkGame.remoteProjectiles.begin(); it != gNetworkGame.remoteProjectiles.end();) {
        RemoteProjectileRecord& record = it->second;
        if (!record.state.active) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            it = gNetworkGame.remoteProjectiles.erase(it);
            continue;
        }
        if (record.state.sceneId != play->sceneNum) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            ++it;
            continue;
        }
        if (!record.actor && gNetworkGame.remoteProjectileActorId >= 0) {
            Actor* actor = Actor_Spawn(&play->actorCtx, play, gNetworkGame.remoteProjectileActorId, record.state.x,
                                       record.state.y, record.state.z, record.state.rotationX, record.state.rotationY,
                                       record.state.rotationZ, 0);
            if (actor) {
                auto* projectile = reinterpret_cast<NetworkRemoteProjectile*>(actor);
                projectile->ownerPlayerId = it->first.first;
                projectile->projectileId = it->first.second;
                record.actor = projectile;
            }
        }
        ++it;
    }
}

void ReceiveRemotePlayerStates() {
    NetworkPlayerStatePacket packet{};
    while (gNetworkGame.runtime && gNetworkGame.runtime->PollPlayerState(packet)) {
        RemotePlayerRecord& record = gNetworkGame.remotes[packet.playerId];
        if (!record.hasState || SequenceIsNewer(static_cast<uint32_t>(packet.sequence),
                                                static_cast<uint32_t>(record.state.sequence))) {
            const bool fishingPoleWasActive =
                record.hasState && record.state.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE;
            if (fishingPoleWasActive && packet.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE &&
                record.state.sceneId == packet.sceneId) {
                CopyNetworkFishingState(packet, record.state);
            }
            if (record.hasState) {
                record.previousState = record.state;
                record.hasPreviousState = true;
            } else {
                record.previousState = packet;
            }
            record.state = packet;
            if (!fishingPoleWasActive || packet.itemAction != NETWORK_PLAYER_ITEM_FISHING_POLE) {
                record.fishingLineInitialized = false;
                record.fishingSinkingLureInitialized = false;
            }
            record.lastPacketMilliseconds = NowMilliseconds();
            record.hasState = true;
        }
    }
    NetworkPlayerStatePacket fishingState{};
    while (gNetworkGame.runtime && gNetworkGame.runtime->PollFishingState(fishingState)) {
        const auto found = gNetworkGame.remotes.find(fishingState.playerId);
        if (found == gNetworkGame.remotes.end() || !found->second.hasState ||
            found->second.state.itemAction != NETWORK_PLAYER_ITEM_FISHING_POLE ||
            found->second.state.sceneId != fishingState.sceneId ||
            static_cast<int32_t>(found->second.state.sequence - fishingState.sequence) > 8) {
            continue;
        }
        found->second.previousState = found->second.state;
        found->second.hasPreviousState = true;
        CopyNetworkFishingState(found->second.state, fishingState);
        found->second.lastPacketMilliseconds = NowMilliseconds();
    }
    NetworkPlayerRemovePacket removal{};
    while (gNetworkGame.runtime && gNetworkGame.runtime->PollPlayerRemove(removal)) {
        const auto found = gNetworkGame.remotes.find(removal.playerId);
        if (found != gNetworkGame.remotes.end()) {
            if (found->second.actor && found->second.actor->actor.update) {
                Actor_Kill(&found->second.actor->actor);
            }
            gNetworkGame.remotes.erase(found);
        }
        for (auto projectile = gNetworkGame.remoteProjectiles.begin();
             projectile != gNetworkGame.remoteProjectiles.end();) {
            if (projectile->first.first == removal.playerId) {
                if (projectile->second.actor && projectile->second.actor->actor.update) {
                    Actor_Kill(&projectile->second.actor->actor);
                }
                projectile = gNetworkGame.remoteProjectiles.erase(projectile);
            } else {
                ++projectile;
            }
        }
    }
    NetworkDynamicObjectStatePacket objectState{};
    while (gNetworkGame.runtime && gNetworkGame.runtime->PollDynamicObjectState(objectState)) {
        const DynamicObjectKey key = std::make_tuple(objectState.sceneId, objectState.roomId, objectState.actorId,
                                                      objectState.actorParams, objectState.homeX, objectState.homeY,
                                                      objectState.homeZ);
        if (objectState.destroyed) {
            gNetworkGame.destroyedObjects.insert(key);
        } else {
            gNetworkGame.destroyedObjects.erase(key);
        }
    }
    NetworkActorEventPacket actorEvent{};
    while (gNetworkGame.runtime && gNetworkGame.runtime->PollActorEvent(actorEvent)) {
        const DynamicObjectKey key = std::make_tuple(actorEvent.sceneId, actorEvent.roomId, actorEvent.actorId,
                                                      actorEvent.actorParams, actorEvent.homeX, actorEvent.homeY,
                                                      actorEvent.homeZ);
        gNetworkGame.actorEvents.emplace(std::make_pair(key, actorEvent.eventType), actorEvent.sourcePlayerId);
    }
    NetworkProjectileStatePacket projectile{};
    while (gNetworkGame.runtime && gNetworkGame.runtime->PollProjectileState(projectile)) {
        RemoteProjectileRecord& record =
            gNetworkGame.remoteProjectiles[std::make_pair(projectile.playerId, projectile.projectileId)];
        record.state = projectile;
    }
}

void SendLocalProjectiles(PlayState* play, Player* player) {
    if (!gNetworkGame.runtime || !gNetworkGame.runtime->IsActive()) {
        return;
    }
    std::set<Actor*> seen;
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].head; actor; actor = actor->next) {
        if (actor->id != ACTOR_EN_ARROW || actor->params < -1 || actor->params > 8 ||
            (actor->parent != &player->actor && gNetworkGame.localProjectiles.count(actor) == 0)) {
            continue;
        }
        seen.insert(actor);
        auto [found, inserted] = gNetworkGame.localProjectiles.try_emplace(
            actor, LocalProjectileRecord{ gNetworkGame.nextProjectileId, false, false, 0xFF });
        if (inserted) {
            ++gNetworkGame.nextProjectileId;
        }
        if (actor->parent == &player->actor) {
            continue;
        }
        const bool firstLaunch = !found->second.launched;
        found->second.launched = true;
        NetworkProjectileStatePacket packet{};
        packet.projectileId = found->second.projectileId;
        packet.sceneId = play->sceneNum;
        packet.active = 1;
        packet.projectileKind = NETWORK_PROJECTILE_ARROW;
        packet.phase = NETWORK_ARROW_FLYING;
        packet.projectileType = static_cast<uint8_t>(std::max<int16_t>(0, actor->params));
        packet.x = actor->world.pos.x;
        packet.y = actor->world.pos.y;
        packet.z = actor->world.pos.z;
        // EnArrow renders with shape.rot.x, which is derived from its current
        // velocity. world.rot.x remains the original launch pitch and is not
        // the transform of the flying/embedded arrow model.
        packet.rotationX = actor->shape.rot.x;
        packet.rotationY = actor->world.rot.y;
        packet.rotationZ = actor->world.rot.z;
        EnArrow* arrow = reinterpret_cast<EnArrow*>(actor);
        if (arrow->touchedPoly) {
            if (!found->second.retired) {
                packet.phase = NETWORK_ARROW_STUCK;
                found->second.retired = true;
                gNetworkGame.runtime->SendProjectileState(packet, true);
            }
            continue;
        }
        if (found->second.retired) {
            continue;
        }
        gNetworkGame.runtime->SendProjectileState(packet, firstLaunch);
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head; actor; actor = actor->next) {
        if (actor->id != ACTOR_EN_BOM || actor->params != 0) {
            continue;
        }
        seen.insert(actor);
        auto [found, inserted] = gNetworkGame.localProjectiles.try_emplace(
            actor, LocalProjectileRecord{ gNetworkGame.nextProjectileId, true, false, 0xFF });
        if (inserted) {
            ++gNetworkGame.nextProjectileId;
        }
        NetworkProjectileStatePacket packet{};
        packet.projectileId = found->second.projectileId;
        packet.sceneId = play->sceneNum;
        packet.active = 1;
        packet.projectileKind = NETWORK_PROJECTILE_BOMB;
        packet.phase = actor->parent == &player->actor ? NETWORK_BOMB_HELD : NETWORK_BOMB_RELEASED;
        packet.x = actor->world.pos.x;
        packet.y = actor->world.pos.y;
        packet.z = actor->world.pos.z;
        packet.rotationY = actor->world.rot.y;
        const float yaw = actor->world.rot.y * (3.14159265358979323846f / 32768.0f);
        packet.velocityX = std::sin(yaw) * actor->speedXZ * 20.0f;
        packet.velocityY = actor->velocity.y * 20.0f;
        packet.velocityZ = std::cos(yaw) * actor->speedXZ * 20.0f;
        const bool lifecycleTransition = inserted || found->second.lastPhase != packet.phase;
        found->second.lastPhase = packet.phase;
        gNetworkGame.runtime->SendProjectileState(packet, lifecycleTransition);
    }
    for (auto it = gNetworkGame.localProjectiles.begin(); it != gNetworkGame.localProjectiles.end();) {
        if (seen.count(it->first) != 0) {
            ++it;
            continue;
        }
        if (it->second.launched && !it->second.retired) {
            NetworkProjectileStatePacket packet{};
            packet.projectileId = it->second.projectileId;
            packet.sceneId = play->sceneNum;
            packet.active = 0;
            gNetworkGame.runtime->SendProjectileState(packet, true);
        }
        it = gNetworkGame.localProjectiles.erase(it);
    }
}

void SendLocalPlayerState(PlayState* play) {
    if (!gNetworkGame.runtime || !gNetworkGame.runtime->IsActive()) {
        return;
    }
    Player* player = GET_PLAYER(play);
    if (!player) {
        return;
    }
    NetworkPlayerStatePacket packet{};
    packet.sceneId = play->sceneNum;
    packet.roomId = play->roomCtx.curRoom.num;
    packet.sequence = static_cast<int32_t>(gNetworkGame.nextSequence++);
    packet.x = player->actor.world.pos.x;
    packet.y = player->actor.world.pos.y;
    packet.z = player->actor.world.pos.z;
    packet.rotationX = player->actor.shape.rot.x;
    packet.rotationY = player->actor.shape.rot.y;
    packet.rotationZ = player->actor.shape.rot.z;
    packet.aimPitch = player->actor.focus.rot.x;
    packet.aimYaw = player->actor.focus.rot.y;
    packet.speed = player->actor.speedXZ;
    packet.stateFlags = (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) ? 0 : NETWORK_PLAYER_VISIBLE;
    if ((player->actor.bgCheckFlags & 1) != 0) {
        packet.stateFlags |= NETWORK_PLAYER_GROUNDED;
    }
    if ((player->actor.bgCheckFlags & 0x20) != 0) {
        packet.stateFlags |= NETWORK_PLAYER_SWIMMING;
    }
    if (((player->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) != 0) ||
        ((player->heldItemAction == PLAYER_IA_BOW) && (player->heldActor != nullptr))) {
        packet.stateFlags |= NETWORK_PLAYER_READY_TO_FIRE;
    }
    if (gNetworkGame.suppressDeathDuringRespawn && play->transitionTrigger == TRANS_TRIGGER_OFF &&
        play->gameplayFrames > 1 && play->gameOverCtx.state == GAMEOVER_INACTIVE && gSaveContext.health > 0) {
        gNetworkGame.suppressDeathDuringRespawn = false;
    }
    if (gSaveContext.health == 0 && !gNetworkGame.suppressDeathDuringRespawn) {
        packet.stateFlags |= NETWORK_PLAYER_DEAD;
    }
    packet.modelGroup = player->modelGroup;
    // Native fishing drives the pole from heldItemAction while itemAction may
    // temporarily return to the generic action during casting/reeling. Keep
    // network classification aligned with the system that owns the visuals.
    const int networkItemAction = player->heldItemAction == PLAYER_IA_FISHING_POLE
                                      ? PLAYER_IA_FISHING_POLE
                                      : player->itemAction;
    packet.itemAction = static_cast<uint8_t>(std::max(0, networkItemAction));
    packet.meleeWeaponState = player->meleeWeaponState;
    packet.bowStringScale = (player->itemAction >= PLAYER_IA_BOW && player->itemAction <= PLAYER_IA_BOW_0E)
                                ? std::clamp(player->unk_858, 0.0f, 1.0f)
                                : 0.0f;
    packet.upperLimbRot[0] = player->upperLimbRot.x;
    packet.upperLimbRot[1] = player->upperLimbRot.y;
    packet.upperLimbRot[2] = player->upperLimbRot.z;
    packet.headLimbRot[0] = player->headLimbRot.x;
    packet.headLimbRot[1] = player->headLimbRot.y;
    packet.headLimbRot[2] = player->headLimbRot.z;
    packet.meleeBase[0] = player->meleeWeaponInfo[0].base.x;
    packet.meleeBase[1] = player->meleeWeaponInfo[0].base.y;
    packet.meleeBase[2] = player->meleeWeaponInfo[0].base.z;
    packet.meleeTip[0] = player->meleeWeaponInfo[0].tip.x;
    packet.meleeTip[1] = player->meleeWeaponInfo[0].tip.y;
    packet.meleeTip[2] = player->meleeWeaponInfo[0].tip.z;
    Vec3f rodTipOffset{};
    Vec3f lureOffset{};
    Vec3f lureDrawOffset{};
    Vec3f lureRot{};
    Vec3f lureHookOffsets[2]{};
    Vec3f lureHookRot[2]{};
    Vec3f fishOffset{};
    Vec3s fishRot{};
    s16 fishLimbRot[8]{};
    if (Fishing_GetNetworkVisualState(play, &packet.fishingState, &rodTipOffset, &lureOffset, &lureDrawOffset,
                                      &packet.fishingRodBendY, &packet.fishingRodBendX,
                                       &packet.fishingRodTwist, &packet.fishingRodCastX, &lureRot,
                                       &packet.fishingLureSpin, &packet.fishingLureZOffset,
                                      lureHookOffsets, lureHookRot, &packet.fishingLineScale,
                                      &packet.fishingLineGravity,
                                      &packet.fishingLureType,
                                      &packet.fishingLineSpooled, &packet.fishingLineHooked,
                                      &packet.fishingFishActive, &packet.fishingFishIsLoach, &fishOffset,
                                      &fishRot, fishLimbRot, &packet.fishingFishLength,
                                      &packet.fishingFishRoomId, &packet.fishingFishActorParams,
                                      &packet.fishingFishHomeX, &packet.fishingFishHomeY,
                                      &packet.fishingFishHomeZ,
                                       &packet.fishingSinkingLureSegmentIndex,
                                       &packet.fishingSinkingLureUnderwater)) {
        packet.fishingRodTipOffset[0] = rodTipOffset.x;
        packet.fishingRodTipOffset[1] = rodTipOffset.y;
        packet.fishingRodTipOffset[2] = rodTipOffset.z;
        packet.fishingLureOffset[0] = lureOffset.x;
        packet.fishingLureOffset[1] = lureOffset.y;
        packet.fishingLureOffset[2] = lureOffset.z;
        packet.fishingLureDrawOffset[0] = lureDrawOffset.x;
        packet.fishingLureDrawOffset[1] = lureDrawOffset.y;
        packet.fishingLureDrawOffset[2] = lureDrawOffset.z;
        packet.fishingLureRot[0] = lureRot.x;
        packet.fishingLureRot[1] = lureRot.y;
        packet.fishingLureRot[2] = lureRot.z;
        for (size_t hook = 0; hook < 2; ++hook) {
            packet.fishingLureHookOffsets[hook][0] = lureHookOffsets[hook].x;
            packet.fishingLureHookOffsets[hook][1] = lureHookOffsets[hook].y;
            packet.fishingLureHookOffsets[hook][2] = lureHookOffsets[hook].z;
            packet.fishingLureHookRot[hook][0] = lureHookRot[hook].x;
            packet.fishingLureHookRot[hook][1] = lureHookRot[hook].y;
        }
        packet.fishingFishOffset[0] = fishOffset.x;
        packet.fishingFishOffset[1] = fishOffset.y;
        packet.fishingFishOffset[2] = fishOffset.z;
        packet.fishingFishRot[0] = fishRot.x;
        packet.fishingFishRot[1] = fishRot.y;
        packet.fishingFishRot[2] = fishRot.z;
        for (size_t limbRot = 0; limbRot < 8; ++limbRot) {
            packet.fishingFishLimbRot[limbRot] = fishLimbRot[limbRot];
        }
    }
    Vec3s pcBowJointTable[PLAYER_LIMB_BUF_COUNT]{};
    const bool pcBowAim = Player_BuildPCBowJointTable(player, pcBowJointTable, false) != 0;
    for (size_t limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb) {
        // The gameplay animation context merges upperSkelAnime into the main
        // table asynchronously. Snapshot the bow upper limbs from their source
        // so remote clients never receive alternating idle/aimed frames.
        const Vec3s& joint = pcBowAim ? pcBowJointTable[limb] : player->skelAnime.jointTable[limb];
        packet.jointTable[limb][0] = joint.x;
        packet.jointTable[limb][1] = joint.y;
        packet.jointTable[limb][2] = joint.z;
    }
    gNetworkGame.runtime->SendPlayerState(packet);
    SendLocalProjectiles(play, player);
}

void ProcessPlayerRespawns(PlayState* play) {
    if (!play || !gNetworkGame.runtime) {
        return;
    }
    NetworkPlayerRespawnPacket respawn{};
    while (gNetworkGame.runtime->PollPlayerRespawn(respawn)) {
        if (respawn.playerId != gNetworkGame.runtime->LocalPlayerId()) {
            continue;
        }
        gSaveContext.healthCapacity = STARTING_HEALTH;
        gSaveContext.health = STARTING_HEALTH;
        gSaveContext.healthAccumulator = 0;
        play->gameOverCtx.state = GAMEOVER_INACTIVE;
        gNetworkGame.suppressDeathDuringRespawn = true;
        Play_TriggerRespawn(play);
        gSaveContext.respawnFlag = -2;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
        play->gameplayFrames = 0;
        Error("Network game: respawning local player %d with three hearts",
              gNetworkGame.runtime->LocalPlayerId());
    }
}

void QueueRemotePlayerNames(PlayState* play) {
    if (!play || !gNetworkGame.runtime) {
        return;
    }

    for (const auto& player : gNetworkGame.runtime->Players()) {
        if (player.playerId <= 0 || player.name.empty()) {
            continue;
        }
        const auto record = gNetworkGame.remotes.find(player.playerId);
        if (record == gNetworkGame.remotes.end() || !record->second.actor || !record->second.hasState ||
            (record->second.state.stateFlags & NETWORK_PLAYER_VISIBLE) == 0 ||
            (record->second.state.stateFlags & NETWORK_PLAYER_DEAD) != 0) {
            continue;
        }

        Vec3f worldPosition = record->second.actor->actor.world.pos;
        worldPosition.y += 78.0f;
        Vec3f obstruction{};
        CollisionPoly* obstructionPoly = nullptr;
        int32_t obstructionBgId = BGCHECK_SCENE;
        Vec3f cameraPosition = play->view.eye;
        if (BgCheck_AnyLineTest3(&play->colCtx, &cameraPosition, &worldPosition, &obstruction,
                                 &obstructionPoly, true, true, true, true, &obstructionBgId)) {
            continue;
        }
        Vec3f clipPosition{};
        float clipW = 0.0f;
        SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &worldPosition, &clipPosition, &clipW);
        if (clipW <= 0.01f) {
            continue;
        }
        const float ndcX = clipPosition.x / clipW;
        const float ndcY = clipPosition.y / clipW;
        if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) {
            continue;
        }

        const float screenX = (ndcX + 1.0f) * (SCREEN_WIDTH * 0.5f);
        const float screenY = (1.0f - ndcY) * (SCREEN_HEIGHT * 0.5f);
        Ship::PathEngineOverlay::QueueCenteredGameText(player.name.c_str(), screenX, screenY,
                                                       0.92f, 0.96f, 1.0f, 0.95f);
    }
}

} // namespace

extern "C" PlayState* gPlayState;

void NetworkGame_PumpMoveLoop() {
    if (!gNetworkGame.runtime) {
        return;
    }
    gNetworkGame.runtime->Update();
    ReceiveRemotePlayerStates();
    if (gPlayState) {
        SendLocalPlayerState(gPlayState);
    }
}

void NetworkGame_RegisterActors() {
    if (!ActorDB::Instance || gNetworkGame.remoteActorId >= 0) {
        return;
    }
    ActorDBInit init;
    init.name = kRemotePlayerActorName;
    init.desc = "Synchronized remote adult Link";
    init.category = ACTORCAT_NPC;
    init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED |
                 ACTOR_FLAG_LOCK_ON_DISABLED | ACTOR_FLAG_IGNORE_QUAKE;
    init.objectId = OBJECT_LINK_BOY;
    init.instanceSize = sizeof(NetworkRemotePlayer);
    init.init = NetworkRemotePlayer_Init;
    init.destroy = NetworkRemotePlayer_Destroy;
    init.update = NetworkRemotePlayer_Update;
    init.draw = NetworkRemotePlayer_Draw;
    gNetworkGame.remoteActorId = static_cast<int16_t>(ActorDB::Instance->AddEntry(init).entry.id);

    ActorDBInit projectileInit;
    projectileInit.name = "NETWORK_REMOTE_ARROW";
    projectileInit.desc = "Non-gameplay synchronized remote arrow";
    projectileInit.category = ACTORCAT_ITEMACTION;
    projectileInit.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED |
                           ACTOR_FLAG_LOCK_ON_DISABLED | ACTOR_FLAG_IGNORE_QUAKE;
    projectileInit.objectId = OBJECT_GAMEPLAY_KEEP;
    projectileInit.instanceSize = sizeof(NetworkRemoteProjectile);
    projectileInit.init = NetworkRemoteProjectile_Init;
    projectileInit.destroy = NetworkRemoteProjectile_Destroy;
    projectileInit.update = NetworkRemoteProjectile_Update;
    projectileInit.draw = NetworkRemoteProjectile_Draw;
    gNetworkGame.remoteProjectileActorId = static_cast<int16_t>(ActorDB::Instance->AddEntry(projectileInit).entry.id);
}

extern "C" void NetworkGame_Initialize(int argc, char* argv[]) {
    if (gNetworkGame.runtime) {
        return;
    }
    gNetworkGame.runtime = std::make_unique<ShipwrightNetworkRuntime>();
    gNetworkGame.multiplayerUI = std::make_unique<PathEngineMultiplayerUI>();
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && std::string(argv[i]) == "--network-smoke-test") {
            gNetworkGame.autoStartTest = true;
        }
    }
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i] ? argv[i] : "";
        if (argument == "--host") {
            uint16_t port = DEFAULT_NETWORK_PORT;
            if (i + 1 < argc && argv[i + 1] && argv[i + 1][0] != '-') {
                const long parsed = std::strtol(argv[++i], nullptr, 10);
                if (parsed > 0 && parsed <= 49151) {
                    port = static_cast<uint16_t>(parsed);
                }
            }
            if (!gNetworkGame.runtime->Host(port)) {
                Error("Game network: failed to host on port %u", static_cast<unsigned>(port));
            } else {
                Error("Game network: hosting secure session on port %u", static_cast<unsigned>(port));
            }
            break;
        }
        if ((argument == "--join" || argument == "--connect") && i + 1 < argc && argv[i + 1]) {
            const std::string address = argv[++i];
            if (!gNetworkGame.runtime->Connect(address)) {
                Error("Game network: failed to connect to %s", address.c_str());
            } else {
                Error("Game network: connecting securely to %s", address.c_str());
            }
            break;
        }
    }
    gNetworkGame.lastFrameMilliseconds = NowMilliseconds();
    Ship::PathEngineOverlay::SetMoveLoopCallback(NetworkGame_PumpMoveLoop);
}

extern "C" int NetworkGame_ShouldAutoStartTest(void) {
    return gNetworkGame.autoStartTest ? 1 : 0;
}

extern "C" void NetworkGame_Shutdown(void) {
    Ship::PathEngineOverlay::SetMoveLoopCallback(nullptr);
    Ship::PathEngineOverlay::SetNetworkTelemetry(false, 0, 0, 0);
    if (gNetworkGame.multiplayerUI) {
        gNetworkGame.multiplayerUI->Shutdown();
        gNetworkGame.multiplayerUI.reset();
    }
    if (gNetworkGame.runtime) {
        gNetworkGame.runtime->Disconnect();
        gNetworkGame.runtime.reset();
    }
    // The game-state actor context has already been destroyed when global
    // shutdown reaches this bridge. Never dereference actor pointers here;
    // scene teardown owns them and their destroy callbacks clear live links.
    gNetworkGame.remotes.clear();
    gNetworkGame.remoteProjectiles.clear();
    gNetworkGame.localProjectiles.clear();
    gNetworkGame.destroyedObjects.clear();
    gNetworkGame.actorEvents.clear();
}

extern "C" void NetworkGame_UpdateTransport(void) {
    if (!gNetworkGame.runtime) {
        return;
    }
    gNetworkGame.runtime->Update();
    Ship::PathEngineOverlay::SetNetworkTelemetry(
        gNetworkGame.runtime->IsActive(), gNetworkGame.runtime->LatencyMilliseconds(),
        gNetworkGame.runtime->InboundBytesPerSecond(), gNetworkGame.runtime->OutboundBytesPerSecond());
    if (gNetworkGame.multiplayerUI) {
        gNetworkGame.multiplayerUI->Update(*gNetworkGame.runtime);
    }
    QueueRemotePlayerNames(gPlayState);
    // Drain high-rate pose packets even outside PlayState. Keeping only the
    // newest packet per player prevents file select and scene loads from
    // accumulating an unbounded pose backlog.
    ReceiveRemotePlayerStates();
    // A dead PlayState intentionally freezes before the vanilla game-over
    // menu. Process the reliable server command here because this frame-level
    // transport hook continues while gameplay simulation is frozen.
    ProcessPlayerRespawns(gPlayState);
}

extern "C" void NetworkGame_ShowNotification(const char* text) {
    if (gNetworkGame.multiplayerUI) {
        gNetworkGame.multiplayerUI->ShowNotification(text);
    }
}

extern "C" void NetworkGame_ClearNotification(void) {
    if (gNetworkGame.multiplayerUI) {
        gNetworkGame.multiplayerUI->ClearNotification();
    }
}

extern "C" void NetworkGame_Update(PlayState* play) {
    if (!play || !gNetworkGame.runtime) {
        return;
    }
    const uint64_t now = NowMilliseconds();
    if (gNetworkGame.autoStartTest && !gNetworkGame.smokeSpawnAdjusted &&
        gNetworkGame.runtime->LocalPlayerId() > 0) {
        Player* player = GET_PLAYER(play);
        if (player) {
            const float offset = (gNetworkGame.runtime->LocalPlayerId() & 1) != 0 ? -80.0f : 80.0f;
            player->actor.world.pos.x += offset;
            player->actor.prevPos = player->actor.world.pos;
            gNetworkGame.smokeSpawnAdjusted = true;
            Error("Network smoke test: positioned local player %d at %.1f %.1f %.1f",
                  gNetworkGame.runtime->LocalPlayerId(), player->actor.world.pos.x, player->actor.world.pos.y,
                  player->actor.world.pos.z);
        }
    }
    if (gNetworkGame.autoStartTest && gNetworkGame.smokeSpawnAdjusted && gNetworkGame.smokeMotionFrames < 60) {
        Player* player = GET_PLAYER(play);
        if (player) {
            const float direction = (gNetworkGame.runtime->LocalPlayerId() & 1) != 0 ? 1.0f : -1.0f;
            player->actor.world.pos.z += direction;
            ++gNetworkGame.smokeMotionFrames;
        }
    }
    if (gNetworkGame.autoStartTest && gNetworkGame.smokeMotionFrames >= 60 &&
        !gNetworkGame.smokeDeathTriggered && ++gNetworkGame.smokePostMotionFrames >= 60) {
        // Exercise the real player death/game-over state and server deadline
        // after both clients have had time to observe remote motion.
        gNetworkGame.smokeDeathTriggered = true;
        gSaveContext.health = 0;
        Error("Network smoke test: triggered local player %d death",
              gNetworkGame.runtime->LocalPlayerId());
    }
    NetworkPlayerDamagePacket damage{};
    while (gNetworkGame.runtime->PollPlayerDamage(damage)) {
        Player* player = GET_PLAYER(play);
        if (player && player->invincibilityTimer == 0) {
            player->actor.colChkInfo.damage = static_cast<u8>(std::clamp<short>(damage.damage, 0, UINT8_MAX));
            player->actor.colChkInfo.acHitEffect = 0;
            // Match the native cylinder-hit path before entering Link's normal
            // damage response: clear/apply the standard hit effect and voice,
            // then let the stock action, knockback, animation and i-frames run.
            func_80838280(player);
            func_80837C0C(play, player, PLAYER_HIT_RESPONSE_NONE, 4.0f, 5.0f, damage.impactYaw, 20);
        }
    }
    ProcessPlayerRespawns(play);
    gNetworkGame.lastFrameMilliseconds = now;
    SpawnOrCullRemoteActors(play);
    SendLocalPlayerState(play);
}

extern "C" int NetworkGame_IsObjectDestroyed(PlayState* play, Actor* actor) {
    return play && actor && gNetworkGame.destroyedObjects.count(DynamicObjectKeyFor(play->sceneNum, actor)) != 0;
}

extern "C" void NetworkGame_NotifyActorEvent(PlayState* play, Actor* actor, unsigned char eventType) {
    if (!play || !actor || !gNetworkGame.runtime || !gNetworkGame.runtime->IsActive()) {
        return;
    }
    const DynamicObjectKey key = DynamicObjectKeyFor(play->sceneNum, actor);
    NetworkActorEventPacket packet{};
    packet.eventId = gNetworkGame.nextActorEventId++;
    packet.sceneId = std::get<0>(key);
    packet.roomId = std::get<1>(key);
    packet.actorId = std::get<2>(key);
    packet.actorParams = std::get<3>(key);
    packet.homeX = std::get<4>(key);
    packet.homeY = std::get<5>(key);
    packet.homeZ = std::get<6>(key);
    packet.x = actor->world.pos.x;
    packet.y = actor->world.pos.y;
    packet.z = actor->world.pos.z;
    packet.eventType = eventType;
    if (eventType == NETWORK_GAME_ACTOR_EVENT_GRASS_CUT ||
        eventType == NETWORK_GAME_ACTOR_EVENT_GRASS_THROWN_BREAK ||
        eventType == NETWORK_GAME_ACTOR_EVENT_BOULDER_BREAK) {
        gNetworkGame.destroyedObjects.insert(key);
    }
    gNetworkGame.runtime->SendActorEvent(packet);
}

extern "C" int NetworkGame_ConsumeActorEvent(PlayState* play, Actor* actor, unsigned char eventType) {
    return NetworkGame_ConsumeActorEventSource(play, actor, eventType, nullptr);
}

extern "C" int NetworkGame_ConsumeActorEventSource(PlayState* play, Actor* actor, unsigned char eventType,
                                                      int* sourcePlayerId) {
    if (!play || !actor) {
        return false;
    }
    const auto event = std::make_pair(DynamicObjectKeyFor(play->sceneNum, actor), eventType);
    const auto found = gNetworkGame.actorEvents.find(event);
    if (found == gNetworkGame.actorEvents.end()) {
        return false;
    }
    if (sourcePlayerId) {
        *sourcePlayerId = found->second;
    }
    gNetworkGame.actorEvents.erase(found);
    return true;
}

extern "C" int NetworkGame_FindRemoteFishingFishOwner(PlayState* play, Actor* actor) {
    if (!play || !actor || actor->id != ACTOR_FISHING) {
        return -1;
    }
    const DynamicObjectKey key = DynamicObjectKeyFor(play->sceneNum, actor);
    for (const auto& [playerId, remote] : gNetworkGame.remotes) {
        if (!remote.hasState || !remote.state.fishingFishActive || remote.state.sceneId != play->sceneNum) {
            continue;
        }
        if (remote.state.fishingFishRoomId == std::get<1>(key) &&
            remote.state.fishingFishActorParams == std::get<3>(key) &&
            remote.state.fishingFishHomeX == std::get<4>(key) &&
            remote.state.fishingFishHomeY == std::get<5>(key) &&
            remote.state.fishingFishHomeZ == std::get<6>(key)) {
            return playerId;
        }
    }
    return -1;
}

extern "C" int NetworkGame_GetRemoteFishingFishState(int playerId, float* x, float* y, float* z,
                                                        short* rotationX, short* rotationY, short* rotationZ,
                                                        short limbRot[8], float* length, unsigned char* isLoach) {
    const auto found = gNetworkGame.remotes.find(playerId);
    if (found == gNetworkGame.remotes.end() || !found->second.hasState ||
        !found->second.state.fishingFishActive || !x || !y || !z || !rotationX || !rotationY ||
        !rotationZ || !limbRot || !length || !isLoach) {
        return false;
    }
    const NetworkPlayerStatePacket& state = found->second.state;
    const NetworkPlayerStatePacket& previous = found->second.hasPreviousState ? found->second.previousState
                                                                               : found->second.state;
    const float blend = found->second.hasPreviousState
                            ? std::clamp(static_cast<float>(NowMilliseconds() - found->second.lastPacketMilliseconds) /
                                             50.0f,
                                         0.0f, 1.0f)
                            : 1.0f;
    const auto blendFloat = [blend](float from, float to) { return from + (to - from) * blend; };
    const auto blendAngle = [blend](short from, short to) {
        return static_cast<short>(from + static_cast<short>(to - from) * blend);
    };
    const Vec3f base = found->second.actor ? found->second.actor->actor.world.pos
                                           : Vec3f{ state.x, state.y, state.z };
    *x = base.x + blendFloat(previous.fishingFishOffset[0], state.fishingFishOffset[0]);
    *y = base.y + blendFloat(previous.fishingFishOffset[1], state.fishingFishOffset[1]);
    *z = base.z + blendFloat(previous.fishingFishOffset[2], state.fishingFishOffset[2]);
    *rotationX = blendAngle(previous.fishingFishRot[0], state.fishingFishRot[0]);
    *rotationY = blendAngle(previous.fishingFishRot[1], state.fishingFishRot[1]);
    *rotationZ = blendAngle(previous.fishingFishRot[2], state.fishingFishRot[2]);
    for (size_t index = 0; index < 8; ++index) {
        limbRot[index] = blendAngle(previous.fishingFishLimbRot[index], state.fishingFishLimbRot[index]);
    }
    *length = blendFloat(previous.fishingFishLength, state.fishingFishLength);
    *isLoach = state.fishingFishIsLoach;
    return true;
}
