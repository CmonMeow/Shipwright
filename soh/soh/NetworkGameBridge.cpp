#include "NetworkGameBridge.h"

#include "ActorDB.h"
#include "Network/ShipwrightNetworkRuntime.h"
#include "ship/window/PathEngineOverlay.h"
#include "PathEngineMultiplayerUI.h"
#include "frame_interpolation.h"

#include "assets/objects/gameplay_keep/gameplay_keep.h"
#include "assets/objects/object_fish/object_fish.h"
#include "global.h"
#include "variables.h"

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
    SkelAnime skelAnime;
    Vec3s jointTable[PLAYER_LIMB_BUF_COUNT];
    Vec3s morphTable[PLAYER_LIMB_BUF_COUNT];
    int32_t playerId;
    uint8_t modelGroup;
    uint8_t itemAction;
    Vec3s upperLimbRot;
    Vec3s headLimbRot;
};

struct RemotePlayerRecord {
    NetworkPlayerStatePacket state{};
    NetworkRemotePlayer* actor = nullptr;
    uint64_t lastPacketMilliseconds = 0;
    bool hasState = false;
};

struct NetworkRemoteProjectile {
    Actor actor;
    int32_t ownerPlayerId;
    int32_t projectileId;
    uint8_t lastPhase;
};

struct RemoteProjectileRecord {
    NetworkProjectileStatePacket state{};
    NetworkRemoteProjectile* actor = nullptr;
};

struct LocalProjectileRecord {
    int32_t projectileId = 0;
    bool launched = false;
};

struct NetworkGameState {
    std::unique_ptr<ShipwrightNetworkRuntime> runtime;
    std::unique_ptr<PathEngineMultiplayerUI> multiplayerUI;
    std::map<int32_t, RemotePlayerRecord> remotes;
    std::map<std::pair<int32_t, int32_t>, RemoteProjectileRecord> remoteProjectiles;
    std::map<Actor*, LocalProjectileRecord> localProjectiles;
    std::set<DynamicObjectKey> destroyedObjects;
    std::set<DynamicObjectKey> activatedObjects;
    uint32_t nextSequence = 1;
    uint64_t lastFrameMilliseconds = 0;
    int16_t remoteActorId = -1;
    int16_t remoteProjectileActorId = -1;
    int32_t nextProjectileId = 1;
};

NetworkGameState gNetworkGame;

extern "C" s32 Fishing_GetNetworkVisualState(PlayState* play, u8* castState, Vec3f* rodTipOffset,
                                               Vec3f* lureOffset, Vec3f lineOffsets[12]);

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
    remote->playerId = thisx->params;
    thisx->room = -1;
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 18.0f);
    SkelAnime_InitLink(play, &remote->skelAnime, gPlayerSkelHeaders,
                       reinterpret_cast<LinkAnimationHeader*>(const_cast<char*>(gPlayerAnim_link_normal_wait)), 9,
                       remote->jointTable, remote->morphTable, PLAYER_LIMB_MAX);
    const auto found = gNetworkGame.remotes.find(remote->playerId);
    if (found == gNetworkGame.remotes.end() || !found->second.hasState) {
        Actor_Kill(thisx);
        return;
    }
    found->second.actor = remote;
    CopyPacketToRemoteActor(remote, found->second.state, true);
}

void NetworkRemotePlayer_Destroy(Actor* thisx, PlayState*) {
    auto* remote = reinterpret_cast<NetworkRemotePlayer*>(thisx);
    const auto found = gNetworkGame.remotes.find(remote->playerId);
    if (found != gNetworkGame.remotes.end() && found->second.actor == remote) {
        found->second.actor = nullptr;
    }
}

void NetworkRemotePlayer_Update(Actor* thisx, PlayState*) {
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
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 55.0f;
    CopyPacketToRemoteActor(remote, state, false);
}

void NetworkRemotePlayer_Draw(Actor* thisx, PlayState* play) {
    auto* remote = reinterpret_cast<NetworkRemotePlayer*>(thisx);
    const auto stateRecord = gNetworkGame.remotes.find(remote->playerId);
    const uint8_t fishingState = stateRecord == gNetworkGame.remotes.end() ? 0 : stateRecord->second.state.fishingState;
    PlayerNetworkDrawData drawData = { remote->modelGroup, PLAYER_SHIELD_MIRROR, remote->itemAction, fishingState,
                                       remote->upperLimbRot, remote->headLimbRot };
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
            const NetworkPlayerStatePacket& state = found->second.state;
            Vec3f tip = { thisx->world.pos.x + state.fishingRodTipOffset[0],
                          thisx->world.pos.y + state.fishingRodTipOffset[1],
                          thisx->world.pos.z + state.fishingRodTipOffset[2] };
            Vec3f lure = { thisx->world.pos.x + state.fishingLureOffset[0],
                           thisx->world.pos.y + state.fishingLureOffset[1],
                           thisx->world.pos.z + state.fishingLureOffset[2] };
            POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 0x14);
            gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 80);
            Vec3f lineStart = tip;
            for (size_t point = 0; point < NETWORK_FISHING_LINE_POINT_COUNT; ++point) {
                Vec3f lineEnd = { thisx->world.pos.x + state.fishingLineOffsets[point][0],
                                  thisx->world.pos.y + state.fishingLineOffsets[point][1],
                                  thisx->world.pos.z + state.fishingLineOffsets[point][2] };
                const float dx = lineEnd.x - lineStart.x;
                const float dy = lineEnd.y - lineStart.y;
                const float dz = lineEnd.z - lineStart.z;
                const float horizontal = std::sqrt(dx * dx + dz * dz);
                const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (distance > 0.01f) {
                    Matrix_Translate(lineStart.x, lineStart.y, lineStart.z, MTXMODE_NEW);
                    Matrix_RotateY(Math_FAtan2F(dx, dz), MTXMODE_APPLY);
                    Matrix_RotateX(-Math_FAtan2F(dy, horizontal), MTXMODE_APPLY);
                    Matrix_Scale(0.0005f, 1.0f, distance * 0.001f, MTXMODE_APPLY);
                    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPDisplayList(POLY_XLU_DISP++, reinterpret_cast<Gfx*>(const_cast<char*>(gFishingLineModelDL)));
                }
                lineStart = lineEnd;
            }

            Matrix_Translate(lure.x, lure.y, lure.z, MTXMODE_NEW);
            Matrix_Scale(0.004f, 0.004f, 0.004f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, reinterpret_cast<Gfx*>(const_cast<char*>(gFishingLureFloatDL)));
        }
    }
    NETWORK_CLOSE_DISPS();
}

void NetworkRemoteProjectile_Init(Actor* thisx, PlayState*) {
    auto* projectile = reinterpret_cast<NetworkRemoteProjectile*>(thisx);
    projectile->ownerPlayerId = -1;
    projectile->projectileId = -1;
    projectile->lastPhase = 0xFF;
    thisx->room = -1;
    ActorShape_Init(&thisx->shape, 0.0f, nullptr, 0.0f);
}

void NetworkRemoteProjectile_Destroy(Actor* thisx, PlayState*) {
    auto* projectile = reinterpret_cast<NetworkRemoteProjectile*>(thisx);
    const auto key = std::make_pair(projectile->ownerPlayerId, projectile->projectileId);
    const auto found = gNetworkGame.remoteProjectiles.find(key);
    if (found != gNetworkGame.remoteProjectiles.end() && found->second.actor == projectile) {
        found->second.actor = nullptr;
    }
}

void NetworkRemoteProjectile_Update(Actor* thisx, PlayState* play) {
    auto* projectile = reinterpret_cast<NetworkRemoteProjectile*>(thisx);
    const auto found = gNetworkGame.remoteProjectiles.find(
        std::make_pair(projectile->ownerPlayerId, projectile->projectileId));
    if (found == gNetworkGame.remoteProjectiles.end() || !found->second.state.active) {
        Actor_Kill(thisx);
        return;
    }
    const NetworkProjectileStatePacket& state = found->second.state;
    if (state.projectileKind == NETWORK_PROJECTILE_BOMB && state.phase == NETWORK_BOMB_EXPLODING &&
        projectile->lastPhase != NETWORK_BOMB_EXPLODING) {
        Vec3f velocity = { 0.0f, 0.0f, 0.0f };
        Vec3f accel = { 0.0f, 0.1f, 0.0f };
        Vec3f effectPos = { state.x, state.y + 10.0f, state.z };
        EffectSsBomb2_SpawnLayered(play, &effectPos, &velocity, &accel, 100, 19);
        Audio_PlayActorSound2(thisx, NA_SE_IT_BOMB_EXPLOSION);
    }
    projectile->lastPhase = state.phase;
    thisx->prevPos = thisx->world.pos;
    thisx->world.pos.x += (state.x - thisx->world.pos.x) * 0.7f;
    thisx->world.pos.y += (state.y - thisx->world.pos.y) * 0.7f;
    thisx->world.pos.z += (state.z - thisx->world.pos.z) * 0.7f;
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
        Gfx_DrawDListOpa(play, reinterpret_cast<Gfx*>(const_cast<char*>(gArrowNearDL)));
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
            record.state = packet;
            record.lastPacketMilliseconds = NowMilliseconds();
            record.hasState = true;
        }
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
            if (objectState.destroyed == 2) {
                gNetworkGame.activatedObjects.insert(key);
            } else {
                gNetworkGame.destroyedObjects.insert(key);
            }
        } else {
            gNetworkGame.destroyedObjects.erase(key);
        }
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
            actor, LocalProjectileRecord{ gNetworkGame.nextProjectileId, false });
        if (inserted) {
            ++gNetworkGame.nextProjectileId;
        }
        if (actor->parent == &player->actor) {
            continue;
        }
        found->second.launched = true;
        NetworkProjectileStatePacket packet{};
        packet.projectileId = found->second.projectileId;
        packet.sceneId = play->sceneNum;
        packet.active = 1;
        packet.projectileKind = NETWORK_PROJECTILE_ARROW;
        packet.phase = 0;
        packet.projectileType = static_cast<uint8_t>(std::max<int16_t>(0, actor->params));
        packet.x = actor->world.pos.x;
        packet.y = actor->world.pos.y;
        packet.z = actor->world.pos.z;
        // world.rot is the launch direction. shape.rot.x is a model-space
        // display angle and must not be interpreted as ballistic pitch.
        packet.rotationX = actor->world.rot.x;
        packet.rotationY = actor->world.rot.y;
        packet.rotationZ = actor->world.rot.z;
        gNetworkGame.runtime->SendProjectileState(packet);
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_EXPLOSIVE].head; actor; actor = actor->next) {
        if (actor->id != ACTOR_EN_BOM || actor->params != 0) {
            continue;
        }
        seen.insert(actor);
        auto [found, inserted] = gNetworkGame.localProjectiles.try_emplace(
            actor, LocalProjectileRecord{ gNetworkGame.nextProjectileId, true });
        if (inserted) {
            ++gNetworkGame.nextProjectileId;
        }
        NetworkProjectileStatePacket packet{};
        packet.projectileId = found->second.projectileId;
        packet.sceneId = play->sceneNum;
        packet.active = 1;
        packet.projectileKind = NETWORK_PROJECTILE_BOMB;
        packet.phase = actor->parent == &player->actor ? NETWORK_BOMB_HELD : NETWORK_BOMB_RELEASED;
        packet.rotationY = actor->world.rot.y;
        const float yaw = actor->world.rot.y * (3.14159265358979323846f / 32768.0f);
        packet.velocityX = std::sin(yaw) * actor->speedXZ * 20.0f;
        packet.velocityY = actor->velocity.y * 20.0f;
        packet.velocityZ = std::cos(yaw) * actor->speedXZ * 20.0f;
        gNetworkGame.runtime->SendProjectileState(packet);
    }
    for (auto it = gNetworkGame.localProjectiles.begin(); it != gNetworkGame.localProjectiles.end();) {
        if (seen.count(it->first) != 0) {
            ++it;
            continue;
        }
        if (it->second.launched) {
            NetworkProjectileStatePacket packet{};
            packet.projectileId = it->second.projectileId;
            packet.sceneId = play->sceneNum;
            packet.active = 0;
            gNetworkGame.runtime->SendProjectileState(packet);
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
    packet.modelGroup = player->modelGroup;
    packet.itemAction = static_cast<uint8_t>(std::max<int>(0, player->itemAction));
    packet.meleeWeaponState = player->meleeWeaponState;
    packet.upperLimbRot[0] = player->upperLimbRot.x;
    packet.upperLimbRot[1] = player->upperLimbRot.y;
    packet.upperLimbRot[2] = player->upperLimbRot.z;
    packet.headLimbRot[0] = player->headLimbRot.x;
    packet.headLimbRot[1] = player->headLimbRot.y;
    packet.headLimbRot[2] = player->headLimbRot.z;
    Vec3f rodTipOffset{};
    Vec3f lureOffset{};
    Vec3f lineOffsets[NETWORK_FISHING_LINE_POINT_COUNT]{};
    if (Fishing_GetNetworkVisualState(play, &packet.fishingState, &rodTipOffset, &lureOffset, lineOffsets)) {
        packet.fishingRodTipOffset[0] = rodTipOffset.x;
        packet.fishingRodTipOffset[1] = rodTipOffset.y;
        packet.fishingRodTipOffset[2] = rodTipOffset.z;
        packet.fishingLureOffset[0] = lureOffset.x;
        packet.fishingLureOffset[1] = lureOffset.y;
        packet.fishingLureOffset[2] = lureOffset.z;
        for (size_t point = 0; point < NETWORK_FISHING_LINE_POINT_COUNT; ++point) {
            packet.fishingLineOffsets[point][0] = lineOffsets[point].x;
            packet.fishingLineOffsets[point][1] = lineOffsets[point].y;
            packet.fishingLineOffsets[point][2] = lineOffsets[point].z;
        }
    }
    for (size_t limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb) {
        packet.jointTable[limb][0] = player->skelAnime.jointTable[limb].x;
        packet.jointTable[limb][1] = player->skelAnime.jointTable[limb].y;
        packet.jointTable[limb][2] = player->skelAnime.jointTable[limb].z;
    }
    gNetworkGame.runtime->SendPlayerState(packet);
    SendLocalProjectiles(play, player);
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
    gNetworkGame.activatedObjects.clear();
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
    // Drain high-rate pose packets even outside PlayState. Keeping only the
    // newest packet per player prevents file select and scene loads from
    // accumulating an unbounded pose backlog.
    ReceiveRemotePlayerStates();
}

extern "C" void NetworkGame_Update(PlayState* play) {
    if (!play || !gNetworkGame.runtime) {
        return;
    }
    const uint64_t now = NowMilliseconds();
    NetworkPlayerDamagePacket damage{};
    while (gNetworkGame.runtime->PollPlayerDamage(damage)) {
        Player* player = GET_PLAYER(play);
        if (player && player->invincibilityTimer == 0) {
            player->actor.colChkInfo.damage = damage.damage;
            func_80837C0C(play, player, PLAYER_HIT_RESPONSE_NONE, 4.0f, 5.0f, damage.impactYaw, 20);
        }
    }
    gNetworkGame.lastFrameMilliseconds = now;
    SpawnOrCullRemoteActors(play);
    SendLocalPlayerState(play);
}

extern "C" int NetworkGame_IsObjectDestroyed(PlayState* play, Actor* actor) {
    return play && actor && gNetworkGame.destroyedObjects.count(DynamicObjectKeyFor(play->sceneNum, actor)) != 0;
}

extern "C" void NetworkGame_NotifyObjectDestroyed(PlayState* play, Actor* actor) {
    if (!play || !actor || !gNetworkGame.runtime || !gNetworkGame.runtime->IsActive()) {
        return;
    }
    const DynamicObjectKey key = DynamicObjectKeyFor(play->sceneNum, actor);
    gNetworkGame.destroyedObjects.insert(key);
    const auto& [sceneId, roomId, actorId, actorParams, homeX, homeY, homeZ] = key;
    NetworkDynamicObjectStatePacket packet{ sceneId, roomId, actorId, actorParams, homeX, homeY, homeZ, 1 };
    gNetworkGame.runtime->SendDynamicObjectState(packet);
}

extern "C" void NetworkGame_NotifyObjectRestored(PlayState* play, Actor* actor) {
    if (!play || !actor || !gNetworkGame.runtime || !gNetworkGame.runtime->IsActive()) {
        return;
    }
    const DynamicObjectKey key = DynamicObjectKeyFor(play->sceneNum, actor);
    gNetworkGame.destroyedObjects.erase(key);
    const auto& [sceneId, roomId, actorId, actorParams, homeX, homeY, homeZ] = key;
    NetworkDynamicObjectStatePacket packet{ sceneId, roomId, actorId, actorParams, homeX, homeY, homeZ, 0 };
    gNetworkGame.runtime->SendDynamicObjectState(packet);
}

extern "C" void NetworkGame_NotifyObjectActivated(PlayState* play, Actor* actor) {
    if (!play || !actor || !gNetworkGame.runtime || !gNetworkGame.runtime->IsActive()) {
        return;
    }
    const DynamicObjectKey key = DynamicObjectKeyFor(play->sceneNum, actor);
    const auto& [sceneId, roomId, actorId, actorParams, homeX, homeY, homeZ] = key;
    NetworkDynamicObjectStatePacket packet{ sceneId, roomId, actorId, actorParams, homeX, homeY, homeZ, 2 };
    gNetworkGame.runtime->SendDynamicObjectState(packet);
}

extern "C" int NetworkGame_ConsumeObjectActivation(PlayState* play, Actor* actor) {
    if (!play || !actor) {
        return 0;
    }
    const DynamicObjectKey key = DynamicObjectKeyFor(play->sceneNum, actor);
    const auto found = gNetworkGame.activatedObjects.find(key);
    if (found == gNetworkGame.activatedObjects.end()) {
        return 0;
    }
    gNetworkGame.activatedObjects.erase(found);
    return 1;
}

extern "C" int NetworkGame_IsExplosionNear(PlayState* play, Actor* actor, float radius) {
    if (!play || !actor || radius <= 0.0f) {
        return 0;
    }
    const float radiusSquared = radius * radius;
    for (const auto& [key, projectile] : gNetworkGame.remoteProjectiles) {
        (void)key;
        if (!projectile.state.active || projectile.state.sceneId != play->sceneNum ||
            projectile.state.projectileKind != NETWORK_PROJECTILE_BOMB ||
            projectile.state.phase != NETWORK_BOMB_EXPLODING) {
            continue;
        }
        const float dx = projectile.state.x - actor->world.pos.x;
        const float dy = projectile.state.y - actor->world.pos.y;
        const float dz = projectile.state.z - actor->world.pos.z;
        if (dx * dx + dy * dy + dz * dz <= radiusSquared) {
            return 1;
        }
    }
    return 0;
}
