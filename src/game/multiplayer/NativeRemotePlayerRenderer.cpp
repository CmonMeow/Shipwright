#include "NativeRemotePlayerRenderer.h"

#include "NetworkProtocol.h"
#include "runtime/actors/ActorDB.h"
#include "resources/ResourceManagerHelpers.h"
#include "rendering/FrameInterpolation.h"
#include "debug/collision/colViewer.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"
#include "assets/objects/object_fish/object_fish.h"
#include "global.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

#include <runtime/log/Log.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>

#ifndef NDEBUG
#define REMOTE_PLAYER_OPEN_DISPS(gfxContext)                                                                     \
    {                                                                                                             \
        FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);                                                   \
        GraphicsContext* __gfxCtx = (gfxContext);                                                                 \
        Gfx* remotePlayerDispRefs[4];                                                                             \
        Graph_OpenDisps(remotePlayerDispRefs, __gfxCtx, __FILE__, __LINE__)
#define REMOTE_PLAYER_CLOSE_DISPS()                                                                               \
        FrameInterpolation_RecordCloseChild();                                                                    \
        Graph_CloseDisps(remotePlayerDispRefs, __gfxCtx, __FILE__, __LINE__);                                     \
    }
#else
#define REMOTE_PLAYER_OPEN_DISPS(gfxContext)                                                                     \
    {                                                                                                             \
        FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);                                                   \
        GraphicsContext* __gfxCtx = (gfxContext);                                                                 \
        (void)__gfxCtx;
#define REMOTE_PLAYER_CLOSE_DISPS()                                                                               \
        FrameInterpolation_RecordCloseChild();                                                                    \
    }
#endif

namespace Game::Multiplayer {
namespace {

constexpr float kRadiansToBinaryAngle = 32768.0f / 3.14159265358979323846f;
constexpr Vec3s kPlayerSkeletonBaseTranslation = { -57, 3377, 0 };

uint64_t EntityKey(Game::Simulation::EntityId entity) {
    return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
}

Game::Simulation::EntityId EntityFromKey(uint64_t key) {
    return { static_cast<uint32_t>(key), static_cast<uint32_t>(key >> 32) };
}

double NowSeconds() {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct NativeRemotePlayer {
    Actor actor;
    SkelAnime skelAnime;
    SkelAnime upperSkelAnime;
    SkelAnime bowArrowSkelAnime;
    // SkelAnime_InitLink aligns supplied frame-table pointers upward. Without
    // explicit alignment that can move the effective start into the array and
    // let a complete Link frame overwrite the following table.
    alignas(16) Vec3s jointTable[PLAYER_LIMB_BUF_COUNT];
    alignas(16) Vec3s morphTable[PLAYER_LIMB_BUF_COUNT];
    alignas(16) Vec3s upperJointTable[PLAYER_LIMB_BUF_COUNT];
    alignas(16) Vec3s upperMorphTable[PLAYER_LIMB_BUF_COUNT];
    int32_t playerId;
    uint64_t presentationEntityKey;
    bool isCorpse;
    uint8_t modelGroup;
    uint8_t itemAction;
    Vec3s upperLimbRot;
    Vec3s headLimbRot;
    ClientPlayerBaseAnimation baseAnimation;
    ClientPlayerUpperAnimation upperAnimation;
    uint8_t baseAnimationItemAction;
    uint8_t upperAnimationItemAction;
    bool animationInitialized;
    uint32_t animationLifeEpoch;
    uint32_t animationActionInstanceTick;
};

struct NativeRecord {
    NativePlayerPresentationState state{};
    NativeRemotePlayer* actor = nullptr;
    bool renderReady = false;
    bool fishingLineInitialized = false;
    bool fishingSinkingLureInitialized = false;
    Vec3f fishingLinePos[NETWORK_FISHING_LINE_POINT_COUNT]{};
    Vec3f fishingLineRot[NETWORK_FISHING_LINE_POINT_COUNT]{};
    Vec3f fishingLineUnk[NETWORK_FISHING_LINE_POINT_COUNT]{};
    Vec3f fishingSinkingLurePos[20]{};
};

struct CorpseRecord {
    Game::Simulation::EntityId entity{};
    NativeRecord presentation{};
};

uint8_t kUpperBodyLimbCopyMap[PLAYER_LIMB_MAX] = {
    false, false, false, false, false, false, false, false, false, false,
    true, true, true, true, true, true, true, true, true, true, true, true,
};

extern "C" void Fishing_UpdatePresentedLine(
    PlayState* play, Actor* collisionActor, Vec3f* rodTip, Vec3f* lurePos,
    Vec3f linePos[NETWORK_FISHING_LINE_POINT_COUNT],
    Vec3f lineRot[NETWORK_FISHING_LINE_POINT_COUNT],
    Vec3f lineUnk[NETWORK_FISHING_LINE_POINT_COUNT], int16_t lineSpooled,
    uint8_t lureType, float lineGravity);
extern "C" void Fishing_UpdatePresentedSinkingLure(
    Vec3f* lurePos, Vec3f positions[20], int16_t playerYaw, uint8_t castState,
    uint8_t underwater);

void CopyStateToActor(NativeRemotePlayer* remote,
                      const NativePlayerPresentationState& state,
                      bool snapPosition) {
    if (!remote) return;
    if (snapPosition) {
        remote->actor.world.pos = { state.x, state.y, state.z };
        remote->actor.prevPos = remote->actor.world.pos;
        remote->actor.shape.rot = {
            state.rotationX, state.rotationY, state.rotationZ
        };
        remote->actor.world.rot = remote->actor.shape.rot;
    }
    remote->actor.speedXZ = state.speed;
    remote->modelGroup = state.modelGroup;
    remote->itemAction = state.itemAction;
    remote->upperLimbRot = {
        state.upperLimbRot[0], state.upperLimbRot[1], state.upperLimbRot[2]
    };
    remote->headLimbRot = {
        state.headLimbRot[0], state.headLimbRot[1], state.headLimbRot[2]
    };
}

LinkAnimationHeader* BaseAnimationAsset(ClientPlayerBaseAnimation animation,
                                        uint8_t itemAction) {
    using Animation = ClientPlayerBaseAnimation;
    const char* asset = gPlayerAnim_link_normal_wait_free;
    switch (animation) {
        case Animation::IdleSword: asset = gPlayerAnim_link_normal_wait; break;
        case Animation::IdleBiggoron: asset = gPlayerAnim_link_fighter_wait_long; break;
        case Animation::BlockingFree: asset = gPlayerAnim_link_normal_defense_wait_free; break;
        case Animation::BlockingSword: asset = gPlayerAnim_link_normal_defense_wait; break;
        case Animation::BlockingBiggoron: asset = gPlayerAnim_link_normal_defense_wait_free; break;
        case Animation::RunForward:
            asset = itemAction == PLAYER_IA_SWORD_MASTER
                ? gPlayerAnim_link_fighter_run
                : itemAction == PLAYER_IA_SWORD_BIGGORON
                    ? gPlayerAnim_link_fighter_run_long
                    : gPlayerAnim_link_normal_run_free;
            break;
        case Animation::WalkBackward: asset = gPlayerAnim_link_normal_back_walk; break;
        case Animation::StrafeLeft:
            asset = itemAction == PLAYER_IA_SWORD_BIGGORON
                ? gPlayerAnim_link_fighter_side_walkL_long
                : itemAction == PLAYER_IA_SWORD_MASTER
                    ? gPlayerAnim_link_anchor_side_walkL
                    : gPlayerAnim_link_normal_side_walkL_free;
            break;
        case Animation::StrafeRight:
            asset = itemAction == PLAYER_IA_SWORD_BIGGORON
                ? gPlayerAnim_link_fighter_side_walkR_long
                : itemAction == PLAYER_IA_SWORD_MASTER
                    ? gPlayerAnim_link_anchor_side_walkR
                    : gPlayerAnim_link_normal_side_walkR_free;
            break;
        case Animation::SwordHeld: asset = gPlayerAnim_link_fighter_power_kiru_wait; break;
        case Animation::BiggoronHeld: asset = gPlayerAnim_link_fighter_Lpower_kiru_wait; break;
        case Animation::MeleeForwardSlash:
            asset = itemAction == PLAYER_IA_SWORD_BIGGORON
                ? gPlayerAnim_link_fighter_Lnormal_kiru
                : gPlayerAnim_link_fighter_normal_kiru;
            break;
        case Animation::MeleeForwardCombo:
            asset = itemAction == PLAYER_IA_SWORD_BIGGORON
                ? gPlayerAnim_link_fighter_Lnormal_kiru_finsh
                : gPlayerAnim_link_fighter_normal_kiru_finsh;
            break;
        case Animation::MeleeRightSlash:
            asset = itemAction == PLAYER_IA_SWORD_BIGGORON
                ? gPlayerAnim_link_fighter_LLside_kiru
                : gPlayerAnim_link_fighter_Lside_kiru;
            break;
        case Animation::MeleeRightCombo:
            asset = itemAction == PLAYER_IA_SWORD_BIGGORON
                ? gPlayerAnim_link_fighter_LLside_kiru_finsh
                : gPlayerAnim_link_fighter_Lside_kiru_finsh;
            break;
        case Animation::MeleeLeftSlash:
            asset = itemAction == PLAYER_IA_SWORD_BIGGORON
                ? gPlayerAnim_link_fighter_LRside_kiru
                : gPlayerAnim_link_fighter_Rside_kiru;
            break;
        case Animation::MeleeLeftCombo:
            asset = itemAction == PLAYER_IA_SWORD_BIGGORON
                ? gPlayerAnim_link_fighter_LRside_kiru_finsh
                : gPlayerAnim_link_fighter_Rside_kiru_finsh;
            break;
        case Animation::SwordSpinAttack: asset = gPlayerAnim_link_fighter_rolling_kiru; break;
        case Animation::BiggoronSpinAttack: asset = gPlayerAnim_link_fighter_Lrolling_kiru; break;
        case Animation::EvadeBackward: asset = gPlayerAnim_link_fighter_backturn_jump; break;
        case Animation::EvadeLeft: asset = gPlayerAnim_link_fighter_Lside_jump; break;
        case Animation::EvadeRight: asset = gPlayerAnim_link_fighter_Rside_jump; break;
        case Animation::Fishing: asset = gPlayerAnim_link_fishing_wait; break;
        case Animation::SwimIdle: asset = gPlayerAnim_link_swimer_swim_wait; break;
        case Animation::SwimForward: asset = gPlayerAnim_link_swimer_swim; break;
        case Animation::SwimBackward: asset = gPlayerAnim_link_swimer_back_swim; break;
        case Animation::SwimLeft: asset = gPlayerAnim_link_swimer_Lside_swim; break;
        case Animation::SwimRight: asset = gPlayerAnim_link_swimer_Rside_swim; break;
        case Animation::Climb: asset = gPlayerAnim_link_normal_climb_up; break;
        case Animation::Falling: asset = gPlayerAnim_link_normal_fall; break;
        case Animation::JumpSlash: asset = gPlayerAnim_link_fighter_jump_kiru; break;
        case Animation::Dead: asset = gPlayerAnim_link_derth_rebirth; break;
        default: break;
    }
    return reinterpret_cast<LinkAnimationHeader*>(const_cast<char*>(asset));
}

LinkAnimationHeader* UpperAnimationAsset(ClientPlayerUpperAnimation animation,
                                         uint8_t itemAction) {
    using Animation = ClientPlayerUpperAnimation;
    const char* asset = gPlayerAnim_link_normal_wait;
    switch (animation) {
        case Animation::Blocking:
            asset = itemAction == PLAYER_IA_SWORD_MASTER
                ? gPlayerAnim_link_normal_defense_wait
                : gPlayerAnim_link_normal_defense_wait_free;
            break;
        case Animation::BowAiming: asset = gPlayerAnim_link_bow_bow_wait; break;
        case Animation::Fishing: asset = gPlayerAnim_link_fishing_wait; break;
        default: break;
    }
    return reinterpret_cast<LinkAnimationHeader*>(const_cast<char*>(asset));
}

float BaseAnimationEndFrame(ClientPlayerBaseAnimation animation,
                            LinkAnimationHeader* asset) {
    float endFrame = Animation_GetLastFrame(asset);
    if (animation == ClientPlayerBaseAnimation::Dead) {
        // Native Player death uses this combined death/rebirth asset but
        // explicitly stops at frame 84. Playing its full range makes a
        // retained corpse stand back up when its owner respawns.
        endFrame = std::min(endFrame, 84.0f);
    }
    return endFrame;
}

void UpdateAnimation(NativeRemotePlayer* remote, PlayState* play,
                     const NativePlayerPresentationState& state) {
    using Base = ClientPlayerBaseAnimation;
    using Upper = ClientPlayerUpperAnimation;
    const bool newActionClock = state.synchronizeActionAnimation &&
        (remote->animationLifeEpoch != state.lifeEpoch ||
          remote->animationActionInstanceTick != state.actionInstanceId);
    if (!remote->animationInitialized ||
        remote->baseAnimation != state.baseAnimation ||
        remote->baseAnimationItemAction != state.itemAction || newActionClock) {
        remote->baseAnimation = state.baseAnimation;
        remote->baseAnimationItemAction = state.itemAction;
        LinkAnimationHeader* animation = BaseAnimationAsset(
            state.baseAnimation, state.itemAction);
        const float lastFrame = BaseAnimationEndFrame(
            state.baseAnimation, animation);
        const bool loop = state.baseAnimation != Base::MeleeForwardSlash &&
                          state.baseAnimation != Base::MeleeForwardCombo &&
                          state.baseAnimation != Base::MeleeRightSlash &&
                          state.baseAnimation != Base::MeleeRightCombo &&
                          state.baseAnimation != Base::MeleeLeftSlash &&
                          state.baseAnimation != Base::MeleeLeftCombo &&
                          state.baseAnimation != Base::SwordSpinAttack &&
                          state.baseAnimation != Base::BiggoronSpinAttack &&
                          state.baseAnimation != Base::EvadeBackward &&
                          state.baseAnimation != Base::EvadeLeft &&
                          state.baseAnimation != Base::EvadeRight &&
                          state.baseAnimation != Base::Dead;
        const float startFrame =
            (state.synchronizeBaseAnimation || state.synchronizeActionAnimation)
            ? lastFrame * std::clamp(state.baseAnimationProgress, 0.0f, 1.0f)
            : 0.0f;
        LinkAnimation_Change(play, &remote->skelAnime, animation,
                             1.0f, startFrame, lastFrame,
                             loop ? ANIMMODE_LOOP : ANIMMODE_ONCE, -4.0f);
    }
    if (state.synchronizeBaseAnimation) {
        LinkAnimationHeader* animation = BaseAnimationAsset(
            state.baseAnimation, state.itemAction);
        const float lastFrame = BaseAnimationEndFrame(
            state.baseAnimation, animation);
        const double elapsedSeconds =
            std::max(0.0, NowSeconds() - state.baseAnimationSampleSeconds);
        const float rawProgress =
            state.baseAnimationProgress +
            static_cast<float>(elapsedSeconds) *
                state.baseAnimationProgressPerSecond;
        const float progress = state.loopBaseAnimationProgress
            ? rawProgress - std::floor(rawProgress)
            : std::clamp(rawProgress, 0.0f, 1.0f);
        remote->skelAnime.curFrame = lastFrame * progress;
        remote->skelAnime.playSpeed = 0.0f;
    } else {
        remote->skelAnime.playSpeed = 1.0f;
    }
    LinkAnimation_Update(play, &remote->skelAnime);

    if (!remote->animationInitialized ||
        remote->upperAnimation != state.upperAnimation ||
        remote->upperAnimationItemAction != state.itemAction) {
        remote->upperAnimation = state.upperAnimation;
        remote->upperAnimationItemAction = state.itemAction;
        if (state.upperAnimation != Upper::None) {
            LinkAnimationHeader* animation = UpperAnimationAsset(
                state.upperAnimation, state.itemAction);
            LinkAnimation_Change(play, &remote->upperSkelAnime, animation, 1.0f,
                                 0.0f, Animation_GetLastFrame(animation),
                                 ANIMMODE_LOOP, -4.0f);
        }
    }
    remote->animationInitialized = true;
    remote->animationLifeEpoch = state.lifeEpoch;
    remote->animationActionInstanceTick = state.actionInstanceId;
    if (state.upperAnimation != Upper::None) {
        LinkAnimation_Update(play, &remote->upperSkelAnime);
        AnimationContext_SetCopyTrue(play, PLAYER_LIMB_MAX,
                                     remote->skelAnime.jointTable,
                                     remote->upperSkelAnime.jointTable,
                                     kUpperBodyLimbCopyMap);
    }
}

} // namespace

struct NativeRemotePlayerRenderer::Impl {
    Game::Client::RemotePlayerReplicaStore* players = nullptr;
    Game::Client::RemoteFishingEntityState* fishing = nullptr;
    Game::Client::CorpsePresentationRegistry* corpses = nullptr;
    std::map<uint64_t, NativeRecord> live;
    std::map<uint64_t, CorpseRecord> dead;
    int32_t localPlayerId = -1;
    int16_t liveActorId = -1;
    int16_t corpseActorId = -1;
};

NativeRemotePlayerRenderer* NativeRemotePlayerRenderer::sActive = nullptr;

NativeRemotePlayerRenderer::NativeRemotePlayerRenderer()
    : mImpl(std::make_unique<Impl>()) {
    sActive = this;
}

NativeRemotePlayerRenderer::~NativeRemotePlayerRenderer() {
    DetachAfterSceneShutdown();
    if (sActive == this) sActive = nullptr;
}

void NativeRemotePlayerRenderer::Bind(
    Game::Client::RemotePlayerReplicaStore* players,
    Game::Client::RemoteFishingEntityState* fishing,
    Game::Client::CorpsePresentationRegistry* corpses) {
    mImpl->players = players;
    mImpl->fishing = fishing;
    mImpl->corpses = corpses;
}

void NativeRemotePlayerRenderer::RegisterActorType() {
    if (!ActorDB::Instance) return;
    const auto registerType = [&](const char* name, const char* description) {
        ActorDBInit init;
        init.name = name;
        init.desc = description;
        init.category = ACTORCAT_NPC;
        init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                     ACTOR_FLAG_LOCK_ON_DISABLED | ACTOR_FLAG_IGNORE_QUAKE;
        init.objectId = OBJECT_LINK_BOY;
        init.instanceSize = sizeof(NativeRemotePlayer);
        init.init = ActorInit;
        init.destroy = ActorDestroy;
        init.update = ActorUpdate;
        init.draw = ActorDraw;
        return static_cast<int16_t>(ActorDB::Instance->AddEntry(init).entry.id);
    };
    if (mImpl->liveActorId < 0) {
        mImpl->liveActorId = registerType(
            "NETWORK_REMOTE_PLAYER",
            "Authoritative remote adult Link presentation");
    }
    if (mImpl->corpseActorId < 0) {
        mImpl->corpseActorId = registerType(
            "NETWORK_REMOTE_CORPSE",
            "Authoritative retained player corpse presentation");
    }
}

NativePlayerPresentationState& NativeRemotePlayerRenderer::UpsertPlayer(
    Game::Simulation::EntityId entity,
    const NativePlayerPresentationState& initialState) {
    NativeRecord& record = mImpl->live[EntityKey(entity)];
    const bool sceneChanged = record.renderReady &&
                              record.state.sceneId != initialState.sceneId;
    if (sceneChanged && record.actor && record.actor->actor.update) {
        Actor_Kill(&record.actor->actor);
        record.actor = nullptr;
    }
    if (sceneChanged) record = {};
    record.state = initialState;
    return record.state;
}

NativePlayerPresentationState* NativeRemotePlayerRenderer::FindPlayer(
    Game::Simulation::EntityId entity) {
    const auto found = mImpl->live.find(EntityKey(entity));
    return found == mImpl->live.end() ? nullptr : &found->second.state;
}

NativePlayerPresentationState* NativeRemotePlayerRenderer::FindPlayer(
    int32_t playerId) {
    if (!mImpl->players) return nullptr;
    const auto* replica = mImpl->players->FindPlayer(playerId);
    return replica ? FindPlayer(replica->lifetime.entity) : nullptr;
}

const NativePlayerPresentationState* NativeRemotePlayerRenderer::FindPlayer(
    int32_t playerId) const {
    if (!mImpl->players) return nullptr;
    const auto* replica = mImpl->players->FindPlayer(playerId);
    if (!replica) return nullptr;
    const auto found = mImpl->live.find(EntityKey(replica->lifetime.entity));
    return found == mImpl->live.end() ? nullptr : &found->second.state;
}

bool NativeRemotePlayerRenderer::IsPlayerReady(int32_t playerId) const {
    if (!mImpl->players) return false;
    const auto* replica = mImpl->players->FindPlayer(playerId);
    if (!replica) return false;
    const auto found = mImpl->live.find(EntityKey(replica->lifetime.entity));
    return found != mImpl->live.end() && found->second.renderReady;
}

void NativeRemotePlayerRenderer::MarkPlayerReady(
    Game::Simulation::EntityId entity) {
    const auto found = mImpl->live.find(EntityKey(entity));
    if (found != mImpl->live.end()) found->second.renderReady = true;
}

void NativeRemotePlayerRenderer::ResetFishingVisuals(
    Game::Simulation::EntityId entity) {
    const auto found = mImpl->live.find(EntityKey(entity));
    if (found == mImpl->live.end()) return;
    found->second.fishingLineInitialized = false;
    found->second.fishingSinkingLureInitialized = false;
}

void NativeRemotePlayerRenderer::RetirePlayer(Game::Simulation::EntityId entity) {
    const auto found = mImpl->live.find(EntityKey(entity));
    if (found == mImpl->live.end()) return;
    if (found->second.actor && found->second.actor->actor.update) {
        Actor_Kill(&found->second.actor->actor);
    }
    mImpl->live.erase(found);
}

void NativeRemotePlayerRenderer::UpsertCorpse(
    Game::Simulation::EntityId entity,
    const NativePlayerPresentationState& state) {
    CorpseRecord& corpse = mImpl->dead[EntityKey(entity)];
    corpse.entity = entity;
    corpse.presentation.state = state;
    corpse.presentation.renderReady = true;
}

void NativeRemotePlayerRenderer::RetireCorpse(Game::Simulation::EntityId entity) {
    const auto found = mImpl->dead.find(EntityKey(entity));
    if (found == mImpl->dead.end()) return;
    if (found->second.presentation.actor &&
        found->second.presentation.actor->actor.update) {
        Actor_Kill(&found->second.presentation.actor->actor);
    }
    mImpl->dead.erase(found);
}

bool NativeRemotePlayerRenderer::HasFishingPlayerInScene(int32_t sceneId) const {
    for (const auto& [key, record] : mImpl->live) {
        (void)key;
        if (record.renderReady && record.state.sceneId == sceneId &&
            record.state.itemAction == PLAYER_IA_FISHING_POLE) {
            return true;
        }
    }
    return false;
}

std::optional<NativePlayerWorldPosition>
NativeRemotePlayerRenderer::WorldPositionForPlayer(int32_t playerId) const {
    if (!mImpl->players) return std::nullopt;
    const auto* replica = mImpl->players->FindPlayer(playerId);
    if (!replica) return std::nullopt;
    const auto found = mImpl->live.find(EntityKey(replica->lifetime.entity));
    if (found == mImpl->live.end() || !found->second.renderReady ||
        !found->second.actor ||
        (found->second.state.stateFlags & NATIVE_PLAYER_VISIBLE) == 0 ||
        (found->second.state.stateFlags & NATIVE_PLAYER_DEAD) != 0) {
        return std::nullopt;
    }
    const Vec3f& position = found->second.actor->actor.world.pos;
    return NativePlayerWorldPosition{ position.x, position.y, position.z };
}

void NativeRemotePlayerRenderer::Reset() {
    for (auto& [key, record] : mImpl->live) {
        (void)key;
        if (record.actor && record.actor->actor.update) {
            Actor_Kill(&record.actor->actor);
        }
    }
    for (auto& [key, record] : mImpl->dead) {
        (void)key;
        if (record.presentation.actor && record.presentation.actor->actor.update) {
            Actor_Kill(&record.presentation.actor->actor);
        }
    }
    mImpl->live.clear();
    mImpl->dead.clear();
    mImpl->localPlayerId = -1;
}

void NativeRemotePlayerRenderer::DetachAfterSceneShutdown() {
    mImpl->live.clear();
    mImpl->dead.clear();
    mImpl->players = nullptr;
    mImpl->fishing = nullptr;
    mImpl->corpses = nullptr;
    mImpl->localPlayerId = -1;
}

void NativeRemotePlayerRenderer::Reconcile(PlayState* play,
                                            int32_t localPlayerId) {
    if (!play || !mImpl->players || !mImpl->corpses) return;
    mImpl->localPlayerId = localPlayerId;
    for (auto it = mImpl->live.begin(); it != mImpl->live.end();) {
        NativeRecord& record = it->second;
        const auto entity = EntityFromKey(it->first);
        const auto* replica = mImpl->players->Find(entity);
        if (!replica) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            it = mImpl->live.erase(it);
            continue;
        }
        if (record.state.playerId == localPlayerId) {
            // Local prediction is the native Player actor. Keep its replica
            // for acknowledgement/reconciliation, but never spawn a second
            // reconstructed body over it.
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            ++it;
            continue;
        }
        if (record.state.sceneId != play->sceneNum) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            ++it;
            continue;
        }
        const bool retainedBodyOwnsPresentation =
            replica->hasSnapshot && mImpl->corpses->OwnsSource(
                entity, replica->snapshot.lifeEpoch);
        if (retainedBodyOwnsPresentation) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            ++it;
            continue;
        }
        const auto handle = mImpl->players->ActorHandleFor(entity);
        if (!record.actor && handle && mImpl->liveActorId >= 0 &&
            record.renderReady) {
            Actor_Spawn(&play->actorCtx, play, mImpl->liveActorId, record.state.x,
                        record.state.y, record.state.z, record.state.rotationX,
                        record.state.rotationY, record.state.rotationZ, *handle);
        }
        ++it;
    }

    for (auto it = mImpl->dead.begin(); it != mImpl->dead.end();) {
        CorpseRecord& corpse = it->second;
        NativeRecord& record = corpse.presentation;
        if (record.state.sceneId != play->sceneNum) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            ++it;
            continue;
        }
        const auto handle = mImpl->corpses->ActorHandleFor(corpse.entity);
        if (!handle) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            it = mImpl->dead.erase(it);
            continue;
        }
        if (!record.actor && mImpl->corpseActorId >= 0 && record.renderReady) {
            Actor_Spawn(&play->actorCtx, play, mImpl->corpseActorId, record.state.x,
                        record.state.y, record.state.z, record.state.rotationX,
                        record.state.rotationY, record.state.rotationZ, *handle);
        }
        ++it;
    }
}

void NativeRemotePlayerRenderer::ActorInit(Actor* actor, PlayState* play) {
    auto* renderer = sActive;
    auto* remote = reinterpret_cast<NativeRemotePlayer*>(actor);
    remote->animationInitialized = false;
    remote->playerId = -1;
    remote->presentationEntityKey = 0;
    remote->isCorpse = false;
    NativeRecord* record = nullptr;

    const bool corpseActor = renderer && actor->id == renderer->mImpl->corpseActorId;
    if (corpseActor && renderer->mImpl->corpses) {
        if (const auto entity =
                renderer->mImpl->corpses->EntityForActorHandle(actor->params)) {
            remote->isCorpse = true;
            remote->presentationEntityKey = EntityKey(*entity);
            const auto found = renderer->mImpl->dead.find(
                remote->presentationEntityKey);
            if (found != renderer->mImpl->dead.end()) {
                record = &found->second.presentation;
                remote->playerId = record->state.playerId;
            }
        }
    }
    if (!corpseActor && renderer && renderer->mImpl->players) {
        if (const auto entity =
                renderer->mImpl->players->EntityForActorHandle(actor->params)) {
            remote->presentationEntityKey = EntityKey(*entity);
            const auto found = renderer->mImpl->live.find(
                remote->presentationEntityKey);
            if (found != renderer->mImpl->live.end()) {
                record = &found->second;
                remote->playerId = record->state.playerId;
            }
        }
    }

    actor->room = -1;
    actor->uncullZoneForward = 32000.0f;
    actor->uncullZoneScale = 350.0f;
    actor->uncullZoneDownward = 700.0f;
    ActorShape_Init(&actor->shape, 0.0f, ActorShadow_DrawCircle, 18.0f);
    SkelAnime_InitLink(
        play, &remote->skelAnime, gPlayerSkelHeaders,
        reinterpret_cast<LinkAnimationHeader*>(
            const_cast<char*>(gPlayerAnim_link_normal_wait)),
        9, remote->jointTable, remote->morphTable, PLAYER_LIMB_MAX);
    remote->skelAnime.baseTransl = kPlayerSkeletonBaseTranslation;
    SkelAnime_InitLink(
        play, &remote->upperSkelAnime, gPlayerSkelHeaders,
        reinterpret_cast<LinkAnimationHeader*>(
            const_cast<char*>(gPlayerAnim_link_normal_wait)),
        9, remote->upperJointTable, remote->upperMorphTable, PLAYER_LIMB_MAX);
    remote->upperSkelAnime.baseTransl = kPlayerSkeletonBaseTranslation;
    SkelAnime_Init(
        play, &remote->bowArrowSkelAnime,
        reinterpret_cast<SkeletonHeader*>(const_cast<char*>(gArrowSkel)),
        reinterpret_cast<AnimationHeader*>(const_cast<char*>(gArrow2Anim)),
        nullptr, nullptr, 0);
    if (!record || !record->renderReady) {
        Actor_Kill(actor);
        return;
    }
    record->actor = remote;
    record->fishingLineInitialized = false;
    record->fishingSinkingLureInitialized = false;
    CopyStateToActor(remote, record->state, true);
    UpdateAnimation(remote, play, record->state);
    actor->focus.pos = actor->world.pos;
    actor->focus.pos.y += 55.0f;
    Error("Network game: spawned remote %s %d in scene %d room %d",
          remote->isCorpse ? "corpse for player" : "player", remote->playerId,
          record->state.sceneId, record->state.roomId);
}

void NativeRemotePlayerRenderer::ActorDestroy(Actor* actor, PlayState* play) {
    auto* renderer = sActive;
    auto* remote = reinterpret_cast<NativeRemotePlayer*>(actor);
    NativeRecord* record = nullptr;
    if (renderer) {
        if (remote->isCorpse) {
            const auto found = renderer->mImpl->dead.find(
                remote->presentationEntityKey);
            if (found != renderer->mImpl->dead.end()) {
                record = &found->second.presentation;
            }
        } else {
            const auto found = renderer->mImpl->live.find(
                remote->presentationEntityKey);
            if (found != renderer->mImpl->live.end()) record = &found->second;
        }
    }
    if (record && record->actor == remote) {
        record->actor = nullptr;
        record->fishingLineInitialized = false;
        record->fishingSinkingLureInitialized = false;
    }
    SkelAnime_Free(&remote->bowArrowSkelAnime, play);
}

void NativeRemotePlayerRenderer::ActorUpdate(Actor* actor, PlayState* play) {
    auto* renderer = sActive;
    auto* remote = reinterpret_cast<NativeRemotePlayer*>(actor);
    if (!renderer) {
        Actor_Kill(actor);
        return;
    }
    NativeRecord* record = nullptr;
    if (remote->isCorpse) {
        const auto found = renderer->mImpl->dead.find(
            remote->presentationEntityKey);
        if (found != renderer->mImpl->dead.end()) {
            record = &found->second.presentation;
        }
    } else {
        const auto found = renderer->mImpl->live.find(
            remote->presentationEntityKey);
        if (found != renderer->mImpl->live.end()) record = &found->second;
    }
    if (!record || !record->renderReady) {
        Actor_Kill(actor);
        return;
    }

    const auto& state = record->state;
    actor->prevPos = actor->world.pos;
    const auto entity = EntityFromKey(remote->presentationEntityKey);
    auto* replica = !remote->isCorpse && renderer->mImpl->players
                        ? renderer->mImpl->players->FindMutable(entity)
                        : nullptr;
    const auto motion = replica
                            ? replica->motion.Evaluate(NowSeconds())
                            : std::nullopt;
    if (motion) {
        actor->world.pos = {
            motion->position.x, motion->position.y, motion->position.z
        };
        actor->shape.rot.y = static_cast<int16_t>(
            std::lround(motion->headingRadians * kRadiansToBinaryAngle));
    } else {
        actor->world.pos = { state.x, state.y, state.z };
        actor->shape.rot.y = state.rotationY;
    }
    actor->shape.rot.x = state.rotationX;
    actor->shape.rot.z = state.rotationZ;
    actor->world.rot = actor->shape.rot;
    if ((state.stateFlags & NATIVE_PLAYER_DEAD) != 0) {
        CopyStateToActor(remote, state, false);
        UpdateAnimation(remote, play, state);
        return;
    }
    actor->focus.pos = actor->world.pos;
    actor->focus.pos.y += 55.0f;
    CopyStateToActor(remote, state, false);
    UpdateAnimation(remote, play, state);
}

void NativeRemotePlayerRenderer::ActorDraw(Actor* actor, PlayState* play) {
    auto* renderer = sActive;
    auto* remote = reinterpret_cast<NativeRemotePlayer*>(actor);
    if (!renderer) return;
    if (!remote->isCorpse &&
        remote->playerId == renderer->mImpl->localPlayerId) {
        return;
    }
    NativeRecord* record = nullptr;
    if (remote->isCorpse) {
        const auto found = renderer->mImpl->dead.find(
            remote->presentationEntityKey);
        if (found != renderer->mImpl->dead.end()) {
            record = &found->second.presentation;
        }
    } else {
        const auto found = renderer->mImpl->live.find(
            remote->presentationEntityKey);
        if (found != renderer->mImpl->live.end()) record = &found->second;
    }

    // Local players never own one of these presentation actors. Retain this
    // guard as a lifecycle safety invariant so a delayed actor cannot draw if
    // its identity becomes the local player during reconnect.
    if (!remote->isCorpse &&
        remote->playerId == renderer->mImpl->localPlayerId) {
        return;
    }

    NativePlayerPresentationState rendered{};
    if (record) {
        rendered = record->state;
        if (!remote->isCorpse && renderer->mImpl->players) {
            if (auto* replica = renderer->mImpl->players->FindMutable(
                    EntityFromKey(remote->presentationEntityKey))) {
                if (const auto fishing = replica->fishing.Evaluate(NowSeconds())) {
                    NativePlayerPresentationComposer::ApplyFishingPresentation(
                        rendered, *fishing);
                }
            }
        }
        if (!remote->isCorpse && renderer->mImpl->fishing) {
            NativePlayerPresentationComposer::ApplyAuthoritativeFishing(
                rendered, *renderer->mImpl->fishing);
        }
    }
    if (!remote->isCorpse && renderer->mImpl->players &&
        renderer->mImpl->corpses) {
        const auto entity = EntityFromKey(remote->presentationEntityKey);
        const auto* replica = renderer->mImpl->players->Find(entity);
        if (replica && replica->hasSnapshot &&
            renderer->mImpl->corpses->OwnsSource(
                entity, replica->snapshot.lifeEpoch)) {
            return;
        }
    }
    const uint8_t fishingState = record ? rendered.fishingState : 0;
    PlayerPresentationDrawData drawData = {
        remote->modelGroup,
        PLAYER_SHIELD_MIRROR,
        remote->itemAction,
        fishingState,
        static_cast<uint8_t>(record &&
                             (rendered.stateFlags & NATIVE_PLAYER_READY_TO_FIRE)),
        static_cast<uint8_t>(record &&
                             (rendered.stateFlags & NATIVE_PLAYER_SHIELDING)),
        { 0, 0 },
        remote->upperLimbRot,
        remote->headLimbRot,
        record ? rendered.fishingRodBendY : 0.0f,
        record ? rendered.fishingRodBendX : 0.0f,
        record ? rendered.fishingRodTwist : 0.0f,
        record ? rendered.fishingRodCastX : 0.0f,
        record ? rendered.bowStringScale : 0.0f,
        &remote->bowArrowSkelAnime,
    };
    REMOTE_PLAYER_OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C,
               reinterpret_cast<uintptr_t>(gCullBackDList));
    gSPClearGeometryMode(POLY_OPA_DISP++, G_CULL_BOTH);
    BeginRenderedPlayerCollision(remote->playerId);
    Player_DrawImpl(play, remote->skelAnime.skeleton, remote->jointTable,
                    remote->skelAnime.dListCount, 0, 0, Player_OverrideLimbDrawPresentation,
                    Player_PostLimbDrawPresentation, &drawData);
    for (int32_t limb = PLAYER_LIMB_ROOT; limb < PLAYER_LIMB_MAX; ++limb) {
        const Vec3f& origin = drawData.limbOrigins[limb];
        RecordRenderedPlayerCollisionLimb(remote->playerId, limb, origin.x,
                                          origin.y, origin.z);
    }
    EndRenderedPlayerCollision(remote->playerId);

    const auto* authoritativeLure =
        !remote->isCorpse && renderer->mImpl->fishing
            ? renderer->mImpl->fishing->LureForOwner(remote->playerId)
            : nullptr;
    if (remote->itemAction == PLAYER_IA_FISHING_POLE && record &&
        authoritativeLure && authoritativeLure->sceneId == play->sceneNum) {
        NativeRecord& native = *record;
        const NativePlayerPresentationState& state = rendered;
        Vec3f lure = {
            authoritativeLure->x, authoritativeLure->y, authoritativeLure->z
        };
        POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 0x14);
        gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, 55);
        const size_t spooled = std::min<size_t>(
            state.fishingLineSpooled, NETWORK_FISHING_LINE_POINT_COUNT - 1);
        Vec3f rodTip = drawData.fishingRodTip;
        const Vec3f lureDraw = {
            actor->world.pos.x + state.fishingLureDrawOffset[0],
            actor->world.pos.y + state.fishingLureDrawOffset[1],
            actor->world.pos.z + state.fishingLureDrawOffset[2],
        };
        if (!native.fishingLineInitialized) {
            const size_t activePoints = NETWORK_FISHING_LINE_POINT_COUNT - spooled;
            for (size_t point = 0; point < NETWORK_FISHING_LINE_POINT_COUNT;
                 ++point) {
                if (point <= spooled || activePoints <= 1) {
                    native.fishingLinePos[point] = rodTip;
                } else {
                    const float amount = static_cast<float>(point - spooled) /
                                         static_cast<float>(activePoints - 1);
                    native.fishingLinePos[point] = {
                        rodTip.x + (lure.x - rodTip.x) * amount,
                        rodTip.y + (lure.y - rodTip.y) * amount,
                        rodTip.z + (lure.z - rodTip.z) * amount,
                    };
                }
                native.fishingLineRot[point] = {};
                native.fishingLineUnk[point] = {};
            }
            native.fishingLineInitialized = true;
        }
        Fishing_UpdatePresentedLine(
            play, actor, &rodTip, &lure, native.fishingLinePos,
            native.fishingLineRot, native.fishingLineUnk,
            static_cast<int16_t>(spooled), state.fishingLureType,
            state.fishingLineGravity);
        const bool taut = state.fishingState == 4 &&
                          (state.fishingLineHooked != 0 ||
                           state.fishingLureType != 2);
        const size_t firstSegment = taut ? 0 : spooled;
        const size_t segmentLimit =
            taut ? 1 : NETWORK_FISHING_LINE_POINT_COUNT - 1;
        for (size_t point = firstSegment; point < segmentLimit; ++point) {
            const bool stockAttachment =
                !taut && point == NETWORK_FISHING_LINE_POINT_COUNT - 3 &&
                state.fishingLureType == 0 && state.fishingState == 3;
            const Vec3f lineStart = taut ? rodTip : native.fishingLinePos[point];
            if (taut || stockAttachment) {
                const Vec3f lineEnd = taut ? lure : lureDraw;
                const float dx = lineEnd.x - lineStart.x;
                const float dy = lineEnd.y - lineStart.y;
                const float dz = lineEnd.z - lineStart.z;
                const float horizontal = std::sqrt(dx * dx + dz * dz);
                const float distance =
                    std::sqrt(dx * dx + dy * dy + dz * dz);
                if (distance > 0.01f) {
                    Matrix_Translate(lineStart.x, lineStart.y, lineStart.z,
                                     MTXMODE_NEW);
                    Matrix_RotateY(std::atan2(dx, dz), MTXMODE_APPLY);
                    Matrix_RotateX(-std::atan2(dy, horizontal), MTXMODE_APPLY);
                    const float lineScale = state.fishingLineScale > 0.0f
                                                ? state.fishingLineScale
                                                : 0.0005f;
                    Matrix_Scale(lineScale, 1.0f,
                                 distance * 0.001f, MTXMODE_APPLY);
                    gSPMatrix(POLY_XLU_DISP++,
                              MATRIX_NEWMTX(play->state.gfxCtx),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPDisplayList(
                        POLY_XLU_DISP++,
                        reinterpret_cast<Gfx*>(
                            const_cast<char*>(gFishingLineModelDL)));
                }
            } else {
                Matrix_Translate(lineStart.x, lineStart.y, lineStart.z,
                                 MTXMODE_NEW);
                Matrix_RotateY(native.fishingLineRot[point].y, MTXMODE_APPLY);
                Matrix_RotateX(native.fishingLineRot[point].x, MTXMODE_APPLY);
                const float lineScale = state.fishingLineScale > 0.0f
                                            ? state.fishingLineScale
                                            : 0.0005f;
                Matrix_Scale(lineScale, 1.0f, 0.005f,
                             MTXMODE_APPLY);
                gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(
                    POLY_XLU_DISP++,
                    reinterpret_cast<Gfx*>(
                        const_cast<char*>(gFishingLineModelDL)));
            }
            if (stockAttachment) break;
        }

        if (state.fishingLureType == 2) {
            static constexpr float sizes[20] = {
                1.0f, 1.5f, 1.8f, 2.0f, 1.8f, 1.6f, 1.4f, 1.2f, 1.0f, 1.0f,
                0.9f, 0.85f, 0.8f, 0.7f, 0.8f, 1.0f, 1.2f, 1.1f, 1.0f, 0.8f,
            };
            const bool underwater = state.fishingSinkingLureUnderwater != 0;
            if (!native.fishingSinkingLureInitialized) {
                std::fill_n(native.fishingSinkingLurePos, 20, lure);
                native.fishingSinkingLureInitialized = true;
            }
            Fishing_UpdatePresentedSinkingLure(
                &lure, native.fishingSinkingLurePos, actor->shape.rot.y,
                state.fishingState, state.fishingSinkingLureUnderwater);
            if (underwater) {
                Gfx_SetupDL_25Opa(play->state.gfxCtx);
                gSPDisplayList(
                    POLY_OPA_DISP++,
                    reinterpret_cast<Gfx*>(const_cast<char*>(
                        gFishingSinkingLureSegmentMaterialDL)));
            } else {
                Gfx_SetupDL_25Xlu(play->state.gfxCtx);
                gSPDisplayList(
                    POLY_XLU_DISP++,
                    reinterpret_cast<Gfx*>(const_cast<char*>(
                        gFishingSinkingLureSegmentMaterialDL)));
            }
            for (int point = 19; point >= 0; --point) {
                const int sizeIndex =
                    point + state.fishingSinkingLureSegmentIndex;
                if (sizeIndex >= 20) continue;
                const Vec3f& position = native.fishingSinkingLurePos[point];
                Matrix_Translate(position.x, position.y, position.z,
                                 MTXMODE_NEW);
                const float scale = sizes[sizeIndex] * 0.04f;
                Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                Matrix_ReplaceRotation(&play->billboardMtxF);
                if (underwater) {
                    gSPMatrix(POLY_OPA_DISP++,
                              MATRIX_NEWMTX(play->state.gfxCtx),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPDisplayList(
                        POLY_OPA_DISP++,
                        reinterpret_cast<Gfx*>(const_cast<char*>(
                            gFishingSinkingLureSegmentModelDL)));
                } else {
                    gSPMatrix(POLY_XLU_DISP++,
                              MATRIX_NEWMTX(play->state.gfxCtx),
                              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPDisplayList(
                        POLY_XLU_DISP++,
                        reinterpret_cast<Gfx*>(const_cast<char*>(
                            gFishingSinkingLureSegmentModelDL)));
                }
            }
        } else {
            Matrix_Translate(lure.x, lure.y, lure.z, MTXMODE_NEW);
            Matrix_RotateY(state.fishingLureRot[1] + state.fishingLureSpin,
                           MTXMODE_APPLY);
            Matrix_RotateX(state.fishingLureRot[0], MTXMODE_APPLY);
            Matrix_Scale(0.004f, 0.004f, 0.004f, MTXMODE_APPLY);
            Matrix_Translate(0.0f, 0.0f, state.fishingLureZOffset,
                             MTXMODE_APPLY);
            Matrix_RotateZ(M_PI / 2.0f, MTXMODE_APPLY);
            Matrix_RotateY(M_PI / 2.0f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(
                POLY_OPA_DISP++,
                reinterpret_cast<Gfx*>(const_cast<char*>(gFishingLureFloatDL)));
            for (size_t hook = 0; hook < 2; ++hook) {
                Matrix_Translate(
                    actor->world.pos.x + state.fishingLureHookOffsets[hook][0],
                    actor->world.pos.y + state.fishingLureHookOffsets[hook][1],
                    actor->world.pos.z + state.fishingLureHookOffsets[hook][2],
                    MTXMODE_NEW);
                Matrix_RotateY(state.fishingLureHookRot[hook][1], MTXMODE_APPLY);
                Matrix_RotateX(state.fishingLureHookRot[hook][0], MTXMODE_APPLY);
                Matrix_Scale(0.004f, 0.004f, 0.005f, MTXMODE_APPLY);
                Matrix_RotateY(M_PI, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(
                    POLY_OPA_DISP++,
                    reinterpret_cast<Gfx*>(
                        const_cast<char*>(gFishingLureHookDL)));
                Matrix_RotateZ(M_PI / 2.0f, MTXMODE_APPLY);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx),
                          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(
                    POLY_OPA_DISP++,
                    reinterpret_cast<Gfx*>(
                        const_cast<char*>(gFishingLureHookDL)));
            }
        }
    }
    REMOTE_PLAYER_CLOSE_DISPS();
}

} // namespace Game::Multiplayer
