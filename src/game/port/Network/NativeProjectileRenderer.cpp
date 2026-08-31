#include "NativeProjectileRenderer.h"

#include "../ActorDB.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"
#include "global.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

#include <algorithm>
#include <chrono>
#include <map>

namespace SoH::Network {
namespace {

struct NativeRemoteProjectile {
    Actor actor;
    SkelAnime skelAnime;
    uint64_t presentationEntityKey = 0;
    uint8_t lastPhase = 0xFF;
    uint8_t phaseAge = 0;
};

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

} // namespace

struct NativeProjectileRenderer::Impl {
    struct Record {
        NativeRemoteProjectile* actor = nullptr;
    };

    Game::Client::RemoteProjectileReplicaStore* replicas = nullptr;
    std::map<uint64_t, Record> records;
    int16_t actorId = -1;
};

NativeProjectileRenderer* NativeProjectileRenderer::sActive = nullptr;

NativeProjectileRenderer::NativeProjectileRenderer() : mImpl(std::make_unique<Impl>()) {
    sActive = this;
}

NativeProjectileRenderer::~NativeProjectileRenderer() {
    DetachAfterSceneShutdown();
    if (sActive == this) sActive = nullptr;
}

void NativeProjectileRenderer::Bind(
    Game::Client::RemoteProjectileReplicaStore* replicas) {
    mImpl->replicas = replicas;
}

void NativeProjectileRenderer::RegisterActorType() {
    if (!ActorDB::Instance || mImpl->actorId >= 0) return;
    ActorDBInit init;
    init.name = "NETWORK_REMOTE_PROJECTILE";
    init.desc = "Non-gameplay authoritative projectile presentation";
    init.category = ACTORCAT_ITEMACTION;
    init.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_LOCK_ON_DISABLED |
                 ACTOR_FLAG_IGNORE_QUAKE;
    init.objectId = OBJECT_GAMEPLAY_KEEP;
    init.instanceSize = sizeof(NativeRemoteProjectile);
    init.init = ActorInit;
    init.destroy = ActorDestroy;
    init.update = ActorUpdate;
    init.draw = ActorDraw;
    mImpl->actorId = static_cast<int16_t>(ActorDB::Instance->AddEntry(init).entry.id);
}

void NativeProjectileRenderer::Track(Game::Simulation::EntityId entity) {
    if (!entity.Valid()) return;
    mImpl->records.try_emplace(EntityKey(entity));
}

void NativeProjectileRenderer::Retire(Game::Simulation::EntityId entity) {
    const auto found = mImpl->records.find(EntityKey(entity));
    if (found == mImpl->records.end()) return;
    if (found->second.actor && found->second.actor->actor.update) {
        Actor_Kill(&found->second.actor->actor);
    }
    mImpl->records.erase(found);
}

void NativeProjectileRenderer::Reconcile(PlayState* play) {
    if (!play || !mImpl->replicas) return;
    for (auto it = mImpl->records.begin(); it != mImpl->records.end();) {
        Impl::Record& record = it->second;
        const auto entity = EntityFromKey(it->first);
        const auto* replica = mImpl->replicas->Find(entity);
        if (!replica || !replica->state.active) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            it = mImpl->records.erase(it);
            continue;
        }
        if (replica->state.sceneId != play->sceneNum) {
            if (record.actor && record.actor->actor.update) {
                Actor_Kill(&record.actor->actor);
            }
            ++it;
            continue;
        }
        const auto actorHandle = mImpl->replicas->ActorHandleFor(entity);
        if (!record.actor && actorHandle && mImpl->actorId >= 0) {
            Actor_Spawn(&play->actorCtx, play, mImpl->actorId,
                        replica->state.position.x, replica->state.position.y,
                        replica->state.position.z, replica->state.rotationX,
                        replica->state.rotationY, replica->state.rotationZ,
                        *actorHandle);
        }
        ++it;
    }
}

void NativeProjectileRenderer::Reset() {
    for (auto& [key, record] : mImpl->records) {
        (void)key;
        if (record.actor && record.actor->actor.update) {
            Actor_Kill(&record.actor->actor);
        }
    }
    mImpl->records.clear();
    mImpl->replicas = nullptr;
}

void NativeProjectileRenderer::DetachAfterSceneShutdown() {
    // Scene teardown has already destroyed the native Actors. Their pointers
    // must not be dereferenced during global shutdown.
    mImpl->records.clear();
    mImpl->replicas = nullptr;
}

void NativeProjectileRenderer::ActorInit(Actor* actor, PlayState* play) {
    auto* renderer = sActive;
    auto* projectile = reinterpret_cast<NativeRemoteProjectile*>(actor);
    projectile->presentationEntityKey = 0;
    projectile->lastPhase = 0xFF;
    projectile->phaseAge = 0;
    if (renderer && renderer->mImpl->replicas) {
        if (const auto entity =
                renderer->mImpl->replicas->EntityForActorHandle(actor->params)) {
            projectile->presentationEntityKey = EntityKey(*entity);
        }
    }
    actor->room = -1;
    actor->uncullZoneForward = 32000.0f;
    actor->uncullZoneScale = 100.0f;
    actor->uncullZoneDownward = 100.0f;
    ActorShape_Init(&actor->shape, 0.0f, nullptr, 0.0f);
    SkelAnime_Init(play, &projectile->skelAnime,
                   reinterpret_cast<SkeletonHeader*>(const_cast<char*>(gArrowSkel)),
                   reinterpret_cast<AnimationHeader*>(const_cast<char*>(gArrow2Anim)),
                   nullptr, nullptr, 0);
    if (!renderer || !renderer->mImpl->replicas) {
        Actor_Kill(actor);
        return;
    }
    const auto found = renderer->mImpl->records.find(projectile->presentationEntityKey);
    const auto* replica = renderer->mImpl->replicas->Find(
        EntityFromKey(projectile->presentationEntityKey));
    if (found == renderer->mImpl->records.end() || !replica ||
        !replica->state.active) {
        Actor_Kill(actor);
        return;
    }
    found->second.actor = projectile;
}

void NativeProjectileRenderer::ActorDestroy(Actor* actor, PlayState* play) {
    auto* renderer = sActive;
    auto* projectile = reinterpret_cast<NativeRemoteProjectile*>(actor);
    if (renderer) {
        const auto found =
            renderer->mImpl->records.find(projectile->presentationEntityKey);
        if (found != renderer->mImpl->records.end() &&
            found->second.actor == projectile) {
            found->second.actor = nullptr;
        }
    }
    SkelAnime_Free(&projectile->skelAnime, play);
}

void NativeProjectileRenderer::ActorUpdate(Actor* actor, PlayState* play) {
    auto* renderer = sActive;
    auto* projectile = reinterpret_cast<NativeRemoteProjectile*>(actor);
    if (!renderer || !renderer->mImpl->replicas ||
        renderer->mImpl->records.count(projectile->presentationEntityKey) == 0) {
        Actor_Kill(actor);
        return;
    }
    auto* replica = renderer->mImpl->replicas->FindMutable(
        EntityFromKey(projectile->presentationEntityKey));
    if (!replica || !replica->state.active) {
        Actor_Kill(actor);
        return;
    }
    const auto& state = replica->state;
    const uint8_t phase = static_cast<uint8_t>(state.phase);
    if (state.phase == Game::Client::RemoteProjectilePhase::ArrowStuck &&
        projectile->lastPhase != phase) {
        Audio_PlayActorSound2(actor, NA_SE_IT_ARROW_STICK_CRE);
    }
    projectile->phaseAge = projectile->lastPhase == phase
                               ? static_cast<uint8_t>(std::min<int>(projectile->phaseAge + 1, 255))
                               : 0;
    projectile->lastPhase = phase;
    actor->prevPos = actor->world.pos;
    const auto pose = replica->motion.Evaluate(NowSeconds());
    if (pose) {
        actor->world.pos = { pose->position.x, pose->position.y, pose->position.z };
        actor->world.rot = actor->shape.rot = {
            pose->rotationX, pose->rotationY, pose->rotationZ
        };
    } else {
        actor->world.pos = {
            state.position.x, state.position.y, state.position.z
        };
        actor->world.rot = actor->shape.rot = {
            state.rotationX, state.rotationY, state.rotationZ
        };
    }
}

void NativeProjectileRenderer::ActorDraw(Actor* actor, PlayState* play) {
    auto* renderer = sActive;
    auto* projectile = reinterpret_cast<NativeRemoteProjectile*>(actor);
    if (!renderer || !renderer->mImpl->replicas) return;
    const auto* replica = renderer->mImpl->replicas->Find(
        EntityFromKey(projectile->presentationEntityKey));
    if (!replica) return;
    SkelAnime_DrawLod(play, projectile->skelAnime.skeleton,
                      projectile->skelAnime.jointTable, nullptr, nullptr,
                      projectile, 0);
}

} // namespace SoH::Network
