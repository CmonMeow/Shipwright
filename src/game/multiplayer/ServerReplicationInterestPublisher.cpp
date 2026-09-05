#include "ServerReplicationInterestPublisher.h"

#include "CorpseNetworkAdapter.h"
#include "FishingNetworkAdapter.h"
#include "PlayerLifecycleNetworkAdapter.h"
#include "ProjectileNetworkAdapter.h"
#include "WorldPvpNetworkAdapter.h"

#include <utility>
#include <variant>

namespace Game::Multiplayer {

namespace {

constexpr float kPlayerInterestRadius = 6000.0f;
const NetMsgFlags kReliable =
    static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

} // namespace

ServerReplicationInterestPublisher::ServerReplicationInterestPublisher(
    Game::Simulation::ServerWorld& world,
    Game::Replication::ServerReplicationCoordinator& replication,
    ClientReplicationInbox& clientInbox)
    : mWorld(world), mReplication(replication), mClientInbox(clientInbox) {
}

void ServerReplicationInterestPublisher::SetDelivery(
    ServerReplicationDelivery delivery) {
    mDelivery = std::move(delivery);
}

std::vector<int32_t> ServerReplicationInterestPublisher::ConnectedObservers() const {
    std::vector<int32_t> observers =
        mDelivery.connectedObservers ? mDelivery.connectedObservers() : std::vector<int32_t>{};
    if (mWorld.PlayerFor(0)) observers.push_back(0);
    return observers;
}

void ServerReplicationInterestPublisher::Deliver(
    int32_t observer, NetAppMessageType type, const NetworkMessageRaw& raw,
    NetMsgFlags flags, Game::Replication::ReplicationStreamKey streamKey) const {
    if (observer > 0 && mDelivery.send) {
        mDelivery.send(observer, type, raw, flags, streamKey);
    }
}

void ServerReplicationInterestPublisher::RefreshPlayers(
    const std::vector<Game::Simulation::PlayerSnapshot>& players) {
    const auto transitions = mReplication.ReconcilePlayers(
        players, ConnectedObservers(), kPlayerInterestRadius);
    for (const auto& transition : transitions) {
        const bool entering =
            transition.action == Game::Replication::PlayerVisibilityAction::Enter;
        if (!entering) {
            mReplication.RemoveQueuedEntity(transition.observerPlayerId,
                                            transition.subject.playerId,
                                            transition.subject.entity);
        }
        const NetworkPlayerLifecyclePacket lifecycle =
            PlayerLifecycleNetworkAdapter::ToPacket(transition.subject, entering);
        NetworkMessageRaw lifecycleRaw;
        EncodeAppPacketRaw(lifecycleRaw, lifecycle);
        if (transition.observerPlayerId == 0) {
            mClientInbox.AcceptPlayerLifecycle(lifecycle);
        } else {
            Deliver(transition.observerPlayerId, NAMTPlayerLifecycle,
                    lifecycleRaw, kReliable);
        }
        if (!entering) continue;

        const int32_t player = transition.subject.playerId;
        const auto authoritative = mWorld.PlayerFor(player);
        const auto fishing = mReplication.FishingPresentationFor(player);
        if (authoritative && authoritative->selectedWeapon == 4 && fishing) {
            const NetworkFishingPresentationPacket packet =
                FishingNetworkAdapter::ToPacket(*fishing);
            NetworkMessageRaw fishingRaw;
            EncodeFishingStateRaw(fishingRaw, packet);
            if (transition.observerPlayerId == 0) {
                mClientInbox.AcceptFishingPresentation(packet);
            } else {
                Deliver(transition.observerPlayerId, NAMTFishingState,
                        fishingRaw, kReliable);
            }
        }
    }
}

void ServerReplicationInterestPublisher::RefreshOwnedEntities() {
    const auto arrows = mWorld.ArrowSnapshots();
    const auto fish = mWorld.FishSnapshots();
    const auto players = mWorld.PlayerSnapshots();
    const auto lures = mWorld.LureSnapshots();

    std::vector<Game::Replication::ReplicatedOwnedEntity> entities;
    entities.reserve(arrows.size() + fish.size() + lures.size());
    for (const auto& arrow : arrows) {
        entities.push_back({
            { Game::Replication::OwnedEntityKind::Arrow,
              arrow.ownerPlayerId, arrow.replicationId },
            arrow.entity, arrow.sceneId, arrow.position, true, arrow
        });
    }
    for (const auto& caught : fish) {
        entities.push_back({
            { Game::Replication::OwnedEntityKind::Fish,
              caught.ownerPlayerId, 1 },
            caught.entity, caught.identity.sceneId, caught.position, false, caught
        });
    }
    for (const auto& lure : lures) {
        entities.push_back({
            { Game::Replication::OwnedEntityKind::Lure,
              lure.ownerPlayerId, 1 },
            lure.entity, lure.sceneId, lure.position, true, lure
        });
    }

    const auto transitions = mReplication.ReconcileOwnedEntities(
        entities, players, ConnectedObservers(), kPlayerInterestRadius);
    for (const auto& transition : transitions) {
        const bool entering =
            transition.action == Game::Replication::OwnedEntityVisibilityAction::Enter;
        if (!entering) {
            mReplication.RemoveQueuedEntity(transition.observerPlayerId,
                                            transition.subject.key.ownerPlayerId,
                                            transition.subject.entity);
        }
        const auto kind = transition.subject.key.kind;
        if (kind == Game::Replication::OwnedEntityKind::Fish) {
            const auto* snapshot = std::get_if<Game::Simulation::FishSnapshot>(
                &transition.subject.payload);
            if (snapshot) {
                const NetworkFishStatePacket packet = FishingNetworkAdapter::ToPacket(
                    *snapshot,
                    mReplication.NextOwnedEntityStateSequence(
                        transition.observerPlayerId, transition.subject.key),
                    entering);
                NetworkMessageRaw raw;
                EncodeAppPacketRaw(raw, packet);
                if (transition.observerPlayerId == 0) {
                    mClientInbox.AcceptFishState(packet);
                } else {
                    Deliver(transition.observerPlayerId, NAMTFishState, raw, kReliable);
                }
            }
            continue;
        }
        if (kind == Game::Replication::OwnedEntityKind::Lure) {
            const auto* snapshot =
                std::get_if<Game::Simulation::FishingLureSnapshot>(
                    &transition.subject.payload);
            if (snapshot) {
                const NetworkLureStatePacket packet = FishingNetworkAdapter::ToPacket(
                    *snapshot,
                    mReplication.NextOwnedEntityStateSequence(
                        transition.observerPlayerId, transition.subject.key),
                    entering);
                NetworkMessageRaw raw;
                EncodeAppPacketRaw(raw, packet);
                if (transition.observerPlayerId == 0) {
                    mClientInbox.AcceptLureState(packet);
                } else {
                    Deliver(transition.observerPlayerId, NAMTLureState, raw, kReliable);
                }
            }
            continue;
        }

        const NetworkProjectileLifecyclePacket lifecycle =
            ProjectileNetworkAdapter::ToLifecyclePacket(transition.subject, entering);
        NetworkMessageRaw lifecycleRaw;
        EncodeAppPacketRaw(lifecycleRaw, lifecycle);
        if (transition.observerPlayerId == 0) {
            mClientInbox.AcceptProjectileLifecycle(lifecycle);
        } else {
            Deliver(transition.observerPlayerId, NAMTProjectileLifecycle,
                    lifecycleRaw, kReliable);
        }
        if (!entering) continue;

        if (kind == Game::Replication::OwnedEntityKind::Arrow) {
            const auto* arrow = std::get_if<Game::Simulation::ArrowSnapshot>(
                &transition.subject.payload);
            if (!arrow) continue;
            const NetworkProjectileStatePacket baseline =
                ProjectileNetworkAdapter::ToPacket(*arrow);
            if (transition.observerPlayerId == 0) {
                mClientInbox.AcceptProjectileState(baseline, 0);
            } else {
                NetworkMessageRaw baselineRaw;
                EncodeAppPacketRaw(baselineRaw, baseline);
                Deliver(transition.observerPlayerId, NAMTProjectileState,
                        baselineRaw, kReliable);
            }
        }
    }
}

void ServerReplicationInterestPublisher::RefreshSpatialEntities() {
    const auto players = mWorld.PlayerSnapshots();
    const auto corpses = mWorld.CorpseSnapshots();
    const auto objectives = mWorld.ObjectiveSnapshots();
    const auto structures = mWorld.StructureSnapshots();
    std::vector<Game::Replication::ReplicatedSpatialEntity> entities;
    entities.reserve(corpses.size() + objectives.size() + structures.size());
    for (const auto& corpse : corpses) {
        entities.push_back({
            { Game::Replication::SpatialEntityKind::Corpse,
              static_cast<int32_t>(corpse.entity.index) },
            corpse.entity, corpse.pose.sourcePlayerId, -1, corpse.pose.sceneId,
            corpse.pose.position, corpse
        });
    }
    for (const auto& objective : objectives) {
        entities.push_back({
            { Game::Replication::SpatialEntityKind::Objective, objective.objectiveKey },
            objective.entity, objective.objectiveKey, -1, objective.sceneId,
            objective.position, objective
        });
    }
    for (const auto& structure : structures) {
        entities.push_back({
            { Game::Replication::SpatialEntityKind::Structure, structure.structureKey },
            structure.entity, structure.structureKey, structure.objectiveKey,
            structure.sceneId, structure.position, structure
        });
    }

    const auto transitions = mReplication.ReconcileSpatialEntities(
        entities, players, ConnectedObservers(), kPlayerInterestRadius);
    for (const auto& transition : transitions) {
        const bool entering =
            transition.action == Game::Replication::SpatialEntityVisibilityAction::Enter;
        if (!entering) {
            mReplication.RemoveQueuedEntity(transition.observerPlayerId,
                                            transition.subject.ownerOrKey,
                                            transition.subject.entity);
        }

        if (transition.subject.key.kind ==
            Game::Replication::SpatialEntityKind::Corpse) {
            const auto* state = std::get_if<Game::Simulation::CorpseSnapshot>(
                &transition.subject.payload);
            if (!state) continue;
            const NetworkCorpseStatePacket packet = CorpseNetworkAdapter::ToPacket(
                *state,
                mReplication.NextSpatialEntityStateSequence(
                    transition.observerPlayerId, transition.subject.key),
                entering);
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, packet);
            if (transition.observerPlayerId == 0) {
                mClientInbox.AcceptCorpseState(packet);
            } else {
                Deliver(transition.observerPlayerId, NAMTCorpseState, raw, kReliable);
            }
            continue;
        }

        if (transition.subject.key.kind ==
            Game::Replication::SpatialEntityKind::Objective) {
            const auto* state = std::get_if<Game::Simulation::ObjectiveSnapshot>(
                &transition.subject.payload);
            if (!state) continue;
            const NetworkObjectiveStatePacket packet =
                WorldPvpNetworkAdapter::ToPacket(
                    *state,
                    mReplication.NextSpatialEntityStateSequence(
                        transition.observerPlayerId, transition.subject.key),
                    entering);
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, packet);
            if (transition.observerPlayerId == 0) {
                mClientInbox.AcceptObjectiveState(packet);
            } else {
                Deliver(transition.observerPlayerId, NAMTObjectiveState, raw, kReliable);
            }
            continue;
        }

        if (transition.subject.key.kind ==
            Game::Replication::SpatialEntityKind::Structure) {
            const auto* state = std::get_if<Game::Simulation::StructureSnapshot>(
                &transition.subject.payload);
            if (!state) continue;
            const NetworkStructureStatePacket packet =
                WorldPvpNetworkAdapter::ToPacket(
                    *state,
                    mReplication.NextSpatialEntityStateSequence(
                        transition.observerPlayerId, transition.subject.key),
                    entering);
            NetworkMessageRaw raw;
            EncodeAppPacketRaw(raw, packet);
            if (transition.observerPlayerId == 0) {
                mClientInbox.AcceptStructureState(packet);
            } else {
                Deliver(transition.observerPlayerId, NAMTStructureState, raw, kReliable);
            }
        }
    }
}

void ServerReplicationInterestPublisher::RefreshAll() {
    // Player lifetimes establish observer/subject scene membership before
    // dependent entity baselines are published.
    RefreshPlayers(mWorld.PlayerSnapshots());
    RefreshOwnedEntities();
    RefreshSpatialEntities();
}

} // namespace Game::Multiplayer
