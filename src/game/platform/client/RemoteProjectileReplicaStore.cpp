#include "RemoteProjectileReplicaStore.h"
#include "../SequenceNumber.h"

#include <cmath>
#include <limits>

namespace Game::Client {
RemoteProjectilePresentationApplyResult RemoteProjectileReplicaStore::Apply(
    const RemoteProjectileReplicaState& state, double receivedSeconds) {
    if (!IsSane(state) || !std::isfinite(receivedSeconds)) return {};
    const auto exact = mReplicas.find(Key(state.entity));
    if (state.active && exact != mReplicas.end()) {
        if (exact->second.state.Terminal() ||
            !Sequence::IsNewer(state.sequence, exact->second.state.sequence)) {
            return {};
        }
    }

    const auto result = mLifetimes.Apply({
        state.entity, state.logicalId, state.sceneId, state.active,
    });
    if (!result.Applied()) return result;
    if (result.previousEntity) mReplicas.erase(Key(*result.previousEntity));
    if (result.update == RemoteProjectilePresentationUpdate::Retired) {
        mReplicas.erase(Key(result.entity));
        return result;
    }

    RemoteProjectileReplica& replica = mReplicas[Key(result.entity)];
    const RemoteProjectileSample sample{
        state.sceneId, state.sequence, static_cast<uint8_t>(state.phase),
        state.Terminal(), state.position, state.velocity,
        state.rotationX, state.rotationY, state.rotationZ,
    };
    if (!replica.motion.Push(sample, receivedSeconds)) return {};
    replica.state = state;
    return result;
}

std::vector<Simulation::EntityId> RemoteProjectileReplicaStore::RetireOwner(
    int32_t ownerPlayerId) {
    auto retired = mLifetimes.RetireOwner(ownerPlayerId);
    for (const auto entity : retired) mReplicas.erase(Key(entity));
    return retired;
}

const RemoteProjectileReplica* RemoteProjectileReplicaStore::Find(
    Simulation::EntityId entity) const {
    const auto found = mReplicas.find(Key(entity));
    return found == mReplicas.end() ? nullptr : &found->second;
}

RemoteProjectileReplica* RemoteProjectileReplicaStore::FindMutable(
    Simulation::EntityId entity) {
    const auto found = mReplicas.find(Key(entity));
    return found == mReplicas.end() ? nullptr : &found->second;
}

const RemoteProjectileReplica* RemoteProjectileReplicaStore::FindLogical(
    Replication::ProjectileLogicalId logicalId) const {
    const auto* lifetime = mLifetimes.FindLogical(logicalId);
    return lifetime ? Find(lifetime->entity) : nullptr;
}

const RemoteProjectilePresentationState* RemoteProjectileReplicaStore::FindByActorHandle(
    int16_t actorHandle) const {
    return mLifetimes.FindByActorHandle(actorHandle);
}

std::optional<int16_t> RemoteProjectileReplicaStore::ActorHandleFor(
    Simulation::EntityId entity) const {
    return mLifetimes.ActorHandleFor(entity);
}

std::optional<Simulation::EntityId> RemoteProjectileReplicaStore::EntityForActorHandle(
    int16_t actorHandle) const {
    return mLifetimes.EntityForActorHandle(actorHandle);
}

void RemoteProjectileReplicaStore::Reset() {
    mLifetimes.Reset();
    mReplicas.clear();
}

uint64_t RemoteProjectileReplicaStore::Key(Simulation::EntityId entity) {
    return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
}

bool RemoteProjectileReplicaStore::IsSane(
    const RemoteProjectileReplicaState& state) {
    const auto bounded = [](float value) {
        return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
    };
    const bool kindIsArrow = state.logicalId.projectileKind == 0;
    return state.entity.Valid() && state.logicalId.ownerPlayerId >= 0 &&
           state.logicalId.projectileId > 0 && kindIsArrow &&
           state.sceneId >= 0 && state.sceneId < 256 &&
           (!state.active || state.sequence != 0) && state.projectileType <= 8 &&
           state.phase <= RemoteProjectilePhase::ArrowBlocked && bounded(state.position.x) &&
           bounded(state.position.y) && bounded(state.position.z) &&
           bounded(state.velocity.x) && bounded(state.velocity.y) &&
           bounded(state.velocity.z);
}

} // namespace Game::Client
