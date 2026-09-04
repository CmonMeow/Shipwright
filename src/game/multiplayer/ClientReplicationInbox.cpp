#include "ClientReplicationInbox.h"

#include "CombatNetworkAdapter.h"
#include "CorpseNetworkAdapter.h"
#include "FishingNetworkAdapter.h"
#include "PlayerLifecycleNetworkAdapter.h"
#include "PlayerSimulationNetworkAdapter.h"
#include "ProjectileNetworkAdapter.h"
#include "SceneNetworkAdapter.h"
#include "WorldPvpNetworkAdapter.h"
#include "platform/SequenceNumber.h"

#include <algorithm>

namespace Game::Multiplayer {

namespace {

Game::Simulation::EntityId PlayerEntity(const NetworkPlayerSnapshotPacket& packet) {
    return { packet.entityIndex, packet.entityGeneration };
}

Game::Simulation::EntityId PlayerEntity(const NetworkFishingPresentationPacket& packet) {
    return { packet.entityIndex, packet.entityGeneration };
}

} // namespace

void ClientReplicationInbox::Reset() {
    mPlayerSnapshots.Clear();
    mSceneEntryStates.clear();
    mObjectiveStates.Clear();
    mStrategicTopologyStates.Clear();
    mStructureStates.Clear();
    mCorpseStates.Clear();
    mFishingPresentations.Clear();
    mPlayerLifecycles.Clear();
    mFishStates.Clear();
    mLureStates.Clear();
    mProjectileStates.Clear();
    mProjectileIntentResults.clear();
    mCombatResults.clear();
    mPlayerRespawns.clear();
    mPlayerLifetimes.Reset();
    mFishLifetimes.Reset();
    mLureLifetimes.Reset();
    mProjectileLifetimes.Reset();
    mActivePlayerScenes.clear();
    mLatestPlayerSnapshotTicks.clear();
    mLatestPlayerLifeEpochs.clear();
    mLatestPlayerRespawnEpochs.clear();
    mLatestFishingPresentationSequences.clear();
    mLatestFishStateSequences.clear();
    mLatestLureStateSequences.clear();
    mLatestObjectiveStateSequences.clear();
    mLatestStrategicTopologyRevision = 0;
    mLatestStructureStateSequences.clear();
    mLatestCorpseStateSequences.clear();
    mLatestProjectileSequences.clear();
    mLatestCombatEventId = 0;
}

bool ClientReplicationInbox::AcceptPlayerLifecycle(
    const NetworkPlayerLifecyclePacket& packet) {
    const auto previous = mPlayerLifetimes.ActiveEntity(packet.playerId);
    const auto previousScene = mActivePlayerScenes.find(packet.playerId);
    if (!PlayerLifecycleNetworkAdapter::Apply(packet, mPlayerLifetimes)) return false;
    const Game::Simulation::EntityId entity{ packet.entityIndex, packet.entityGeneration };
    const bool replaced = previous && *previous != entity;
    const bool sceneChanged = packet.active &&
        previousScene != mActivePlayerScenes.end() &&
        previousScene->second != packet.sceneId;
    if (packet.active) {
        mActivePlayerScenes[packet.playerId] = packet.sceneId;
    } else {
        mActivePlayerScenes.erase(packet.playerId);
    }
    if (!packet.active || replaced) {
        mLatestPlayerSnapshotTicks.erase(packet.playerId);
        mLatestPlayerLifeEpochs.erase(packet.playerId);
        mLatestPlayerRespawnEpochs.erase(packet.playerId);
        mLatestFishingPresentationSequences.erase(packet.playerId);
        mPlayerSnapshots.Erase(packet.playerId);
        mFishingPresentations.Erase(packet.playerId);
        // Aggregate owner retirement invalidates pending active presentation,
        // but an already-admitted terminal state must remain observable. A
        // reliable release followed immediately by a scene leave otherwise
        // loses the removal before presentation can consume it.
        mFishStates.EraseIf(packet.playerId,
                            [](const Game::Client::RemoteFishEntity& state) {
                                return state.active;
                            });
        mLureStates.EraseIf(packet.playerId,
                            [](const Game::Client::RemoteLureEntity& state) {
                                return state.active;
                            });
    } else if (sceneChanged) {
        mLatestPlayerSnapshotTicks.erase(packet.playerId);
        mLatestFishingPresentationSequences.erase(packet.playerId);
        mPlayerSnapshots.Erase(packet.playerId);
        mFishingPresentations.Erase(packet.playerId);
        mFishStates.EraseIf(packet.playerId,
                            [](const Game::Client::RemoteFishEntity& state) {
                                return state.active;
                            });
        mLureStates.EraseIf(packet.playerId,
                            [](const Game::Client::RemoteLureEntity& state) {
                                return state.active;
                            });
    }
    QueuePlayerLifecycle(
        PlayerLifecycleNetworkAdapter::ToPresentationState(packet));
    return true;
}

bool ClientReplicationInbox::AcceptPlayerSnapshot(const NetworkPlayerSnapshotPacket& packet) {
    const auto activeScene = mActivePlayerScenes.find(packet.playerId);
    if (!PlayerSimulationNetworkAdapter::IsSane(packet) ||
        !mPlayerLifetimes.Matches(packet.playerId, PlayerEntity(packet)) ||
        activeScene == mActivePlayerScenes.end() ||
        activeScene->second != packet.sceneId) return false;
    const auto life = mLatestPlayerLifeEpochs.find(packet.playerId);
    if (life != mLatestPlayerLifeEpochs.end() && packet.lifeEpoch != life->second) {
        if (!Game::Sequence::IsNewer(packet.lifeEpoch, life->second)) return false;
        mLatestPlayerSnapshotTicks.erase(packet.playerId);
        mPlayerSnapshots.Erase(packet.playerId);
    }
    mLatestPlayerLifeEpochs[packet.playerId] = packet.lifeEpoch;
    const auto previous = mLatestPlayerSnapshotTicks.find(packet.playerId);
    if (previous != mLatestPlayerSnapshotTicks.end() &&
        !Game::Sequence::IsNewer(packet.serverTick, previous->second)) return false;
    mLatestPlayerSnapshotTicks[packet.playerId] = packet.serverTick;
    QueuePlayerSnapshot(PlayerSimulationNetworkAdapter::ToSnapshot(packet));
    return true;
}

bool ClientReplicationInbox::AcceptSceneEntryState(
    const NetworkSceneEntryStatePacket& packet, int32_t localPlayerId,
    uint32_t localLifeEpoch) {
    if (!SceneNetworkAdapter::IsSane(packet) || packet.playerId != localPlayerId ||
        (packet.requestSequence != 0 && packet.lifeEpoch != localLifeEpoch)) {
        return false;
    }
    if (packet.accepted) {
        const Game::Simulation::EntityId entity{
            packet.entityIndex, packet.entityGeneration
        };
        if (!mPlayerLifetimes.Matches(localPlayerId, entity)) return false;
        const auto previousScene = mActivePlayerScenes.find(localPlayerId);
        if (previousScene != mActivePlayerScenes.end() &&
            previousScene->second != packet.sceneId) {
            mLatestPlayerSnapshotTicks.erase(localPlayerId);
            mLatestFishingPresentationSequences.erase(localPlayerId);
            mPlayerSnapshots.Erase(localPlayerId);
            mFishingPresentations.Erase(localPlayerId);
        }
        // For an owner, the correlated scene-admission reply is the reliable
        // scope transition. Interest replication intentionally excludes self,
        // so no separate self lifecycle enter is required on remote clients.
        mActivePlayerScenes[localPlayerId] = packet.sceneId;
    }
    QueueSceneEntryState(SceneNetworkAdapter::ToAuthority(packet));
    return true;
}

bool ClientReplicationInbox::AcceptFishingPresentation(
    const NetworkFishingPresentationPacket& packet, int32_t localPlayerId) {
    const auto activeScene = mActivePlayerScenes.find(packet.playerId);
    if (packet.playerId == localPlayerId || !FishingNetworkAdapter::IsSane(packet) ||
        !mPlayerLifetimes.Matches(packet.playerId, PlayerEntity(packet)) ||
        activeScene == mActivePlayerScenes.end() ||
        activeScene->second != packet.sceneId) return false;
    const auto previous = mLatestFishingPresentationSequences.find(packet.playerId);
    if (previous != mLatestFishingPresentationSequences.end() &&
        !Game::Sequence::IsNewer(static_cast<uint32_t>(packet.sequence), previous->second)) return false;
    mLatestFishingPresentationSequences[packet.playerId] =
        static_cast<uint32_t>(packet.sequence);
    QueueFishingPresentation(FishingNetworkAdapter::ToState(packet));
    return true;
}

bool ClientReplicationInbox::AcceptFishState(const NetworkFishStatePacket& packet) {
    const auto activeScene = mActivePlayerScenes.find(packet.ownerPlayerId);
    if (!FishingNetworkAdapter::IsSane(packet) ||
        (packet.active &&
         (!mPlayerLifetimes.ActiveEntity(packet.ownerPlayerId) ||
          activeScene == mActivePlayerScenes.end() ||
          activeScene->second != packet.sceneId))) {
        return false;
    }
    const auto latest = mLatestFishStateSequences.find(packet.ownerPlayerId);
    if (latest != mLatestFishStateSequences.end() &&
        !Game::Sequence::IsNewer(packet.sequence, latest->second)) return false;
    if (!FishingNetworkAdapter::ApplyLifetime(packet, mFishLifetimes).Accepted()) return false;
    mLatestFishStateSequences[packet.ownerPlayerId] = packet.sequence;
    QueueFishState(FishingNetworkAdapter::ToRemoteEntity(packet));
    return true;
}

bool ClientReplicationInbox::AcceptLureState(const NetworkLureStatePacket& packet) {
    const auto activeScene = mActivePlayerScenes.find(packet.ownerPlayerId);
    if (!FishingNetworkAdapter::IsSane(packet) ||
        (packet.active &&
         (!mPlayerLifetimes.ActiveEntity(packet.ownerPlayerId) ||
          activeScene == mActivePlayerScenes.end() ||
          activeScene->second != packet.sceneId))) {
        return false;
    }
    const auto latest = mLatestLureStateSequences.find(packet.ownerPlayerId);
    if (latest != mLatestLureStateSequences.end() &&
        !Game::Sequence::IsNewer(packet.sequence, latest->second)) return false;
    if (!FishingNetworkAdapter::ApplyLifetime(packet, mLureLifetimes).Accepted()) return false;
    mLatestLureStateSequences[packet.ownerPlayerId] = packet.sequence;
    QueueLureState(FishingNetworkAdapter::ToRemoteEntity(packet));
    return true;
}

bool ClientReplicationInbox::AcceptProjectileLifecycle(
    const NetworkProjectileLifecyclePacket& packet) {
    const auto applied = ProjectileNetworkAdapter::ApplyLifecycle(packet, mProjectileLifetimes);
    if (!applied.Accepted()) return false;
    const auto keyFor = [&](Game::Simulation::EntityId entity) {
        return ProjectileLifetimeKey{ packet.playerId, packet.projectileId,
                                      packet.projectileKind, entity.index,
                                      entity.generation };
    };
    if (applied.kind == ProjectileNetworkAdapter::LifecycleApplyKind::Replaced &&
        applied.previousEntity) {
        mLatestProjectileSequences.erase(keyFor(*applied.previousEntity));
        mProjectileStates.Erase(
            { packet.playerId, packet.projectileId, packet.projectileKind });
    } else if (applied.kind == ProjectileNetworkAdapter::LifecycleApplyKind::Retired) {
        mLatestProjectileSequences.erase(keyFor(applied.entity));
        QueueProjectileState(
            ProjectileNetworkAdapter::ToRetiredPresentationState(packet));
    }
    return true;
}

bool ClientReplicationInbox::AcceptProjectileIntentResult(
    const NetworkProjectileIntentResultPacket& packet, uint32_t localLifeEpoch) {
    if (!ProjectileNetworkAdapter::IsSane(packet) || localLifeEpoch == 0 ||
        packet.lifeEpoch != localLifeEpoch) {
        return false;
    }
    QueueProjectileIntentResult(
        ProjectileNetworkAdapter::ToIntentDecision(packet));
    return true;
}

bool ClientReplicationInbox::AcceptProjectileState(
    const NetworkProjectileStatePacket& packet, int32_t localPlayerId) {
    if (!ProjectileNetworkAdapter::IsSane(packet)) return false;
    if (!ProjectileNetworkAdapter::MatchesActiveLifetime(packet, mProjectileLifetimes)) return false;
    if (packet.playerId == localPlayerId) {
        const bool terminalArrow = packet.projectileKind == NETWORK_PROJECTILE_ARROW &&
            (packet.active == 0 || packet.phase == NETWORK_ARROW_STUCK ||
             packet.phase == NETWORK_ARROW_BLOCKED);
        if (!terminalArrow) return false;
    }
    const ProjectileLifetimeKey key{ packet.playerId, packet.projectileId,
                                     packet.projectileKind, packet.entityIndex,
                                     packet.entityGeneration };
    const auto latest = mLatestProjectileSequences.find(key);
    if (latest != mLatestProjectileSequences.end() &&
        !Game::Sequence::IsNewer(packet.sequence, latest->second)) return false;
    mLatestProjectileSequences[key] = packet.sequence;
    QueueProjectileState(ProjectileNetworkAdapter::ToPresentationState(packet));
    return true;
}

bool ClientReplicationInbox::AcceptCombatResult(const NetworkCombatResultPacket& packet) {
    if (!CombatNetworkAdapter::IsSane(packet) ||
        !CombatNetworkAdapter::MatchesActiveLifetimes(packet, mPlayerLifetimes)) return false;
    if (mLatestCombatEventId != 0 &&
        !Game::Sequence::IsNewer(packet.eventId, mLatestCombatEventId)) {
        return false;
    }
    mLatestCombatEventId = packet.eventId;
    QueueCombatResult(CombatNetworkAdapter::ToEvent(packet));
    return true;
}

bool ClientReplicationInbox::AcceptPlayerRespawn(
    const NetworkPlayerRespawnPacket& packet) {
    const auto activeScene = mActivePlayerScenes.find(packet.playerId);
    if (!PlayerLifecycleNetworkAdapter::MatchesActiveLifetime(packet, mPlayerLifetimes) ||
        activeScene == mActivePlayerScenes.end() ||
        activeScene->second != packet.sceneId) {
        return false;
    }
    const auto previousRespawn = mLatestPlayerRespawnEpochs.find(packet.playerId);
    if (previousRespawn != mLatestPlayerRespawnEpochs.end() &&
        !Game::Sequence::IsNewer(packet.lifeEpoch, previousRespawn->second)) {
        return false;
    }
    const auto life = mLatestPlayerLifeEpochs.find(packet.playerId);
    if (life != mLatestPlayerLifeEpochs.end() && packet.lifeEpoch != life->second &&
        !Game::Sequence::IsNewer(packet.lifeEpoch, life->second)) {
        return false;
    }
    if (life == mLatestPlayerLifeEpochs.end() || packet.lifeEpoch != life->second) {
        mLatestPlayerSnapshotTicks.erase(packet.playerId);
        mPlayerSnapshots.Erase(packet.playerId);

        // A respawn starts a new local incarnation. Projectile decisions do
        // not retain a life epoch after wire admission, so none may cross this
        // boundary and accidentally acknowledge a reused new-life sequence.
        mProjectileIntentResults.clear();

        // Gameplay presentation may be paused while dead, leaving reliable
        // combat events queued. Events involving the old incarnation are no
        // longer meaningful after its authoritative respawn.
        std::erase_if(mCombatResults, [&packet](const auto& event) {
            return event.sourcePlayerId == packet.playerId ||
                   event.targetPlayerId == packet.playerId;
        });
    }
    mLatestPlayerLifeEpochs[packet.playerId] = packet.lifeEpoch;
    mLatestPlayerRespawnEpochs[packet.playerId] = packet.lifeEpoch;
    QueuePlayerRespawn(PlayerLifecycleNetworkAdapter::ToRespawnEvent(packet));
    return true;
}

bool ClientReplicationInbox::AcceptObjectiveState(
    const NetworkObjectiveStatePacket& packet) {
    if (!WorldPvpNetworkAdapter::IsSane(packet)) return false;
    const auto latest = mLatestObjectiveStateSequences.find(packet.objectiveKey);
    if (latest != mLatestObjectiveStateSequences.end() &&
        !Game::Sequence::IsNewer(packet.sequence, latest->second)) {
        return false;
    }
    mLatestObjectiveStateSequences[packet.objectiveKey] = packet.sequence;
    QueueObjectiveState(WorldPvpNetworkAdapter::ToClientState(packet));
    return true;
}

bool ClientReplicationInbox::AcceptStrategicTopology(
    const NetworkStrategicTopologyPacket& packet) {
    if (!WorldPvpNetworkAdapter::IsSane(packet) ||
        (mLatestStrategicTopologyRevision != 0 &&
         !Game::Sequence::IsNewer(packet.revision,
                                  mLatestStrategicTopologyRevision))) {
        return false;
    }
    mLatestStrategicTopologyRevision = packet.revision;
    QueueStrategicTopology(WorldPvpNetworkAdapter::ToClientState(packet));
    return true;
}

bool ClientReplicationInbox::AcceptStructureState(
    const NetworkStructureStatePacket& packet) {
    if (!WorldPvpNetworkAdapter::IsSane(packet)) return false;
    const auto latest = mLatestStructureStateSequences.find(packet.structureKey);
    if (latest != mLatestStructureStateSequences.end() &&
        !Game::Sequence::IsNewer(packet.sequence, latest->second)) {
        return false;
    }
    mLatestStructureStateSequences[packet.structureKey] = packet.sequence;
    QueueStructureState(WorldPvpNetworkAdapter::ToClientState(packet));
    return true;
}

bool ClientReplicationInbox::AcceptCorpseState(const NetworkCorpseStatePacket& packet) {
    if (!CorpseNetworkAdapter::IsSane(packet)) return false;
    const auto latest = mLatestCorpseStateSequences.find(packet.entityIndex);
    if (latest != mLatestCorpseStateSequences.end() &&
        !Game::Sequence::IsNewer(packet.sequence, latest->second)) {
        return false;
    }
    mLatestCorpseStateSequences[packet.entityIndex] = packet.sequence;
    QueueCorpseState(CorpseNetworkAdapter::ToPresentationState(packet));
    return true;
}

void ClientReplicationInbox::QueueSceneEntryState(
    const Game::Client::LocalSceneAuthority& authority) {
    QueueEvent(mSceneEntryStates, authority);
}

void ClientReplicationInbox::QueuePlayerRespawn(
    const Game::Simulation::PlayerRespawnEvent& event) {
    QueueEvent(mPlayerRespawns, event);
}

void ClientReplicationInbox::QueueCombatResult(
    const Game::Simulation::CombatResultEvent& event) {
    QueueEvent(mCombatResults, event);
}

void ClientReplicationInbox::QueuePlayerLifecycle(
    const Game::Client::RemotePlayerPresentationState& state) {
    mPlayerLifecycles.Push(state.playerId, state);
}

void ClientReplicationInbox::QueueFishState(
    const Game::Client::RemoteFishEntity& state) {
    mFishStates.Push(state.ownerPlayerId, state);
}

void ClientReplicationInbox::QueueLureState(
    const Game::Client::RemoteLureEntity& state) {
    mLureStates.Push(state.ownerPlayerId, state);
}

void ClientReplicationInbox::QueueProjectileState(
    const Game::Client::RemoteProjectileReplicaState& state) {
    mProjectileStates.Push(
        { state.logicalId.ownerPlayerId, state.logicalId.projectileId,
          state.logicalId.projectileKind }, state);
}

void ClientReplicationInbox::QueueCorpseState(
    const Game::Client::CorpsePresentationState& state) {
    mCorpseStates.Push(state.entity.index, state);
}

void ClientReplicationInbox::QueuePlayerSnapshot(
    const Game::Simulation::PlayerSnapshot& snapshot) {
    mPlayerSnapshots.Push(snapshot.ownerPlayerId, snapshot);
}

void ClientReplicationInbox::QueueFishingPresentation(
    const Game::Replication::FishingPresentationState& state) {
    mFishingPresentations.Push(state.playerId, state);
}

void ClientReplicationInbox::QueueObjectiveState(
    const Game::Client::ReplicatedObjectiveState& state) {
    mObjectiveStates.Push(state.snapshot.objectiveKey, state);
}

void ClientReplicationInbox::QueueStrategicTopology(
    const Game::Client::ReplicatedStrategicTopologyState& state) {
    mStrategicTopologyStates.Push(0, state);
}

void ClientReplicationInbox::QueueStructureState(
    const Game::Client::ReplicatedStructureState& state) {
    mStructureStates.Push(state.snapshot.structureKey, state);
}

void ClientReplicationInbox::QueueProjectileIntentResult(
    const Game::Client::LocalProjectileIntentDecision& decision) {
    QueueEvent(mProjectileIntentResults, decision);
}

#define DEFINE_POLL(Type, Member) \
    bool ClientReplicationInbox::Poll(Type& packet) { return PollQueue(Member, packet); }

DEFINE_POLL(Game::Client::LocalSceneAuthority, mSceneEntryStates)
DEFINE_POLL(Game::Simulation::CombatResultEvent, mCombatResults)
DEFINE_POLL(Game::Simulation::PlayerRespawnEvent, mPlayerRespawns)
DEFINE_POLL(Game::Client::LocalProjectileIntentDecision, mProjectileIntentResults)

#undef DEFINE_POLL

bool ClientReplicationInbox::Poll(Game::Simulation::PlayerSnapshot& snapshot) {
    return mPlayerSnapshots.Poll(snapshot);
}

bool ClientReplicationInbox::Poll(
    Game::Client::RemotePlayerPresentationState& state) {
    return mPlayerLifecycles.Poll(state);
}

bool ClientReplicationInbox::Poll(
    Game::Replication::FishingPresentationState& state) {
    return mFishingPresentations.Poll(state);
}

bool ClientReplicationInbox::Poll(Game::Client::ReplicatedObjectiveState& state) {
    return mObjectiveStates.Poll(state);
}

bool ClientReplicationInbox::Poll(
    Game::Client::ReplicatedStrategicTopologyState& state) {
    return mStrategicTopologyStates.Poll(state);
}

bool ClientReplicationInbox::Poll(Game::Client::ReplicatedStructureState& state) {
    return mStructureStates.Poll(state);
}

bool ClientReplicationInbox::Poll(Game::Client::RemoteFishEntity& state) {
    return mFishStates.Poll(state);
}

bool ClientReplicationInbox::Poll(Game::Client::RemoteLureEntity& state) {
    return mLureStates.Poll(state);
}

bool ClientReplicationInbox::Poll(
    Game::Client::RemoteProjectileReplicaState& state) {
    return mProjectileStates.Poll(state);
}

bool ClientReplicationInbox::Poll(Game::Client::CorpsePresentationState& state) {
    return mCorpseStates.Poll(state);
}

} // namespace Game::Multiplayer
