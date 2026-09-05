#include "RemotePlayerReplicaStore.h"
#include "../SequenceNumber.h"

#include <cmath>

namespace Game::Client {
RemotePlayerPresentationApplyResult RemotePlayerReplicaStore::ApplyLifecycle(
    const RemotePlayerPresentationState& state) {
    const auto previous = mLifetimes.FindPlayer(state.playerId);
    const int32_t previousScene = previous ? previous->sceneId : -1;
    const auto result = mLifetimes.Apply(state);
    if (!result.Applied()) return result;

    if (result.previousEntity) mReplicas.erase(Key(*result.previousEntity));
    if (result.update == RemotePlayerPresentationUpdate::Retired) {
        mReplicas.erase(Key(result.entity));
        return result;
    }

    RemotePlayerReplica& replica = mReplicas[Key(result.entity)];
    if (previousScene >= 0 && previousScene != state.sceneId) {
        replica = {};
    }
    replica.lifetime = state;
    return result;
}

bool RemotePlayerReplicaStore::ApplySnapshot(
    const Simulation::PlayerSnapshot& snapshot, double receivedSeconds) {
    if (!SnapshotIsSane(snapshot)) return false;
    const auto* lifetime = mLifetimes.FindPlayer(snapshot.ownerPlayerId);
    if (!lifetime || lifetime->entity != snapshot.entity ||
        lifetime->lifeEpoch != snapshot.lifeEpoch ||
        lifetime->sceneId != snapshot.sceneId) {
        return false;
    }
    auto found = mReplicas.find(Key(snapshot.entity));
    if (found == mReplicas.end()) return false;
    RemotePlayerReplica& replica = found->second;
    if (replica.hasSnapshot) {
        if (snapshot.lifeEpoch != replica.snapshot.lifeEpoch &&
            !Sequence::IsNewer(snapshot.lifeEpoch, replica.snapshot.lifeEpoch)) {
            return false;
        }
        if (!Sequence::IsNewer(snapshot.serverTick, replica.snapshot.serverTick)) {
            return false;
        }
    }
    if (!replica.motion.Push({ snapshot.sceneId, snapshot.serverTick,
                               snapshot.lifeEpoch, snapshot.position,
                               snapshot.velocity, snapshot.headingRadians },
                             receivedSeconds)) {
        return false;
    }
    replica.snapshot = snapshot;
    replica.hasSnapshot = true;
    if (snapshot.selectedWeapon != 4) replica.fishing.Reset();
    return true;
}

bool RemotePlayerReplicaStore::ApplyFishing(
    const Replication::FishingPresentationState& state, double receivedSeconds) {
    const auto* lifetime = mLifetimes.FindPlayer(state.playerId);
    if (!lifetime || lifetime->entity != state.entity ||
        lifetime->lifeEpoch != state.lifeEpoch ||
        lifetime->sceneId != state.sceneId) {
        return false;
    }
    auto found = mReplicas.find(Key(state.entity));
    return found != mReplicas.end() &&
           found->second.fishing.Push(state, receivedSeconds);
}

const RemotePlayerReplica* RemotePlayerReplicaStore::Find(
    Simulation::EntityId entity) const {
    const auto found = mReplicas.find(Key(entity));
    return found == mReplicas.end() ? nullptr : &found->second;
}

RemotePlayerReplica* RemotePlayerReplicaStore::FindMutable(
    Simulation::EntityId entity) {
    const auto found = mReplicas.find(Key(entity));
    return found == mReplicas.end() ? nullptr : &found->second;
}

const RemotePlayerReplica* RemotePlayerReplicaStore::FindPlayer(
    int32_t playerId) const {
    const auto* lifetime = mLifetimes.FindPlayer(playerId);
    return lifetime ? Find(lifetime->entity) : nullptr;
}

RemotePlayerReplica* RemotePlayerReplicaStore::FindPlayerMutable(int32_t playerId) {
    const auto* lifetime = mLifetimes.FindPlayer(playerId);
    return lifetime ? FindMutable(lifetime->entity) : nullptr;
}

const RemotePlayerPresentationState* RemotePlayerReplicaStore::FindByActorHandle(
    int16_t actorHandle) const {
    return mLifetimes.FindByActorHandle(actorHandle);
}

std::optional<int16_t> RemotePlayerReplicaStore::ActorHandleFor(
    Simulation::EntityId entity) const {
    return mLifetimes.ActorHandleFor(entity);
}

std::optional<Simulation::EntityId> RemotePlayerReplicaStore::EntityForActorHandle(
    int16_t actorHandle) const {
    return mLifetimes.EntityForActorHandle(actorHandle);
}

void RemotePlayerReplicaStore::Reset() {
    mLifetimes.Reset();
    mReplicas.clear();
}

uint64_t RemotePlayerReplicaStore::Key(Simulation::EntityId entity) {
    return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
}

bool RemotePlayerReplicaStore::SnapshotIsSane(
    const Simulation::PlayerSnapshot& snapshot) {
    constexpr uint16_t knownActions = Simulation::PLAYER_ACTION_PRIMARY |
                                      Simulation::PLAYER_ACTION_BLOCK |
                                      Simulation::PLAYER_ACTION_AIM |
                                      Simulation::PLAYER_ACTION_EVADE;
    const auto bounded = [](float value) {
        return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
    };
    return snapshot.entity.Valid() && snapshot.ownerPlayerId >= 0 &&
           snapshot.sceneId >= 0 && snapshot.sceneId < 4096 &&
           snapshot.serverTick != 0 && snapshot.lifeEpoch != 0 &&
           bounded(snapshot.position.x) && bounded(snapshot.position.y) &&
           bounded(snapshot.position.z) && bounded(snapshot.velocity.x) &&
           bounded(snapshot.velocity.y) && bounded(snapshot.velocity.z) &&
           bounded(snapshot.headingRadians) && bounded(snapshot.aimPitchRadians) &&
           (snapshot.heldActions & ~knownActions) == 0 &&
           snapshot.selectedWeapon <= 4 && snapshot.health <= 48 &&
           snapshot.actionState <= Simulation::PlayerActionState::SpinAttacking &&
           snapshot.actionStartTick <= snapshot.serverTick &&
           snapshot.team <= Simulation::TeamId::Green &&
           snapshot.locomotionMode <= Simulation::PlayerLocomotionMode::Climbing;
}

} // namespace Game::Client
