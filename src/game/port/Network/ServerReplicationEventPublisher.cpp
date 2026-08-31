#include "ServerReplicationEventPublisher.h"

#include "CombatNetworkAdapter.h"
#include "FishingNetworkAdapter.h"
#include "PlayerLifecycleNetworkAdapter.h"
#include "PlayerSimulationNetworkAdapter.h"
#include "ProjectileNetworkAdapter.h"
#include "WorldPvpNetworkAdapter.h"

#include <sysdef.h>

#include <algorithm>
#include <set>
#include <utility>

namespace SoH::Network {

namespace {

const NetMsgFlags kReliable =
    static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

} // namespace

ServerReplicationEventPublisher::ServerReplicationEventPublisher(
    Game::Simulation::ServerWorld& world,
    Game::Replication::ServerReplicationCoordinator& replication,
    ClientReplicationInbox& clientInbox,
    ServerReplicationInterestPublisher& interestPublisher)
    : mWorld(world), mReplication(replication), mClientInbox(clientInbox),
      mInterestPublisher(interestPublisher),
      mPublishedStrategicSites(world.StrategicSites()),
      mPublishedSupplyRoutes(world.SupplyRoutes()),
      mPublishedInfluenceAdjacencies(world.InfluenceAdjacencies()) {
}

void ServerReplicationEventPublisher::PublishStrategicTopology() {
    const auto sites = mWorld.StrategicSites();
    const auto routes = mWorld.SupplyRoutes();
    const auto adjacencies = mWorld.InfluenceAdjacencies();
    if (sites == mPublishedStrategicSites && routes == mPublishedSupplyRoutes &&
        adjacencies == mPublishedInfluenceAdjacencies) {
        return;
    }
    mPublishedStrategicSites = sites;
    mPublishedSupplyRoutes = routes;
    mPublishedInfluenceAdjacencies = adjacencies;
    if (++mStrategicTopologyRevision == 0) ++mStrategicTopologyRevision;
    for (const auto& player : mWorld.PlayerSnapshots()) {
        PublishStrategicTopologyTo(player.ownerPlayerId);
    }
}

void ServerReplicationEventPublisher::PublishStrategicTopologyTo(
    int32_t observer) {
    if (observer < 0) return;
    const NetworkStrategicTopologyPacket packet =
        WorldPvpNetworkAdapter::ToPacket(mPublishedStrategicSites,
                                         mPublishedSupplyRoutes,
                                         mPublishedInfluenceAdjacencies,
                                         mStrategicTopologyRevision);
    if (!WorldPvpNetworkAdapter::IsSane(packet)) return;
    if (observer == 0) {
        mClientInbox.AcceptStrategicTopology(packet);
        return;
    }
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, packet);
    const Game::Replication::ReplicationStreamKey streamKey{
        static_cast<uint16_t>(NAMTStrategicTopology), 0, {}, 0
    };
    Deliver(observer, NAMTStrategicTopology, raw, kReliable, streamKey);
}

void ServerReplicationEventPublisher::SetDelivery(
    ServerReplicationDelivery delivery) {
    mDelivery = std::move(delivery);
}

void ServerReplicationEventPublisher::Deliver(
    int32_t observer, NetAppMessageType type, const NetworkMessageRaw& raw,
    NetMsgFlags flags, Game::Replication::ReplicationStreamKey streamKey) const {
    if (observer > 0 && mDelivery.send) {
        mDelivery.send(observer, type, raw, flags, streamKey);
    }
}

void ServerReplicationEventPublisher::DeliverOwnedEntity(
    NetAppMessageType type, const NetworkMessageRaw& raw,
    const Game::Replication::OwnedEntityKey& entity, bool includeOwner,
    NetMsgFlags flags, Game::Replication::ReplicationStreamKey streamKey) const {
    bool ownerIndexed = false;
    for (const int32_t observer : mReplication.OwnedEntityObservers(entity)) {
        if (observer == entity.ownerPlayerId) {
            ownerIndexed = true;
            if (!includeOwner) continue;
        }
        Deliver(observer, type, raw, flags, streamKey);
    }
    if (includeOwner && entity.ownerPlayerId > 0 && !ownerIndexed &&
        mDelivery.connectedPlayer && mDelivery.connectedPlayer(entity.ownerPlayerId)) {
        Deliver(entity.ownerPlayerId, type, raw, flags, streamKey);
    }
}

void ServerReplicationEventPublisher::PublishPlayerSnapshots() {
    const auto snapshots = mWorld.PlayerSnapshots();
    mInterestPublisher.RefreshPlayers(snapshots);
    mInterestPublisher.RefreshSpatialEntities();

    for (const auto& authoritative : snapshots) {
        const NetworkPlayerSnapshotPacket packet =
            PlayerSimulationNetworkAdapter::ToPacket(authoritative);
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        const Game::Replication::ReplicationStreamKey streamKey{
            static_cast<uint16_t>(NAMTPlayerSnapshot), packet.playerId,
            authoritative.entity, 0
        };
        if (packet.playerId > 0) {
            Deliver(packet.playerId, NAMTPlayerSnapshot, raw,
                    NMFHighPriority, streamKey);
        }
        bool hostVisible = packet.playerId == 0;
        for (const int32_t observer : mReplication.PlayerObservers(packet.playerId)) {
            if (observer == 0) {
                hostVisible = true;
            } else if (observer != packet.playerId) {
                Deliver(observer, NAMTPlayerSnapshot, raw,
                        NMFHighPriority, streamKey);
            }
        }
        if (hostVisible) mClientInbox.AcceptPlayerSnapshot(packet);
    }
}

void ServerReplicationEventPublisher::PublishObjectiveSnapshots() {
    for (const auto& event : mWorld.DrainObjectiveCapturedEvents()) {
        Error("Objective %d captured: team %u -> %u", event.objectiveKey,
              static_cast<unsigned>(event.previousOwner),
              static_cast<unsigned>(event.newOwner));
    }
    for (const auto& objective : mWorld.ObjectiveSnapshots()) {
        const Game::Replication::SpatialEntityKey relevanceKey{
            Game::Replication::SpatialEntityKind::Objective, objective.objectiveKey
        };
        const Game::Replication::ReplicationStreamKey streamKey{
            static_cast<uint16_t>(NAMTObjectiveState), objective.objectiveKey,
            objective.entity, 0
        };
        for (const int32_t observer :
             mReplication.SpatialEntityObservers(relevanceKey)) {
            const NetworkObjectiveStatePacket packet =
                WorldPvpNetworkAdapter::ToPacket(
                    objective,
                    mReplication.NextSpatialEntityStateSequence(observer, relevanceKey));
            if (observer == 0) {
                mClientInbox.AcceptObjectiveState(packet);
            } else {
                NetworkMessageRaw raw;
                EncodeAppPacketRaw(raw, packet);
                Deliver(observer, NAMTObjectiveState, raw, NMFNone, streamKey);
            }
        }
    }
}

void ServerReplicationEventPublisher::PublishStructureSnapshots() {
    std::set<int32_t> changedStructures;
    for (const auto& event : mWorld.DrainStructureEvents()) {
        changedStructures.insert(event.structureKey);
        Error("Structure %d event %u from team %u amount %u", event.structureKey,
              static_cast<unsigned>(event.kind),
              static_cast<unsigned>(event.sourceTeam), event.amount);
    }
    for (const auto& structure : mWorld.StructureSnapshots()) {
        const Game::Replication::SpatialEntityKey relevanceKey{
            Game::Replication::SpatialEntityKind::Structure, structure.structureKey
        };
        const Game::Replication::ReplicationStreamKey streamKey{
            static_cast<uint16_t>(NAMTStructureState), structure.structureKey,
            structure.entity, 0
        };
        for (const int32_t observer :
             mReplication.SpatialEntityObservers(relevanceKey)) {
            const NetworkStructureStatePacket packet =
                WorldPvpNetworkAdapter::ToPacket(
                    structure,
                    mReplication.NextSpatialEntityStateSequence(observer, relevanceKey));
            if (observer == 0) {
                mClientInbox.AcceptStructureState(packet);
            } else {
                NetworkMessageRaw raw;
                EncodeAppPacketRaw(raw, packet);
                Deliver(observer, NAMTStructureState, raw,
                        changedStructures.contains(structure.structureKey)
                            ? kReliable : NMFNone,
                        streamKey);
            }
        }
    }
}

void ServerReplicationEventPublisher::PublishStructureState(
    const Game::Simulation::StructureSnapshot& structure) {
    const Game::Replication::SpatialEntityKey relevanceKey{
        Game::Replication::SpatialEntityKind::Structure, structure.structureKey
    };
    for (const int32_t observer :
         mReplication.SpatialEntityObservers(relevanceKey)) {
        const NetworkStructureStatePacket packet =
            WorldPvpNetworkAdapter::ToPacket(
                structure,
                mReplication.NextSpatialEntityStateSequence(observer, relevanceKey));
        if (observer == 0) {
            mClientInbox.AcceptStructureState(packet);
        } else {
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, packet);
            Deliver(observer, NAMTStructureState, raw, kReliable);
        }
    }
}

bool ServerReplicationEventPublisher::PublishFishingPresentation(
    const Game::Replication::FishingPresentationState& presentation,
    const Game::Simulation::PlayerSnapshot& authoritativePlayer) {
    if (mReplication.UpdateFishingPresentation(presentation, authoritativePlayer) !=
        Game::Replication::FishingPresentationUpdateResult::Accepted) {
        return false;
    }
    const NetworkFishingPresentationPacket packet =
        FishingNetworkAdapter::ToPacket(presentation);
    NetworkMessageRaw raw;
    EncodeFishingStateRaw(raw, packet);
    const Game::Replication::ReplicationStreamKey streamKey{
        static_cast<uint16_t>(NAMTFishingState), presentation.playerId,
        authoritativePlayer.entity, 0
    };
    for (const int32_t observer :
         mReplication.PlayerObservers(presentation.playerId)) {
        if (observer == 0) {
            mClientInbox.AcceptFishingPresentation(packet, 0);
        } else {
            Deliver(observer, NAMTFishingState, raw, NMFHighPriority, streamKey);
        }
    }
    return true;
}

void ServerReplicationEventPublisher::PublishProjectileEvents() {
    const auto events = mWorld.DrainArrowEvents();
    const bool hasCreation = std::any_of(events.begin(), events.end(),
        [](const Game::Simulation::ArrowEvent& event) {
            return event.kind == Game::Simulation::ArrowEventKind::Created;
        });
    if (hasCreation) mInterestPublisher.RefreshOwnedEntities();

    for (const auto& event : events) {
        const NetworkProjectileStatePacket packet =
            ProjectileNetworkAdapter::ToPacket(event.arrow);
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        const bool periodic =
            event.kind == Game::Simulation::ArrowEventKind::Snapshot;
        const bool created =
            event.kind == Game::Simulation::ArrowEventKind::Created;
        const Game::Replication::ReplicationStreamKey streamKey{
            static_cast<uint16_t>(NAMTProjectileState), packet.playerId,
            event.arrow.entity, packet.projectileId
        };
        const Game::Replication::OwnedEntityKey relevanceKey{
            Game::Replication::OwnedEntityKind::Arrow, packet.playerId,
            packet.projectileId
        };
        DeliverOwnedEntity(NAMTProjectileState, raw, relevanceKey,
                           !created && !periodic,
                           periodic ? NMFHighPriority : kReliable, streamKey);
        if (mReplication.OwnedEntityVisible(0, relevanceKey)) {
            mClientInbox.AcceptProjectileState(packet, 0);
        }
    }
}

void ServerReplicationEventPublisher::PublishFishingEvents() {
    for (const auto& event : mWorld.DrainFishingLureEvents()) {
        const bool active =
            event.kind != Game::Simulation::FishingLureEventKind::Removed;
        const Game::Replication::OwnedEntityKey key{
            Game::Replication::OwnedEntityKind::Lure,
            event.lure.ownerPlayerId, 1
        };
        if (mReplication.OwnedEntityVisible(0, key)) {
            const NetworkLureStatePacket packet = FishingNetworkAdapter::ToPacket(
                event.lure, mReplication.NextOwnedEntityStateSequence(0, key), active);
            mClientInbox.AcceptLureState(packet);
        }
        if (event.kind != Game::Simulation::FishingLureEventKind::Snapshot) continue;
        for (const int32_t observer : mReplication.OwnedEntityObservers(key)) {
            if (observer <= 0) continue;
            const NetworkLureStatePacket packet = FishingNetworkAdapter::ToPacket(
                event.lure,
                mReplication.NextOwnedEntityStateSequence(observer, key), active);
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, packet);
            const Game::Replication::ReplicationStreamKey streamKey{
                static_cast<uint16_t>(NAMTLureState), event.lure.ownerPlayerId,
                event.lure.entity, 0
            };
            Deliver(observer, NAMTLureState, raw, NMFHighPriority, streamKey);
        }
    }
}

void ServerReplicationEventPublisher::PublishCombatResults() {
    const auto results = mWorld.DrainCombatResults();
    for (const auto& batch :
         mReplication.BuildCombatBatches(results, mWorld.PlayerSnapshots())) {
        const NetworkCombatResultPacket packet =
            CombatNetworkAdapter::ToPacket(batch.result);
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        for (const int32_t observer : batch.observers) {
            if (observer == 0) {
                mClientInbox.AcceptCombatResult(packet);
            } else {
                Deliver(observer, NAMTCombatResult, raw, kReliable);
            }
        }
    }
}

void ServerReplicationEventPublisher::PublishLifeEvents() {
    const auto events = mWorld.DrainPlayerLifeEvents();
    if (std::any_of(events.begin(), events.end(), [](const auto& event) {
            return event.kind == Game::Simulation::PlayerLifeEventKind::Died;
        })) {
        // Death and retained-body creation are one authoritative transaction.
        // Reconcile the new corpse lifetime before returning from this publish
        // pass instead of waiting for the later respawn.
        mInterestPublisher.RefreshSpatialEntities();
    }
    for (const auto& event : events) {
        if (event.kind == Game::Simulation::PlayerLifeEventKind::Died) {
            Error("Server authority: player %d died", event.playerId);
            continue;
        }
        const auto authoritative = mWorld.PlayerFor(event.playerId);
        if (!authoritative || authoritative->entity != event.entity ||
            authoritative->lifeEpoch != event.lifeEpoch) continue;
        const NetworkPlayerRespawnPacket packet =
            PlayerLifecycleNetworkAdapter::ToRespawnPacket(*authoritative);
        if (event.playerId == 0) {
            mClientInbox.AcceptPlayerRespawn(packet);
        } else {
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, packet);
            Deliver(event.playerId, NAMTPlayerRespawn, raw, kReliable);
        }
        Error("Server authority: tick-scheduled respawn sent to player %d at tick %u",
              event.playerId, event.serverTick);
    }
}

} // namespace SoH::Network
