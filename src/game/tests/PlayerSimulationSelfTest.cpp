#include "../platform/simulation/PlayerSimulation.h"
#include "../platform/simulation/ArticulatedPlayerHitRig.h"
#include "../platform/simulation/AuthoritativePlayerHitRig.h"
#include "../platform/simulation/AuthoritativeMeleeWeapon.h"
#include "../platform/simulation/ProjectileSimulation.h"
#include "../platform/simulation/FishingSimulation.h"
#include "../platform/simulation/SpatialGridIndex.h"
#include "../platform/simulation/ObjectiveSimulation.h"
#include "../platform/simulation/StructureSimulation.h"
#include "../platform/simulation/CorpseSimulation.h"
#include "../platform/simulation/ClientPrediction.h"
#include "../platform/client/LocalFishIntentStream.h"
#include "../platform/client/LocalPrimaryActionPresentation.h"
#include "../platform/client/ClientWorldState.h"
#include "../platform/client/ClientGameplaySession.h"
#include "../platform/client/ClientSessionGenerationTracker.h"
#include "../platform/client/PresentationFrameBudget.h"
#include "../platform/client/LocalFishingUpdateStream.h"
#include "../platform/client/LocalPlayerVitals.h"
#include "../platform/client/CorpsePresentationRegistry.h"
#include "../platform/client/LocalProjectileIntentStream.h"
#include "../platform/client/NativePresentationBindingRegistry.h"
#include "../platform/client/LocalSceneAdmission.h"
#include "../platform/client/RemoteFishingPresentationInterpolation.h"
#include "../platform/client/RemoteFishingEntityState.h"
#include "../platform/client/RemotePlayerInterpolation.h"
#include "../platform/client/RemotePlayerReplicaStore.h"
#include "../platform/client/RemotePlayerPresentationRegistry.h"
#include "../platform/client/RemoteProjectileInterpolation.h"
#include "../platform/client/RemoteProjectilePresentationRegistry.h"
#include "../platform/client/RemoteProjectileReplicaStore.h"
#include "../platform/simulation/ServerWorld.h"
#include "../platform/simulation/ServerGameplayIngress.h"
#include "../platform/SequenceNumber.h"
#include "../platform/simulation/SceneTransitionAuthority.h"
#include "../platform/replication/PlayerReplicationSystem.h"
#include "../platform/replication/CombatReplicationSystem.h"
#include "../platform/replication/EntityLifetimeRegistry.h"
#include "../platform/replication/FishingPresentationAuthority.h"
#include "../platform/replication/ProjectileLifetimeRegistry.h"
#include "../platform/replication/OwnedEntityReplicationSystem.h"
#include "../platform/replication/ReplicationBudgetSystem.h"
#include "../platform/replication/ReplicationQueueSystem.h"
#include "../platform/replication/ReplicationCadence.h"
#include "../platform/replication/ServerReplicationCoordinator.h"
#include "../platform/replication/SpatialEntityReplicationSystem.h"
#include "../platform/server/ServerWorldManagement.h"
#include "ServerWorldTestAccess.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace {

using Game::Simulation::PlayerCommand;
using Game::Simulation::PlayerActionState;
using Game::Simulation::PlayerSimulation;
using Game::Simulation::PlayerSpawn;
using Game::Simulation::PlayerSnapshot;
using Game::Simulation::ProjectileSimulation;
using Game::Simulation::FishingSimulation;
using Game::Simulation::ServerWorld;
using Game::Simulation::ServerGameplayIngress;
using Game::Simulation::ServerWorldTestAccess;
using Game::Simulation::SceneTransitionAuthority;
using Game::Simulation::ServerIntentAdmission;
using Game::Simulation::ServerIntentKind;
using Game::Simulation::ServerIntentResult;
using Game::Simulation::SpatialGridIndex;
using Game::Simulation::ObjectiveSimulation;
using Game::Simulation::StructureSimulation;
using Game::Simulation::StrategicSiteKind;
using Game::Simulation::StrategicWorldTopology;
using Game::Simulation::CorpseSimulation;
using Game::Simulation::ClientPrediction;
using Game::Replication::ReplicationCadence;
using Game::Client::ClientWorldState;
using Game::Client::ClientWorldStateUpdate;
using Game::Client::LocalFishIntentAction;
using Game::Client::LocalFishIntentRequest;
using Game::Client::LocalFishIntentStream;
using Game::Client::LocalFishingUpdate;
using Game::Client::LocalFishingUpdateStream;
using Game::Client::CorpsePresentationRegistry;
using Game::Client::CorpsePresentationState;
using Game::Client::CorpsePresentationUpdate;
using Game::Client::LocalProjectileIntent;
using Game::Client::LocalProjectileIntentKind;
using Game::Client::LocalProjectileIntentStream;
using Game::Client::LocalProjectilePresentation;
using Game::Client::LocalSceneAdmission;
using Game::Client::LocalSceneAuthority;
using Game::Client::LocalSceneAuthorityKind;
using Game::Client::RemoteFishingPresentationInterpolation;
using Game::Client::RemoteFishEntity;
using Game::Client::RemoteFishIdentity;
using Game::Client::RemoteFishingEntityState;
using Game::Client::RemoteFishingEntityUpdate;
using Game::Client::RemoteLureEntity;
using Game::Client::RemoteMotionSample;
using Game::Client::RemotePlayerReplicaStore;
using Game::Client::RemotePlayerPresentationRegistry;
using Game::Client::RemotePlayerPresentationState;
using Game::Client::RemotePlayerPresentationUpdate;
using Game::Client::RemoteProjectilePresentationRegistry;
using Game::Client::RemoteProjectilePresentationState;
using Game::Client::RemoteProjectilePresentationUpdate;
using Game::Client::RemoteProjectilePhase;
using Game::Client::RemoteProjectileReplicaState;
using Game::Client::RemoteProjectileReplicaStore;
using Game::Client::RemotePlayerInterpolation;
using Game::Client::RemoteProjectileInterpolation;
using Game::Client::RemoteProjectileSample;
using Game::Replication::PlayerReplicationSystem;
using Game::Replication::CombatReplicationSystem;
using Game::Replication::EntityLifetimeRegistry;
using Game::Replication::PlayerVisibilityAction;
using Game::Replication::OwnedEntityKind;
using Game::Replication::OwnedEntityReplicationSystem;
using Game::Replication::OwnedEntityVisibilityAction;
using Game::Replication::ReplicatedOwnedEntity;
using Game::Replication::ReplicationBudgetConfig;
using Game::Replication::ReplicationBudgetSystem;
using Game::Replication::ReplicationPriority;
using Game::Replication::ReplicationQueueSystem;
using Game::Replication::ReplicationStreamKey;
using Game::Replication::ReplicationSubmission;
using Game::Replication::ReplicatedSpatialEntity;
using Game::Replication::ServerReplicationCoordinator;
using Game::Replication::SpatialEntityKind;
using Game::Replication::SpatialEntityReplicationSystem;
using Game::Replication::SpatialEntityVisibilityAction;

bool NearlyEqual(float left, float right) {
    return std::abs(left - right) < 0.01f;
}

} // namespace

int main() {
    if (Game::Sequence::IsNewer(7, 7) ||
        !Game::Sequence::IsNewer(8, 7) ||
        Game::Sequence::IsNewer(7, 8) ||
        !Game::Sequence::IsNewer(1, UINT32_MAX) ||
        Game::Sequence::IsNewer(UINT32_MAX, 1) ||
        Game::Sequence::IsNewer(0x80000000U, 0) ||
        Game::Sequence::IsNewer(0, 0x80000000U) ||
        !Game::Sequence::IsAtOrAfter(7, 7) ||
        !Game::Sequence::IsAtOrAfter(1, UINT32_MAX) ||
        Game::Sequence::IsAtOrAfter(0x80000000U, 0)) {
        return 395;
    }

    {
        using Game::Client::ClientSessionGenerationTracker;
        using Game::Client::ClientSessionGenerationUpdate;

        ClientSessionGenerationTracker sessionGeneration;
        if (sessionGeneration.Observe(0) !=
                ClientSessionGenerationUpdate::Invalid ||
            sessionGeneration.Current()) {
            return 478;
        }
        const auto established = sessionGeneration.Observe(7);
        if (established != ClientSessionGenerationUpdate::Established ||
            !ClientSessionGenerationTracker::RequiresStateReset(established) ||
            sessionGeneration.Current().value_or(0) != 7) {
            return 479;
        }
        const auto unchanged = sessionGeneration.Observe(7);
        if (unchanged != ClientSessionGenerationUpdate::Unchanged ||
            ClientSessionGenerationTracker::RequiresStateReset(unchanged)) {
            return 480;
        }
        const auto replaced = sessionGeneration.Observe(8);
        if (replaced != ClientSessionGenerationUpdate::Replaced ||
            !ClientSessionGenerationTracker::RequiresStateReset(replaced) ||
            sessionGeneration.Current().value_or(0) != 8) {
            return 481;
        }
        sessionGeneration.Reset();
        if (sessionGeneration.Current() ||
            sessionGeneration.Observe(8) !=
                ClientSessionGenerationUpdate::Established) {
            return 482;
        }
    }

    {
        Game::Client::PresentationFrameBudget frameBudget;
        if (Game::Client::PresentationFrameBudget::FrameCount(20, 20) != 1 ||
            Game::Client::PresentationFrameBudget::FrameCount(60, 20) != 3 ||
            Game::Client::PresentationFrameBudget::FrameCount(144, 20) != 8 ||
            Game::Client::PresentationFrameBudget::FrameCount(240, 20) != 12) {
            return 477;
        }
        frameBudget.BeginBatch(60, 30);
        if (!frameBudget.CanPresentIntermediate(0.0)) return 447;

        // A 20 ms present cannot fit both an intermediate and the newest
        // native state inside a 30 Hz simulation interval.
        frameBudget.ObservePresent(0.020);
        if (frameBudget.CanPresentIntermediate(0.0)) return 448;

        // Changing presentation rate resets the estimate to that rate's real
        // interval rather than carrying stale timing from the old setting.
        frameBudget.BeginBatch(240, 30);
        if (!frameBudget.CanPresentIntermediate(0.010)) return 449;
        frameBudget.ObservePresent(0.020);
        if (frameBudget.CanPresentIntermediate(0.011)) return 450;
    }

    {
        Game::Simulation::ServerWorld managedWorld;
        int spatialRefreshes = 0;
        Game::Server::ServerWorldManagement management(
            managedWorld, [&spatialRefreshes]() { ++spatialRefreshes; });

        Game::Simulation::ObjectiveDefinition objective{};
        objective.objectiveKey = 701;
        objective.sceneId = 118;
        objective.position = { 10.0f, 20.0f, 30.0f };
        if (!management.EnsureObjective(objective).Valid() ||
            spatialRefreshes != 1) {
            return 451;
        }

        Game::Simulation::StructureDefinition structure{};
        structure.structureKey = 702;
        structure.objectiveKey = objective.objectiveKey;
        structure.sceneId = objective.sceneId;
        structure.position = { 40.0f, 20.0f, 30.0f };
        if (!management.EnsureStructure(structure).Valid() ||
            spatialRefreshes != 2) {
            return 452;
        }

        if (!management.EnsureStrategicSite(
                objective, StrategicSiteKind::Keep, 7).Valid() ||
             !management.EnsureStrategicSite(
                { 703, 118, { -80.0f, 20.0f, 30.0f }, 100.0f,
                  Game::Simulation::TeamId::Neutral },
                StrategicSiteKind::Camp, 8).Valid() ||
             !management.EnsureSupplyRoute({ 704, 703, objective.objectiveKey }) ||
             !management.EnsureInfluenceAdjacency({ 706, 7, 8 }) ||
             spatialRefreshes != 6 ||
             management.EnsureSupplyRoute({ 705, objective.objectiveKey, 703 }) ||
             management.EnsureInfluenceAdjacency({ 707, 8, 7 }) ||
             management.EnsureInfluenceAdjacency({ 707, 7, 8 }) ||
             spatialRefreshes != 6) {
            return 452;
        }

        // Objective removal also retires its dependent structures. The whole
        // aggregate mutation publishes exactly one spatial refresh.
        if (!management.RemoveObjective(objective.objectiveKey) ||
             spatialRefreshes != 7 || managedWorld.ObjectiveSnapshots().size() != 1 ||
            !managedWorld.StructureSnapshots().empty() ||
             managedWorld.StrategicSites().size() != 1 ||
             !managedWorld.SupplyRoutes().empty() ||
             !managedWorld.InfluenceAdjacencies().empty()) {
            return 454;
        }
        if (!management.RemoveObjective(703) || spatialRefreshes != 8 ||
             management.RemoveObjective(objective.objectiveKey) ||
             spatialRefreshes != 8) {
            return 455;
        }

    }
    {
        Game::Simulation::ServerWorld topologyWorld;
        const Game::Simulation::ObjectiveDefinition firstRegion{
            710, 118, {}, 100.0f, Game::Simulation::TeamId::Neutral
        };
        const Game::Simulation::ObjectiveDefinition duplicateRegion{
            711, 118, {}, 100.0f, Game::Simulation::TeamId::Neutral
        };
        const Game::Simulation::ObjectiveDefinition otherSceneRegion{
            712, 119, {}, 100.0f, Game::Simulation::TeamId::Neutral
        };
        if (!topologyWorld.EnsureStrategicSite(
                firstRegion, StrategicSiteKind::Keep, 20).Valid() ||
            topologyWorld.EnsureStrategicSite(
                duplicateRegion, StrategicSiteKind::Tower, 20).Valid() ||
            topologyWorld.ObjectiveSnapshots().size() != 1 ||
            !topologyWorld.EnsureStrategicSite(
                otherSceneRegion, StrategicSiteKind::Camp, 21).Valid() ||
            topologyWorld.EnsureInfluenceAdjacency({ 713, 20, 21 }) ||
            !topologyWorld.InfluenceAdjacencies().empty()) {
            return 458;
        }
    }

    {
    Game::Client::ClientGameplaySession clientSession;
    Game::Simulation::ObjectiveSnapshot sessionObjective{};
    sessionObjective.entity = { 900, 1 };
    sessionObjective.objectiveKey = 5;
    sessionObjective.sceneId = 118;
    if (!clientSession.World().ApplyObjective(sessionObjective, true).Applied()) {
        return 430;
    }
    const auto firstFishIntent = clientSession.FishIntents().BeginHook();
    if (!firstFishIntent || firstFishIntent->sequence != 1) return 431;

    if (!clientSession.Projectiles().BindPresentation({ 100, 118 }) ||
        !clientSession.Projectiles().RequestArrowFire(100, 118)) {
        return 432;
    }
    const auto firstProjectile = clientSession.Projectiles().NextIntent();
    if (!firstProjectile || firstProjectile->sequence != 1 ||
        !clientSession.Projectiles().Resolve(firstProjectile->sequence, true)) {
        return 433;
    }
    const auto firstFishingUpdate = clientSession.FishingUpdates().Evaluate(
        { 118, true, 1, true, false }, 1.0);
    if (firstFishingUpdate.presentationSequence != 1 ||
        firstFishingUpdate.controlSequence != 1) {
        return 434;
    }

    clientSession.BeginScene();
    if (clientSession.Projectiles().TrackedCount() != 0 ||
        clientSession.Projectiles().AwaitingResultCount() != 0 ||
        !clientSession.Projectiles().BindPresentation({ 101, 119 }) ||
        !clientSession.Projectiles().RequestArrowFire(101, 119)) {
        return 435;
    }
    const auto sceneProjectile = clientSession.Projectiles().NextIntent();
    const auto sceneFishingUpdate = clientSession.FishingUpdates().Evaluate(
        { 119, true, 1, true, false }, 2.0);
    if (!sceneProjectile || sceneProjectile->sequence != 2 ||
        sceneFishingUpdate.presentationSequence != 2 ||
        sceneFishingUpdate.controlSequence != 2) {
        return 436;
    }

    Game::Client::LocalPlayerInputSample oldLifeSample{};
    oldLifeSample.clientTick = 1;
    oldLifeSample.lifeEpoch = 1;
    oldLifeSample.sceneId = 119;
    if (!clientSession.Commands().Build(oldLifeSample)) return 437;
    Game::Simulation::PlayerSnapshot localVitals{};
    localVitals.entity = { 901, 1 };
    localVitals.ownerPlayerId = 7;
    localVitals.sceneId = 119;
    localVitals.serverTick = 1;
    localVitals.lifeEpoch = 1;
    localVitals.health = 32;
    if (clientSession.Vitals().Apply(localVitals, 7) !=
        Game::Client::LocalPlayerVitalsUpdate::Applied) {
        return 438;
    }
    if (!clientSession.Scene().Prepare(119)) return 439;

    clientSession.BeginLife(4);
    Game::Client::LocalPlayerInputSample newLifeSample = oldLifeSample;
    newLifeSample.lifeEpoch = 4;
    const auto newLifeCommand = clientSession.Commands().Build(newLifeSample);
    const auto newLifeFish = clientSession.FishIntents().BeginHook();
    if (!newLifeCommand || newLifeCommand->sequence != 1 ||
        !newLifeFish || newLifeFish->sequence != 1 ||
        clientSession.Vitals().HasState() ||
        clientSession.Projectiles().TrackedCount() != 0 ||
        clientSession.Scene().PendingScene() ||
        clientSession.Scene().LifeEpoch().value_or(0) != 4 ||
        clientSession.Prediction().LifeEpoch() != 4) {
        return 440;
    }

    clientSession.ResetSession();
    if (clientSession.World().ObjectiveCount() != 0 ||
        clientSession.Scene().LifeEpoch() ||
        clientSession.Prediction().LifeEpoch() != 0 ||
        clientSession.Vitals().HasState()) {
        return 441;
    }
    }

    ServerWorld ingressWorld;
    const auto ingressPlayer = ingressWorld.AdmitPlayer(
        41, PlayerSpawn{ 118, {}, 0.0f });
    const auto otherIngressPlayer = ingressWorld.AdmitPlayer(
        42, PlayerSpawn{ 119, { 100.0f, 0.0f, 0.0f }, 0.0f });
    if (!ingressPlayer || !otherIngressPlayer) return 397;
    ServerGameplayIngress ingress(ingressWorld);
    if (!ingress.ExecuteWeaponSelection(
            41, { 42, 1, ingressPlayer->lifeEpoch, 4 })) {
        return 398;
    }
    const auto ingressEquipped = ingressWorld.PlayerFor(41);
    const auto otherIngressEquipped = ingressWorld.PlayerFor(42);
    if (!ingressEquipped || ingressEquipped->selectedWeapon != 4 ||
        !otherIngressEquipped || otherIngressEquipped->selectedWeapon == 4) {
        return 399;
    }
    if (ingress.ExecuteWeaponSelection(
            41, { 42, 2, ingressPlayer->lifeEpoch + 1, 3 })) {
        return 402;
    }
    Game::Replication::FishingPresentationIntent spoofedFishingPresentation{};
    spoofedFishingPresentation.lifeEpoch = ingressPlayer->lifeEpoch;
    spoofedFishingPresentation.presentation.playerId = 42;
    spoofedFishingPresentation.presentation.entity = otherIngressPlayer->entity;
    spoofedFishingPresentation.presentation.sceneId = 119;
    spoofedFishingPresentation.presentation.sequence = 1;
    spoofedFishingPresentation.presentation.state = 5;
    spoofedFishingPresentation.presentation.lureDrawOffset = { 10.0f, 20.0f, 30.0f };
    const auto admittedFishingPresentation = ingress.AdmitFishingPresentation(
        41, spoofedFishingPresentation);
    if (!admittedFishingPresentation ||
        admittedFishingPresentation->presentation.playerId != 41 ||
        admittedFishingPresentation->presentation.entity != ingressPlayer->entity ||
        admittedFishingPresentation->presentation.sceneId != 118 ||
        admittedFishingPresentation->presentation.state != 0 ||
        admittedFishingPresentation->presentation.lureDrawOffset !=
            std::array<float, 3>{} ||
        admittedFishingPresentation->authoritativePlayer.ownerPlayerId != 41) {
        return 419;
    }
    spoofedFishingPresentation.lifeEpoch = ingressPlayer->lifeEpoch + 1;
    if (ingress.AdmitFishingPresentation(41, spoofedFishingPresentation)) {
        return 420;
    }
    spoofedFishingPresentation.lifeEpoch = otherIngressPlayer->lifeEpoch;
    if (ingress.AdmitFishingPresentation(42, spoofedFishingPresentation)) {
        return 421;
    }
    PlayerCommand spoofedIngressCommand{};
    spoofedIngressCommand.ownerPlayerId = 42;
    spoofedIngressCommand.sceneId = 119;
    spoofedIngressCommand.sequence = 1;
    spoofedIngressCommand.lifeEpoch = ingressPlayer->lifeEpoch;
    spoofedIngressCommand.moveY = 1.0f;
    if (!ingress.SubmitPlayerCommand(41, spoofedIngressCommand)) return 400;
    ServerWorldTestAccess::Players(ingressWorld).StepFixed();
    const auto ingressMoved = ingressWorld.PlayerFor(41);
    const auto otherIngressStill = ingressWorld.PlayerFor(42);
    if (!ingressMoved || ingressMoved->sceneId != 118 ||
        ingressMoved->lastProcessedCommand != 1 ||
        !NearlyEqual(ingressMoved->position.z, 6.0f) ||
        !otherIngressStill || otherIngressStill->lastProcessedCommand != 0 ||
        ingress.SubmitPlayerCommand(-1, spoofedIngressCommand)) {
        return 401;
    }

    PlayerSimulation simulation;
    const auto firstEntity = simulation.EnsurePlayer(7, PlayerSpawn{ 118, { 0.0f, 0.0f, 0.0f }, 0.0f });
    PlayerCommand command{};
    command.ownerPlayerId = 7;
    command.sequence = 1;
    command.sceneId = 118;
    command.moveY = 1.0f;
    if (!simulation.SubmitCommand(command) || simulation.SubmitCommand(command)) {
        return 1;
    }

    simulation.StepFixed();
    const auto moved = simulation.SnapshotForPlayer(7);
    if (!moved || moved->lastProcessedCommand != 1 || !NearlyEqual(moved->position.z, 6.0f)) {
        return 3;
    }

    command.sequence = 2;
    command.moveY = 0.0f;
    command.moveX = 1.0f;
    if (!simulation.SubmitCommand(command)) {
        return 4;
    }
    simulation.StepFixed();
    const auto strafed = simulation.SnapshotForPlayer(7);
    if (!strafed || strafed->lastProcessedCommand != 2 || !NearlyEqual(strafed->position.x, 6.0f)) {
        return 5;
    }

    simulation.RemovePlayer(7);
    if (simulation.SnapshotForPlayer(7) ||
        simulation.EnsurePlayer(-1, PlayerSpawn{ 118, {}, 0.0f }).Valid()) {
        return 516;
    }
    const auto replacement = simulation.EnsurePlayer(7, PlayerSpawn{ 118, {}, 0.0f });
    if (replacement.index != firstEntity.index || replacement.generation == firstEntity.generation) {
        return 6;
    }
    if (simulation.EnsurePlayer(7, PlayerSpawn{ 119, { 500.0f, 0.0f, 0.0f }, 0.0f }) !=
            replacement ||
        !simulation.SnapshotForPlayer(7)) {
        return 517;
    }
    simulation.Reset();
    const auto afterReset = simulation.EnsurePlayer(8, PlayerSpawn{ 118, {}, 0.0f });
    if (!afterReset.Valid() || simulation.SnapshotForPlayer(7) ||
        !simulation.SnapshotForPlayer(8)) {
        return 518;
    }

    PlayerSimulation spatialCombatPlayers;
    spatialCombatPlayers.EnsurePlayer(100, PlayerSpawn{ 118, {}, 0.0f });
    spatialCombatPlayers.EnsurePlayer(
        101, PlayerSpawn{ 118, { 5000.0f, 0.0f, 0.0f }, 0.0f });
    spatialCombatPlayers.EnsurePlayer(102, PlayerSpawn{ 119, {}, 0.0f });
    auto nearbyPlayers = spatialCombatPlayers.SnapshotsNearSegment(
        118, { 0.0f, 40.0f, 0.0f }, { 50.0f, 40.0f, 0.0f }, 80.0f);
    if (nearbyPlayers.size() != 1 || nearbyPlayers.front().ownerPlayerId != 100) {
        return 522;
    }
    if (!spatialCombatPlayers.ChangeScene(
            101, PlayerSpawn{ 118, { 100.0f, 0.0f, 0.0f }, 0.0f })) {
        return 523;
    }
    nearbyPlayers = spatialCombatPlayers.SnapshotsNearSegment(
        118, { 0.0f, 40.0f, 0.0f }, { 50.0f, 40.0f, 0.0f }, 80.0f);
    if (nearbyPlayers.size() != 2 || nearbyPlayers[0].ownerPlayerId != 100 ||
        nearbyPlayers[1].ownerPlayerId != 101) {
        return 524;
    }
    spatialCombatPlayers.RemovePlayer(100);
    spatialCombatPlayers.Reset();
    if (!spatialCombatPlayers.SnapshotsNearSegment(
             118, {}, { 50.0f, 0.0f, 0.0f }, 80.0f).empty()) {
        return 525;
    }

    PlayerSimulation collisionSimulation;
    collisionSimulation.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3& from, const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            if (NearlyEqual(from.x, to.x) && NearlyEqual(from.z, to.z)) {
                impact = { to.x, 0.0f, to.z };
                return true;
            }
            return to.z > 2.0f;
        });
    collisionSimulation.EnsurePlayer(9, PlayerSpawn{ 118, {}, 0.0f });
    command.ownerPlayerId = 9;
    command.sequence = 1;
    command.moveX = 0.0f;
    command.moveY = 1.0f;
    collisionSimulation.SubmitCommand(command);
    collisionSimulation.StepFixed();
    const auto blocked = collisionSimulation.SnapshotForPlayer(9);
    if (!blocked || !NearlyEqual(blocked->position.z, 0.0f)) {
        return 7;
    }

    PlayerSimulation bodyRadiusCollision;
    bodyRadiusCollision.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3& from,
           const Game::Simulation::Vec3& to, Game::Simulation::Vec3& impact) {
            if (NearlyEqual(from.x, to.x) && NearlyEqual(from.z, to.z)) {
                impact = { to.x, 0.0f, to.z };
                return true;
            }
            if (from.x < 17.0f && to.x >= 17.0f) {
                impact = { 17.0f, to.y, to.z };
                return true;
            }
            return false;
        });
    bodyRadiusCollision.EnsurePlayer(10, PlayerSpawn{ 118, {}, 0.0f });
    PlayerCommand radiusMove{};
    radiusMove.ownerPlayerId = 10;
    radiusMove.sequence = 1;
    radiusMove.sceneId = 118;
    radiusMove.moveX = 1.0f;
    if (!bodyRadiusCollision.SubmitCommand(radiusMove)) return 392;
    bodyRadiusCollision.StepFixed();
    const auto radiusBlocked = bodyRadiusCollision.SnapshotForPlayer(10);
    if (!radiusBlocked || !NearlyEqual(radiusBlocked->position.x, 0.0f)) return 393;

    PlayerSimulation waterLocomotion;
    waterLocomotion.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3& from,
           const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            if (NearlyEqual(from.x, to.x) && NearlyEqual(from.z, to.z)) {
                impact = { to.x, 0.0f, to.z };
                return true;
            }
            return false;
        });
    waterLocomotion.SetWaterSurfaceQuery(
        [](int32_t sceneId, const Game::Simulation::Vec3& position,
           float& surfaceY) {
            if (sceneId != 118 || position.x < 3.0f || position.x > 9.0f) {
                return false;
            }
            surfaceY = 80.0f;
            return true;
        });
    waterLocomotion.EnsurePlayer(32, PlayerSpawn{ 118, {}, 0.0f });
    const auto initiallyGrounded = waterLocomotion.SnapshotForPlayer(32);
    PlayerCommand enterWater{};
    enterWater.ownerPlayerId = 32;
    enterWater.sequence = 1;
    enterWater.sceneId = 118;
    enterWater.moveX = 1.0f;
    if (!initiallyGrounded || initiallyGrounded->locomotionMode !=
                                  Game::Simulation::PlayerLocomotionMode::Grounded ||
        !waterLocomotion.SubmitCommand(enterWater)) {
        return 426;
    }
    waterLocomotion.StepFixed();
    const auto swimming = waterLocomotion.SnapshotForPlayer(32);
    if (!swimming || swimming->locomotionMode !=
                         Game::Simulation::PlayerLocomotionMode::Swimming ||
        !NearlyEqual(swimming->position.x, 6.0f) ||
        !NearlyEqual(swimming->position.y, 80.0f)) {
        return 427;
    }
    ClientPrediction waterPrediction;
    if (!waterPrediction.SeedAuthoritative(*initiallyGrounded)) return 428;
    waterPrediction.RecordCommand(
        enterWater, Game::Simulation::kPlayerSimulationTickSeconds);
    if (!waterPrediction.Reconcile(*swimming, { 6.0f, 0.0f, 0.0f }) ||
        !NearlyEqual(waterPrediction.PendingCorrection().y, 80.0f)) {
        return 428;
    }
    enterWater.sequence = 2;
    enterWater.actionSequence = 1;
    enterWater.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    if (!waterLocomotion.SelectWeapon(32, 1)) return 464;
    if (!waterLocomotion.SubmitCommand(enterWater)) return 429;
    waterLocomotion.StepFixed();
    const auto returnedToGround = waterLocomotion.SnapshotForPlayer(32);
    if (!returnedToGround || returnedToGround->locomotionMode !=
                                 Game::Simulation::PlayerLocomotionMode::Grounded ||
        !NearlyEqual(returnedToGround->position.x, 12.0f) ||
        !NearlyEqual(returnedToGround->position.y, 0.0f) ||
        returnedToGround->actionState != PlayerActionState::Idle) {
        return 442;
    }

    PlayerSimulation ledgeLocomotion;
    ledgeLocomotion.SetCollisionSceneQuery(
        [](int32_t sceneId) { return sceneId == 118; });
    ledgeLocomotion.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3& from,
           const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            if (!NearlyEqual(from.x, to.x) || !NearlyEqual(from.z, to.z)) {
                return false;
            }
            const float floorY = from.x < 3.0f ? 0.0f : -120.0f;
            if (from.y >= floorY && to.y <= floorY) {
                impact = { to.x, floorY, to.z };
                return true;
            }
            return false;
        });
    ledgeLocomotion.EnsurePlayer(33, PlayerSpawn{ 118, {}, 0.0f });
    if (!ledgeLocomotion.SelectWeapon(33, 1)) return 465;
    PlayerCommand leaveLedge{};
    leaveLedge.ownerPlayerId = 33;
    leaveLedge.sequence = 1;
    leaveLedge.sceneId = 118;
    leaveLedge.moveX = 1.0f;
    leaveLedge.actionSequence = 1;
    leaveLedge.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    if (!ledgeLocomotion.SubmitCommand(leaveLedge)) return 460;
    ledgeLocomotion.StepFixed();
    const auto falling = ledgeLocomotion.SnapshotForPlayer(33);
    if (!falling || falling->locomotionMode !=
                        Game::Simulation::PlayerLocomotionMode::Airborne ||
        !NearlyEqual(falling->position.x, 6.0f) ||
        !(falling->position.y < 0.0f) || !(falling->velocity.y < 0.0f) ||
        falling->actionState != PlayerActionState::Idle) {
        return 461;
    }
    leaveLedge.sequence = 2;
    leaveLedge.actionSequence = 2;
    leaveLedge.moveX = 0.0f;
    if (!ledgeLocomotion.SubmitCommand(leaveLedge)) return 462;
    ledgeLocomotion.StepFixed();
    const auto jumpSlashing = ledgeLocomotion.SnapshotForPlayer(33);
    if (!jumpSlashing ||
        jumpSlashing->locomotionMode !=
            Game::Simulation::PlayerLocomotionMode::Airborne ||
        jumpSlashing->actionState != PlayerActionState::JumpSlashing) {
        return 473;
    }
    for (int tick = 0; tick < 30; ++tick) {
        ledgeLocomotion.StepFixed();
    }
    const auto landed = ledgeLocomotion.SnapshotForPlayer(33);
    if (!landed || landed->locomotionMode !=
                       Game::Simulation::PlayerLocomotionMode::Grounded ||
        !NearlyEqual(landed->position.y, -120.0f) ||
        !NearlyEqual(landed->velocity.y, 0.0f) ||
        landed->actionState != PlayerActionState::Idle) {
        return 463;
    }

    PlayerSimulation bodyCollisionForward;
    PlayerSimulation bodyCollisionReverse;
    const auto configureBodyCollision = [](PlayerSimulation& target, bool reverse) {
        const PlayerSpawn leftSpawn{ 118, { -18.0f, 0.0f, 0.0f }, 0.0f };
        const PlayerSpawn rightSpawn{ 118, { 18.0f, 0.0f, 0.0f }, 0.0f };
        if (reverse) {
            target.EnsurePlayer(21, rightSpawn);
            target.EnsurePlayer(20, leftSpawn);
        } else {
            target.EnsurePlayer(20, leftSpawn);
            target.EnsurePlayer(21, rightSpawn);
        }
        PlayerCommand left{};
        left.ownerPlayerId = 20;
        left.sequence = 1;
        left.sceneId = 118;
        left.moveX = 1.0f;
        PlayerCommand right = left;
        right.ownerPlayerId = 21;
        right.moveX = -1.0f;
        return target.SubmitCommand(left) && target.SubmitCommand(right);
    };
    if (!configureBodyCollision(bodyCollisionForward, false) ||
        !configureBodyCollision(bodyCollisionReverse, true)) {
        return 361;
    }
    bodyCollisionForward.StepFixed();
    bodyCollisionForward.StepFixed();
    bodyCollisionReverse.StepFixed();
    bodyCollisionReverse.StepFixed();
    const auto forwardLeft = bodyCollisionForward.SnapshotForPlayer(20);
    const auto forwardRight = bodyCollisionForward.SnapshotForPlayer(21);
    const auto reverseLeft = bodyCollisionReverse.SnapshotForPlayer(20);
    const auto reverseRight = bodyCollisionReverse.SnapshotForPlayer(21);
    if (!forwardLeft || !forwardRight || !reverseLeft || !reverseRight ||
        forwardRight->position.x - forwardLeft->position.x < 23.99f ||
        !NearlyEqual(forwardLeft->position.x, reverseLeft->position.x) ||
        !NearlyEqual(forwardRight->position.x, reverseRight->position.x)) {
        return 362;
    }

    PlayerSimulation sceneIsolatedBodies;
    sceneIsolatedBodies.EnsurePlayer(30, PlayerSpawn{ 118, {}, 0.0f });
    sceneIsolatedBodies.EnsurePlayer(31, PlayerSpawn{ 119, {}, 0.0f });
    sceneIsolatedBodies.StepFixed();
    const auto sceneA = sceneIsolatedBodies.SnapshotForPlayer(30);
    const auto sceneB = sceneIsolatedBodies.SnapshotForPlayer(31);
    if (!sceneA || !sceneB || !NearlyEqual(sceneA->position.x, 0.0f) ||
        !NearlyEqual(sceneB->position.x, 0.0f)) {
        return 363;
    }

    Game::Client::LocalPlayerVitals localVitals;
    PlayerSnapshot vitalSnapshot{};
    vitalSnapshot.entity = { 50, 2 };
    vitalSnapshot.ownerPlayerId = 50;
    vitalSnapshot.sceneId = 118;
    vitalSnapshot.serverTick = 10;
    vitalSnapshot.lifeEpoch = 1;
    vitalSnapshot.health = 40;
    if (localVitals.Apply(vitalSnapshot, 51) !=
            Game::Client::LocalPlayerVitalsUpdate::Invalid ||
        localVitals.HasState() ||
        localVitals.Apply(vitalSnapshot, 50) !=
            Game::Client::LocalPlayerVitalsUpdate::Applied ||
        localVitals.Health() != 40) {
        return 364;
    }
    vitalSnapshot.serverTick = 9;
    vitalSnapshot.health = 8;
    if (localVitals.Apply(vitalSnapshot, 50) !=
            Game::Client::LocalPlayerVitalsUpdate::Stale ||
        localVitals.Health() != 40) {
        return 365;
    }
    vitalSnapshot.serverTick = 11;
    vitalSnapshot.health = 32;
    if (localVitals.Apply(vitalSnapshot, 50) !=
            Game::Client::LocalPlayerVitalsUpdate::Applied ||
        localVitals.Health() != 32) {
        return 366;
    }
    vitalSnapshot.lifeEpoch = 2;
    vitalSnapshot.serverTick = 12;
    vitalSnapshot.health = 48;
    if (localVitals.Apply(vitalSnapshot, 50) !=
            Game::Client::LocalPlayerVitalsUpdate::Applied ||
        localVitals.Health() != 48 || localVitals.LifeEpoch() != 2) {
        return 367;
    }
    vitalSnapshot.lifeEpoch = 1;
    vitalSnapshot.serverTick = 13;
    vitalSnapshot.health = 0;
    if (localVitals.Apply(vitalSnapshot, 50) !=
            Game::Client::LocalPlayerVitalsUpdate::Stale ||
        localVitals.Health() != 48) {
        return 368;
    }
    localVitals.Reset();
    if (localVitals.HasState() || localVitals.Health() != 0 ||
        localVitals.LifeEpoch() != 0) {
        return 369;
    }

    PlayerSimulation actionSimulation;
    actionSimulation.EnsurePlayer(11, PlayerSpawn{ 118, {}, 0.0f });
    if (!actionSimulation.SelectWeapon(11, 1)) return 359;
    command.ownerPlayerId = 11;
    command.sequence = 1;
    command.moveY = 0.0f;
    command.actionSequence = 1;
    command.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    actionSimulation.SubmitCommand(command);
    actionSimulation.StepFixed();
    const auto windup = actionSimulation.SnapshotForPlayer(11);
    if (!windup || windup->actionState != PlayerActionState::PrimaryWindup) {
        return 8;
    }
    for (int tick = 2; tick <= 15; ++tick) actionSimulation.StepFixed();
    const auto recovered = actionSimulation.SnapshotForPlayer(11);
    if (!recovered || recovered->actionState != PlayerActionState::Idle) {
        return 9;
    }

    PlayerSimulation bowActionSimulation;
    bowActionSimulation.EnsurePlayer(12, PlayerSpawn{ 118, {}, 0.0f });
    if (!bowActionSimulation.SelectWeapon(12, 3)) return 360;
    PlayerCommand bowHeld{};
    bowHeld.ownerPlayerId = 12;
    bowHeld.sequence = 1;
    bowHeld.sceneId = 118;
    bowHeld.heldActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    bowActionSimulation.SubmitCommand(bowHeld);
    bowActionSimulation.StepFixed();
    const auto bowDrawing = bowActionSimulation.SnapshotForPlayer(12);
    if (!bowDrawing || bowDrawing->actionState != PlayerActionState::Aiming) {
        return 128;
    }
    bowHeld.sequence = 2;
    bowHeld.heldActions = 0;
    bowActionSimulation.SubmitCommand(bowHeld);
    bowActionSimulation.StepFixed();
    const auto bowReleased = bowActionSimulation.SnapshotForPlayer(12);
    if (!bowReleased || bowReleased->actionState != PlayerActionState::Idle) {
        return 129;
    }

    PlayerSimulation combatSimulation;
    combatSimulation.EnsurePlayer(20, PlayerSpawn{ 118, {}, 0.0f });
    combatSimulation.EnsurePlayer(21, PlayerSpawn{ 118, { 0.0f, 0.0f, 40.0f }, 3.14159265358979323846f });
    if (!combatSimulation.SelectWeapon(20, 1) || !combatSimulation.SelectWeapon(21, 1)) return 361;
    command.ownerPlayerId = 20;
    command.sequence = 1;
    command.actionSequence = 1;
    command.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    combatSimulation.SubmitCommand(command);
    PlayerCommand block{};
    block.ownerPlayerId = 21;
    block.sequence = 1;
    block.sceneId = 118;
    block.headingRadians = 3.14159265358979323846f;
    block.heldActions = Game::Simulation::PLAYER_ACTION_BLOCK;
    combatSimulation.SubmitCommand(block);
    for (int tick = 1; tick <= 7; ++tick) combatSimulation.StepFixed();
    const auto blockedEvents = combatSimulation.DrainCombatResults();
    const auto blockingTarget = combatSimulation.SnapshotForPlayer(21);
    if (blockedEvents.size() != 1 ||
        blockedEvents.front().eventId != 1 ||
        blockedEvents.front().result != Game::Simulation::CombatResultKind::Blocked ||
        blockedEvents.front().attackKind != Game::Simulation::CombatAttackKind::Melee ||
        blockedEvents.front().hitRegion != Game::Simulation::PlayerHitRegion::None ||
        !blockedEvents.front().sourceEntity.Valid() || !blockedEvents.front().targetEntity.Valid() ||
        !blockingTarget ||
        blockingTarget->health != 48) {
        std::printf("blocked events=%zu result=%d health=%u action=%u\n", blockedEvents.size(),
                    blockedEvents.empty() ? -1 : static_cast<int>(blockedEvents.front().result),
                    blockingTarget ? blockingTarget->health : 0,
                    blockingTarget ? static_cast<unsigned>(blockingTarget->actionState) : 0);
        return 10;
    }

    for (int tick = 8; tick <= 13; ++tick) combatSimulation.StepFixed();
    block.sequence = 2;
    block.heldActions = 0;
    combatSimulation.SubmitCommand(block);
    command.sequence = 2;
    command.actionSequence = 2;
    command.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    combatSimulation.SubmitCommand(command);
    for (int tick = 14; tick <= 22; ++tick) combatSimulation.StepFixed();
    const auto hitEvents = combatSimulation.DrainCombatResults();
    const auto damagedTarget = combatSimulation.SnapshotForPlayer(21);
    if (hitEvents.size() != 1 ||
        hitEvents.front().eventId != 2 ||
        hitEvents.front().result != Game::Simulation::CombatResultKind::Damaged ||
        hitEvents.front().attackKind != Game::Simulation::CombatAttackKind::Melee ||
        hitEvents.front().hitRegion == Game::Simulation::PlayerHitRegion::None ||
        hitEvents.front().damage != Game::Simulation::DamageForPlayerHitRegion(
                                        8, hitEvents.front().hitRegion) ||
        !damagedTarget ||
        damagedTarget->health != 48 - hitEvents.front().damage) {
        return 11;
    }
    if (!combatSimulation.ApplyDamage(20, 21, 40, 0) || !combatSimulation.RespawnPlayer(21)) {
        return 12;
    }
    const auto respawned = combatSimulation.SnapshotForPlayer(21);
    if (!respawned || respawned->health != 48 || !NearlyEqual(respawned->position.z, 40.0f)) {
        return 13;
    }

    PlayerSimulation projectilePlayers;
    projectilePlayers.EnsurePlayer(30, PlayerSpawn{ 118, {}, 0.0f });
    projectilePlayers.EnsurePlayer(31, PlayerSpawn{ 118, { 0.0f, 0.0f, 80.0f }, 3.14159265358979323846f });
    ProjectileSimulation projectiles;
    const auto spawnedArrow = projectiles.SpawnArrow(
        { 30, 118, 0, { 0.0f, 42.0f, 14.0f }, { 0.0f, 0.0f, 3000.0f }, 0 });
    if (!spawnedArrow || spawnedArrow->replicationId <= 0 ||
        !projectiles.HasArrow(30, spawnedArrow->replicationId)) {
        return 14;
    }
    ProjectileSimulation indexedProjectiles;
    const auto indexedOwner30 = indexedProjectiles.SpawnArrow(
        { 30, 118, 0, {}, { 0.0f, 0.0f, 1.0f }, 0 });
    const auto indexedOwner31 = indexedProjectiles.SpawnArrow(
        { 31, 118, 0, {}, { 0.0f, 0.0f, 1.0f }, 0 });
    if (!indexedOwner30 || !indexedOwner31 ||
        indexedProjectiles.HasArrow(31, indexedOwner30->replicationId) ||
        indexedProjectiles.RemoveArrow(31, indexedOwner30->replicationId)) {
        return 519;
    }
    indexedProjectiles.RemoveOwnedBy(30);
    if (indexedProjectiles.HasArrow(30, indexedOwner30->replicationId) ||
        !indexedProjectiles.HasArrow(31, indexedOwner31->replicationId) ||
        indexedProjectiles.Snapshots().size() != 1) {
        return 520;
    }
    indexedProjectiles.Reset();
    if (indexedProjectiles.HasArrow(31, indexedOwner31->replicationId) ||
        !indexedProjectiles.Snapshots().empty()) {
        return 521;
    }
    projectiles.StepFixed(projectilePlayers);
    projectiles.StepFixed(projectilePlayers);
    const auto projectileEvents = projectiles.DrainEvents();
    const auto projectileDamage = projectilePlayers.DrainCombatResults();
    const auto projectileTarget = projectilePlayers.SnapshotForPlayer(31);
    if (projectileEvents.size() < 2 || projectileEvents.front().kind != Game::Simulation::ArrowEventKind::Created ||
        projectileEvents.back().kind != Game::Simulation::ArrowEventKind::HitPlayer ||
        projectileDamage.size() != 1 ||
        projectileDamage.front().eventId != 1 ||
        projectileDamage.front().attackKind != Game::Simulation::CombatAttackKind::Arrow ||
        projectileDamage.front().result != Game::Simulation::CombatResultKind::Damaged ||
        projectileDamage.front().hitRegion == Game::Simulation::PlayerHitRegion::None ||
        projectileDamage.front().damage != Game::Simulation::DamageForPlayerHitRegion(
                                               8, projectileDamage.front().hitRegion) ||
        !projectileTarget ||
        projectileTarget->health != 48 - projectileDamage.front().damage ||
        projectiles.HasArrow(30, spawnedArrow->replicationId)) {
        return 15;
    }

    PlayerCommand projectileBlock{};
    projectileBlock.ownerPlayerId = 31;
    projectileBlock.sequence = 1;
    projectileBlock.sceneId = 118;
    projectileBlock.headingRadians = 3.14159265358979323846f;
    projectileBlock.heldActions = Game::Simulation::PLAYER_ACTION_BLOCK;
    if (!projectilePlayers.SelectWeapon(31, 1)) return 362;
    projectilePlayers.SubmitCommand(projectileBlock);
    projectilePlayers.StepFixed();
    const auto blockedArrow = projectiles.SpawnArrow(
        { 30, 118, 0, { 0.0f, 42.0f, 14.0f }, { 0.0f, 0.0f, 3000.0f }, 0 });
    projectiles.StepFixed(projectilePlayers);
    projectiles.StepFixed(projectilePlayers);
    const auto shieldEvents = projectiles.DrainEvents();
    const auto shieldResults = projectilePlayers.DrainCombatResults();
    const auto shieldTarget = projectilePlayers.SnapshotForPlayer(31);
    if (!blockedArrow || shieldEvents.empty() ||
        shieldEvents.back().kind != Game::Simulation::ArrowEventKind::Blocked ||
        shieldResults.size() != 1 ||
        shieldResults.front().eventId != 2 ||
        shieldResults.front().attackKind != Game::Simulation::CombatAttackKind::Arrow ||
        shieldResults.front().result != Game::Simulation::CombatResultKind::Blocked ||
        shieldResults.front().hitRegion != Game::Simulation::PlayerHitRegion::None ||
        !shieldResults.front().sourceEntity.Valid() || !shieldResults.front().targetEntity.Valid() ||
        !shieldTarget ||
        shieldTarget->health != 48 - projectileDamage.front().damage) {
        return 16;
    }

    PlayerSimulation teamPlayers;
    teamPlayers.EnsurePlayer(32, PlayerSpawn{ 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    teamPlayers.EnsurePlayer(
        33, PlayerSpawn{ 118, { 0.0f, 0.0f, 40.0f }, 0.0f, Game::Simulation::TeamId::Red });
    teamPlayers.EnsurePlayer(
        34, PlayerSpawn{ 118, { 0.0f, 0.0f, 80.0f }, 0.0f, Game::Simulation::TeamId::Blue });
    ProjectileSimulation teamProjectiles;
    if (!teamProjectiles.SpawnArrow(
            { 32, 118, 0, { 0.0f, 42.0f, 14.0f }, { 0.0f, 0.0f, 3000.0f }, 0 })) {
        return 126;
    }
    teamProjectiles.StepFixed(teamPlayers);
    teamProjectiles.StepFixed(teamPlayers);
    const auto teamDamage = teamPlayers.DrainCombatResults();
    const auto friendlyTarget = teamPlayers.SnapshotForPlayer(33);
    const auto enemyTarget = teamPlayers.SnapshotForPlayer(34);
    if (teamDamage.size() != 1 || teamDamage.front().targetPlayerId != 34 ||
        teamDamage.front().damage != Game::Simulation::DamageForPlayerHitRegion(
                                         8, teamDamage.front().hitRegion) ||
        !friendlyTarget || friendlyTarget->health != 48 ||
        !enemyTarget || enemyTarget->health != 48 - teamDamage.front().damage) {
        return 127;
    }

    PlayerSimulation emptyWorldPlayers;
    ProjectileSimulation worldProjectiles;
    worldProjectiles.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3& from, const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            if (from.z < 100.0f && to.z >= 100.0f) {
                impact = { 0.0f, 42.0f, 100.0f };
                return true;
            }
            return false;
        });
    worldProjectiles.SpawnArrow({ 40, 118, 0, { 0.0f, 42.0f, 14.0f },
                                  { 0.0f, 0.0f, 3000.0f }, 0 });
    worldProjectiles.StepFixed(emptyWorldPlayers);
    worldProjectiles.StepFixed(emptyWorldPlayers);
    const auto worldEvents = worldProjectiles.DrainEvents();
    const auto stuckArrows = worldProjectiles.Snapshots();
    if (worldEvents.empty() || worldEvents.back().kind != Game::Simulation::ArrowEventKind::Stuck ||
        stuckArrows.size() != 1 || stuckArrows.front().phase != Game::Simulation::ArrowPhase::Stuck ||
        !NearlyEqual(stuckArrows.front().position.z, 100.0f)) {
        return 17;
    }

    ProjectileSimulation retentionProjectiles;
    retentionProjectiles.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3&, const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            impact = to;
            return true;
        });
    for (int32_t id = 1; id <= 100; ++id) {
        if (!retentionProjectiles.SpawnArrow(
                { 50, 118, 0, { static_cast<float>(id), 42.0f, 0.0f }, { 0.0f, 0.0f, 60.0f }, 0 })) {
            return 18;
        }
    }
    retentionProjectiles.StepFixed(emptyWorldPlayers);
    if (retentionProjectiles.Snapshots().size() != 99) {
        return 19;
    }

    FishingSimulation fishing;
    fishing.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3& from, const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            if (from.x < 100.0f && from.z < 100.0f && to.z >= 100.0f) {
                impact = { from.x, from.y, 100.0f };
                return true;
            }
            return false;
        });
    fishing.SetWaterSurfaceQuery(
        [](int32_t sceneId, const Game::Simulation::Vec3&, float& surfaceY) {
            if (sceneId != 118) return false;
            surfaceY = 30.0f;
            return true;
        });
    PlayerSimulation fishingPlayers;
    fishingPlayers.EnsurePlayer(70, PlayerSpawn{ 118, {}, 0.0f });
    fishingPlayers.EnsurePlayer(71, PlayerSpawn{ 118, { 200.0f, 0.0f, 0.0f }, 0.0f });
    const auto owner70 = fishingPlayers.SnapshotForPlayer(70);
    const auto owner71 = fishingPlayers.SnapshotForPlayer(71);
    if (!owner70 || !owner71 ||
        !fishing.ApplyLureControl(70, 118, true, false, 2, *owner70) ||
        !fishing.ApplyLureControl(71, 118, true, false, 0, *owner71)) {
        return 23;
    }
    const auto initialLureSnapshots = fishing.LureSnapshots();
    if (initialLureSnapshots.size() != 2 ||
        initialLureSnapshots[0].ownerPlayerId != 70 ||
        initialLureSnapshots[1].ownerPlayerId != 71) {
        return 514;
    }
    const auto initialLure = fishing.LureForPlayer(70);
    for (int step = 0; step < 12; ++step) fishing.StepFixed(fishingPlayers);
    const auto collidedLure = fishing.LureForPlayer(70);
    for (int step = 0; step < 120; ++step) fishing.StepFixed(fishingPlayers);
    const auto waterLure = fishing.LureForPlayer(71);
    if (!initialLure || !collidedLure || !NearlyEqual(initialLure->position.z, 0.0f) ||
        !NearlyEqual(collidedLure->position.z, 98.0f) ||
        collidedLure->phase != Game::Simulation::FishingLurePhase::Settled || !waterLure ||
        !NearlyEqual(waterLure->position.y, 28.0f) ||
        waterLure->phase != Game::Simulation::FishingLurePhase::Settled || !fishing.RemoveLure(71) ||
        fishing.LureForPlayer(71).has_value()) {
        return 23;
    }
    const auto remainingLureSnapshots = fishing.LureSnapshots();
    if (remainingLureSnapshots.size() != 1 ||
        remainingLureSnapshots.front().ownerPlayerId != 70) {
        return 515;
    }
    const Game::Simulation::FishIdentity fishA{
        118, Game::Simulation::MakeFishSpawnKey(118, 0, 10, 20, 30)
    };
    const Game::Simulation::FishIdentity fishB{
        118, Game::Simulation::MakeFishSpawnKey(118, 0, 40, 20, 30)
    };
    const auto hookLure = fishing.LureForPlayer(70);
    if (!hookLure || fishing.HookNearestRegistered(70) ||
        !fishing.RegisterFish({ fishA, { 10.0f, 20.0f, 30.0f },
                                Game::Simulation::FishSpecies::HylianBass, 12.5f }) ||
        fishing.RegisterFish({ fishA, { 10.0f, 20.0f, 30.0f },
                               Game::Simulation::FishSpecies::HylianBass, 12.5f }) ||
        !fishing.RegisterFish({ fishB, { 40.0f, 20.0f, 30.0f },
                                Game::Simulation::FishSpecies::HylianLoach, 20.0f }) ||
        fishing.RegisteredFishCount() != 2 ||
        !fishing.HookNearestRegistered(70) ||
        fishing.HookNearestRegistered(71) ||
        fishing.HookNearestRegistered(70)) {
        return 24;
    }
    const auto ownedFish = fishing.FishOwnedBy(70);
    const auto fishOwner = fishing.OwnerOf(fishA);
    if (!ownedFish || !fishOwner || *fishOwner != 70 || ownedFish->identity != fishA ||
        ownedFish->species != Game::Simulation::FishSpecies::HylianBass ||
        !NearlyEqual(ownedFish->length, 12.5f)) {
        return 25;
    }
    if (
        !NearlyEqual(ownedFish->position.x, hookLure->position.x) ||
        !NearlyEqual(ownedFish->position.y, hookLure->position.y) ||
        !NearlyEqual(ownedFish->position.z, hookLure->position.z)) {
        return 341;
    }
    const auto releasedFish = fishing.ReleaseOwnedBy(70);
    if (releasedFish.size() != 1 || fishing.OwnerOf(fishA).has_value() ||
        !fishing.ApplyLureControl(71, 118, true, false, 0, *owner71)) {
        return 26;
    }
    for (int step = 0; step < 120; ++step) fishing.StepFixed(fishingPlayers);
    const auto player71SettledLure = fishing.LureForPlayer(71);
    if (!player71SettledLure ||
        !fishing.RegisterFish({
            { 118, Game::Simulation::MakeFishSpawnKey(
                       118, 0,
                       static_cast<int32_t>(std::lround(player71SettledLure->position.x)),
                       static_cast<int32_t>(std::lround(player71SettledLure->position.y)),
                       static_cast<int32_t>(std::lround(player71SettledLure->position.z))) },
            player71SettledLure->position,
            Game::Simulation::FishSpecies::HylianBass, 9.0f }) ||
        !fishing.HookNearestRegistered(71)) return 26;
    const auto fishOwnedBy71 = fishing.FishOwnedBy(71);
    if (!fishOwnedBy71 || !fishing.Release(fishOwnedBy71->identity, 71) ||
        !fishing.Snapshots().empty()) {
        return 27;
    }

    FishingSimulation hookValidation;
    hookValidation.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3&,
           const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            impact = to;
            return true;
        });
    PlayerSimulation hookValidationPlayers;
    hookValidationPlayers.EnsurePlayer(79, PlayerSpawn{ 118, {}, 0.0f });
    const auto hookValidationPlayer = hookValidationPlayers.SnapshotForPlayer(79);
    const Game::Simulation::FishIdentity nearbyFish{
        118, Game::Simulation::MakeFishSpawnKey(118, 0, 0, 42, 20)
    };
    if (!hookValidationPlayer ||
        !hookValidation.RegisterFish({ nearbyFish, { 0.0f, 42.0f, 20.0f },
                                       Game::Simulation::FishSpecies::HylianBass, 10.0f }) ||
        !hookValidation.ApplyLureControl(
            79, 118, true, false, 2, *hookValidationPlayer) ||
        hookValidation.HookNearestRegistered(79)) {
        return 411;
    }
    hookValidation.StepFixed(hookValidationPlayers);
    if (!hookValidation.HookNearestRegistered(79)) return 411;

    FishingSimulation distantHookValidation;
    distantHookValidation.SetCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3&,
           const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            impact = to;
            return true;
        });
    const Game::Simulation::FishIdentity distantFish{
        118, Game::Simulation::MakeFishSpawnKey(118, 0, 10000, 42, 10000)
    };
    if (!distantHookValidation.RegisterFish({
            distantFish, { 10000.0f, 42.0f, 10000.0f },
            Game::Simulation::FishSpecies::HylianBass, 10.0f }) ||
        !distantHookValidation.ApplyLureControl(
            79, 118, true, false, 2, *hookValidationPlayer)) {
        return 412;
    }
    distantHookValidation.StepFixed(hookValidationPlayers);
    if (distantHookValidation.HookNearestRegistered(79)) return 412;

    ServerWorld fishingAuthority;
    fishingAuthority.SetFishingCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3&,
           const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            impact = to;
            return true;
        });
    const Game::Simulation::FishIdentity boundedFish{
        118, Game::Simulation::MakeFishSpawnKey(118, 0, 0, 0, 0)
    };
    Game::Simulation::FishDefinition boundedDefinition{};
    boundedDefinition.identity = boundedFish;
    boundedDefinition.spawnPosition = {};
    boundedDefinition.length = 18.0f;
    boundedDefinition.bounded = true;
    boundedDefinition.minX = -100.0f;
    boundedDefinition.maxX = 100.0f;
    boundedDefinition.minY = -100.0f;
    boundedDefinition.maxY = 100.0f;
    boundedDefinition.minZ = -100.0f;
    boundedDefinition.maxZ = 100.0f;
    const auto authorityFishingPlayer = fishingAuthority.AdmitPlayer(72, { 118, {}, 0.0f });
    if (!authorityFishingPlayer || !fishingAuthority.RegisterFish(boundedDefinition) ||
        fishingAuthority.ExecuteLureControl(
            { 72, 1, authorityFishingPlayer->lifeEpoch, true, false }) ||
        fishingAuthority.ExecuteFishAction(
            { 72, 1, authorityFishingPlayer->lifeEpoch,
              Game::Simulation::FishActionKind::Hook })) {
        return 314;
    }
    Game::Simulation::PlayerCommand fishingCommand{};
    fishingCommand.ownerPlayerId = 72;
    fishingCommand.sequence = 1;
    fishingCommand.sceneId = 118;
    if (!fishingAuthority.ExecuteWeaponSelection(
            { 72, 1, authorityFishingPlayer->lifeEpoch, 4 }) ||
        !fishingAuthority.SubmitPlayerCommand(fishingCommand)) return 315;
    ServerWorldTestAccess::Players(fishingAuthority).StepFixed();
    const auto equippedFishingPlayer = fishingAuthority.PlayerFor(72);
    if (!equippedFishingPlayer || equippedFishingPlayer->selectedWeapon != 4 ||
        fishingAuthority.ExecuteLureControl(
            { 72, 1, equippedFishingPlayer->lifeEpoch + 1, true, false }) ||
        !fishingAuthority.ExecuteLureControl(
            { 72, 1, equippedFishingPlayer->lifeEpoch, true, false }) ||
        fishingAuthority.LureForPlayer(72) ||
        !fishingAuthority.ExecuteLureControl(
            { 72, 2, equippedFishingPlayer->lifeEpoch, true, false }) ||
        !fishingAuthority.ExecuteLureControl(
            { 72, 2, equippedFishingPlayer->lifeEpoch, true, false }) ||
        fishingAuthority.ExecuteFishAction(
            { 72, 2, equippedFishingPlayer->lifeEpoch,
              Game::Simulation::FishActionKind::Release }) ||
        fishingAuthority.ExecuteFishAction(
            { 72, 3, equippedFishingPlayer->lifeEpoch + 1,
              Game::Simulation::FishActionKind::Hook })) {
        return 316;
    }
    ServerWorldTestAccess::Fishing(fishingAuthority).StepFixed(
        ServerWorldTestAccess::Players(fishingAuthority));
    if (!fishingAuthority.ExecuteFishAction(
            { 72, 3, equippedFishingPlayer->lifeEpoch,
              Game::Simulation::FishActionKind::Hook })) return 316;
    const auto authoritativeLure = fishingAuthority.LureForPlayer(72);
    const auto authoritativeFish = fishingAuthority.FishOwnedBy(72);
    if (!authoritativeFish || authoritativeFish->identity != boundedFish ||
        !NearlyEqual(authoritativeFish->length, 18.0f) ||
        !authoritativeLure || authoritativeLure->lureType != 2 ||
        !NearlyEqual(authoritativeFish->position.x, authoritativeLure->position.x) ||
        !NearlyEqual(authoritativeFish->position.y, authoritativeLure->position.y) ||
        !NearlyEqual(authoritativeFish->position.z, authoritativeLure->position.z) ||
        fishingAuthority.ExecuteFishAction(
            { 72, 4, equippedFishingPlayer->lifeEpoch + 1,
              Game::Simulation::FishActionKind::Release }) ||
        !fishingAuthority.ExecuteFishAction(
            { 72, 5, equippedFishingPlayer->lifeEpoch,
              Game::Simulation::FishActionKind::Release }) ||
        fishingAuthority.FishOwnedBy(72)) {
        return 317;
    }

    ServerWorld groundedInteractionAuthority;
    groundedInteractionAuthority.SetPlayerCollisionSceneQuery(
        [](int32_t sceneId) { return sceneId == 118; });
    groundedInteractionAuthority.SetPlayerCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3& from,
           const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            if (!NearlyEqual(from.x, to.x) || !NearlyEqual(from.z, to.z) ||
                from.y < 0.0f || to.y > 0.0f) {
                return false;
            }
            impact = { to.x, 0.0f, to.z };
            return true;
        });
    groundedInteractionAuthority.SetPlayerWaterSurfaceQuery(
        [](int32_t sceneId, const Game::Simulation::Vec3& position,
           float& surfaceY) {
            if (sceneId != 118 || position.x < 3.0f) return false;
            surfaceY = 80.0f;
            return true;
        });
    const auto interactionPlayer = groundedInteractionAuthority.AdmitPlayer(
        74, { 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    if (!interactionPlayer ||
        !groundedInteractionAuthority.ExecuteWeaponSelection(
            { 74, 1, interactionPlayer->lifeEpoch, 4 }) ||
        !groundedInteractionAuthority.ExecuteLureControl(
            { 74, 1, interactionPlayer->lifeEpoch, true, false }) ||
        !groundedInteractionAuthority.LureForPlayer(74)) {
        return 467;
    }
    PlayerCommand enterInteractionWater{};
    enterInteractionWater.ownerPlayerId = 74;
    enterInteractionWater.sequence = 1;
    enterInteractionWater.lifeEpoch = interactionPlayer->lifeEpoch;
    enterInteractionWater.sceneId = 118;
    enterInteractionWater.moveX = 1.0f;
    if (!groundedInteractionAuthority.SubmitPlayerCommand(
            enterInteractionWater)) {
        return 468;
    }
    const auto interactionStart = ServerWorld::Clock::time_point{
        std::chrono::seconds(1)
    };
    groundedInteractionAuthority.Advance(interactionStart);
    groundedInteractionAuthority.Advance(interactionStart +
                                           std::chrono::milliseconds(50));
    const auto swimmingInteractionPlayer =
        groundedInteractionAuthority.PlayerFor(74);
    const auto lureLifecycle =
        groundedInteractionAuthority.DrainFishingLureEvents();
    const bool lureRetired = std::any_of(
        lureLifecycle.begin(), lureLifecycle.end(), [](const auto& event) {
            return event.kind ==
                Game::Simulation::FishingLureEventKind::Removed;
        });
    if (!swimmingInteractionPlayer ||
        Game::Simulation::CanPerformGroundedAction(
            *swimmingInteractionPlayer) ||
        !Game::Simulation::CanPerformFishingAction(
            *swimmingInteractionPlayer) ||
        swimmingInteractionPlayer->locomotionMode !=
            Game::Simulation::PlayerLocomotionMode::Swimming ||
        !groundedInteractionAuthority.LureForPlayer(74) || lureRetired) {
        return 469;
    }
    if (!groundedInteractionAuthority.ExecuteLureControl(
            { 74, 2, interactionPlayer->lifeEpoch, true, true })) {
        return 470;
    }
    const auto interactionObjective =
        groundedInteractionAuthority.EnsureObjective(
            { 9100, 118, { 6.0f, 80.0f, 0.0f }, 100.0f,
              Game::Simulation::TeamId::Red });
    const auto interactionStructure =
        groundedInteractionAuthority.EnsureStructure(
            { 9101, 9100, 118, { 6.0f, 80.0f, 0.0f }, 100, 25 });
    const auto swimmingBuild = groundedInteractionAuthority.ExecuteStructureAction(
        { 74, 1, interactionPlayer->lifeEpoch, 9101,
          Game::Simulation::StructureActionKind::Build });
    if (!interactionObjective.Valid() || !interactionStructure.Valid() ||
        swimmingBuild.result !=
            Game::Simulation::StructureActionResult::PlayerUnavailable ||
        groundedInteractionAuthority.StructureSnapshots().front().buildProgress != 0) {
        return 471;
    }
    if (!groundedInteractionAuthority.ExecuteWeaponSelection(
            { 74, 2, interactionPlayer->lifeEpoch, 3 })) {
        return 472;
    }
    PlayerCommand swimmingAim = enterInteractionWater;
    swimmingAim.sequence = 2;
    swimmingAim.moveX = 0.0f;
    swimmingAim.heldActions = Game::Simulation::PLAYER_ACTION_AIM;
    if (!groundedInteractionAuthority.SubmitPlayerCommand(swimmingAim)) {
        return 472;
    }
    ServerWorldTestAccess::Players(groundedInteractionAuthority).StepFixed();
    if (groundedInteractionAuthority.PlayerFor(74)->actionState !=
            PlayerActionState::Idle ||
        groundedInteractionAuthority.ExecuteArrowFire(
            { 74, 1, interactionPlayer->lifeEpoch }).accepted ||
        !groundedInteractionAuthority.ArrowSnapshots().empty()) {
        return 472;
    }

    ServerWorld airborneItemAuthority;
    airborneItemAuthority.SetPlayerCollisionSceneQuery(
        [](int32_t sceneId) { return sceneId == 118; });
    airborneItemAuthority.SetPlayerCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3& from,
           const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            if (!NearlyEqual(from.x, to.x) || !NearlyEqual(from.z, to.z)) return false;
            const float floorY = from.x < 3.0f ? 0.0f : -120.0f;
            if (from.y < floorY || to.y > floorY) return false;
            impact = { to.x, floorY, to.z };
            return true;
        });
    const auto airborneItemPlayer = airborneItemAuthority.AdmitPlayer(
        75, { 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    if (!airborneItemPlayer ||
        !airborneItemAuthority.ExecuteWeaponSelection(
            { 75, 1, airborneItemPlayer->lifeEpoch, 3 })) return 474;
    PlayerCommand airborneItemCommand{};
    airborneItemCommand.ownerPlayerId = 75;
    airborneItemCommand.sequence = 1;
    airborneItemCommand.lifeEpoch = airborneItemPlayer->lifeEpoch;
    airborneItemCommand.sceneId = 118;
    airborneItemCommand.moveX = 1.0f;
    if (!airborneItemAuthority.SubmitPlayerCommand(airborneItemCommand)) return 475;
    ServerWorldTestAccess::Players(airborneItemAuthority).StepFixed();
    airborneItemCommand.sequence = 2;
    airborneItemCommand.moveX = 0.0f;
    airborneItemCommand.heldActions = Game::Simulation::PLAYER_ACTION_AIM;
    if (!airborneItemAuthority.SubmitPlayerCommand(airborneItemCommand)) return 475;
    for (uint32_t tick = 0; tick <= Game::Simulation::kBowMinimumDrawDurationTicks;
         ++tick) {
        if (tick != 0) {
            ++airborneItemCommand.sequence;
            if (!airborneItemAuthority.SubmitPlayerCommand(airborneItemCommand)) return 475;
        }
        ServerWorldTestAccess::Players(airborneItemAuthority).StepFixed();
    }
    const auto airborneAiming = airborneItemAuthority.PlayerFor(75);
    if (!airborneAiming) return 476;
    if (airborneAiming->locomotionMode !=
        Game::Simulation::PlayerLocomotionMode::Airborne) return 481;
    if (airborneAiming->actionState != PlayerActionState::Aiming) return 482;
    if (!airborneItemAuthority.ExecuteArrowFire(
             { 75, 1, airborneItemPlayer->lifeEpoch }).accepted) return 479;
    if (airborneItemAuthority.ArrowSnapshots().empty()) return 480;
    if (!airborneItemAuthority.ExecuteWeaponSelection(
            { 75, 2, airborneItemPlayer->lifeEpoch, 4 }) ||
        !airborneItemAuthority.ExecuteLureControl(
            { 75, 1, airborneItemPlayer->lifeEpoch, true, false }) ||
        !airborneItemAuthority.LureForPlayer(75)) {
        return 477;
    }
    if (!airborneItemAuthority.ExecuteWeaponSelection(
            { 75, 3, airborneItemPlayer->lifeEpoch, 1 })) return 478;
    ++airborneItemCommand.sequence;
    airborneItemCommand.heldActions = Game::Simulation::PLAYER_ACTION_BLOCK;
    if (!airborneItemAuthority.SubmitPlayerCommand(airborneItemCommand)) return 478;
    ServerWorldTestAccess::Players(airborneItemAuthority).StepFixed();
    if (airborneItemAuthority.PlayerFor(75)->actionState !=
        PlayerActionState::Blocking) return 478;

    ServerWorld loadoutAuthority;
    const uint8_t swordOnlyMask =
        Game::Simulation::WeaponSlotMask(Game::Simulation::PlayerWeaponSlot::None) |
        Game::Simulation::WeaponSlotMask(Game::Simulation::PlayerWeaponSlot::OneHandedSword);
    if (loadoutAuthority.ConfigurePlayerLoadout(76, { swordOnlyMask }) ||
        !loadoutAuthority.AdmitPlayer(76, { 118, {}, 0.0f }) ||
        loadoutAuthority.ConfigurePlayerLoadout(76, { 0 }) ||
        !loadoutAuthority.ConfigurePlayerLoadout(76, { swordOnlyMask })) {
        return 342;
    }
    PlayerCommand deniedBow{};
    deniedBow.ownerPlayerId = 76;
    deniedBow.sequence = 1;
    deniedBow.actionSequence = 1;
    deniedBow.sceneId = 118;
    deniedBow.moveY = 1.0f;
    deniedBow.heldActions = Game::Simulation::PLAYER_ACTION_PRIMARY |
                            Game::Simulation::PLAYER_ACTION_AIM;
    deniedBow.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    if (loadoutAuthority.ExecuteWeaponSelection(
            { 76, 1, loadoutAuthority.PlayerFor(76)->lifeEpoch,
              static_cast<uint8_t>(Game::Simulation::PlayerWeaponSlot::Bow) })) {
        return 343;
    }
    if (!loadoutAuthority.SubmitPlayerCommand(deniedBow)) return 343;
    ServerWorldTestAccess::Players(loadoutAuthority).StepFixed();
    const auto deniedSnapshot = loadoutAuthority.PlayerFor(76);
    if (!deniedSnapshot || deniedSnapshot->selectedWeapon != 0 ||
        deniedSnapshot->heldActions != 0 ||
        deniedSnapshot->actionState != PlayerActionState::Idle ||
        deniedSnapshot->position.z <= 0.0f || deniedSnapshot->lastProcessedCommand != 1) {
        return 344;
    }
    PlayerCommand allowedSword = deniedBow;
    allowedSword.sequence = 2;
    allowedSword.actionSequence = 2;
    allowedSword.moveY = 0.0f;
    allowedSword.heldActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    allowedSword.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    const uint32_t loadoutLifeEpoch = loadoutAuthority.PlayerFor(76)->lifeEpoch;
    if (!loadoutAuthority.ExecuteWeaponSelection(
            { 76, 2, loadoutLifeEpoch,
              static_cast<uint8_t>(Game::Simulation::PlayerWeaponSlot::OneHandedSword) }) ||
        loadoutAuthority.ExecuteWeaponSelection(
            { 76, 2, loadoutLifeEpoch,
              static_cast<uint8_t>(Game::Simulation::PlayerWeaponSlot::None) }) ||
        loadoutAuthority.ExecuteWeaponSelection(
            { 76, 1, loadoutLifeEpoch,
              static_cast<uint8_t>(Game::Simulation::PlayerWeaponSlot::None) }) ||
        loadoutAuthority.ExecuteWeaponSelection(
            { 76, 3, loadoutLifeEpoch + 1,
              static_cast<uint8_t>(Game::Simulation::PlayerWeaponSlot::None) }) ||
        !loadoutAuthority.ExecuteWeaponSelection(
            { 76, 3, loadoutLifeEpoch,
              static_cast<uint8_t>(Game::Simulation::PlayerWeaponSlot::OneHandedSword) }) ||
        !loadoutAuthority.SubmitPlayerCommand(allowedSword)) return 345;
    ServerWorldTestAccess::Players(loadoutAuthority).StepFixed();
    const auto allowedSnapshot = loadoutAuthority.PlayerFor(76);
    if (!allowedSnapshot || allowedSnapshot->selectedWeapon != 1 ||
        allowedSnapshot->actionState != PlayerActionState::PrimaryWindup) {
        return 346;
    }

    ServerWorld arrowAuthority;
    const auto authorityArrowPlayer =
        arrowAuthority.AdmitPlayer(73, { 118, { 100.0f, 10.0f, 200.0f }, 0.0f });
    if (!authorityArrowPlayer ||
        arrowAuthority.ExecuteArrowFire(
            { 73, 1, authorityArrowPlayer->lifeEpoch }).accepted) {
        return 318;
    }
    Game::Simulation::PlayerCommand bowCommand{};
    bowCommand.ownerPlayerId = 73;
    bowCommand.sequence = 1;
    bowCommand.sceneId = 118;
    bowCommand.headingRadians = 1.57079632679f;
    bowCommand.aimPitchRadians = 0.25f;
    bowCommand.heldActions = Game::Simulation::PLAYER_ACTION_AIM;
    if (!arrowAuthority.ExecuteWeaponSelection(
            { 73, 1, arrowAuthority.PlayerFor(73)->lifeEpoch, 3 }) ||
        !arrowAuthority.SubmitPlayerCommand(bowCommand)) return 319;
    ServerWorldTestAccess::Players(arrowAuthority).StepFixed();
    const uint32_t arrowLifeEpoch = arrowAuthority.PlayerFor(73)->lifeEpoch;
    const auto staleArrowDecision =
        arrowAuthority.ExecuteArrowFire({ 73, 2, arrowLifeEpoch + 1 });
    const auto prematureArrowDecision =
        arrowAuthority.ExecuteArrowFire({ 73, 2, arrowLifeEpoch });
    const auto duplicatePrematureArrowDecision =
        arrowAuthority.ExecuteArrowFire({ 73, 2, arrowLifeEpoch });
    for (uint32_t tick = 0;
         tick < Game::Simulation::kBowMinimumDrawDurationTicks; ++tick) {
        bowCommand.sequence = 2 + tick;
        if (!arrowAuthority.SubmitPlayerCommand(bowCommand)) return 320;
        ServerWorldTestAccess::Players(arrowAuthority).StepFixed();
    }
    const auto acceptedArrowDecision =
        arrowAuthority.ExecuteArrowFire({ 73, 3, arrowLifeEpoch });
    const auto duplicateArrowDecision =
        arrowAuthority.ExecuteArrowFire({ 73, 3, arrowLifeEpoch });
    const auto consumedDrawDecision =
        arrowAuthority.ExecuteArrowFire({ 73, 4, arrowLifeEpoch });
    if (staleArrowDecision.accepted || prematureArrowDecision.accepted ||
        duplicatePrematureArrowDecision.accepted || !acceptedArrowDecision.accepted ||
        acceptedArrowDecision.projectileId <= 0 ||
        !duplicateArrowDecision.accepted ||
        duplicateArrowDecision.projectileId != acceptedArrowDecision.projectileId ||
        consumedDrawDecision.accepted || consumedDrawDecision.projectileId != 0 ||
        ServerWorldTestAccess::Players(arrowAuthority).BowShotReady(73)) {
        return 320;
    }
    const auto authoritativeArrows = arrowAuthority.ArrowSnapshots();
    if (authoritativeArrows.size() != 1 || !NearlyEqual(authoritativeArrows.front().position.x, 114.0f) ||
        authoritativeArrows.front().projectileType != 2 ||
        !NearlyEqual(authoritativeArrows.front().position.y, 52.0f) ||
        !NearlyEqual(authoritativeArrows.front().position.z, 200.0f) ||
        !NearlyEqual(authoritativeArrows.front().velocity.x, std::cos(0.25f) * 3000.0f) ||
        !NearlyEqual(authoritativeArrows.front().velocity.y, -std::sin(0.25f) * 3000.0f)) {
        return 321;
    }

    SpatialGridIndex interest;
    interest.Update(1, 118, {});
    interest.Update(2, 118, { 500.0f, 0.0f, 0.0f });
    interest.Update(3, 118, { 7000.0f, 0.0f, 0.0f });
    interest.Update(4, 119, {});
    const auto nearby = interest.CandidatesNear(118, {}, 1000.0f);
    if (nearby != std::vector<Game::Simulation::SpatialIndexId>({ 1, 2 })) {
        return 32;
    }
    interest.Update(2, 118, { 8000.0f, 0.0f, 0.0f });
    interest.Remove(1);
    if (!interest.CandidatesNear(118, {}, 1000.0f).empty()) {
        return 33;
    }
    interest.Reset();
    if (!interest.CandidatesNear(118, {}, 10000.0f).empty()) {
        return 34;
    }

    PlayerSimulation teams;
    teams.EnsurePlayer(90, PlayerSpawn{ 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    teams.EnsurePlayer(91, PlayerSpawn{ 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    teams.EnsurePlayer(92, PlayerSpawn{ 118, {}, 0.0f, Game::Simulation::TeamId::Blue });
    if (teams.ApplyDamage(90, 91, 8, 0) || !teams.ApplyDamage(90, 92, 8, 0)) {
        return 35;
    }
    const auto ally = teams.SnapshotForPlayer(91);
    const auto enemy = teams.SnapshotForPlayer(92);
    if (!ally || !enemy || ally->health != 48 || enemy->health != 40 ||
        ally->team != Game::Simulation::TeamId::Red || enemy->team != Game::Simulation::TeamId::Blue ||
        !teams.SetPlayerTeam(91, Game::Simulation::TeamId::Blue) ||
        teams.TeamForPlayer(91) != Game::Simulation::TeamId::Blue) {
        return 36;
    }
    if (!teams.ApplyDamage(90, 91, 8, 0)) {
        return 37;
    }

    ObjectiveSimulation objectives;
    const auto objectiveId = objectives.EnsureObjective({ 100, 118, {}, 300.0f, Game::Simulation::TeamId::Neutral });
    if (!objectiveId.Valid() || objectives.EnsureObjective({ 100, 119, {}, 1.0f }) != objectiveId) {
        return 38;
    }
    PlayerSimulation objectivePlayers;
    objectivePlayers.EnsurePlayer(
        200, PlayerSpawn{ 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    objectivePlayers.EnsurePlayer(
        201, PlayerSpawn{ 118, {}, 0.0f, Game::Simulation::TeamId::Blue });
    objectivePlayers.EnsurePlayer(
        202, PlayerSpawn{ 118, { 5000.0f, 0.0f, 0.0f }, 0.0f,
                          Game::Simulation::TeamId::Blue });
    objectivePlayers.EnsurePlayer(
        203, PlayerSpawn{ 119, {}, 0.0f, Game::Simulation::TeamId::Blue });
    objectives.Update(objectivePlayers, 0.25f);
    const auto contestedObjective = objectives.SnapshotForObjective(100);
    if (!contestedObjective || !contestedObjective->contested || contestedObjective->captureProgress != 0.0f) {
        return 39;
    }
    objectivePlayers.RemovePlayer(201);
    for (int i = 0; i < 20; ++i) {
        objectives.Update(objectivePlayers, 0.25f);
    }
    const auto capturedObjective = objectives.SnapshotForObjective(100);
    const auto captureEvents = objectives.DrainCapturedEvents();
    if (!capturedObjective || capturedObjective->owner != Game::Simulation::TeamId::Red ||
        capturedObjective->contested || captureEvents.size() != 1 ||
        captureEvents.front().previousOwner != Game::Simulation::TeamId::Neutral ||
        captureEvents.front().newOwner != Game::Simulation::TeamId::Red) {
        return 40;
    }
    if (!objectivePlayers.ApplyDamage(-1, 200, 48, 0)) return 526;
    objectives.Update(objectivePlayers, 0.25f);
    if (!objectives.DrainCapturedEvents().empty() || !objectives.RemoveObjective(100) ||
        objectives.SnapshotForObjective(100).has_value()) {
        return 41;
    }
    if (objectives.EnsureObjective({ -1, 118, {}, 300.0f }).Valid()) return 527;
    const auto indexedReplacementObjective = objectives.EnsureObjective(
        { 100, 118, {}, 300.0f, Game::Simulation::TeamId::Neutral });
    if (!indexedReplacementObjective.Valid() ||
        indexedReplacementObjective.index != objectiveId.index ||
        indexedReplacementObjective.generation == objectiveId.generation) {
        return 528;
    }
    objectives.Restore({ *capturedObjective });
    const auto restoredObjective = objectives.SnapshotForObjective(100);
    if (!restoredObjective ||
        restoredObjective->owner != Game::Simulation::TeamId::Red ||
        !objectives.RemoveObjective(100)) {
        return 529;
    }
    objectives.Reset();
    if (objectives.SnapshotForObjective(100)) return 530;

    StructureSimulation structures;
    const auto structureId = structures.EnsureStructure({ 200, 100, 118, {}, 500, 100 });
    if (!structureId.Valid() || structures.EnsureStructure({ 200, 999, 119, {}, 1, 1 }) != structureId ||
        structures.ContributeBuild(200, Game::Simulation::TeamId::Blue,
                                   Game::Simulation::TeamId::Red, 100)) {
        return 42;
    }
    if (!structures.ContributeBuild(200, Game::Simulation::TeamId::Red,
                                    Game::Simulation::TeamId::Red, 40) ||
        !structures.ContributeBuild(200, Game::Simulation::TeamId::Red,
                                    Game::Simulation::TeamId::Red, 60)) {
        return 43;
    }
    auto structure = structures.SnapshotForStructure(200);
    auto structureEvents = structures.DrainEvents();
    if (!structure || structure->phase != Game::Simulation::StructurePhase::Active || structure->health != 500 ||
        structure->team != Game::Simulation::TeamId::Red || structureEvents.size() != 2 ||
        structureEvents.front().kind != Game::Simulation::StructureEventKind::BuildStarted ||
        structureEvents.back().kind != Game::Simulation::StructureEventKind::Built) {
        return 44;
    }
    if (structures.ApplyDamage(200, Game::Simulation::TeamId::Red, 100) ||
        !structures.ApplyDamage(200, Game::Simulation::TeamId::Blue, 200) ||
        !structures.Repair(200, Game::Simulation::TeamId::Red, 50)) {
        return 45;
    }
    structure = structures.SnapshotForStructure(200);
    if (!structure || structure->health != 350 ||
        !structures.ApplyDamage(200, Game::Simulation::TeamId::Blue, 999) ||
        !structures.ResetStructure(200)) {
        return 46;
    }
    structure = structures.SnapshotForStructure(200);
    if (!structure || structure->phase != Game::Simulation::StructurePhase::Planned || structure->health != 0 ||
        structure->buildProgress != 0 || structure->team != Game::Simulation::TeamId::Neutral ||
        !structures.RemoveStructure(200) || !structures.Snapshots().empty()) {
        return 47;
    }

    StructureSimulation spatialStructures;
    if (spatialStructures.EnsureStructure({ -1, 100, 118, {}, 100, 1 }).Valid()) {
        return 531;
    }
    const auto nearStructure = spatialStructures.EnsureStructure(
        { 300, 100, 118, {}, 100, 1 });
    spatialStructures.EnsureStructure(
        { 301, 100, 118, { 5000.0f, 0.0f, 0.0f }, 100, 1 });
    spatialStructures.EnsureStructure({ 302, 100, 119, {}, 100, 1 });
    for (const int32_t key : { 300, 301, 302 }) {
        if (!spatialStructures.ContributeBuild(
                key, Game::Simulation::TeamId::Red,
                Game::Simulation::TeamId::Red, 1)) {
            return 532;
        }
    }
    Game::Simulation::StructureHit structureHit{};
    if (!spatialStructures.FirstSegmentHit(
            118, { -100.0f, 90.0f, 0.0f }, { 100.0f, 90.0f, 0.0f },
            structureHit) ||
        structureHit.structureKey != 300 ||
        !spatialStructures.FirstSegmentHit(
            118, { 4900.0f, 90.0f, 0.0f },
            { 5100.0f, 90.0f, 0.0f }, structureHit) ||
        structureHit.structureKey != 301) {
        return 533;
    }
    if (!spatialStructures.RemoveStructure(300) ||
        spatialStructures.FirstSegmentHit(
            118, { -100.0f, 90.0f, 0.0f }, { 100.0f, 90.0f, 0.0f },
            structureHit)) {
        return 534;
    }
    const auto replacementStructure = spatialStructures.EnsureStructure(
        { 300, 100, 118, {}, 100, 1 });
    if (!replacementStructure.Valid() ||
        replacementStructure.index != nearStructure.index ||
        replacementStructure.generation == nearStructure.generation ||
        !spatialStructures.ContributeBuild(
            300, Game::Simulation::TeamId::Red,
            Game::Simulation::TeamId::Red, 1)) {
        return 535;
    }
    const auto retainedStructures = spatialStructures.Snapshots();
    spatialStructures.Restore(retainedStructures);
    if (!spatialStructures.FirstSegmentHit(
            118, { -100.0f, 90.0f, 0.0f }, { 100.0f, 90.0f, 0.0f },
            structureHit) ||
        structureHit.structureKey != 300) {
        return 536;
    }
    spatialStructures.Reset();
    if (!spatialStructures.Snapshots().empty() ||
        spatialStructures.FirstSegmentHit(
            118, { -100.0f, 90.0f, 0.0f }, { 100.0f, 90.0f, 0.0f },
            structureHit)) {
        return 537;
    }

    CorpseSimulation corpses;
    for (int32_t index = 0; index < 100; ++index) {
        Game::Simulation::CorpsePose pose{};
        pose.sourcePlayerId = index;
        pose.sourcePlayerEntity = { static_cast<uint32_t>(index), 1 };
        pose.sourceLifeEpoch = 1;
        pose.sceneId = 118;
        pose.position.x = static_cast<float>(index);
        if (!corpses.Create(pose).Valid()) return 59;
    }
    const auto retainedCorpseSnapshots = corpses.Snapshots();
    if (retainedCorpseSnapshots.size() != 99 ||
        std::any_of(retainedCorpseSnapshots.begin(), retainedCorpseSnapshots.end(),
                    [](const auto& corpse) {
                        return corpse.pose.sourcePlayerId == 0;
                    })) {
        return 60;
    }
    Game::Simulation::CorpsePose replacementPose{};
    replacementPose.sourcePlayerId = 101;
    replacementPose.sourcePlayerEntity = { 101, 1 };
    replacementPose.sourceLifeEpoch = 1;
    replacementPose.sceneId = 118;
    const auto replacementId = corpses.Create(replacementPose);
    if (!replacementId.Valid() || replacementId.generation < 2 || corpses.Snapshots().size() != 99) return 61;

    ClientPrediction prediction;
    const auto seedPrediction = [](
        ClientPrediction& client, uint32_t lifeEpoch, int32_t sceneId,
        const Game::Simulation::Vec3& position = {},
        Game::Simulation::PlayerLocomotionMode locomotion =
            Game::Simulation::PlayerLocomotionMode::Grounded) {
        PlayerSnapshot baseline{};
        baseline.lifeEpoch = lifeEpoch;
        baseline.sceneId = sceneId;
        baseline.position = position;
        baseline.locomotionMode = locomotion;
        return client.SeedAuthoritative(baseline);
    };
    const auto reconcilePosition = [](ClientPrediction& client, uint32_t sequence,
                                      uint32_t lifeEpoch, int32_t sceneId,
                                      const Game::Simulation::Vec3& authoritativePosition,
                                      const Game::Simulation::Vec3& currentPosition) {
        PlayerSnapshot snapshot{};
        snapshot.sceneId = sceneId;
        snapshot.serverTick = sequence;
        snapshot.actionStartTick = sequence;
        snapshot.lastProcessedCommand = sequence;
        snapshot.lifeEpoch = lifeEpoch;
        snapshot.position = authoritativePosition;
        return client.Reconcile(snapshot, currentPosition);
    };
    PlayerCommand correctionCommand{};
    correctionCommand.sequence = 1;
    correctionCommand.lifeEpoch = 1;
    correctionCommand.sceneId = 118;
    if (!seedPrediction(prediction, 1, 118, { 10.0f, 0.0f, 0.0f })) return 61;
    prediction.RecordCommand(correctionCommand);
    correctionCommand.sequence = 2;
    correctionCommand.moveX = 1.0f;
    prediction.RecordCommand(correctionCommand, 1.0f / 90.0f);
    if (!reconcilePosition(prediction, 1, 1, 118, { 8.0f, 0.0f, 0.0f },
                           { 12.0f, 0.0f, 0.0f }) ||
        prediction.PendingCommandCount() != 1 ||
        !NearlyEqual(prediction.PendingCorrection().x, -2.0f)) {
        return 62;
    }
    const auto firstCorrection = prediction.ConsumeCorrection(0.15f, 0.15f, 200.0f);
    if (!NearlyEqual(firstCorrection.x, -1.0f) ||
        !NearlyEqual(prediction.PendingCorrection().x, -1.0f) ||
        !reconcilePosition(prediction, 2, 1, 118, { 11.0f, 0.0f, 0.0f },
                           { 11.0f, 0.0f, 0.0f }) ||
        !NearlyEqual(prediction.PendingCorrection().x, 0.0f)) {
        return 63;
    }
    if (reconcilePosition(prediction, 2, 1, 118, { 500.0f, 0.0f, 0.0f },
                           { 11.0f, 0.0f, 0.0f })) return 64;
    correctionCommand.sequence = 3;
    prediction.RecordCommand(correctionCommand, 0.05f);
    if (!reconcilePosition(prediction, 3, 1, 118, { 30.0f, 0.0f, 0.0f },
                           { 20.0f, 0.0f, 0.0f }) ||
        !NearlyEqual(prediction.PendingCorrection().x, 10.0f)) {
        return 65;
    }
    prediction.Reset();
    correctionCommand = {};
    correctionCommand.sequence = 10;
    correctionCommand.lifeEpoch = 1;
    correctionCommand.sceneId = 118;
    if (!seedPrediction(prediction, 1, 118, { 90.0f, 0.0f, 0.0f })) return 108;
    prediction.RecordCommand(correctionCommand);
    correctionCommand.sequence = 11;
    prediction.RecordCommand(correctionCommand);
    PlayerSnapshot correctionAuthority{};
    correctionAuthority.sceneId = 118;
    correctionAuthority.serverTick = 100;
    correctionAuthority.actionStartTick = 100;
    correctionAuthority.lastProcessedCommand = 9;
    correctionAuthority.lifeEpoch = 1;
    correctionAuthority.position = { 80.0f, 0.0f, 0.0f };
    if (!prediction.Reconcile(correctionAuthority, { 100.0f, 0.0f, 0.0f }) ||
        prediction.PendingCommandCount() != 2 ||
        !NearlyEqual(prediction.PendingCorrection().x, -20.0f)) {
        return 109;
    }
    correctionAuthority.position = { 500.0f, 0.0f, 0.0f };
    if (prediction.Reconcile(correctionAuthority, { 100.0f, 0.0f, 0.0f }) ||
        !NearlyEqual(prediction.ConsumeCorrection(1.0f / 30.0f, 0.15f, 10.0f).x, -20.0f)) {
        return 110;
    }
    prediction.Reset();
    if (prediction.PendingCommandCount() != 0 ||
        !NearlyEqual(prediction.PendingCorrection().x, 0.0f)) {
        return 66;
    }
    PlayerCommand predictedMove{};
    predictedMove.sequence = 20;
    predictedMove.lifeEpoch = 1;
    predictedMove.sceneId = 118;
    predictedMove.moveY = 1.0f;
    if (!seedPrediction(prediction, 1, 118)) return 112;
    prediction.RecordCommand(predictedMove);
    predictedMove.sequence = 21;
    prediction.RecordCommand(predictedMove);
    if (!reconcilePosition(prediction, 21, 1, 118, { 0.0f, 0.0f, 6.0f },
                           { 0.0f, 0.0f, 6.0f }) ||
        !NearlyEqual(prediction.PendingCorrection().z, 0.0f)) {
        return 113;
    }
    predictedMove.heldActions = Game::Simulation::PLAYER_ACTION_BLOCK;
    const auto guardVelocity = Game::Simulation::CalculatePlayerVelocity(predictedMove);
    if (!NearlyEqual(guardVelocity.x, 0.0f) || !NearlyEqual(guardVelocity.z, 80.0f)) {
        return 114;
    }

    const auto correctionAfterOneSecond = [&reconcilePosition](int updatesPerSecond) {
        ClientPrediction cadencePrediction;
        PlayerCommand command{};
        command.sequence = 1;
        command.lifeEpoch = 1;
        command.sceneId = 118;
        PlayerSnapshot baseline{};
        baseline.lifeEpoch = 1;
        baseline.sceneId = 118;
        baseline.position = { 10.0f, 0.0f, 0.0f };
        if (!cadencePrediction.SeedAuthoritative(baseline)) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        cadencePrediction.RecordCommand(command);
        if (!reconcilePosition(cadencePrediction, 1, 1, 118, {},
                               { 10.0f, 0.0f, 0.0f })) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        const float step = 1.0f / static_cast<float>(updatesPerSecond);
        for (int update = 0; update < updatesPerSecond; ++update) {
            cadencePrediction.ConsumeCorrection(step);
        }
        return cadencePrediction.PendingCorrection().x;
    };
    const float correction20 = correctionAfterOneSecond(20);
    const float correction38 = correctionAfterOneSecond(38);
    const float correction60 = correctionAfterOneSecond(60);
    if (!std::isfinite(correction20) ||
        std::abs(correction20 - correction38) > 0.001f ||
        std::abs(correction20 - correction60) > 0.001f) {
        return 169;
    }
    ClientPrediction stalledCorrection;
    PlayerCommand stalledCommand{};
    stalledCommand.sequence = 1;
    stalledCommand.lifeEpoch = 1;
    stalledCommand.sceneId = 118;
    if (!seedPrediction(stalledCorrection, 1, 118,
                        { 100.0f, 0.0f, 0.0f })) return 169;
    stalledCorrection.RecordCommand(stalledCommand);
    if (!reconcilePosition(stalledCorrection, 1, 1, 118, {},
                           { 100.0f, 0.0f, 0.0f }) ||
        std::abs(stalledCorrection.ConsumeCorrection(5.0f).x) >= 100.0f ||
        std::abs(stalledCorrection.PendingCorrection().x) < 1.0f) {
        return 170;
    }
    stalledCorrection.Reset();
    stalledCommand.sequence = 2;
    if (!seedPrediction(stalledCorrection, 1, 118,
                        { 250.0f, 0.0f, 0.0f })) return 170;
    stalledCorrection.RecordCommand(stalledCommand);
    if (!reconcilePosition(stalledCorrection, 2, 1, 118, {},
                           { 250.0f, 0.0f, 0.0f }) ||
        !NearlyEqual(stalledCorrection.ConsumeCorrection(0.0f).x, -250.0f) ||
        !NearlyEqual(stalledCorrection.PendingCorrection().x, 0.0f)) {
        return 171;
    }
    const auto commandPredictionError = [&reconcilePosition](int updatesPerSecond) {
        ClientPrediction cadencePrediction;
        PlayerCommand command{};
        command.lifeEpoch = 1;
        command.sceneId = 118;
        command.moveY = 1.0f;
        const float step = 1.0f / static_cast<float>(updatesPerSecond);
        PlayerSnapshot baseline{};
        baseline.lifeEpoch = 1;
        baseline.sceneId = 118;
        if (!cadencePrediction.SeedAuthoritative(baseline)) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        for (int update = 1; update <= updatesPerSecond; ++update) {
            command.sequence = static_cast<uint32_t>(update);
            cadencePrediction.RecordCommand(command, step);
        }
        if (!reconcilePosition(cadencePrediction,
                               static_cast<uint32_t>(updatesPerSecond), 1, 118,
                               { 0.0f, 0.0f, 180.0f },
                               { 0.0f, 0.0f, 180.0f })) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        return cadencePrediction.PendingCorrection().z;
    };
    const float commandError20 = commandPredictionError(20);
    const float commandError38 = commandPredictionError(38);
    const float commandError60 = commandPredictionError(60);
    if (!std::isfinite(commandError20) || std::abs(commandError20) > 0.001f ||
        std::abs(commandError38) > 0.001f || std::abs(commandError60) > 0.001f) {
        return 172;
    }

    const auto recordForwardCommands = [](ClientPrediction& client, uint32_t count) {
        PlayerCommand command{};
        command.lifeEpoch = 1;
        command.sceneId = 118;
        command.moveY = 1.0f;
        PlayerSnapshot baseline{};
        baseline.lifeEpoch = 1;
        baseline.sceneId = 118;
        if (!client.SeedAuthoritative(baseline)) return false;
        for (uint32_t sequence = 1; sequence <= count; ++sequence) {
            command.sequence = sequence;
            client.RecordCommand(command, 1.0f / 30.0f);
        }
        return true;
    };
    const auto movementAuthority = [](uint32_t sequence, float positionZ) {
        PlayerSnapshot snapshot{};
        snapshot.sceneId = 118;
        snapshot.serverTick = 100;
        snapshot.lastProcessedCommand = sequence;
        snapshot.lifeEpoch = 1;
        snapshot.position.z = positionZ;
        snapshot.actionStartTick = 100;
        return snapshot;
    };
    ClientPrediction replayPrediction;
    if (!recordForwardCommands(replayPrediction, 3)) return 283;
    if (!replayPrediction.Reconcile(movementAuthority(1, 6.0f),
                                    { 0.0f, 0.0f, 18.0f }) ||
        replayPrediction.PendingCommandCount() != 2 ||
        !NearlyEqual(replayPrediction.PendingCorrection().z, 0.0f)) {
        return 284;
    }
    ClientPrediction collisionDivergence;
    if (!recordForwardCommands(collisionDivergence, 3)) return 283;
    if (!collisionDivergence.Reconcile(movementAuthority(1, 6.0f),
                                       { 0.0f, 0.0f, 10.0f }) ||
        !NearlyEqual(collisionDivergence.PendingCorrection().z, 8.0f)) {
        return 285;
    }
    ClientPrediction skippedMovement;
    if (!recordForwardCommands(skippedMovement, 3)) return 283;
    if (!skippedMovement.Reconcile(movementAuthority(2, 12.0f),
                                   { 0.0f, 0.0f, 18.0f }) ||
        skippedMovement.PendingCommandCount() != 1 ||
        !NearlyEqual(skippedMovement.PendingCorrection().z, 0.0f)) {
        return 286;
    }
    ClientPrediction evictedAcknowledgement;
    if (!recordForwardCommands(evictedAcknowledgement, 257)) return 283;
    if (evictedAcknowledgement.PendingCommandCount() != 256 ||
        !evictedAcknowledgement.Reconcile(movementAuthority(1, 6.0f),
                                          { 0.0f, 0.0f, 1542.0f }) ||
        evictedAcknowledgement.PendingCommandCount() != 256 ||
        !NearlyEqual(evictedAcknowledgement.PendingCorrection().z, 0.0f)) {
        return 287;
    }
    ClientPrediction duplicateCommand;
    PlayerCommand duplicateMove{};
    duplicateMove.lifeEpoch = 1;
    duplicateMove.sceneId = 118;
    duplicateMove.moveY = 1.0f;
    duplicateMove.sequence = 1;
    if (!seedPrediction(duplicateCommand, 1, 118)) return 287;
    duplicateCommand.RecordCommand(duplicateMove, 1.0f / 30.0f);
    duplicateCommand.RecordCommand(duplicateMove, 0.25f);
    duplicateMove.sequence = 2;
    duplicateCommand.RecordCommand(duplicateMove, 1.0f / 30.0f);
    if (!duplicateCommand.Reconcile(movementAuthority(2, 12.0f),
                                    { 0.0f, 0.0f, 12.0f }) ||
        !NearlyEqual(duplicateCommand.PendingCorrection().z, 0.0f)) {
        return 288;
    }

    const auto recordPredictionCommand = [](ClientPrediction& client,
                                             PlayerCommand command,
                                             const Game::Simulation::Vec3& seed = {}) {
        if (!client.HasAuthoritativeSeed()) {
            PlayerSnapshot baseline{};
            baseline.lifeEpoch = command.lifeEpoch;
            baseline.sceneId = command.sceneId;
            baseline.position = seed;
            baseline.selectedWeapon = 1;
            if (!client.SeedAuthoritative(baseline)) return false;
        }
        client.RecordCommand(
            command, Game::Simulation::kPlayerSimulationTickSeconds, 1);
        return true;
    };
    PlayerCommand replayBaseline{};
    replayBaseline.sequence = 1;
    replayBaseline.lifeEpoch = 1;
    replayBaseline.sceneId = 118;
    PlayerCommand replayEvade = replayBaseline;
    replayEvade.sequence = 2;
    replayEvade.actionSequence = 1;
    replayEvade.moveY = 1.0f;
    replayEvade.pressedActions = Game::Simulation::PLAYER_ACTION_EVADE;
    PlayerCommand replayAfterEvade = replayEvade;
    replayAfterEvade.sequence = 3;
    replayAfterEvade.actionSequence = 0;
    replayAfterEvade.pressedActions = 0;

    ClientPrediction evadeReplay;
    recordPredictionCommand(evadeReplay, replayBaseline);
    recordPredictionCommand(evadeReplay, replayEvade);
    recordPredictionCommand(evadeReplay, replayAfterEvade);
    PlayerSnapshot idleAuthority{};
    idleAuthority.sceneId = 118;
    idleAuthority.serverTick = 100;
    idleAuthority.lastProcessedCommand = 1;
    idleAuthority.lifeEpoch = 1;
    idleAuthority.actionStartTick = 100;
    idleAuthority.selectedWeapon = 1;
    if (!evadeReplay.Reconcile(idleAuthority, { 0.0f, 0.0f, -8.0f }) ||
        evadeReplay.PendingCommandCount() != 2 ||
        !NearlyEqual(evadeReplay.PendingCorrection().z, 0.0f)) {
        return 356;
    }

    ClientPrediction airborneReplay;
    recordPredictionCommand(airborneReplay, replayBaseline);
    recordPredictionCommand(airborneReplay, replayEvade);
    PlayerSnapshot airborneAuthority = idleAuthority;
    airborneAuthority.locomotionMode =
        Game::Simulation::PlayerLocomotionMode::Airborne;
    // An airborne evade edge replays as ordinary forward movement (6 units),
    // not the grounded backflip displacement (-4 units).
    if (!airborneReplay.Reconcile(airborneAuthority,
                                  { 0.0f, 0.0f, 6.0f }) ||
        !NearlyEqual(airborneReplay.PendingCorrection().z, 0.0f) ||
        airborneReplay.PredictedActionState() != PlayerActionState::Idle) {
        return 466;
    }

    PlayerCommand airbornePrimary = replayBaseline;
    airbornePrimary.sequence = 2;
    airbornePrimary.actionSequence = 1;
    airbornePrimary.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    ClientPrediction jumpSlashReplay;
    recordPredictionCommand(jumpSlashReplay, replayBaseline);
    recordPredictionCommand(jumpSlashReplay, airbornePrimary);
    if (!jumpSlashReplay.Reconcile(airborneAuthority, {}) ||
        jumpSlashReplay.PredictedLocomotionMode() !=
            Game::Simulation::PlayerLocomotionMode::Airborne ||
        jumpSlashReplay.PredictedActionState() !=
            PlayerActionState::JumpSlashing) {
        return 483;
    }
    airbornePrimary.sequence = 3;
    airbornePrimary.actionSequence = 2;
    recordPredictionCommand(jumpSlashReplay, airbornePrimary);
    PlayerSnapshot jumpSlashAuthority = airborneAuthority;
    jumpSlashAuthority.serverTick = 101;
    jumpSlashAuthority.lastProcessedCommand = 2;
    jumpSlashAuthority.actionState = PlayerActionState::JumpSlashing;
    jumpSlashAuthority.actionStartTick = 100;
    if (!jumpSlashReplay.Reconcile(jumpSlashAuthority, {}) ||
        jumpSlashReplay.PredictedActionState() !=
            PlayerActionState::JumpSlashing) {
        return 484;
    }

    PlayerCommand airborneBlock = replayBaseline;
    airborneBlock.sequence = 2;
    airborneBlock.heldActions = Game::Simulation::PLAYER_ACTION_BLOCK;
    ClientPrediction airborneBlockReplay;
    recordPredictionCommand(airborneBlockReplay, replayBaseline);
    recordPredictionCommand(airborneBlockReplay, airborneBlock);
    if (!airborneBlockReplay.Reconcile(airborneAuthority, {}) ||
        airborneBlockReplay.PredictedActionState() !=
            PlayerActionState::Blocking) {
        return 485;
    }

    PlayerCommand airborneAim = replayBaseline;
    airborneAim.sequence = 2;
    airborneAim.heldActions = Game::Simulation::PLAYER_ACTION_AIM;
    ClientPrediction airborneAimReplay;
    if (!seedPrediction(airborneAimReplay, 1, 118)) return 485;
    airborneAimReplay.RecordCommand(
        replayBaseline, Game::Simulation::kPlayerSimulationTickSeconds, 3);
    airborneAimReplay.RecordCommand(
        airborneAim, Game::Simulation::kPlayerSimulationTickSeconds, 3);
    if (!airborneAimReplay.Reconcile(airborneAuthority, {}) ||
        airborneAimReplay.PredictedActionState() != PlayerActionState::Aiming) {
        return 486;
    }

    ClientPrediction authoritativeEvadeReplay;
    recordPredictionCommand(authoritativeEvadeReplay, replayBaseline,
                            { 0.0f, 0.0f, -4.0f });
    PlayerCommand heldDuringEvade = replayBaseline;
    heldDuringEvade.sequence = 2;
    heldDuringEvade.moveY = 1.0f;
    recordPredictionCommand(authoritativeEvadeReplay, heldDuringEvade);
    PlayerSnapshot evadeAuthority = idleAuthority;
    evadeAuthority.position = { 0.0f, 0.0f, -4.0f };
    evadeAuthority.velocity = { 0.0f, 0.0f, -120.0f };
    evadeAuthority.actionState = PlayerActionState::Evading;
    evadeAuthority.actionStartTick = 99;
    if (!authoritativeEvadeReplay.Reconcile(evadeAuthority,
                                            { 0.0f, 0.0f, -8.0f }) ||
        !NearlyEqual(authoritativeEvadeReplay.PendingCorrection().z, 0.0f)) {
        return 357;
    }

    ClientPrediction busyActionReplay;
    recordPredictionCommand(busyActionReplay, replayBaseline);
    PlayerCommand ignoredBusyEvade = replayEvade;
    ignoredBusyEvade.moveY = 0.0f;
    recordPredictionCommand(busyActionReplay, ignoredBusyEvade);
    PlayerSnapshot busyAuthority = idleAuthority;
    busyAuthority.actionState = PlayerActionState::PrimaryActive;
    busyAuthority.actionStartTick = 98;
    if (!busyActionReplay.Reconcile(busyAuthority, {}) ||
        !NearlyEqual(busyActionReplay.PendingCorrection().z, 0.0f)) {
        return 358;
    }

    // Prediction exposes the same windup/active/recovery phases as authority;
    // it must not report an entire sword action as windup.
    ClientPrediction exactPrimaryPrediction;
    if (Game::Client::EvaluateLocalPrimaryActionPresentation(
            exactPrimaryPrediction, true).state !=
        Game::Client::LocalPrimaryActionPresentationState::Unavailable) {
        return 506;
    }
    PlayerCommand predictedPrimary = replayBaseline;
    predictedPrimary.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    recordPredictionCommand(exactPrimaryPrediction, predictedPrimary);
    const auto activePrimaryPresentation =
        Game::Client::EvaluateLocalPrimaryActionPresentation(
            exactPrimaryPrediction, true);
    if (activePrimaryPresentation.state !=
            Game::Client::LocalPrimaryActionPresentationState::Active ||
        !NearlyEqual(activePrimaryPresentation.progress, 0.0f)) {
        return 507;
    }
    if (exactPrimaryPrediction.PredictedActionState() !=
            PlayerActionState::PrimaryWindup ||
        !NearlyEqual(exactPrimaryPrediction.PredictedActionProgress(), 0.0f)) {
        return 500;
    }
    PlayerCommand predictedHeld = replayBaseline;
    predictedHeld.pressedActions = 0;
    predictedHeld.sequence = 2;
    recordPredictionCommand(exactPrimaryPrediction, predictedHeld);
    const float advancingWindupProgress =
        exactPrimaryPrediction.PredictedActionProgress();
    if (advancingWindupProgress <= 0.0f || advancingWindupProgress >= 0.2f) {
        return 505;
    }
    predictedHeld.sequence = 3;
    recordPredictionCommand(exactPrimaryPrediction, predictedHeld);
    if (exactPrimaryPrediction.PredictedActionState() !=
            PlayerActionState::PrimaryActive ||
        exactPrimaryPrediction.PredictedActionProgress() < 0.2f ||
        exactPrimaryPrediction.PredictedActionProgress() >= 0.65f) {
        return 501;
    }
    for (uint32_t sequence = 4; sequence <= 8; ++sequence) {
        predictedHeld.sequence = sequence;
        recordPredictionCommand(exactPrimaryPrediction, predictedHeld);
    }
    if (exactPrimaryPrediction.PredictedActionState() !=
            PlayerActionState::PrimaryRecovery ||
        exactPrimaryPrediction.PredictedActionProgress() < 0.65f ||
        exactPrimaryPrediction.PredictedActionProgress() > 1.0f) {
        return 502;
    }
    for (uint32_t sequence = 9; sequence <= 13; ++sequence) {
        predictedHeld.sequence = sequence;
        recordPredictionCommand(exactPrimaryPrediction, predictedHeld);
    }
    if (exactPrimaryPrediction.PredictedActionState() !=
            PlayerActionState::Idle ||
        !NearlyEqual(exactPrimaryPrediction.PredictedActionProgress(), 0.0f)) {
        return 503;
    }
    if (Game::Client::EvaluateLocalPrimaryActionPresentation(
            exactPrimaryPrediction, true).state !=
            Game::Client::LocalPrimaryActionPresentationState::Idle ||
        Game::Client::EvaluateLocalPrimaryActionPresentation(
            exactPrimaryPrediction, false).state !=
            Game::Client::LocalPrimaryActionPresentationState::Unavailable) {
        return 508;
    }

    ClientPrediction longFramePrimaryPrediction;
    predictedPrimary.sequence = 1;
    if (!seedPrediction(longFramePrimaryPrediction, 1, 118)) return 503;
    longFramePrimaryPrediction.RecordCommand(
        predictedPrimary, Game::Simulation::kPlayerSimulationTickSeconds, 1);
    predictedHeld.sequence = 2;
    longFramePrimaryPrediction.RecordCommand(predictedHeld, 0.25f, 1);
    if (longFramePrimaryPrediction.PredictedActionState() !=
            PlayerActionState::PrimaryRecovery ||
        longFramePrimaryPrediction.PredictedActionProgress() < 0.65f ||
        longFramePrimaryPrediction.PredictedActionProgress() > 1.0f) {
        return 504;
    }

    ClientPrediction recordedEvade;
    recordPredictionCommand(recordedEvade, replayBaseline);
    recordPredictionCommand(recordedEvade, replayEvade);
    PlayerSnapshot recordedEvadeAuthority = evadeAuthority;
    recordedEvadeAuthority.lastProcessedCommand = 2;
    if (!recordedEvade.Reconcile(recordedEvadeAuthority,
                                 { 0.0f, 0.0f, -4.0f }) ||
        !NearlyEqual(recordedEvade.PendingCorrection().z, 0.0f)) {
        return 364;
    }
    recordPredictionCommand(recordedEvade, replayAfterEvade);
    recordedEvadeAuthority.serverTick = 101;
    recordedEvadeAuthority.lastProcessedCommand = 3;
    recordedEvadeAuthority.position = { 0.0f, 0.0f, -8.0f };
    if (!recordedEvade.Reconcile(recordedEvadeAuthority,
                                 { 0.0f, 0.0f, -8.0f }) ||
        !NearlyEqual(recordedEvade.PendingCorrection().z, 0.0f)) {
        return 365;
    }

    ClientPrediction continuedAuthoritativeEvade;
    recordPredictionCommand(continuedAuthoritativeEvade, replayBaseline);
    if (!continuedAuthoritativeEvade.Reconcile(
            evadeAuthority, { 0.0f, 0.0f, -4.0f })) {
        return 366;
    }
    recordPredictionCommand(continuedAuthoritativeEvade, heldDuringEvade);
    PlayerSnapshot continuedEvadeAuthority = evadeAuthority;
    continuedEvadeAuthority.lastProcessedCommand = 2;
    continuedEvadeAuthority.position = { 0.0f, 0.0f, -8.0f };
    if (!continuedAuthoritativeEvade.Reconcile(
            continuedEvadeAuthority, { 0.0f, 0.0f, -8.0f }) ||
        !NearlyEqual(continuedAuthoritativeEvade.PendingCorrection().z, 0.0f)) {
        return 367;
    }

    ClientPrediction lifetimeBoundPrediction;
    PlayerCommand lifetimeCommand = replayBaseline;
    if (!seedPrediction(lifetimeBoundPrediction, 1, 118)) return 388;
    lifetimeBoundPrediction.RecordCommand(lifetimeCommand);
    if (lifetimeBoundPrediction.LifeEpoch() != 1 ||
        lifetimeBoundPrediction.PendingCommandCount() != 1) {
        return 389;
    }
    lifetimeBoundPrediction.Reset(2);
    lifetimeCommand.sequence = 2;
    lifetimeBoundPrediction.RecordCommand(lifetimeCommand);
    PlayerSnapshot staleLifetimeAuthority = idleAuthority;
    staleLifetimeAuthority.lastProcessedCommand = 2;
    if (lifetimeBoundPrediction.PendingCommandCount() != 0 ||
        lifetimeBoundPrediction.Reconcile(staleLifetimeAuthority, {}) ||
        lifetimeBoundPrediction.LifeEpoch() != 2) {
        return 390;
    }
    lifetimeCommand.lifeEpoch = 2;
    if (!seedPrediction(lifetimeBoundPrediction, 2, 118)) return 390;
    lifetimeBoundPrediction.RecordCommand(lifetimeCommand);
    PlayerSnapshot currentLifetimeAuthority = staleLifetimeAuthority;
    currentLifetimeAuthority.lifeEpoch = 2;
    if (lifetimeBoundPrediction.PendingCommandCount() != 1 ||
        !lifetimeBoundPrediction.Reconcile(currentLifetimeAuthority, {}) ||
        lifetimeBoundPrediction.PendingCommandCount() != 0) {
        return 391;
    }

    constexpr float pi = 3.14159265358979323846f;
    const auto motionSample = [](uint32_t tick, int32_t scene, uint32_t life,
                                 float x, float heading) {
        return RemoteMotionSample{ scene, tick, life, { x, 0.0f, 0.0f },
                                   { 300.0f, 0.0f, 0.0f }, heading };
    };
    RemotePlayerInterpolation interpolation;
    if (interpolation.Evaluate(0.0) ||
        !interpolation.Push(motionSample(100, 118, 1, 0.0f, pi * 179.0f / 180.0f), 0.0)) {
        return 162;
    }
    const auto firstRemotePose = interpolation.Evaluate(0.0);
    if (!firstRemotePose || !NearlyEqual(firstRemotePose->position.x, 0.0f) ||
        !interpolation.Push(motionSample(101, 118, 1, 10.0f, -pi * 179.0f / 180.0f), 1.0 / 30.0) ||
        !interpolation.Push(motionSample(102, 118, 1, 20.0f, -pi * 177.0f / 180.0f), 2.0 / 30.0)) {
        return 163;
    }
    const auto midpoint = interpolation.Evaluate(2.5 / 30.0);
    if (!midpoint || std::abs(midpoint->position.x - 5.0f) > 0.001f ||
        std::abs(std::abs(midpoint->headingRadians) - pi) > 0.001f || midpoint->extrapolated) {
        return 164;
    }

    RemotePlayerInterpolation denseCadence;
    RemotePlayerInterpolation sparseCadence;
    for (uint32_t tick = 100; tick <= 102; ++tick) {
        const double received = static_cast<double>(tick - 100) / 30.0;
        const auto sample = motionSample(tick, 118, 1, static_cast<float>((tick - 100) * 10), 0.0f);
        if (!denseCadence.Push(sample, received) || !sparseCadence.Push(sample, received)) return 165;
    }
    denseCadence.Evaluate(2.25 / 30.0);
    denseCadence.Evaluate(2.5 / 30.0);
    denseCadence.Evaluate(2.75 / 30.0);
    const auto densePose = denseCadence.Evaluate(3.0 / 30.0);
    const auto sparsePose = sparseCadence.Evaluate(3.0 / 30.0);
    const auto extrapolated = interpolation.Evaluate(7.0 / 30.0);
    if (!densePose || !sparsePose ||
        std::abs(densePose->position.x - sparsePose->position.x) > 0.001f ||
        !extrapolated || !extrapolated->extrapolated ||
        std::abs(extrapolated->position.x - 50.0f) > 0.001f) {
        return 166;
    }
    if (!interpolation.Push(motionSample(1, 119, 2, 999.0f, 0.0f), 8.0 / 30.0) ||
        interpolation.SampleCount() != 1 ||
        interpolation.Push(motionSample(1, 119, 2, 1000.0f, 0.0f), 9.0 / 30.0)) {
        return 167;
    }
    const auto resetPose = interpolation.Evaluate(8.0 / 30.0);
    if (!resetPose || !NearlyEqual(resetPose->position.x, 999.0f)) return 168;

    const auto projectileSample = [](uint32_t sequence, int32_t scene, uint8_t phase,
                                     float x, int16_t rotationY, bool terminal = false) {
        return RemoteProjectileSample{ scene, sequence, phase, terminal,
                                       { x, 0.0f, 0.0f }, { 200.0f, 0.0f, 0.0f },
                                       0, rotationY, 0 };
    };
    RemoteProjectileInterpolation projectileInterpolation;
    if (projectileInterpolation.Evaluate(0.0) ||
        !projectileInterpolation.Push(projectileSample(10, 118, 0, 0.0f, 32760), 0.0) ||
        !projectileInterpolation.Push(projectileSample(11, 118, 0, 10.0f, -32760), 0.05)) {
        return 173;
    }
    const auto projectileMidpoint = projectileInterpolation.Evaluate(0.075);
    if (!projectileMidpoint ||
        std::abs(projectileMidpoint->position.x - 5.0f) > 0.001f ||
        std::abs(std::abs(static_cast<int32_t>(projectileMidpoint->rotationY)) - 32768) > 1 ||
        projectileMidpoint->extrapolated || projectileMidpoint->terminal) {
        return 174;
    }

    RemoteProjectileInterpolation denseProjectileCadence;
    RemoteProjectileInterpolation sparseProjectileCadence;
    for (uint32_t sequence = 10; sequence <= 12; ++sequence) {
        const double received = static_cast<double>(sequence - 10) / 20.0;
        const auto sample = projectileSample(sequence, 118, 0,
                                             static_cast<float>((sequence - 10) * 10), 0);
        if (!denseProjectileCadence.Push(sample, received) ||
            !sparseProjectileCadence.Push(sample, received)) {
            return 175;
        }
    }
    denseProjectileCadence.Evaluate(0.1125);
    denseProjectileCadence.Evaluate(0.125);
    denseProjectileCadence.Evaluate(0.1375);
    const auto denseProjectilePose = denseProjectileCadence.Evaluate(0.15);
    const auto sparseProjectilePose = sparseProjectileCadence.Evaluate(0.15);
    if (!denseProjectilePose || !sparseProjectilePose ||
        std::abs(denseProjectilePose->position.x - sparseProjectilePose->position.x) > 0.001f ||
        std::abs(denseProjectilePose->position.x - 20.0f) > 0.001f) {
        return 176;
    }
    const auto cappedProjectilePose = sparseProjectileCadence.Evaluate(1.0);
    if (!cappedProjectilePose || !cappedProjectilePose->extrapolated ||
        std::abs(cappedProjectilePose->position.x - 40.0f) > 0.001f) {
        return 177;
    }

    RemoteProjectileInterpolation packetGapInterpolation;
    if (!packetGapInterpolation.Push(projectileSample(12, 118, 0, 20.0f, 0), 0.1) ||
        !packetGapInterpolation.Push(projectileSample(14, 118, 0, 40.0f, 0), 0.2)) {
        return 178;
    }
    const auto packetGapPose = packetGapInterpolation.Evaluate(0.2);
    if (!packetGapPose || std::abs(packetGapPose->position.x - 30.0f) > 0.001f ||
        packetGapInterpolation.Push(projectileSample(14, 118, 0, 41.0f, 0), 0.21) ||
        !packetGapInterpolation.Push(projectileSample(15, 118, 1, 55.0f, 1234, true), 0.25)) {
        return 179;
    }
    const auto terminalProjectilePose = packetGapInterpolation.Evaluate(10.0);
    if (!terminalProjectilePose || !terminalProjectilePose->terminal ||
        terminalProjectilePose->extrapolated ||
        !NearlyEqual(terminalProjectilePose->position.x, 55.0f) ||
        terminalProjectilePose->rotationY != 1234 ||
        packetGapInterpolation.SampleCount() != 1 ||
        !packetGapInterpolation.Push(projectileSample(1, 119, 0, 999.0f, 0), 10.1) ||
        packetGapInterpolation.SampleCount() != 1) {
        return 180;
    }

    LocalFishingUpdateStream fishingUpdates;
    const LocalFishingUpdate inactiveFishing{ 118, false, 0, false, false };
    const LocalFishingUpdate activeFishing{ 118, true, 1, true, false };
    if (fishingUpdates.Evaluate(inactiveFishing, 0.0).SendPresentation()) return 181;
    const auto firstFishingSend = fishingUpdates.Evaluate(activeFishing, 0.001);
    if (firstFishingSend.presentationSequence != 1 || firstFishingSend.controlSequence != 1 ||
        !firstFishingSend.reliableControl ||
        fishingUpdates.Evaluate(activeFishing, 0.001).SendPresentation() ||
        fishingUpdates.Evaluate(activeFishing, 0.001).SendControl()) {
        return 182;
    }
    const auto periodicFishingSend = fishingUpdates.Evaluate(activeFishing, 0.051);
    if (periodicFishingSend.presentationSequence != 2 ||
        periodicFishingSend.controlSequence != 2 ||
        periodicFishingSend.reliableControl) {
        return 183;
    }
    LocalFishingUpdate reelingFishing = activeFishing;
    reelingFishing.reelHeld = true;
    const auto immediateReelSend = fishingUpdates.Evaluate(reelingFishing, 0.052);
    if (immediateReelSend.SendPresentation() || immediateReelSend.controlSequence != 3 ||
        immediateReelSend.reliableControl) {
        return 184;
    }
    LocalFishingUpdate changedFishingState = reelingFishing;
    changedFishingState.fishingState = 2;
    const auto immediateStateSend = fishingUpdates.Evaluate(changedFishingState, 0.053);
    if (immediateStateSend.presentationSequence != 3 || immediateStateSend.SendControl()) {
        return 185;
    }
    const auto stopFishingSend = fishingUpdates.Evaluate(inactiveFishing, 0.054);
    if (stopFishingSend.SendPresentation() || stopFishingSend.controlSequence != 4 ||
        !stopFishingSend.reliableControl ||
        fishingUpdates.Evaluate(inactiveFishing, 1.0).SendControl()) {
        return 186;
    }

    const auto countFishingCadence = [](int callbacksPerSecond, bool duplicateCallbacks) {
        LocalFishingUpdateStream stream;
        const LocalFishingUpdate state{ 118, true, 1, true, false };
        uint32_t presentationCount = 0;
        uint32_t controlCount = 0;
        for (int callback = 0; callback <= callbacksPerSecond; ++callback) {
            const double now = static_cast<double>(callback) / callbacksPerSecond;
            const auto decision = stream.Evaluate(state, now);
            presentationCount += decision.SendPresentation();
            controlCount += decision.SendControl();
            if (duplicateCallbacks) {
                const auto duplicate = stream.Evaluate(state, now);
                presentationCount += duplicate.SendPresentation();
                controlCount += duplicate.SendControl();
            }
        }
        return std::pair{ presentationCount, controlCount };
    };
    const auto cadence20 = countFishingCadence(20, false);
    const auto cadence60 = countFishingCadence(60, true);
    if (cadence20 != std::pair<uint32_t, uint32_t>{ 21, 21 } || cadence60 != cadence20) {
        return 187;
    }
    const LocalFishingUpdate resumedFishing{ 118, true, 2, true, false };
    const auto resumeSend = fishingUpdates.Evaluate(resumedFishing, 1.001);
    LocalFishingUpdate changedScene = resumedFishing;
    changedScene.sceneId = 119;
    const auto sceneSend = fishingUpdates.Evaluate(changedScene, 1.002);
    if (!resumeSend.SendPresentation() || !resumeSend.SendControl() ||
        !resumeSend.reliableControl || !sceneSend.SendPresentation() ||
        !sceneSend.SendControl() || !sceneSend.reliableControl) {
        return 188;
    }
    fishingUpdates.BeginScene();
    const LocalFishingUpdate newSceneInactive{ 120, false, 0, false, false };
    const LocalFishingUpdate newSceneActive{ 120, true, 1, true, false };
    const auto inactiveAfterTransition = fishingUpdates.Evaluate(newSceneInactive, 0.0);
    const auto activeAfterTransition = fishingUpdates.Evaluate(newSceneActive, 0.001);
    if (inactiveAfterTransition.SendPresentation() || inactiveAfterTransition.SendControl() ||
        activeAfterTransition.presentationSequence != sceneSend.presentationSequence + 1 ||
        activeAfterTransition.controlSequence != sceneSend.controlSequence + 1) {
        return 393;
    }

    LocalFishIntentStream fishIntents;
    const auto firstFishIntent = fishIntents.BeginHook();
    const auto duplicateHook = fishIntents.BeginHook();
    const bool resolvedHook = firstFishIntent &&
        fishIntents.Resolve(firstFishIntent->sequence, true);
    const auto releaseFishIntent = fishIntents.EndHook();
    const auto duplicateRelease = fishIntents.EndHook();
    const bool resolvedRelease = releaseFishIntent &&
        fishIntents.Resolve(releaseFishIntent->sequence, true);
    if (!firstFishIntent || firstFishIntent->sequence != 1 ||
        firstFishIntent->request.action != LocalFishIntentAction::Hook ||
        duplicateHook || !resolvedHook ||
        !releaseFishIntent || releaseFishIntent->sequence != 2 ||
        releaseFishIntent->request.action != LocalFishIntentAction::Release ||
        duplicateRelease || !resolvedRelease || fishIntents.HookActive()) {
        return 252;
    }
    const auto failedHook = fishIntents.BeginHook();
    if (!failedHook ||
        !fishIntents.Resolve(failedHook->sequence, false) ||
        fishIntents.HookActive() ||
        fishIntents.Resolve(failedHook->sequence, true)) {
        return 509;
    }
    fishIntents.Reset();
    const auto resetHook = fishIntents.BeginHook();
    if (!resetHook || resetHook->sequence != 1 || !fishIntents.HookActive() ||
        !fishIntents.Resolve(resetHook->sequence, true)) return 254;

    const auto remoteFishingSample = [](uint32_t sequence, uint32_t generation,
                                        uint8_t state, float value,
                                        int16_t fishRotation) {
        Game::Replication::FishingPresentationState sample{};
        sample.playerId = 7;
        sample.entity = { 4, generation };
        sample.sceneId = 118;
        sample.sequence = sequence;
        sample.state = state;
        sample.rodTipOffset[0] = value;
        sample.rodBendY = value;
        sample.rodTwist = value > 0.0f ? -3.13f : 3.13f;
        sample.lineScale = 0.004f;
        sample.lineGravity = value;
        sample.fishRotation[1] = fishRotation;
        sample.fishLimbRotation[0] = fishRotation;
        return sample;
    };
    RemoteFishingPresentationInterpolation remoteFishing;
    if (remoteFishing.Evaluate(0.0) ||
        !remoteFishing.Push(remoteFishingSample(10, 1, 2, 0.0f, 32760), 0.0) ||
        !remoteFishing.Push(remoteFishingSample(11, 1, 2, 10.0f, -32760), 0.05)) {
        return 189;
    }
    const auto remoteFishingMidpoint = remoteFishing.Evaluate(0.075);
    if (!remoteFishingMidpoint ||
        std::abs(remoteFishingMidpoint->rodBendY - 5.0f) > 0.001f ||
        std::abs(std::abs(remoteFishingMidpoint->rodTwist) - pi) > 0.02f ||
        std::abs(std::abs(static_cast<int32_t>(remoteFishingMidpoint->fishRotation[1])) - 32768) > 1) {
        return 190;
    }
    const auto heldFishingPose = remoteFishing.Evaluate(10.0);
    if (!heldFishingPose || std::abs(heldFishingPose->rodBendY - 10.0f) > 0.001f ||
        remoteFishing.Push(remoteFishingSample(11, 1, 2, 11.0f, 0), 10.1)) {
        return 191;
    }
    if (!remoteFishing.Push(remoteFishingSample(12, 1, 3, 30.0f, 1234), 10.2) ||
        remoteFishing.SampleCount() != 1) {
        return 192;
    }
    const auto phaseResetFishingPose = remoteFishing.Evaluate(10.2);
    if (!phaseResetFishingPose ||
        std::abs(phaseResetFishingPose->rodBendY - 30.0f) > 0.001f ||
        !remoteFishing.Push(remoteFishingSample(1, 2, 3, 99.0f, 0), 10.3) ||
        remoteFishing.SampleCount() != 1) {
        return 193;
    }

    LocalProjectileIntentStream localProjectiles;
    LocalProjectilePresentation arrowPresentation{};
    arrowPresentation.presentationId = 0x100;
    arrowPresentation.sceneId = 118;
    if (!localProjectiles.BindPresentation(arrowPresentation) ||
        localProjectiles.NextIntent() || localProjectiles.TrackedCount() != 1 ||
        !localProjectiles.Tracks(arrowPresentation.presentationId) ||
        localProjectiles.PresentationForProjectile(1)) {
        return 194;
    }

    if (!localProjectiles.RequestArrowFire(arrowPresentation.presentationId, 118) ||
        localProjectiles.RequestArrowFire(arrowPresentation.presentationId, 118) ||
        localProjectiles.RequestArrowFire(arrowPresentation.presentationId, 119)) return 408;
    const auto arrowIntent = localProjectiles.NextIntent();
    const auto duplicateArrowIntent = localProjectiles.NextIntent();
    if (!arrowIntent || !duplicateArrowIntent ||
        arrowIntent->kind != LocalProjectileIntentKind::FireArrow ||
        arrowIntent->sequence != 1 ||
        duplicateArrowIntent->sequence != arrowIntent->sequence ||
        localProjectiles.Resolve(999, true) ||
        !localProjectiles.Resolve(arrowIntent->sequence, true) ||
        localProjectiles.AwaitingResultCount() != 1 ||
        localProjectiles.ApplyAuthorityResult(
            arrowIntent->sequence, 0,
            arrowIntent->kind, true) ||
        !localProjectiles.ApplyAuthorityResult(
            arrowIntent->sequence, 7001,
            arrowIntent->kind, true) ||
        localProjectiles.AwaitingResultCount() != 0 ||
        localProjectiles.PresentationForProjectile(7001) != arrowPresentation.presentationId ||
        localProjectiles.NextIntent()) {
        return 195;
    }
    if (!localProjectiles.BindPresentation(arrowPresentation) ||
        localProjectiles.RequestArrowFire(arrowPresentation.presentationId, 118) ||
        localProjectiles.NextIntent() || localProjectiles.TrackedCount() != 1) {
        return 196;
    }

    if (!localProjectiles.Retire(arrowPresentation.presentationId) ||
        localProjectiles.Retire(arrowPresentation.presentationId)) {
        return 409;
    }
    if (localProjectiles.TrackedCount() != 0 ||
        localProjectiles.PresentationForProjectile(7001)) {
        return 197;
    }
    if (!localProjectiles.BindPresentation(arrowPresentation) ||
        !localProjectiles.RequestArrowFire(arrowPresentation.presentationId, 118)) return 410;
    const auto failedArrowIntent = localProjectiles.NextIntent();
    if (!failedArrowIntent || failedArrowIntent->sequence != 2 ||
        !localProjectiles.Resolve(failedArrowIntent->sequence, false) ||
        localProjectiles.AwaitingResultCount() != 0) {
        return 198;
    }
    const auto retriedArrowIntent = localProjectiles.NextIntent();
    if (!retriedArrowIntent || retriedArrowIntent->sequence != 3 ||
        !localProjectiles.Resolve(retriedArrowIntent->sequence, true)) {
        return 199;
    }
    const auto rejectedArrow = localProjectiles.ApplyAuthorityResult(
        retriedArrowIntent->sequence, 0,
        retriedArrowIntent->kind, false);
    if (!rejectedArrow || rejectedArrow->accepted ||
        rejectedArrow->presentationId != arrowPresentation.presentationId ||
        localProjectiles.AwaitingResultCount() != 0) return 199;

    if (!localProjectiles.Retire(arrowPresentation.presentationId)) return 412;
    arrowPresentation.presentationId = 0x102;
    if (!localProjectiles.BindPresentation(arrowPresentation) ||
        !localProjectiles.RequestArrowFire(arrowPresentation.presentationId, 118)) return 413;
    const auto departingSceneArrow = localProjectiles.NextIntent();
    if (!departingSceneArrow || departingSceneArrow->sequence != 4 ||
        !localProjectiles.Resolve(departingSceneArrow->sequence, true) ||
        localProjectiles.AwaitingResultCount() != 1) {
        return 394;
    }
    localProjectiles.BeginScene();
    if (localProjectiles.TrackedCount() != 0 ||
        localProjectiles.AwaitingResultCount() != 0 ||
        localProjectiles.ApplyAuthorityResult(
            departingSceneArrow->sequence, 7002,
            departingSceneArrow->kind, true)) {
        return 395;
    }
    arrowPresentation.presentationId = 0x103;
    arrowPresentation.sceneId = 119;
    if (!localProjectiles.BindPresentation(arrowPresentation) ||
        !localProjectiles.RequestArrowFire(arrowPresentation.presentationId, 119)) return 414;
    const auto destinationArrow = localProjectiles.NextIntent();
    if (!destinationArrow || destinationArrow->sequence != 5) return 396;

    if (!localProjectiles.Retire(arrowPresentation.presentationId)) return 415;
    LocalProjectilePresentation invalidProjectile = arrowPresentation;
    invalidProjectile.presentationId = 0;
    invalidProjectile.sceneId = -1;
    if (localProjectiles.BindPresentation(invalidProjectile) ||
        localProjectiles.TrackedCount() != 0 || localProjectiles.NextIntent()) {
        return 203;
    }

    LocalProjectileIntentStream wrappingProjectiles(std::numeric_limits<uint32_t>::max());
    if (!wrappingProjectiles.BindPresentation(arrowPresentation) ||
        !wrappingProjectiles.RequestArrowFire(arrowPresentation.presentationId, 119)) return 416;
    const auto lastSequenceIntent = wrappingProjectiles.NextIntent();
    if (!lastSequenceIntent || lastSequenceIntent->sequence != std::numeric_limits<uint32_t>::max() ||
        !wrappingProjectiles.Resolve(lastSequenceIntent->sequence, true)) {
        return 204;
    }
    if (!wrappingProjectiles.Retire(arrowPresentation.presentationId)) return 417;
    arrowPresentation.presentationId = 0x101;
    if (!wrappingProjectiles.BindPresentation(arrowPresentation) ||
        !wrappingProjectiles.RequestArrowFire(arrowPresentation.presentationId, 119)) return 418;
    const auto wrappedSequenceIntent = wrappingProjectiles.NextIntent();
    if (!wrappedSequenceIntent || wrappedSequenceIntent->sequence != 1) return 205;

    int nativeProjectile = 0;
    Game::Client::NativePresentationBindingRegistry<int> nativeBindings;
    const auto firstBinding = nativeBindings.Observe(&nativeProjectile);
    if (firstBinding == 0 || nativeBindings.Observe(&nativeProjectile) != firstBinding ||
        nativeBindings.Resolve(firstBinding) != &nativeProjectile || nativeBindings.Size() != 1) {
        return 385;
    }
    if (!nativeBindings.Forget(&nativeProjectile) || nativeBindings.Resolve(firstBinding) != nullptr ||
        nativeBindings.Size() != 0) {
        return 386;
    }
    const auto reusedAddressBinding = nativeBindings.Observe(&nativeProjectile);
    if (reusedAddressBinding == 0 || reusedAddressBinding == firstBinding ||
        nativeBindings.Resolve(reusedAddressBinding) != &nativeProjectile) {
        return 387;
    }
    nativeBindings.Reset();
    const auto postResetBinding = nativeBindings.Observe(&nativeProjectile);
    if (nativeBindings.Resolve(reusedAddressBinding) != nullptr ||
        postResetBinding == reusedAddressBinding || postResetBinding == firstBinding) {
        return 388;
    }

    LocalSceneAdmission localScenes;
    LocalSceneAuthority localSceneState{};
    localSceneState.playerId = 7;
    localSceneState.entity = { 4, 1 };
    localSceneState.lifeEpoch = 1;
    localSceneState.sceneId = 110;
    localSceneState.position = { 10.0f, 20.0f, 30.0f };
    localSceneState.heading = 1234;
    localSceneState.accepted = true;
    const auto localBootstrap = localScenes.Apply(localSceneState);
    if (localBootstrap.kind != LocalSceneAuthorityKind::Bootstrap ||
        !localScenes.IsAuthorized(110) || localScenes.IsAuthorized(118) ||
        localScenes.AuthorizedScene() != 110 ||
        localScenes.AuthorizedEntity() != Game::Simulation::EntityId{ 4, 1 } ||
        localScenes.Prepare(110) || localScenes.PendingPlacementScene() != 110 ||
        localScenes.TakePlacement(118)) {
        return 206;
    }
    const auto bootstrapPlacement = localScenes.TakePlacement(110);
    if (!bootstrapPlacement || bootstrapPlacement->position.x != 10.0f ||
        bootstrapPlacement->heading != 1234 || localScenes.TakePlacement(110) ||
        localScenes.PendingPlacementScene()) {
        return 391;
    }

    const auto scene118Request = localScenes.Prepare(118);
    if (!scene118Request || scene118Request->sequence != 1 ||
        scene118Request->sceneId != 118 || localScenes.Prepare(118) ||
        localScenes.ResolveTransport(99, true) ||
        !localScenes.ResolveTransport(scene118Request->sequence, false)) {
        return 207;
    }
    const auto retriedScene118 = localScenes.Prepare(118);
    if (!retriedScene118 || retriedScene118->sequence != 2 ||
        !localScenes.ResolveTransport(retriedScene118->sequence, true) ||
        localScenes.PendingScene() != 118) {
        return 208;
    }

    const auto scene119Request = localScenes.Prepare(119);
    if (!scene119Request || scene119Request->sequence != 3 ||
        !localScenes.ResolveTransport(scene119Request->sequence, true) ||
        localScenes.PendingScene() != 119) {
        return 209;
    }
    LocalSceneAuthority staleLocalScene = localSceneState;
    staleLocalScene.requestSequence = retriedScene118->sequence;
    staleLocalScene.sceneId = 118;
    if (localScenes.Apply(staleLocalScene).Applied() || localScenes.PendingScene() != 119 ||
        localScenes.Apply(localSceneState).Applied()) {
        return 210;
    }

    LocalSceneAuthority mismatchedLocalScene = localSceneState;
    mismatchedLocalScene.requestSequence = scene119Request->sequence;
    mismatchedLocalScene.sceneId = 118;
    if (localScenes.Apply(mismatchedLocalScene).Applied() || localScenes.PendingScene()) return 211;
    const auto retriedScene119 = localScenes.Prepare(119);
    if (!retriedScene119 || retriedScene119->sequence != 4 ||
        !localScenes.ResolveTransport(retriedScene119->sequence, true)) {
        return 212;
    }
    LocalSceneAuthority acceptedLocalScene119 = localSceneState;
    acceptedLocalScene119.requestSequence = retriedScene119->sequence;
    acceptedLocalScene119.sceneId = 119;
    const auto acceptedLocalResult = localScenes.Apply(acceptedLocalScene119);
    if (acceptedLocalResult.kind != LocalSceneAuthorityKind::Accepted ||
        !localScenes.IsAuthorized(119) || localScenes.PendingScene() ||
        localScenes.PendingPlacementScene() != 119 || localScenes.TakePlacement(110)) {
        return 213;
    }
    const auto acceptedPlacement = localScenes.TakePlacement(119);
    if (!acceptedPlacement || acceptedPlacement->requestSequence != retriedScene119->sequence ||
        localScenes.TakePlacement(119)) {
        return 392;
    }

    const auto rejectedRequest = localScenes.Prepare(120);
    if (!rejectedRequest || !localScenes.ResolveTransport(rejectedRequest->sequence, true)) return 214;
    LocalSceneAuthority rejectedLocalScene = acceptedLocalScene119;
    rejectedLocalScene.requestSequence = rejectedRequest->sequence;
    rejectedLocalScene.accepted = false;
    LocalSceneAuthority wrongGenerationScene = rejectedLocalScene;
    ++wrongGenerationScene.entity.generation;
    if (localScenes.Apply(wrongGenerationScene).Applied() || localScenes.PendingScene() != 120) {
        return 220;
    }
    const auto rejectedLocalResult = localScenes.Apply(rejectedLocalScene);
    if (rejectedLocalResult.kind != LocalSceneAuthorityKind::Rejected ||
        !localScenes.IsAuthorized(119) || localScenes.Prepare(120)) {
        return 215;
    }

    const auto oldLifePending = localScenes.Prepare(121);
    if (!oldLifePending || !localScenes.ResolveTransport(oldLifePending->sequence, true) ||
        !localScenes.ObserveLifeEpoch(2) || localScenes.PendingScene() ||
        localScenes.PendingPlacementScene() || localScenes.ObserveLifeEpoch(1)) {
        return 347;
    }
    LocalSceneAuthority delayedOldLife = acceptedLocalScene119;
    delayedOldLife.requestSequence = oldLifePending->sequence;
    delayedOldLife.lifeEpoch = 1;
    delayedOldLife.sceneId = 121;
    if (localScenes.Apply(delayedOldLife).Applied()) return 348;
    const auto currentLifeRequest = localScenes.Prepare(121);
    if (!currentLifeRequest ||
        !localScenes.ResolveTransport(currentLifeRequest->sequence, true)) {
        return 349;
    }
    LocalSceneAuthority currentLifeReply = delayedOldLife;
    currentLifeReply.requestSequence = currentLifeRequest->sequence;
    currentLifeReply.lifeEpoch = 2;
    if (localScenes.Apply(currentLifeReply).kind != LocalSceneAuthorityKind::Accepted ||
        !localScenes.IsAuthorized(121)) {
        return 350;
    }

    LocalSceneAuthority invalidLocalScene = rejectedLocalScene;
    invalidLocalScene.entity.generation = 0;
    if (localScenes.Apply(invalidLocalScene).Applied() || localScenes.Prepare(-1)) return 216;
    localScenes.Reset();
    if (localScenes.AuthorizedScene() || localScenes.PendingScene() ||
        localScenes.PendingPlacementScene() || localScenes.IsAuthorized(119)) {
        return 217;
    }

    LocalSceneAdmission wrappingScenes(std::numeric_limits<uint32_t>::max());
    const auto finalSceneSequence = wrappingScenes.Prepare(118);
    if (!finalSceneSequence ||
        finalSceneSequence->sequence != std::numeric_limits<uint32_t>::max() ||
        !wrappingScenes.ResolveTransport(finalSceneSequence->sequence, false)) {
        return 218;
    }
    const auto wrappedSceneSequence = wrappingScenes.Prepare(118);
    if (!wrappedSceneSequence || wrappedSceneSequence->sequence != 1) return 219;

    RemoteFishingEntityState remoteFishingEntities;
    RemoteFishEntity remoteFish{};
    remoteFish.ownerPlayerId = 7;
    remoteFish.entity = { 31, 1 };
    remoteFish.identity = {
        118, Game::Simulation::MakeFishSpawnKey(118, 0, 100, 200, 300)
    };
    remoteFish.x = 110.0f;
    remoteFish.y = 220.0f;
    remoteFish.z = 330.0f;
    remoteFish.length = 42.0f;
    remoteFish.active = true;
    if (remoteFishingEntities.ApplyFish(remoteFish) != RemoteFishingEntityUpdate::Established ||
        remoteFishingEntities.FishCount() != 1 ||
        remoteFishingEntities.OwnerForFish(remoteFish.identity) != 7 ||
        remoteFishingEntities.EntityForFish(remoteFish.identity) != remoteFish.entity ||
        remoteFishingEntities.FishEntityForOwner(7) != remoteFish.entity ||
        remoteFishingEntities.FindFish(remoteFish.entity) == nullptr) {
        return 221;
    }
    remoteFish.x = 120.0f;
    if (remoteFishingEntities.ApplyFish(remoteFish) != RemoteFishingEntityUpdate::Updated ||
        !remoteFishingEntities.FishForOwner(7) ||
        !NearlyEqual(remoteFishingEntities.FishForOwner(7)->x, 120.0f)) {
        return 222;
    }

    RemoteFishEntity wrongFishRetirement = remoteFish;
    ++wrongFishRetirement.entity.generation;
    wrongFishRetirement.active = false;
    if (remoteFishingEntities.ApplyFish(wrongFishRetirement) !=
            RemoteFishingEntityUpdate::Ignored ||
        remoteFishingEntities.FishCount() != 1) {
        return 223;
    }
    RemoteFishEntity replacementFish = wrongFishRetirement;
    replacementFish.active = true;
    if (remoteFishingEntities.ApplyFish(replacementFish) !=
            RemoteFishingEntityUpdate::Replaced ||
        remoteFishingEntities.FishForOwner(7)->entity != replacementFish.entity ||
        remoteFishingEntities.FindFish(remoteFish.entity) != nullptr ||
        remoteFishingEntities.FindFish(replacementFish.entity) == nullptr) {
        return 224;
    }
    RemoteFishEntity staleRetirement = remoteFish;
    staleRetirement.active = false;
    if (remoteFishingEntities.ApplyFish(staleRetirement) != RemoteFishingEntityUpdate::Ignored ||
        remoteFishingEntities.FishForOwner(7)->entity != replacementFish.entity) {
        return 225;
    }
    replacementFish.active = false;
    if (remoteFishingEntities.ApplyFish(replacementFish) != RemoteFishingEntityUpdate::Retired ||
        remoteFishingEntities.FishForOwner(7) || remoteFishingEntities.FishCount() != 0) {
        return 226;
    }
    RemoteLureEntity remoteLure{};
    remoteLure.ownerPlayerId = 7;
    remoteLure.entity = { 32, 1 };
    remoteLure.sceneId = 118;
    remoteLure.x = 1.0f;
    remoteLure.y = 2.0f;
    remoteLure.z = 3.0f;
    remoteLure.phase = 1;
    remoteLure.lureType = 2;
    remoteLure.active = true;
    if (remoteFishingEntities.ApplyLure(remoteLure) != RemoteFishingEntityUpdate::Established ||
        !remoteFishingEntities.LureForOwner(7) || remoteFishingEntities.LureCount() != 1 ||
        remoteFishingEntities.LureEntityForOwner(7) != remoteLure.entity ||
        remoteFishingEntities.FindLure(remoteLure.entity) == nullptr) {
        return 228;
    }
    remoteLure.phase = 2;
    if (remoteFishingEntities.ApplyLure(remoteLure) != RemoteFishingEntityUpdate::Updated ||
        remoteFishingEntities.LureForOwner(7)->phase != 2) {
        return 229;
    }
    RemoteLureEntity wrongLureRetirement = remoteLure;
    ++wrongLureRetirement.entity.generation;
    wrongLureRetirement.active = false;
    if (remoteFishingEntities.ApplyLure(wrongLureRetirement) !=
            RemoteFishingEntityUpdate::Ignored ||
        remoteFishingEntities.LureCount() != 1) {
        return 230;
    }
    remoteLure.active = false;
    if (remoteFishingEntities.ApplyLure(remoteLure) != RemoteFishingEntityUpdate::Retired ||
        remoteFishingEntities.LureForOwner(7)) {
        return 231;
    }

    replacementFish.active = true;
    if (remoteFishingEntities.ApplyFish(replacementFish) !=
            RemoteFishingEntityUpdate::Established) {
        return 232;
    }
    remoteLure.active = true;
    if (remoteFishingEntities.ApplyLure(remoteLure) !=
            RemoteFishingEntityUpdate::Established) {
        return 233;
    }
    remoteFishingEntities.RemoveOwner(7);
    if (remoteFishingEntities.FishCount() != 0 || remoteFishingEntities.LureCount() != 0) {
        return 234;
    }
    if (remoteFishingEntities.ApplyFish(replacementFish) !=
            RemoteFishingEntityUpdate::Established) {
        return 235;
    }
    remoteFishingEntities.Reset();
    if (remoteFishingEntities.FishCount() != 0 || remoteFishingEntities.LureCount() != 0) {
        return 236;
    }

    RemoteFishingEntityState indexedFishingEntities;
    RemoteFishEntity indexedFish = replacementFish;
    indexedFish.active = true;
    indexedFish.ownerPlayerId = 7;
    indexedFish.entity = { 51, 1 };
    if (indexedFishingEntities.ApplyFish(indexedFish) !=
            RemoteFishingEntityUpdate::Established) {
        return 403;
    }
    RemoteFishEntity movedIdentityFish = indexedFish;
    movedIdentityFish.entity = { 51, 2 };
    ++movedIdentityFish.identity.spawnKey;
    if (indexedFishingEntities.ApplyFish(movedIdentityFish) !=
            RemoteFishingEntityUpdate::Replaced ||
        indexedFishingEntities.OwnerForFish(indexedFish.identity) ||
        indexedFishingEntities.OwnerForFish(movedIdentityFish.identity) != 7) {
        return 404;
    }
    RemoteFishEntity conflictingIdentityFish = movedIdentityFish;
    conflictingIdentityFish.ownerPlayerId = 8;
    conflictingIdentityFish.entity = { 52, 1 };
    if (indexedFishingEntities.ApplyFish(conflictingIdentityFish) !=
            RemoteFishingEntityUpdate::Ignored ||
        indexedFishingEntities.FishForOwner(8) ||
        indexedFishingEntities.OwnerForFish(movedIdentityFish.identity) != 7) {
        return 405;
    }
    RemoteFishEntity conflictingEntityFish = movedIdentityFish;
    conflictingEntityFish.ownerPlayerId = 8;
    if (indexedFishingEntities.ApplyFish(conflictingEntityFish) !=
            RemoteFishingEntityUpdate::Ignored ||
        indexedFishingEntities.FishForOwner(8)) {
        return 408;
    }
    indexedFishingEntities.RemoveOwner(7);
    if (indexedFishingEntities.OwnerForFish(movedIdentityFish.identity) ||
        indexedFishingEntities.FishCount() != 0 ||
        indexedFishingEntities.ApplyFish(conflictingIdentityFish) !=
            RemoteFishingEntityUpdate::Established ||
        indexedFishingEntities.OwnerForFish(movedIdentityFish.identity) != 8) {
        return 406;
    }
    indexedFishingEntities.Reset();
    if (indexedFishingEntities.OwnerForFish(movedIdentityFish.identity) ||
        indexedFishingEntities.FishCount() != 0) {
        return 407;
    }

    RemoteFishingEntityState semanticFishingEntities;
    if (semanticFishingEntities.ApplyFish(remoteFish) !=
            RemoteFishingEntityUpdate::Established) {
        return 237;
    }
    remoteFish.x = 130.0f;
    if (semanticFishingEntities.ApplyFish(remoteFish) != RemoteFishingEntityUpdate::Updated ||
        !NearlyEqual(semanticFishingEntities.FishForOwner(7)->x, 130.0f)) {
        return 238;
    }
    remoteFish.identity.spawnKey = 0;
    if (semanticFishingEntities.ApplyFish(remoteFish) != RemoteFishingEntityUpdate::Ignored) {
        return 239;
    }

    CorpsePresentationRegistry corpsePresentations;
    CorpsePresentationState corpsePresentation{};
    corpsePresentation.entity = { 41, 1 };
    corpsePresentation.sourcePlayerId = 7;
    corpsePresentation.sourcePlayerEntity = { 7, 2 };
    corpsePresentation.sourceLifeEpoch = 3;
    corpsePresentation.sceneId = 118;
    corpsePresentation.roomId = -1;
    corpsePresentation.x = 10.0f;
    corpsePresentation.y = 20.0f;
    corpsePresentation.z = 30.0f;
    corpsePresentation.rotation[1] = 1234;
    corpsePresentation.selectedWeapon = 2;
    corpsePresentation.active = true;
    const auto establishedCorpse = corpsePresentations.Apply(corpsePresentation);
    if (establishedCorpse.update != CorpsePresentationUpdate::Established ||
        establishedCorpse.actorHandle != -1000 || establishedCorpse.previousEntity ||
        corpsePresentations.Size() != 1 ||
        corpsePresentations.ActorHandleFor(corpsePresentation.entity) != -1000 ||
        corpsePresentations.EntityForActorHandle(-1000) != corpsePresentation.entity ||
        !corpsePresentations.FindByActorHandle(-1000) ||
        !corpsePresentations.OwnsSource(corpsePresentation.sourcePlayerEntity,
                                        corpsePresentation.sourceLifeEpoch) ||
        corpsePresentations.FindForSource(corpsePresentation.sourcePlayerEntity,
                                           corpsePresentation.sourceLifeEpoch) == nullptr) {
        return 240;
    }
    corpsePresentation.x = 11.0f;
    const auto updatedCorpse = corpsePresentations.Apply(corpsePresentation);
    if (updatedCorpse.update != CorpsePresentationUpdate::Updated ||
        updatedCorpse.actorHandle != establishedCorpse.actorHandle ||
        !NearlyEqual(corpsePresentations.Find(corpsePresentation.entity)->x, 11.0f)) {
        return 241;
    }

    CorpsePresentationState mutableSource = corpsePresentation;
    ++mutableSource.sourceLifeEpoch;
    mutableSource.x = 999.0f;
    if (corpsePresentations.Apply(mutableSource).Applied() ||
        !NearlyEqual(corpsePresentations.Find(corpsePresentation.entity)->x,
                     11.0f) ||
        corpsePresentations.OwnsSource(mutableSource.sourcePlayerEntity,
                                       mutableSource.sourceLifeEpoch)) {
        return 241;
    }

    CorpsePresentationState wrongCorpseRetirement = corpsePresentation;
    ++wrongCorpseRetirement.entity.generation;
    wrongCorpseRetirement.active = false;
    if (corpsePresentations.Apply(wrongCorpseRetirement).Applied() ||
        corpsePresentations.Size() != 1) {
        return 242;
    }
    CorpsePresentationState replacementCorpse = wrongCorpseRetirement;
    replacementCorpse.active = true;
    const auto replacedCorpse = corpsePresentations.Apply(replacementCorpse);
    if (replacedCorpse.update != CorpsePresentationUpdate::Replaced ||
        replacedCorpse.previousEntity != corpsePresentation.entity ||
        replacedCorpse.actorHandle != establishedCorpse.actorHandle ||
        corpsePresentations.Find(corpsePresentation.entity) ||
        !corpsePresentations.Find(replacementCorpse.entity) ||
        !corpsePresentations.OwnsSource(replacementCorpse.sourcePlayerEntity,
                                        replacementCorpse.sourceLifeEpoch)) {
        return 243;
    }
    corpsePresentation.active = false;
    if (corpsePresentations.Apply(corpsePresentation).Applied() ||
        corpsePresentations.Size() != 1) {
        return 244;
    }
    corpsePresentation.active = true;
    if (corpsePresentations.Apply(corpsePresentation).Applied() ||
        corpsePresentations.Size() != 1 ||
        !corpsePresentations.Find(replacementCorpse.entity)) {
        return 251;
    }
    replacementCorpse.active = false;
    const auto retiredCorpse = corpsePresentations.Apply(replacementCorpse);
    if (retiredCorpse.update != CorpsePresentationUpdate::Retired ||
        retiredCorpse.actorHandle != establishedCorpse.actorHandle ||
        corpsePresentations.Size() != 0 || corpsePresentations.FindByActorHandle(-1000) ||
        corpsePresentations.OwnsSource(replacementCorpse.sourcePlayerEntity,
                                       replacementCorpse.sourceLifeEpoch)) {
        return 245;
    }

    CorpsePresentationState duplicateSource = corpsePresentation;
    duplicateSource.entity = { 50, 1 };
    duplicateSource.sourcePlayerEntity = { 70, 4 };
    duplicateSource.sourceLifeEpoch = 9;
    duplicateSource.active = true;
    const auto firstSourceOwner = corpsePresentations.Apply(duplicateSource);
    duplicateSource.entity = { 51, 1 };
    const auto replacementSourceOwner = corpsePresentations.Apply(duplicateSource);
    if (firstSourceOwner.update != CorpsePresentationUpdate::Established ||
        replacementSourceOwner.update != CorpsePresentationUpdate::Replaced ||
        replacementSourceOwner.previousEntity != Game::Simulation::EntityId{ 50, 1 } ||
        replacementSourceOwner.actorHandle != firstSourceOwner.actorHandle ||
        corpsePresentations.Size() != 1 ||
        corpsePresentations.Find(Game::Simulation::EntityId{ 50, 1 }) ||
        !corpsePresentations.Find(Game::Simulation::EntityId{ 51, 1 }) ||
        !corpsePresentations.OwnsSource(duplicateSource.sourcePlayerEntity,
                                        duplicateSource.sourceLifeEpoch)) {
        return 245;
    }
    duplicateSource.active = false;
    if (corpsePresentations.Apply(duplicateSource).update !=
            CorpsePresentationUpdate::Retired ||
        corpsePresentations.Size() != 0) {
        return 245;
    }

    for (uint32_t corpse = 0; corpse < 99; ++corpse) {
        CorpsePresentationState retained = corpsePresentation;
        retained.entity = { 100 + corpse, 1 };
        retained.sourcePlayerEntity = { 100 + corpse, 1 };
        retained.sourceLifeEpoch = 1;
        retained.active = true;
        const auto retainedResult = corpsePresentations.Apply(retained);
        if (retainedResult.update != CorpsePresentationUpdate::Established ||
            retainedResult.actorHandle >= 0 ||
            corpsePresentations.EntityForActorHandle(retainedResult.actorHandle) != retained.entity) {
            return 246;
        }
    }
    if (corpsePresentations.Size() != 99) return 247;
    corpsePresentations.Reset();
    if (corpsePresentations.Size() != 0) return 248;
    corpsePresentation.entity = { 999, 1 };
    corpsePresentation.active = true;
    if (corpsePresentations.Apply(corpsePresentation).actorHandle != -1000) return 249;
    corpsePresentation.entity.generation = 0;
    if (corpsePresentations.Apply(corpsePresentation).Applied()) return 250;

    RemotePlayerPresentationRegistry remotePlayers;
    RemotePlayerPresentationState remotePlayer{};
    remotePlayer.entity = { 50, 2 };
    remotePlayer.playerId = 100000;
    remotePlayer.sceneId = 118;
    remotePlayer.active = true;
    const auto establishedRemote = remotePlayers.Apply(remotePlayer);
    if (establishedRemote.update != RemotePlayerPresentationUpdate::Established ||
        establishedRemote.actorHandle != 1 || remotePlayers.Size() != 1 ||
        remotePlayers.FindPlayer(100000) == nullptr ||
        remotePlayers.EntityForActorHandle(1) != remotePlayer.entity) {
        return 256;
    }
    remotePlayer.sceneId = 119;
    const auto updatedRemote = remotePlayers.Apply(remotePlayer);
    if (updatedRemote.update != RemotePlayerPresentationUpdate::Updated ||
        updatedRemote.actorHandle != 1 ||
        remotePlayers.Find(remotePlayer.entity)->sceneId != 119) {
        return 257;
    }
    RemotePlayerPresentationState staleRemote = remotePlayer;
    staleRemote.entity.generation = 1;
    if (remotePlayers.Apply(staleRemote).Applied() ||
        remotePlayers.FindPlayer(remotePlayer.playerId)->entity != remotePlayer.entity) {
        return 258;
    }
    RemotePlayerPresentationState replacementRemote = remotePlayer;
    replacementRemote.entity.generation = 3;
    const auto replacedRemote = remotePlayers.Apply(replacementRemote);
    if (replacedRemote.update != RemotePlayerPresentationUpdate::Replaced ||
        replacedRemote.previousEntity != remotePlayer.entity ||
        replacedRemote.actorHandle != establishedRemote.actorHandle ||
        remotePlayers.Find(remotePlayer.entity) != nullptr ||
        remotePlayers.FindPlayer(remotePlayer.playerId)->entity != replacementRemote.entity) {
        return 259;
    }
    RemotePlayerPresentationState conflictingRemote = replacementRemote;
    conflictingRemote.playerId = 100001;
    if (remotePlayers.Apply(conflictingRemote).Applied() || remotePlayers.Size() != 1) {
        return 260;
    }
    remotePlayer.active = false;
    if (remotePlayers.Apply(remotePlayer).Applied() || remotePlayers.Size() != 1) {
        return 261;
    }
    replacementRemote.active = false;
    const auto retiredRemote = remotePlayers.Apply(replacementRemote);
    if (retiredRemote.update != RemotePlayerPresentationUpdate::Retired ||
        remotePlayers.Size() != 0 || remotePlayers.FindByActorHandle(1) != nullptr) {
        return 262;
    }
    std::set<int16_t> remoteActorHandles;
    for (int32_t playerId = 0; playerId < 1024; ++playerId) {
        RemotePlayerPresentationState retainedRemote{};
        retainedRemote.entity = { static_cast<uint32_t>(1000 + playerId), 1 };
        retainedRemote.playerId = 1000000 + playerId;
        retainedRemote.sceneId = 118;
        retainedRemote.active = true;
        const auto retained = remotePlayers.Apply(retainedRemote);
        if (retained.update != RemotePlayerPresentationUpdate::Established ||
            retained.actorHandle <= 0 || !remoteActorHandles.insert(retained.actorHandle).second) {
            return 263;
        }
    }
    if (remotePlayers.Size() != 1024) return 264;
    remotePlayers.Reset();
    if (remotePlayers.Size() != 0 || remotePlayers.Apply({ { 9, 0 }, 1, 118, true }).Applied()) {
        return 265;
    }

    RemotePlayerReplicaStore replicas;
    RemotePlayerPresentationState replicaLifetime{ { 80, 1 }, 700, 118, true };
    const auto replicaEstablished = replicas.ApplyLifecycle(replicaLifetime);
    if (replicaEstablished.update != RemotePlayerPresentationUpdate::Established ||
        replicaEstablished.actorHandle != 1 || replicas.Size() != 1 ||
        replicas.FindPlayer(700) == nullptr ||
        replicas.EntityForActorHandle(1) != replicaLifetime.entity) {
        return 289;
    }
    PlayerSnapshot replicaSnapshot{};
    replicaSnapshot.entity = replicaLifetime.entity;
    replicaSnapshot.ownerPlayerId = replicaLifetime.playerId;
    replicaSnapshot.sceneId = replicaLifetime.sceneId;
    replicaSnapshot.serverTick = 10;
    replicaSnapshot.lifeEpoch = 1;
    replicaSnapshot.selectedWeapon = 4;
    if (!replicas.ApplySnapshot(replicaSnapshot, 1.0) ||
        replicas.ApplySnapshot(replicaSnapshot, 1.1) ||
        !replicas.FindPlayer(700)->hasSnapshot ||
        replicas.FindPlayer(700)->motion.SampleCount() != 1) {
        return 290;
    }
    PlayerSnapshot staleLifeReplica = replicaSnapshot;
    staleLifeReplica.serverTick = 11;
    staleLifeReplica.lifeEpoch = 2;
    if (!replicas.ApplySnapshot(staleLifeReplica, 1.1)) return 297;
    staleLifeReplica.serverTick = 12;
    staleLifeReplica.lifeEpoch = 1;
    if (replicas.ApplySnapshot(staleLifeReplica, 1.2)) return 298;
    Game::Replication::FishingPresentationState replicaFishing{};
    replicaFishing.playerId = 700;
    replicaFishing.entity = replicaLifetime.entity;
    replicaFishing.sceneId = 118;
    replicaFishing.sequence = 1;
    if (!replicas.ApplyFishing(replicaFishing, 1.0) ||
        replicas.FindPlayer(700)->fishing.SampleCount() != 1) {
        return 291;
    }
    PlayerSnapshot wrongReplicaSnapshot = replicaSnapshot;
    wrongReplicaSnapshot.serverTick = 11;
    wrongReplicaSnapshot.entity.generation = 2;
    replicaFishing.sceneId = 119;
    replicaFishing.sequence = 2;
    if (replicas.ApplySnapshot(wrongReplicaSnapshot, 1.2) ||
        replicas.ApplyFishing(replicaFishing, 1.2)) {
        return 292;
    }
    replicaLifetime.sceneId = 119;
    const auto replicaSceneChange = replicas.ApplyLifecycle(replicaLifetime);
    const auto* changedReplica = replicas.FindPlayer(700);
    if (replicaSceneChange.update != RemotePlayerPresentationUpdate::Updated ||
        replicaSceneChange.actorHandle != 1 || !changedReplica ||
        changedReplica->hasSnapshot || changedReplica->motion.SampleCount() != 0 ||
        changedReplica->fishing.SampleCount() != 0) {
        return 293;
    }
    replicaSnapshot.sceneId = 119;
    replicaSnapshot.serverTick = 13;
    replicaSnapshot.lifeEpoch = 2;
    if (!replicas.ApplySnapshot(replicaSnapshot, 2.0)) return 294;
    RemotePlayerPresentationState replicaReplacement = replicaLifetime;
    replicaReplacement.entity.generation = 2;
    const auto replicaReplaced = replicas.ApplyLifecycle(replicaReplacement);
    if (replicaReplaced.update != RemotePlayerPresentationUpdate::Replaced ||
        replicaReplaced.previousEntity != replicaLifetime.entity ||
        replicaReplaced.actorHandle != 1 || replicas.Find(replicaLifetime.entity) ||
        !replicas.Find(replicaReplacement.entity) ||
        replicas.Find(replicaReplacement.entity)->hasSnapshot ||
        replicas.ApplySnapshot(replicaSnapshot, 2.1)) {
        return 295;
    }
    replicaReplacement.active = false;
    const auto replicaRetired = replicas.ApplyLifecycle(replicaReplacement);
    if (replicaRetired.update != RemotePlayerPresentationUpdate::Retired ||
        replicas.Size() != 0 || replicas.FindByActorHandle(1)) {
        return 296;
    }

    RemoteProjectilePresentationRegistry remoteProjectiles;
    RemoteProjectilePresentationState remoteProjectile{};
    remoteProjectile.entity = { 70, 2 };
    remoteProjectile.logicalId = { 100000, 42, 0 };
    remoteProjectile.sceneId = 118;
    remoteProjectile.active = true;
    const auto establishedProjectile = remoteProjectiles.Apply(remoteProjectile);
    if (establishedProjectile.update != RemoteProjectilePresentationUpdate::Established ||
        establishedProjectile.actorHandle != 1 || remoteProjectiles.Size() != 1 ||
        remoteProjectiles.FindLogical(remoteProjectile.logicalId) == nullptr ||
        remoteProjectiles.EntityForActorHandle(1) != remoteProjectile.entity) {
        return 266;
    }
    RemoteProjectilePresentationState conflictingProjectile = remoteProjectile;
    conflictingProjectile.logicalId.projectileKind = 1;
    if (remoteProjectiles.Apply(conflictingProjectile).Applied() ||
        remoteProjectiles.Size() != 1) {
        return 267;
    }
    RemoteProjectilePresentationState staleProjectile = remoteProjectile;
    staleProjectile.entity.generation = 1;
    if (remoteProjectiles.Apply(staleProjectile).Applied() ||
        remoteProjectiles.FindLogical(remoteProjectile.logicalId)->entity != remoteProjectile.entity) {
        return 268;
    }
    RemoteProjectilePresentationState replacementProjectile = remoteProjectile;
    replacementProjectile.entity.generation = 3;
    const auto replacedProjectile = remoteProjectiles.Apply(replacementProjectile);
    if (replacedProjectile.update != RemoteProjectilePresentationUpdate::Replaced ||
        replacedProjectile.previousEntity != remoteProjectile.entity ||
        replacedProjectile.actorHandle != establishedProjectile.actorHandle ||
        remoteProjectiles.Find(remoteProjectile.entity) != nullptr ||
        remoteProjectiles.FindLogical(remoteProjectile.logicalId)->entity !=
            replacementProjectile.entity) {
        return 269;
    }
    remoteProjectile.active = false;
    if (remoteProjectiles.Apply(remoteProjectile).Applied() || remoteProjectiles.Size() != 1) {
        return 270;
    }
    replacementProjectile.active = false;
    const auto retiredProjectile = remoteProjectiles.Apply(replacementProjectile);
    if (retiredProjectile.update != RemoteProjectilePresentationUpdate::Retired ||
        remoteProjectiles.Size() != 0 || remoteProjectiles.FindByActorHandle(1) != nullptr) {
        return 271;
    }
    std::set<int16_t> projectileActorHandles;
    for (int32_t projectileId = 1; projectileId <= 1024; ++projectileId) {
        RemoteProjectilePresentationState retainedProjectile{};
        retainedProjectile.entity = { static_cast<uint32_t>(2000 + projectileId), 1 };
        retainedProjectile.logicalId = { 200000, projectileId,
                                         static_cast<uint8_t>(projectileId & 1) };
        retainedProjectile.sceneId = 118;
        retainedProjectile.active = true;
        const auto retained = remoteProjectiles.Apply(retainedProjectile);
        if (retained.update != RemoteProjectilePresentationUpdate::Established ||
            retained.actorHandle <= 0 ||
            !projectileActorHandles.insert(retained.actorHandle).second) {
            return 272;
        }
    }
    if (remoteProjectiles.Size() != 1024 ||
        remoteProjectiles.RetireOwner(200000).size() != 1024 ||
        remoteProjectiles.Size() != 0) {
        return 273;
    }
    remoteProjectiles.Reset();
    if (remoteProjectiles.Size() != 0 ||
        remoteProjectiles.Apply({ { 5, 0 }, { 1, 1, 0 }, 118, true }).Applied()) {
        return 274;
    }

    RemoteProjectileReplicaStore projectileReplicas;
    RemoteProjectileReplicaState arrowReplica{};
    arrowReplica.entity = { 90, 1 };
    arrowReplica.logicalId = { 800, 12, 0 };
    arrowReplica.sceneId = 118;
    arrowReplica.sequence = 1;
    arrowReplica.active = true;
    arrowReplica.phase = RemoteProjectilePhase::ArrowFlying;
    arrowReplica.position = { 1.0f, 2.0f, 3.0f };
    arrowReplica.velocity = { 4.0f, 5.0f, 6.0f };
    const auto arrowReplicaEstablished = projectileReplicas.Apply(arrowReplica, 1.0);
    if (arrowReplicaEstablished.update !=
            RemoteProjectilePresentationUpdate::Established ||
        arrowReplicaEstablished.actorHandle != 1 || projectileReplicas.Size() != 1 ||
        projectileReplicas.FindLogical(arrowReplica.logicalId) == nullptr ||
        projectileReplicas.Find(arrowReplica.entity)->motion.SampleCount() != 1 ||
        projectileReplicas.EntityForActorHandle(1) != arrowReplica.entity) {
        return 299;
    }
    if (projectileReplicas.Apply(arrowReplica, 1.1).Applied()) return 300;
    arrowReplica.sequence = 2;
    arrowReplica.phase = RemoteProjectilePhase::ArrowStuck;
    arrowReplica.position = { 10.0f, 20.0f, 30.0f };
    const auto arrowTerminal = projectileReplicas.Apply(arrowReplica, 1.2);
    auto* terminalReplica = projectileReplicas.FindMutable(arrowReplica.entity);
    if (arrowTerminal.update != RemoteProjectilePresentationUpdate::Updated ||
        !terminalReplica || !terminalReplica->state.Terminal() ||
        !terminalReplica->motion.Evaluate(1.2)->terminal ||
        !NearlyEqual(terminalReplica->motion.Evaluate(1.2)->position.x, 10.0f)) {
        return 301;
    }
    arrowReplica.sequence = 3;
    arrowReplica.phase = RemoteProjectilePhase::ArrowFlying;
    if (projectileReplicas.Apply(arrowReplica, 1.3).Applied() ||
        !projectileReplicas.Find(arrowReplica.entity)->state.Terminal()) {
        return 302;
    }
    RemoteProjectileReplicaState replacementArrow = arrowReplica;
    replacementArrow.entity.generation = 2;
    replacementArrow.sequence = 1;
    replacementArrow.phase = RemoteProjectilePhase::ArrowFlying;
    const auto arrowReplaced = projectileReplicas.Apply(replacementArrow, 2.0);
    if (arrowReplaced.update != RemoteProjectilePresentationUpdate::Replaced ||
        arrowReplaced.previousEntity != arrowReplica.entity ||
        arrowReplaced.actorHandle != 1 || projectileReplicas.Find(arrowReplica.entity) ||
        !projectileReplicas.Find(replacementArrow.entity)) {
        return 304;
    }
    if (projectileReplicas.RetireOwner(800).size() != 1 ||
        projectileReplicas.Size() != 0 || projectileReplicas.FindByActorHandle(1)) {
        return 305;
    }

    ClientWorldState clientWorld;
    Game::Simulation::ObjectiveSnapshot clientObjective{};
    clientObjective.entity = { 300, 2 };
    clientObjective.objectiveKey = 10;
    clientObjective.sceneId = 118;
    clientObjective.position = { 10.0f, 0.0f, 20.0f };
    clientObjective.captureRadius = 300.0f;
    const auto objectiveEstablished = clientWorld.ApplyObjective(clientObjective, true);
    clientObjective.captureProgress = 25.0f;
    const auto objectiveUpdated = clientWorld.ApplyObjective(clientObjective, true);
    if (objectiveEstablished.update != ClientWorldStateUpdate::Established ||
        objectiveUpdated.update != ClientWorldStateUpdate::Updated ||
        !NearlyEqual(clientWorld.FindObjective(10)->captureProgress, 25.0f)) {
        return 275;
    }
    Game::Simulation::ObjectiveSnapshot staleObjective = clientObjective;
    staleObjective.entity.generation = 1;
    if (clientWorld.ApplyObjective(staleObjective, true).Applied()) return 276;
    Game::Simulation::ObjectiveSnapshot replacementObjective = clientObjective;
    replacementObjective.entity.generation = 3;
    const auto objectiveReplaced = clientWorld.ApplyObjective(replacementObjective, true);
    if (objectiveReplaced.update != ClientWorldStateUpdate::Replaced ||
        objectiveReplaced.previousEntity != clientObjective.entity ||
        clientWorld.FindObjective(10)->entity != replacementObjective.entity ||
        clientWorld.ApplyObjective(clientObjective, false).Applied()) {
        return 277;
    }

    Game::Simulation::StructureSnapshot clientStructure{};
    clientStructure.entity = { 301, 1 };
    clientStructure.structureKey = 20;
    clientStructure.objectiveKey = 10;
    clientStructure.sceneId = 118;
    clientStructure.position = { 30.0f, 0.0f, 40.0f };
    clientStructure.maximumHealth = 1000;
    clientStructure.requiredBuild = 100;
    if (clientWorld.ApplyStructure(clientStructure, true).update !=
            ClientWorldStateUpdate::Established ||
        clientWorld.FindStructure(20) == nullptr) {
        return 278;
    }
    Game::Simulation::StructureSnapshot conflictingStructure = clientStructure;
    conflictingStructure.structureKey = 21;
    if (clientWorld.ApplyStructure(conflictingStructure, true).Applied()) return 279;

    if (clientWorld.ApplyObjective(replacementObjective, false).update !=
            ClientWorldStateUpdate::Retired ||
        clientWorld.ApplyStructure(clientStructure, false).update !=
            ClientWorldStateUpdate::Retired ||
        clientWorld.ObjectiveCount() != 0 || clientWorld.StructureCount() != 0) {
        return 283;
    }
    clientWorld.Reset();

    ServerWorld serverWorld;
    ServerWorldTestAccess::Players(serverWorld).EnsurePlayer(
        500, PlayerSpawn{ 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    ServerWorldTestAccess::Objectives(serverWorld).EnsureObjective(
        { 500, 118, {}, 500.0f, Game::Simulation::TeamId::Neutral });
    const auto schedulerStart = ServerWorld::Clock::now();
    if (serverWorld.Advance(schedulerStart).worldSteps != 0) return 67;
    uint32_t worldSteps = 0;
    uint32_t playerSteps = 0;
    for (int32_t step = 1; step <= 120; ++step) {
        const auto update = serverWorld.Advance(
            schedulerStart + std::chrono::nanoseconds(16666667LL * step));
        worldSteps += update.worldSteps;
        playerSteps += update.playerSteps;
    }
    const auto scheduledObjective =
        ServerWorldTestAccess::Objectives(serverWorld).SnapshotForObjective(500);
    if (worldSteps != 120 || playerSteps != 60 || serverWorld.CurrentTick() != 120 ||
        ServerWorldTestAccess::Players(serverWorld).CurrentTick() != 60 || !scheduledObjective ||
        !NearlyEqual(scheduledObjective->captureProgress, 40.0f)) {
        return 68;
    }

    ServerWorld catchupWorld;
    catchupWorld.Advance(schedulerStart);
    const auto cappedCatchup = catchupWorld.Advance(schedulerStart + std::chrono::seconds(1));
    if (cappedCatchup.worldSteps != 15 || cappedCatchup.playerSteps != 7 ||
        catchupWorld.CurrentTick() != 15) {
        return 69;
    }

    PlayerSimulation lifeSimulation;
    lifeSimulation.EnsurePlayer(700, { 118, {}, 0.0f });
    if (!lifeSimulation.ApplyDamage(-1, 700, 48, 0)) return 70;
    const auto deathEvents = lifeSimulation.DrainLifeEvents();
    if (deathEvents.size() != 1 ||
        deathEvents.front().kind != Game::Simulation::PlayerLifeEventKind::Died ||
        deathEvents.front().playerId != 700 || deathEvents.front().serverTick != 0) {
        return 71;
    }
    for (int32_t tick = 0; tick < 149; ++tick) lifeSimulation.StepFixed();
    const auto stillDead = lifeSimulation.SnapshotForPlayer(700);
    if (!stillDead || stillDead->health != 0 || !lifeSimulation.DrainLifeEvents().empty()) return 72;
    lifeSimulation.StepFixed();
    const auto tickRespawned = lifeSimulation.SnapshotForPlayer(700);
    const auto respawnEvents = lifeSimulation.DrainLifeEvents();
    if (!tickRespawned || tickRespawned->health != 48 || tickRespawned->serverTick != 150 ||
        respawnEvents.size() != 1 ||
        respawnEvents.front().kind != Game::Simulation::PlayerLifeEventKind::Respawned ||
        respawnEvents.front().serverTick != 150) {
        return 73;
    }

    PlayerReplicationSystem replication;
    PlayerSnapshot observerOne{};
    observerOne.entity = { 10, 1 };
    observerOne.ownerPlayerId = 1;
    observerOne.sceneId = 118;
    PlayerSnapshot observerTwo = observerOne;
    observerTwo.entity = { 11, 1 };
    observerTwo.ownerPlayerId = 2;
    observerTwo.position = { 500.0f, 0.0f, 0.0f };
    PlayerSnapshot distantObserver = observerOne;
    distantObserver.entity = { 12, 1 };
    distantObserver.ownerPlayerId = 3;
    distantObserver.position = { 7000.0f, 0.0f, 0.0f };
    std::vector<PlayerSnapshot> replicatedPlayers{ observerOne, observerTwo, distantObserver };
    auto visibility = replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f);
    if (visibility.size() != 2 ||
        visibility[0].observerPlayerId != 1 || visibility[0].subject.playerId != 2 ||
        visibility[0].action != PlayerVisibilityAction::Enter ||
        visibility[1].observerPlayerId != 2 || visibility[1].subject.playerId != 1 ||
        visibility[1].action != PlayerVisibilityAction::Enter ||
        !replication.IsVisible(1, 2) || !replication.IsVisible(2, 1) ||
        replication.IsVisible(1, 3)) {
        return 74;
    }
    if (!replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f).empty()) return 75;
    if (replication.ObserversForPlayer(1) != std::vector<int32_t>{ 2 } ||
        replication.ObserversForPlayer(2) != std::vector<int32_t>{ 1 }) {
        return 506;
    }
    if (!replication.Reconcile(replicatedPlayers, { 1, 3 }, 1000.0f).empty() ||
        !replication.ObserversForPlayer(1).empty() ||
        replication.ObserversForPlayer(2) != std::vector<int32_t>{ 1 }) {
        return 507;
    }
    visibility = replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f);
    if (visibility.size() != 1 || visibility[0].observerPlayerId != 2 ||
        visibility[0].subject.playerId != 1 ||
        visibility[0].action != PlayerVisibilityAction::Enter ||
        replication.ObserversForPlayer(1) != std::vector<int32_t>{ 2 }) {
        return 508;
    }

    // Reliable visibility does not flap when movement jitters around the
    // 1000-unit enter boundary. Existing pairs remain through the 1100-unit
    // leave radius and cannot re-enter until they cross the enter radius.
    replicatedPlayers[1].position = { 1050.0f, 0.0f, 0.0f };
    if (!replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f).empty() ||
        !replication.IsVisible(1, 2) || !replication.IsVisible(2, 1)) {
        return 370;
    }
    replicatedPlayers[1].position = { 1101.0f, 0.0f, 0.0f };
    visibility = replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f);
    if (visibility.size() != 2 || replication.IsVisible(1, 2) ||
        replication.IsVisible(2, 1)) {
        return 371;
    }
    replicatedPlayers[1].position = { 1050.0f, 0.0f, 0.0f };
    if (!replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f).empty() ||
        replication.IsVisible(1, 2) || replication.IsVisible(2, 1)) {
        return 372;
    }
    replicatedPlayers[1].position = { 950.0f, 0.0f, 0.0f };
    visibility = replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f);
    if (visibility.size() != 2 || !replication.IsVisible(1, 2) ||
        !replication.IsVisible(2, 1)) {
        return 373;
    }
    replicatedPlayers[1].position = { 500.0f, 0.0f, 0.0f };
    if (!replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f).empty()) return 374;

    replicatedPlayers[1].position = { 8000.0f, 0.0f, 0.0f };
    visibility = replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f);
    if (visibility.size() != 4 ||
        visibility[0].observerPlayerId != 1 || visibility[0].subject.playerId != 2 ||
        visibility[0].action != PlayerVisibilityAction::Leave ||
        visibility[1].observerPlayerId != 2 || visibility[1].subject.playerId != 1 ||
        visibility[1].action != PlayerVisibilityAction::Leave ||
        visibility[2].observerPlayerId != 2 || visibility[2].subject.playerId != 3 ||
        visibility[2].action != PlayerVisibilityAction::Enter ||
        visibility[3].observerPlayerId != 3 || visibility[3].subject.playerId != 2 ||
        visibility[3].action != PlayerVisibilityAction::Enter) {
        return 76;
    }

    replicatedPlayers[1].position = { 500.0f, 0.0f, 0.0f };
    visibility = replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f);
    if (visibility.size() != 4 ||
        visibility[0].observerPlayerId != 1 || visibility[0].subject.playerId != 2 ||
        visibility[0].action != PlayerVisibilityAction::Enter ||
        visibility[1].observerPlayerId != 2 || visibility[1].subject.playerId != 3 ||
        visibility[1].action != PlayerVisibilityAction::Leave ||
        visibility[2].observerPlayerId != 2 || visibility[2].subject.playerId != 1 ||
        visibility[2].action != PlayerVisibilityAction::Enter ||
        visibility[3].observerPlayerId != 3 || visibility[3].subject.playerId != 2 ||
        visibility[3].action != PlayerVisibilityAction::Leave) {
        return 77;
    }

    const auto oldLifetime = replicatedPlayers[1].entity;
    replicatedPlayers[1].entity.generation = 2;
    visibility = replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f);
    if (visibility.size() != 2 || visibility[0].action != PlayerVisibilityAction::Leave ||
        visibility[0].subject.entity != oldLifetime ||
        visibility[1].action != PlayerVisibilityAction::Enter ||
        visibility[1].subject.entity != replicatedPlayers[1].entity ||
        replication.ObserversForPlayer(2) != std::vector<int32_t>{ 1 }) {
        return 78;
    }
    const auto removal = replication.RemovePlayer(2);
    if (removal.size() != 1 || removal[0].observerPlayerId != 1 ||
        removal[0].subject.entity != replicatedPlayers[1].entity ||
        removal[0].action != PlayerVisibilityAction::Leave || replication.IsVisible(1, 2) ||
        !replication.ObserversForPlayer(1).empty() ||
        !replication.ObserversForPlayer(2).empty()) {
        return 79;
    }

    // Owned entities follow their own authoritative position rather than the
    // owner's visibility. An arrow crossing into observer one's interest area
    // remains visible even after its shooter moves outside that area. Reusing
    // the same client projectile number still retires the old server generation
    // before introducing the replacement.
    replicatedPlayers[1].entity.generation = 2;
    replicatedPlayers[1].position = { 5000.0f, 0.0f, 0.0f };
    replication.Reconcile(replicatedPlayers, { 1, 2, 3 }, 1000.0f);
    OwnedEntityReplicationSystem ownedReplication;
    ReplicatedOwnedEntity arrow{
        { OwnedEntityKind::Arrow, 2, 9001 }, { 30, 1 }, 118, { 500.0f, 0.0f, 0.0f }, false
    };
    auto ownedVisibility = ownedReplication.Reconcile(
        { arrow }, { 1, 2, 3 }, replicatedPlayers, 1000.0f);
    if (ownedVisibility.size() != 1 || ownedVisibility[0].observerPlayerId != 1 ||
        ownedVisibility[0].action != OwnedEntityVisibilityAction::Enter ||
        ownedVisibility[0].subject.entity != arrow.entity ||
        ownedReplication.IsVisible(2, arrow.key) ||
        ownedReplication.ObserversFor(arrow.key) != std::vector<int32_t>{ 1 }) {
        return 80;
    }
    if (!ownedReplication.Reconcile(
             { arrow }, { 1, 2, 3 }, replicatedPlayers, 1000.0f).empty()) return 81;

    arrow.position = { 1050.0f, 0.0f, 0.0f };
    if (!ownedReplication.Reconcile(
             { arrow }, { 1, 2, 3 }, replicatedPlayers, 1000.0f).empty() ||
        !ownedReplication.IsVisible(1, arrow.key)) {
        return 375;
    }
    arrow.position = { 1101.0f, 0.0f, 0.0f };
    ownedVisibility = ownedReplication.Reconcile(
        { arrow }, { 1, 2, 3 }, replicatedPlayers, 1000.0f);
    if (ownedVisibility.size() != 1 || ownedVisibility[0].lifetimeEnded ||
        ownedReplication.IsVisible(1, arrow.key) ||
        !ownedReplication.ObserversFor(arrow.key).empty()) {
        return 376;
    }
    arrow.position = { 1050.0f, 0.0f, 0.0f };
    if (!ownedReplication.Reconcile(
             { arrow }, { 1, 2, 3 }, replicatedPlayers, 1000.0f).empty() ||
        ownedReplication.IsVisible(1, arrow.key)) {
        return 377;
    }
    arrow.position = { 950.0f, 0.0f, 0.0f };
    ownedVisibility = ownedReplication.Reconcile(
        { arrow }, { 1, 2, 3 }, replicatedPlayers, 1000.0f);
    if (ownedVisibility.size() != 1 ||
        ownedVisibility[0].action != OwnedEntityVisibilityAction::Enter ||
        !ownedReplication.IsVisible(1, arrow.key) ||
        ownedReplication.ObserversFor(arrow.key) != std::vector<int32_t>{ 1 }) {
        return 378;
    }
    arrow.position = { 500.0f, 0.0f, 0.0f };
    if (!ownedReplication.Reconcile(
             { arrow }, { 1, 2, 3 }, replicatedPlayers, 1000.0f).empty()) return 379;

    const auto oldArrowLifetime = arrow.entity;
    arrow.entity.generation = 2;
    ownedVisibility = ownedReplication.Reconcile(
        { arrow }, { 1, 2, 3 }, replicatedPlayers, 1000.0f);
    if (ownedVisibility.size() != 2 ||
        ownedVisibility[0].action != OwnedEntityVisibilityAction::Leave ||
        ownedVisibility[0].subject.entity != oldArrowLifetime ||
        !ownedVisibility[0].lifetimeEnded ||
        ownedVisibility[1].action != OwnedEntityVisibilityAction::Enter ||
        ownedVisibility[1].subject.entity != arrow.entity ||
        ownedVisibility[1].lifetimeEnded) {
        return 82;
    }

    ownedVisibility = ownedReplication.Reconcile(
        {}, { 1, 2, 3 }, replicatedPlayers, 1000.0f);
    if (ownedVisibility.size() != 1 ||
        ownedVisibility[0].action != OwnedEntityVisibilityAction::Leave ||
        ownedVisibility[0].subject.entity != arrow.entity ||
        !ownedVisibility[0].lifetimeEnded ||
        ownedReplication.IsVisible(1, arrow.key) ||
        !ownedReplication.ObserversFor(arrow.key).empty()) {
        return 83;
    }
    ownedReplication.Reset();

    // Different owner-following kinds may share the same logical sub-id. A
    // lure is also visible to its predicting owner while its hooked fish is
    // only replicated to observers of that owner.
    const Game::Simulation::FishSnapshot fishPayload{
        { 31, 1 }, { 118, Game::Simulation::MakeFishSpawnKey(118, 2, 500, 0, 0) }, 2,
        { 500.0f, 0.0f, 0.0f }, Game::Simulation::FishSpecies::HylianLoach, 19.5f
    };
    const Game::Simulation::FishingLureSnapshot lurePayload{
        { 32, 1 }, 2, 118, { 510.0f, 0.0f, 0.0f },
        Game::Simulation::FishingLurePhase::Hooked, 2
    };
    const ReplicatedOwnedEntity fishEntity{
        { OwnedEntityKind::Fish, 2, 1 }, fishPayload.entity, 118,
        fishPayload.position, false, fishPayload
    };
    const ReplicatedOwnedEntity lureEntity{
        { OwnedEntityKind::Lure, 2, 1 }, lurePayload.entity, 118,
        lurePayload.position, true, lurePayload
    };
    std::vector<ReplicatedOwnedEntity> ownedCandidates{ fishEntity, lureEntity };
    for (int32_t index = 0; index < 100; ++index) {
        ownedCandidates.push_back({
            { OwnedEntityKind::Arrow, 99, 100 + index },
            { static_cast<uint32_t>(1000 + index), 1 }, 118,
            { 10000.0f + static_cast<float>(index), 0.0f, 0.0f }, false
        });
    }
    ownedVisibility = ownedReplication.Reconcile(
        ownedCandidates, { 1, 2, 3 }, replicatedPlayers, 1000.0f);
    if (ownedVisibility.size() != 3 ||
        !ownedReplication.IsVisible(1, fishEntity.key) ||
        !ownedReplication.IsVisible(1, lureEntity.key) ||
        ownedReplication.IsVisible(2, fishEntity.key) ||
        !ownedReplication.IsVisible(2, lureEntity.key) ||
        ownedReplication.ObserversFor(fishEntity.key) !=
            std::vector<int32_t>{ 1 } ||
        ownedReplication.ObserversFor(lureEntity.key) !=
            std::vector<int32_t>({ 1, 2 }) ||
        ownedReplication.LastCandidateCount() != 4) {
        return 102;
    }
    if (!ownedReplication.Reconcile(
             ownedCandidates, { 1, 3 }, replicatedPlayers, 1000.0f).empty() ||
        ownedReplication.ObserversFor(lureEntity.key) !=
            std::vector<int32_t>{ 1 }) {
        return 509;
    }
    ownedVisibility = ownedReplication.Reconcile(
        ownedCandidates, { 1, 2, 3 }, replicatedPlayers, 1000.0f);
    if (ownedVisibility.size() != 1 ||
        ownedVisibility[0].observerPlayerId != 2 ||
        ownedVisibility[0].subject.key != lureEntity.key ||
        ownedVisibility[0].action != OwnedEntityVisibilityAction::Enter ||
        ownedReplication.ObserversFor(lureEntity.key) !=
            std::vector<int32_t>({ 1, 2 })) {
        return 510;
    }
    if (ownedReplication.NextStateSequence(-1, fishEntity.key) != 0 ||
        ownedReplication.NextStateSequence(1, {}) != 0 ||
        ownedReplication.NextStateSequence(1, fishEntity.key) != 1 ||
        ownedReplication.NextStateSequence(1, fishEntity.key) != 2 ||
        ownedReplication.NextStateSequence(1, lureEntity.key) != 1 ||
        ownedReplication.NextStateSequence(2, lureEntity.key) != 1) {
        return 339;
    }
    ownedVisibility = ownedReplication.RemoveObserver(2);
    const auto* retiredOwnerLure = ownedVisibility.empty()
        ? nullptr
        : std::get_if<Game::Simulation::FishingLureSnapshot>(
              &ownedVisibility[0].subject.payload);
    if (ownedVisibility.size() != 1 ||
        ownedVisibility[0].subject.key.kind != OwnedEntityKind::Lure ||
        ownedVisibility[0].action != OwnedEntityVisibilityAction::Leave ||
        ownedVisibility[0].lifetimeEnded ||
        !retiredOwnerLure || retiredOwnerLure->entity != lurePayload.entity ||
        retiredOwnerLure->phase != Game::Simulation::FishingLurePhase::Hooked ||
        retiredOwnerLure->lureType != 2 ||
        ownedReplication.ObserversFor(lureEntity.key) !=
            std::vector<int32_t>{ 1 } ||
        ownedReplication.NextStateSequence(2, lureEntity.key) != 1) {
        return 103;
    }
    ownedVisibility = ownedReplication.Reconcile(
        {}, { 1, 3 }, replicatedPlayers, 1000.0f);
    const auto fishLeave = std::find_if(
        ownedVisibility.begin(), ownedVisibility.end(), [](const auto& transition) {
            return transition.subject.key.kind == OwnedEntityKind::Fish;
        });
    const auto* retiredFish = fishLeave == ownedVisibility.end()
        ? nullptr
        : std::get_if<Game::Simulation::FishSnapshot>(&fishLeave->subject.payload);
    if (ownedVisibility.size() != 2 || !retiredFish ||
        !fishLeave->lifetimeEnded ||
        retiredFish->entity != fishPayload.entity ||
        retiredFish->identity != fishPayload.identity ||
        retiredFish->species != Game::Simulation::FishSpecies::HylianLoach ||
        !NearlyEqual(retiredFish->length, 19.5f) ||
        !ownedReplication.ObserversFor(fishEntity.key).empty() ||
        !ownedReplication.ObserversFor(lureEntity.key).empty()) {
        return 338;
    }
    ownedReplication.Reset();

    ReplicationBudgetSystem budgets({ 1000, std::chrono::milliseconds(1000), { 50, 30, 20 } });
    const auto budgetStart = ReplicationBudgetSystem::Clock::now();
    budgets.UpdateObservers({ 1, 2 }, budgetStart);
    if (!budgets.TryConsume(1, ReplicationPriority::High, 500) ||
        budgets.TryConsume(1, ReplicationPriority::High, 1) ||
        !budgets.TryConsume(1, ReplicationPriority::Normal, 300) ||
        budgets.TryConsume(1, ReplicationPriority::Normal, 1) ||
        !budgets.TryConsume(1, ReplicationPriority::Low, 200) ||
        budgets.TryConsume(1, ReplicationPriority::Low, 1)) {
        return 84;
    }
    // Reliable/critical traffic bypasses disposable pools, and another
    // observer has an isolated budget even when observer one is exhausted.
    if (!budgets.TryConsume(1, ReplicationPriority::Critical, 5000) ||
        !budgets.TryConsume(2, ReplicationPriority::High, 500)) {
        return 85;
    }
    budgets.UpdateObservers({ 1, 2 }, budgetStart + std::chrono::milliseconds(500));
    if (!budgets.TryConsume(1, ReplicationPriority::High, 250) ||
        budgets.TryConsume(1, ReplicationPriority::High, 1)) {
        return 86;
    }
    const auto budgetStats = budgets.StatsFor(1);
    if (budgetStats.acceptedPackets != 5 || budgetStats.deferredPackets != 4 ||
        budgetStats.acceptedBytes != 6250 || budgetStats.deferredBytes != 4) {
        return 87;
    }
    budgets.UpdateObservers({ 2 }, budgetStart + std::chrono::seconds(1));
    if (budgets.ObserverCount() != 1 || budgets.StatsFor(1).acceptedPackets != 0) return 88;
    budgets.Reset();

    ReplicationBudgetSystem queueBudgets(
        { 1000, std::chrono::milliseconds(1000), { 50, 30, 20 } });
    queueBudgets.UpdateObservers({ 9 }, budgetStart);
    ReplicationQueueSystem queue;
    queue.UpdateObservers({ 9 });
    const ReplicationStreamKey streamOne{ 1, 10, { 1, 1 }, 0 };
    const ReplicationStreamKey streamTwo{ 1, 11, { 2, 1 }, 0 };
    const ReplicationStreamKey streamThree{ 1, 12, { 3, 1 }, 0 };
    if (!queue.Enqueue(9, streamOne, ReplicationPriority::High, "old", true) ||
        !queue.Enqueue(9, streamOne, ReplicationPriority::High, "new", true) ||
        queue.PendingCount(9) != 1 || queue.StatsFor(9).coalesced != 1) {
        return 89;
    }
    std::vector<std::string> sentPayloads;
    const auto sender = [&sentPayloads](int32_t observer, const std::string& payload, bool high) {
        if (observer != 9 || !high) return false;
        sentPayloads.push_back(payload);
        return true;
    };
    if (queue.FlushObserver(9, queueBudgets, sender, 1) != 1 ||
        sentPayloads != std::vector<std::string>{ "new" }) {
        return 90;
    }

    // Requeueing the just-sent low key cannot monopolize the pool: the cursor
    // starts the next drain after it, then wraps in deterministic key order.
    queue.Enqueue(9, streamOne, ReplicationPriority::High, "one", true);
    queue.Enqueue(9, streamTwo, ReplicationPriority::High, "two", true);
    queue.Enqueue(9, streamThree, ReplicationPriority::High, "three", true);
    if (queue.FlushObserver(9, queueBudgets, sender, 1) != 1 || sentPayloads.back() != "two") return 91;
    queue.Enqueue(9, streamTwo, ReplicationPriority::High, "two-new", true);
    if (queue.FlushObserver(9, queueBudgets, sender, 1) != 1 || sentPayloads.back() != "three") return 92;

    ReplicationBudgetSystem retryBudgets(
        { 1000, std::chrono::milliseconds(1000), { 50, 30, 20 } });
    retryBudgets.UpdateObservers({ 10 }, budgetStart);
    queue.UpdateObservers({ 9, 10 });
    queue.Enqueue(10, streamOne, ReplicationPriority::High, "retry", true);
    if (queue.FlushObserver(10, retryBudgets,
                            [](int32_t, const std::string&, bool) { return false; }, 1) != 0 ||
        queue.PendingCount(10) != 1 || queue.StatsFor(10).sendRetries != 1) {
        return 93;
    }
    queue.UpdateObservers({ 9 });
    if (queue.PendingCount(10) != 0 || queue.TotalPendingCount() == 0) return 94;

    ReplicationBudgetSystem priorityBudgets(
        { 3000, std::chrono::milliseconds(1000), { 50, 30, 20 } });
    priorityBudgets.UpdateObservers({ 11 }, budgetStart);
    queue.UpdateObservers({ 9, 11 });
    queue.Enqueue(11, { 1, 1, { 1, 1 }, 0 }, ReplicationPriority::High, "high", true);
    queue.Enqueue(11, { 2, 1, { 2, 1 }, 0 }, ReplicationPriority::Normal, "normal", true);
    queue.Enqueue(11, { 3, 1, { 3, 1 }, 0 }, ReplicationPriority::Low, "low", true);
    std::vector<std::string> priorityOrder;
    if (queue.FlushObserver(
            11, priorityBudgets,
            [&priorityOrder](int32_t, const std::string& payload, bool) {
                priorityOrder.push_back(payload);
                return true;
            },
            1) != 3 ||
        priorityOrder != std::vector<std::string>{ "high", "normal", "low" }) {
        return 95;
    }
    queue.Reset();

    SpatialEntityReplicationSystem spatialReplication;
    PlayerSnapshot spatialObserverOne{};
    spatialObserverOne.entity = { 40, 1 };
    spatialObserverOne.ownerPlayerId = 20;
    spatialObserverOne.sceneId = 118;
    PlayerSnapshot spatialObserverTwo = spatialObserverOne;
    spatialObserverTwo.entity = { 41, 1 };
    spatialObserverTwo.ownerPlayerId = 21;
    spatialObserverTwo.position = { 5000.0f, 0.0f, 0.0f };
    PlayerSnapshot spatialObserverThree = spatialObserverOne;
    spatialObserverThree.entity = { 42, 1 };
    spatialObserverThree.ownerPlayerId = 22;
    spatialObserverThree.sceneId = 119;
    ReplicatedSpatialEntity nearCorpse{
        { SpatialEntityKind::Corpse, 100 }, { 50, 1 }, 100, -1, 118, { 100.0f, 0.0f, 0.0f }
    };
    ReplicatedSpatialEntity farCorpse{
        { SpatialEntityKind::Corpse, 101 }, { 51, 1 }, 101, -1, 118, { 5100.0f, 0.0f, 0.0f }
    };
    ReplicatedSpatialEntity otherSceneCorpse{
        { SpatialEntityKind::Corpse, 102 }, { 52, 1 }, 102, -1, 119, { 0.0f, 0.0f, 0.0f }
    };
    ReplicatedSpatialEntity verticallyDistantCorpse{
        { SpatialEntityKind::Corpse, 103 }, { 53, 1 }, 103, -1, 118,
        { 200.0f, 5000.0f, 0.0f }
    };
    std::vector<PlayerSnapshot> spatialPlayers{
        spatialObserverOne, spatialObserverTwo, spatialObserverThree
    };
    std::vector<ReplicatedSpatialEntity> spatialEntities{
        nearCorpse, farCorpse, otherSceneCorpse, verticallyDistantCorpse
    };
    for (int32_t index = 0; index < 100; ++index) {
        spatialEntities.push_back({
            { SpatialEntityKind::Corpse, 1000 + index },
            { static_cast<uint32_t>(1000 + index), 1 }, 1000 + index, -1, 118,
            { 100000.0f + static_cast<float>(index * 2000), 0.0f, 0.0f }
        });
    }
    auto spatialTransitions = spatialReplication.Reconcile(
        spatialEntities, spatialPlayers, { 20, 21, 22 }, 1000.0f);
    if (spatialTransitions.size() != 3 ||
        spatialTransitions[0].observerPlayerId != 20 ||
        spatialTransitions[0].subject.key != nearCorpse.key ||
        spatialTransitions[1].observerPlayerId != 21 ||
        spatialTransitions[1].subject.key != farCorpse.key ||
        spatialTransitions[2].observerPlayerId != 22 ||
        spatialTransitions[2].subject.key != otherSceneCorpse.key ||
        spatialReplication.LastCandidateCount() != 4) {
        return 96;
    }
    if (!spatialReplication.Reconcile(spatialEntities, spatialPlayers,
                                      { 20, 21, 22 }, 1000.0f).empty()) return 97;
    if (spatialReplication.ObserversFor(nearCorpse.key) !=
            std::vector<int32_t>{ 20 } ||
        spatialReplication.ObserversFor(farCorpse.key) !=
            std::vector<int32_t>{ 21 } ||
        spatialReplication.ObserversFor(otherSceneCorpse.key) !=
            std::vector<int32_t>{ 22 }) {
        return 511;
    }
    if (!spatialReplication.Reconcile(spatialEntities, spatialPlayers,
                                      { 20, 21 }, 1000.0f).empty() ||
        !spatialReplication.ObserversFor(otherSceneCorpse.key).empty()) {
        return 512;
    }
    spatialTransitions = spatialReplication.Reconcile(
        spatialEntities, spatialPlayers, { 20, 21, 22 }, 1000.0f);
    if (spatialTransitions.size() != 1 ||
        spatialTransitions[0].observerPlayerId != 22 ||
        spatialTransitions[0].subject.key != otherSceneCorpse.key ||
        spatialTransitions[0].action != SpatialEntityVisibilityAction::Enter ||
        spatialReplication.ObserversFor(otherSceneCorpse.key) !=
            std::vector<int32_t>{ 22 }) {
        return 513;
    }

    spatialEntities[0].position = { 1050.0f, 0.0f, 0.0f };
    if (!spatialReplication.Reconcile(spatialEntities, spatialPlayers,
                                      { 20, 21, 22 }, 1000.0f).empty() ||
        !spatialReplication.IsVisible(20, nearCorpse.key)) {
        return 380;
    }
    spatialEntities[0].position = { 1101.0f, 0.0f, 0.0f };
    spatialTransitions = spatialReplication.Reconcile(
        spatialEntities, spatialPlayers, { 20, 21, 22 }, 1000.0f);
    if (spatialTransitions.size() != 1 || spatialTransitions[0].lifetimeEnded ||
        spatialReplication.IsVisible(20, nearCorpse.key)) {
        return 381;
    }
    spatialEntities[0].position = { 1050.0f, 0.0f, 0.0f };
    if (!spatialReplication.Reconcile(spatialEntities, spatialPlayers,
                                      { 20, 21, 22 }, 1000.0f).empty() ||
        spatialReplication.IsVisible(20, nearCorpse.key)) {
        return 382;
    }
    spatialEntities[0].position = { 950.0f, 0.0f, 0.0f };
    spatialTransitions = spatialReplication.Reconcile(
        spatialEntities, spatialPlayers, { 20, 21, 22 }, 1000.0f);
    if (spatialTransitions.size() != 1 ||
        spatialTransitions[0].action != SpatialEntityVisibilityAction::Enter ||
        !spatialReplication.IsVisible(20, nearCorpse.key)) {
        return 383;
    }
    spatialEntities[0].position = { 100.0f, 0.0f, 0.0f };
    if (!spatialReplication.Reconcile(spatialEntities, spatialPlayers,
                                      { 20, 21, 22 }, 1000.0f).empty()) return 384;
    if (spatialReplication.NextStateSequence(-1, nearCorpse.key) != 0 ||
        spatialReplication.NextStateSequence(20, { SpatialEntityKind::Corpse, -1 }) != 0 ||
        spatialReplication.NextStateSequence(20, nearCorpse.key) != 1 ||
        spatialReplication.NextStateSequence(20, nearCorpse.key) != 2 ||
        spatialReplication.NextStateSequence(21, nearCorpse.key) != 1 ||
        spatialReplication.NextStateSequence(20, farCorpse.key) != 1) {
        return 135;
    }

    spatialPlayers[0].position = { 5000.0f, 0.0f, 0.0f };
    spatialTransitions = spatialReplication.Reconcile(
        spatialEntities, spatialPlayers, { 20, 21, 22 }, 1000.0f);
    if (spatialTransitions.size() != 2 ||
        spatialTransitions[0].action != SpatialEntityVisibilityAction::Leave ||
        spatialTransitions[0].subject.entity != nearCorpse.entity ||
        spatialTransitions[0].lifetimeEnded ||
        spatialTransitions[1].action != SpatialEntityVisibilityAction::Enter ||
        spatialTransitions[1].subject.entity != farCorpse.entity) {
        return 98;
    }

    const auto oldFarLifetime = spatialEntities[1].entity;
    spatialEntities[1].entity.generation = 2;
    spatialTransitions = spatialReplication.Reconcile(
        spatialEntities, spatialPlayers, { 20, 21, 22 }, 1000.0f);
    if (spatialTransitions.size() != 4 ||
        spatialTransitions[0].action != SpatialEntityVisibilityAction::Leave ||
        spatialTransitions[0].subject.entity != oldFarLifetime ||
        !spatialTransitions[0].lifetimeEnded ||
        spatialTransitions[1].action != SpatialEntityVisibilityAction::Enter ||
        spatialTransitions[1].subject.entity != spatialEntities[1].entity ||
        spatialTransitions[2].action != SpatialEntityVisibilityAction::Leave ||
        spatialTransitions[3].action != SpatialEntityVisibilityAction::Enter) {
        return 99;
    }
    spatialEntities.erase(spatialEntities.begin() + 1);
    spatialTransitions = spatialReplication.Reconcile(
        spatialEntities, spatialPlayers, { 20, 21, 22 }, 1000.0f);
    if (spatialTransitions.size() != 2 ||
        spatialTransitions[0].action != SpatialEntityVisibilityAction::Leave ||
        spatialTransitions[1].action != SpatialEntityVisibilityAction::Leave ||
        !spatialTransitions[0].lifetimeEnded ||
        !spatialTransitions[1].lifetimeEnded ||
        spatialReplication.IsVisible(20, farCorpse.key) ||
        spatialReplication.IsVisible(21, farCorpse.key)) {
        return 100;
    }
    spatialReplication.RemoveObserver(20);
    if (spatialReplication.NextStateSequence(20, nearCorpse.key) != 1) return 136;
    spatialReplication.RemoveObserver(20);
    if (spatialReplication.VisibleCount(20) != 0) return 101;

    spatialReplication.Reset();

    // Reliable action and disposable movement packets are independent input
    // streams. A newer movement packet may overtake a reliable press without
    // losing it, while a disconnected/stalled client cannot keep moving or
    // execute an old edge after its input stream expires.
    PlayerSimulation commandIngestion;
    commandIngestion.EnsurePlayer(70, { 118, {}, 0.0f, Game::Simulation::TeamId::Neutral });
    if (!commandIngestion.SelectWeapon(70, 1)) return 363;
    PlayerCommand pressed{};
    pressed.ownerPlayerId = 70;
    pressed.sequence = 1;
    pressed.sceneId = 118;
    pressed.moveY = 1.0f;
    pressed.actionSequence = 1;
    pressed.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    PlayerCommand newest = pressed;
    newest.sequence = 2;
    newest.actionSequence = 0;
    newest.pressedActions = 0;
    // The reliable action-bearing packet deliberately arrives second with an
    // older movement sequence. It executes once against the newer pose.
    if (!commandIngestion.SubmitCommand(newest) || !commandIngestion.SubmitCommand(pressed)) return 104;
    commandIngestion.StepFixed();
    auto ingestionSnapshot = commandIngestion.SnapshotForPlayer(70);
    if (!ingestionSnapshot || ingestionSnapshot->lastProcessedCommand != 2 ||
        ingestionSnapshot->actionState != PlayerActionState::PrimaryWindup ||
        ingestionSnapshot->position.z <= 0.0f) {
        return 105;
    }
    PlayerCommand currentPressed = newest;
    currentPressed.actionSequence = 2;
    currentPressed.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    if (!commandIngestion.SubmitCommand(currentPressed)) return 105;
    commandIngestion.StepFixed();
    ingestionSnapshot = commandIngestion.SnapshotForPlayer(70);
    if (!ingestionSnapshot ||
        ingestionSnapshot->actionState != PlayerActionState::PrimaryWindup) {
        return 105;
    }
    for (int tick = 0; tick < 5; ++tick) commandIngestion.StepFixed();
    const float positionBeforeTimeout = commandIngestion.SnapshotForPlayer(70)->position.z;
    commandIngestion.StepFixed();
    ingestionSnapshot = commandIngestion.SnapshotForPlayer(70);
    if (!ingestionSnapshot || ingestionSnapshot->position.z != positionBeforeTimeout ||
        ingestionSnapshot->velocity.x != 0.0f || ingestionSnapshot->velocity.z != 0.0f ||
        ingestionSnapshot->heldActions != 0) {
        return 106;
    }
    for (int tick = 0; tick < 8; ++tick) commandIngestion.StepFixed();
    if (commandIngestion.SnapshotForPlayer(70)->actionState != PlayerActionState::Idle) return 107;

    // A reliable edge for the latest sequence is still too old once that
    // movement window has timed out. Consume its action sequence, but never
    // execute it later when the player's recovery/idle state happens to allow it.
    PlayerCommand expiredPressed = currentPressed;
    expiredPressed.actionSequence = 3;
    if (!commandIngestion.SubmitCommand(expiredPressed)) return 422;
    commandIngestion.StepFixed();
    ingestionSnapshot = commandIngestion.SnapshotForPlayer(70);
    if (!ingestionSnapshot || ingestionSnapshot->actionState != PlayerActionState::Idle ||
        commandIngestion.SubmitCommand(expiredPressed)) {
        return 423;
    }
    PlayerCommand freshPressed = expiredPressed;
    freshPressed.sequence = 3;
    freshPressed.actionSequence = 4;
    if (!commandIngestion.SubmitCommand(freshPressed)) return 424;
    commandIngestion.StepFixed();
    ingestionSnapshot = commandIngestion.SnapshotForPlayer(70);
    if (!ingestionSnapshot ||
        ingestionSnapshot->actionState != PlayerActionState::PrimaryWindup) {
        return 425;
    }
    if (!commandIngestion.RespawnPlayer(70) || commandIngestion.SubmitCommand(newest) ||
        commandIngestion.SubmitCommand(pressed)) {
        return 108;
    }

    // A real death advances lifeEpoch and creates a fresh command namespace.
    // Sequence one from the new incarnation must not be rejected against the
    // previous life's movement or reliable-action replay floors.
    if (!commandIngestion.ApplyDamage(-1, 70, 48, 0)) return 392;
    for (uint32_t tick = 0; tick < 150; ++tick) commandIngestion.StepFixed();
    const auto respawnedCommandPlayer = commandIngestion.SnapshotForPlayer(70);
    if (!respawnedCommandPlayer || respawnedCommandPlayer->lifeEpoch != 2 ||
        respawnedCommandPlayer->lastProcessedCommand != 0 ||
        respawnedCommandPlayer->heldActions != 0) {
        return 393;
    }
    PlayerCommand newLifeCommand = pressed;
    newLifeCommand.sequence = 1;
    newLifeCommand.actionSequence = 1;
    newLifeCommand.lifeEpoch = 2;
    if (!commandIngestion.SubmitCommand(newLifeCommand)) return 394;
    commandIngestion.StepFixed();
    const auto acceptedNewLifeCommand = commandIngestion.SnapshotForPlayer(70);
    if (!acceptedNewLifeCommand || acceptedNewLifeCommand->lastProcessedCommand != 1) {
        return 395;
    }

    // Prediction observes the first reliable edge immediately. When multiple
    // edges reach authority before one fixed tick, that first edge must win as
    // well; later edges are consumed on the same tick rather than replacing it
    // or becoming a delayed action.
    PlayerSimulation orderedActionEdges;
    orderedActionEdges.EnsurePlayer(71, { 118, {}, 0.0f, Game::Simulation::TeamId::Neutral });
    if (!orderedActionEdges.SelectWeapon(71, 1)) return 350;
    PlayerCommand pendingPrimary = pressed;
    pendingPrimary.ownerPlayerId = 71;
    pendingPrimary.sequence = 1;
    pendingPrimary.actionSequence = 1;
    PlayerCommand pendingEvade = pendingPrimary;
    pendingEvade.sequence = 2;
    pendingEvade.actionSequence = 2;
    pendingEvade.pressedActions = Game::Simulation::PLAYER_ACTION_EVADE;
    if (!orderedActionEdges.SubmitCommand(pendingPrimary) ||
        !orderedActionEdges.SubmitCommand(pendingEvade)) {
        return 108;
    }
    orderedActionEdges.StepFixed();
    auto orderedActionSnapshot = orderedActionEdges.SnapshotForPlayer(71);
    if (!orderedActionSnapshot || orderedActionSnapshot->lastProcessedCommand != 2 ||
        orderedActionSnapshot->actionState != PlayerActionState::PrimaryWindup ||
        !NearlyEqual(orderedActionSnapshot->velocity.x, 0.0f) ||
        !NearlyEqual(orderedActionSnapshot->velocity.z, 180.0f) ||
        !NearlyEqual(orderedActionSnapshot->position.z, 6.0f)) {
        return 351;
    }
    // Another attack pressed while primary owns the action state is consumed
    // on that command tick. It must not execute after recovery ends.
    PlayerCommand busyPrimary = pendingPrimary;
    busyPrimary.sequence = 3;
    busyPrimary.actionSequence = 3;
    if (!orderedActionEdges.SubmitCommand(busyPrimary)) return 352;
    orderedActionEdges.StepFixed();
    for (int tick = 0; tick < 11; ++tick) orderedActionEdges.StepFixed();
    orderedActionSnapshot = orderedActionEdges.SnapshotForPlayer(71);
    if (!orderedActionSnapshot || orderedActionSnapshot->actionState != PlayerActionState::Idle ||
        orderedActionSnapshot->lastProcessedCommand != 3) {
        return 353;
    }

    PlayerSimulation sideEvade;
    sideEvade.EnsurePlayer(72, { 118, {}, 0.0f, Game::Simulation::TeamId::Neutral });
    PlayerCommand sideHop = pendingEvade;
    sideHop.ownerPlayerId = 72;
    sideHop.moveX = -1.0f;
    sideHop.moveY = 0.0f;
    if (!sideEvade.SubmitCommand(sideHop)) return 354;
    sideEvade.StepFixed();
    const auto sideSnapshot = sideEvade.SnapshotForPlayer(72);
    if (!sideSnapshot || sideSnapshot->actionState != PlayerActionState::Evading ||
        !NearlyEqual(sideSnapshot->velocity.x, -170.0f) ||
        !NearlyEqual(sideSnapshot->velocity.z, 0.0f) || sideSnapshot->position.x >= 0.0f) {
        return 355;
    }

    const auto runBodyCollision = [](bool reverseCreation) {
        PlayerSimulation bodySimulation;
        if (reverseCreation) {
            bodySimulation.EnsurePlayer(81, { 118, { 0.0f, 0.0f, 29.0f }, 0.0f });
            bodySimulation.EnsurePlayer(80, { 118, {}, 0.0f });
        } else {
            bodySimulation.EnsurePlayer(80, { 118, {}, 0.0f });
            bodySimulation.EnsurePlayer(81, { 118, { 0.0f, 0.0f, 29.0f }, 0.0f });
        }
        PlayerCommand advance{};
        advance.ownerPlayerId = 80;
        advance.sequence = 1;
        advance.sceneId = 118;
        advance.moveY = 1.0f;
        bodySimulation.SubmitCommand(advance);
        for (int tick = 0; tick < 3; ++tick) bodySimulation.StepFixed();
        return std::pair{ *bodySimulation.SnapshotForPlayer(80),
                          *bodySimulation.SnapshotForPlayer(81) };
    };
    const auto bodyForward = runBodyCollision(false);
    const auto bodyReverse = runBodyCollision(true);
    const float bodyDistance = std::hypot(bodyForward.second.position.x - bodyForward.first.position.x,
                                          bodyForward.second.position.z - bodyForward.first.position.z);
    if (bodyDistance < 23.99f || bodyForward.first.position.z >= 5.01f ||
        !NearlyEqual(bodyForward.second.position.z, 29.0f)) {
        return 111;
    }
    if (!NearlyEqual(bodyForward.first.position.x, bodyReverse.first.position.x) ||
        !NearlyEqual(bodyForward.first.position.z, bodyReverse.first.position.z) ||
        !NearlyEqual(bodyForward.second.position.x, bodyReverse.second.position.x) ||
        !NearlyEqual(bodyForward.second.position.z, bodyReverse.second.position.z)) {
        return 112;
    }

    CombatReplicationSystem combatReplication;
    PlayerSnapshot combatSource{};
    combatSource.entity = { 90, 1 };
    combatSource.ownerPlayerId = 90;
    combatSource.sceneId = 118;
    PlayerSnapshot combatTarget = combatSource;
    combatTarget.entity = { 91, 1 };
    combatTarget.ownerPlayerId = 91;
    combatTarget.position.z = 40.0f;
    PlayerSnapshot distantCombatObserver = combatSource;
    distantCombatObserver.entity = { 92, 1 };
    distantCombatObserver.ownerPlayerId = 92;
    distantCombatObserver.position.x = 500.0f;
    PlayerSnapshot otherSceneObserver = combatSource;
    otherSceneObserver.entity = { 93, 1 };
    otherSceneObserver.ownerPlayerId = 93;
    otherSceneObserver.sceneId = 119;
    PlayerSnapshot combatWitness = combatSource;
    combatWitness.entity = { 94, 1 };
    combatWitness.ownerPlayerId = 94;
    combatWitness.position.z = 60.0f;
    Game::Simulation::CombatResultEvent replicatedBlock{
        1, 90, 91, combatSource.entity, combatTarget.entity, 118,
        Game::Simulation::CombatAttackKind::Arrow,
        Game::Simulation::CombatResultKind::Blocked, 0, 0,
        { 0.0f, 30.0f, 20.0f }
    };
    std::vector<PlayerSnapshot> combatPlayers{
        combatSource, combatTarget, distantCombatObserver, otherSceneObserver,
        combatWitness
    };
    PlayerReplicationSystem combatInterest;
    combatInterest.Reconcile(combatPlayers, { 90, 91, 92, 93, 94 }, 100.0f);
    auto combatBatches = combatReplication.BuildBatches(
        { replicatedBlock }, combatPlayers, combatInterest);
    if (combatBatches.size() != 1 ||
        combatBatches.front().observers != std::vector<int32_t>({ 90, 91, 94 }) ||
        combatBatches.front().result.targetEntity != combatTarget.entity) {
        return 115;
    }
    combatPlayers.back().sceneId = 119;
    combatInterest.Reconcile(combatPlayers, { 90, 91, 92, 93, 94 }, 100.0f);
    combatBatches = combatReplication.BuildBatches(
        { replicatedBlock }, combatPlayers, combatInterest);
    if (combatBatches.size() != 1 ||
        combatBatches.front().observers != std::vector<int32_t>({ 90, 91 })) {
        return 392;
    }
    combatPlayers[1].health = 0;
    combatInterest.Reconcile(combatPlayers, { 90, 91, 92, 93, 94 }, 100.0f);
    combatBatches = combatReplication.BuildBatches(
        { replicatedBlock }, combatPlayers, combatInterest);
    if (combatBatches.size() != 1 ||
        combatBatches.front().observers != std::vector<int32_t>({ 90, 91 })) {
        return 393;
    }
    combatInterest.Reconcile(combatPlayers, { 91, 92, 93, 94 }, 100.0f);
    combatBatches = combatReplication.BuildBatches(
        { replicatedBlock }, combatPlayers, combatInterest);
    if (combatBatches.size() != 1 ||
        combatBatches.front().observers != std::vector<int32_t>({ 91 })) {
        return 394;
    }
    combatInterest.Reconcile(combatPlayers, { 90, 91, 92, 93, 94 }, 100.0f);
    replicatedBlock.targetEntity.generation = 2;
    if (!combatReplication.BuildBatches(
             { replicatedBlock }, combatPlayers, combatInterest).empty()) {
        return 116;
    }
    replicatedBlock.targetEntity = combatTarget.entity;
    replicatedBlock.damage = 1;
    if (!combatReplication.BuildBatches(
             { replicatedBlock }, combatPlayers, combatInterest).empty()) {
        return 117;
    }
    replicatedBlock.damage = 0;
    replicatedBlock.eventId = 0;
    if (!combatReplication.BuildBatches(
             { replicatedBlock }, combatPlayers, combatInterest).empty()) {
        return 117;
    }

    SceneTransitionAuthority sceneAuthority(128, 110);
    if (!sceneAuthority.ConfigureSpawn({ 110, {}, 0.0f }) ||
        !sceneAuthority.ConfigureSpawn({ 73, { 666.0f, -87.0f, 354.0f }, 1.0f }) ||
        sceneAuthority.ConfigureSpawn({ 128, {}, 0.0f })) {
        return 118;
    }
    const auto bootstrapScene = sceneAuthority.Evaluate(5, 1, 1, true);
    const auto ungrantedScene = sceneAuthority.Evaluate(5, 2, 1, false);
    if (!bootstrapScene ||
        bootstrapScene->result != Game::Simulation::SceneEntryResult::Accepted ||
        !bootstrapScene->spawn || bootstrapScene->spawn->sceneId != 110 ||
        !ungrantedScene ||
        ungrantedScene->result != Game::Simulation::SceneEntryResult::Rejected ||
        !sceneAuthority.Grant(5, 1, 73)) {
        return 119;
    }
    const auto acceptedScene = sceneAuthority.Evaluate(5, 3, 1, false);
    if (!acceptedScene ||
        acceptedScene->result != Game::Simulation::SceneEntryResult::Accepted ||
        !acceptedScene->spawn || acceptedScene->spawn->sceneId != 73 ||
        !NearlyEqual(acceptedScene->spawn->position.x, 666.0f)) {
        return 119;
    }
    if (!sceneAuthority.Grant(5, 2, 73)) return 120;
    const auto rejectedScene = sceneAuthority.Evaluate(5, 4, 1, false);
    if (!rejectedScene ||
        rejectedScene->result != Game::Simulation::SceneEntryResult::Rejected ||
        rejectedScene->spawn || !rejectedScene->fallbackSpawn ||
        rejectedScene->fallbackSpawn->sceneId != 110) {
        return 120;
    }
    if (sceneAuthority.Grant(5, 1, 100)) return 121;
    sceneAuthority.Reset();
    const auto noFallback = sceneAuthority.Evaluate(6, 1, 1, true);
    if (!noFallback || noFallback->result != Game::Simulation::SceneEntryResult::Rejected ||
        noFallback->fallbackSpawn) {
        return 122;
    }

    ServerWorld sceneWorld;
    if (!sceneWorld.ConfigureSceneSpawn({ 110, {}, 0.0f })) return 328;
    const auto fallbackEntry = sceneWorld.ExecuteSceneEntry({ 601, 1, 1 });
    if (!fallbackEntry || !fallbackEntry->accepted || !fallbackEntry->admitted ||
        !fallbackEntry->player || fallbackEntry->player->sceneId != 110 ||
        !sceneWorld.ConfigureSceneSpawn(
            { 73, { 666.0f, -87.0f, 354.0f }, 1.0f })) {
        return 329;
    }
    const auto replayedFallback = sceneWorld.ExecuteSceneEntry({ 601, 1, 1 });
    if (!replayedFallback || !replayedFallback->accepted || replayedFallback->changedScene ||
        !replayedFallback->player || replayedFallback->player->sceneId != 110) return 329;
    const auto ungrantedEntry = sceneWorld.ExecuteSceneEntry({ 601, 2, 1 });
    if (!ungrantedEntry || ungrantedEntry->accepted || ungrantedEntry->changedScene ||
        !sceneWorld.AuthorizeSceneTransition(601, 73)) return 330;
    const auto aggregateEntry = sceneWorld.ExecuteSceneEntry({ 601, 3, 1 });
    if (!aggregateEntry || !aggregateEntry->accepted || aggregateEntry->admitted ||
        !aggregateEntry->changedScene || !aggregateEntry->player ||
        aggregateEntry->player->sceneId != 73 ||
        !NearlyEqual(aggregateEntry->player->position.x, 666.0f)) {
        return 330;
    }
    const auto replayedEntry = sceneWorld.ExecuteSceneEntry({ 601, 3, 1 });
    if (!replayedEntry || !replayedEntry->accepted || replayedEntry->changedScene ||
        !replayedEntry->player || replayedEntry->player->sceneId != 73) return 330;
    if (!ServerWorldTestAccess::Players(sceneWorld).ApplyDamage(-1, 601, 48, 0)) {
        return 351;
    }
    for (int32_t tick = 0; tick < 150; ++tick) {
        ServerWorldTestAccess::Players(sceneWorld).StepFixed();
    }
    const auto sceneRespawned = sceneWorld.PlayerFor(601);
    if (!sceneRespawned || sceneRespawned->lifeEpoch != 2 ||
        sceneWorld.ExecuteSceneEntry({ 601, 4, 1 })) {
        return 352;
    }
    const auto noGrantCurrentLife = sceneWorld.ExecuteSceneEntry({ 601, 4, 2 });
    if (!noGrantCurrentLife || noGrantCurrentLife->accepted ||
        !sceneWorld.AuthorizeSceneTransition(601, 110)) return 353;
    const auto currentLifeEntry = sceneWorld.ExecuteSceneEntry({ 601, 5, 2 });
    if (!currentLifeEntry || !currentLifeEntry->accepted ||
        !currentLifeEntry->changedScene || !currentLifeEntry->player ||
        currentLifeEntry->player->sceneId != 110) {
        return 353;
    }

    EntityLifetimeRegistry lifetimes;
    if (!lifetimes.Establish(7, { 17, 1 }) || !lifetimes.Matches(7, { 17, 1 }) ||
        lifetimes.Retire(7, { 17, 2 }) || lifetimes.Size() != 1) {
        return 123;
    }
    if (!lifetimes.Establish(7, { 17, 2 }) || lifetimes.Matches(7, { 17, 1 }) ||
        !lifetimes.Matches(7, { 17, 2 })) {
        return 124;
    }
    if (!lifetimes.Retire(7, { 17, 2 }) || lifetimes.Size() != 0 ||
        lifetimes.Establish(-1, { 17, 3 }) || lifetimes.Establish(7, {})) {
        return 125;
    }

    Game::Replication::ProjectileLifetimeRegistry projectileLifetimes;
    const Game::Replication::ProjectileLogicalId logicalArrow{ 7, 41, 0 };
    if (!projectileLifetimes.Establish(logicalArrow, { 21, 1 }) ||
        !projectileLifetimes.Matches(logicalArrow, { 21, 1 }) ||
        projectileLifetimes.Retire(logicalArrow, { 21, 2 })) {
        return 126;
    }
    if (!projectileLifetimes.Establish(logicalArrow, { 21, 2 }) ||
        projectileLifetimes.Matches(logicalArrow, { 21, 1 }) ||
        !projectileLifetimes.Retire(logicalArrow, { 21, 2 }) ||
        projectileLifetimes.Size() != 0) {
        return 127;
    }
    if (projectileLifetimes.Establish({ -1, 41, 0 }, { 21, 3 }) ||
        projectileLifetimes.Establish({ 7, 0, 0 }, { 21, 3 }) ||
        projectileLifetimes.Establish(logicalArrow, {})) {
        return 128;
    }

    ServerReplicationCoordinator coordinator(
        { 1000, std::chrono::milliseconds(1000), { 50, 30, 20 } });
    const auto coordinatorStart = ServerReplicationCoordinator::Clock::now();
    coordinator.UpdateObservers({ 201, 202 }, coordinatorStart);
    PlayerSnapshot coordinatorOne{};
    coordinatorOne.entity = { 201, 1 };
    coordinatorOne.ownerPlayerId = 201;
    coordinatorOne.sceneId = 118;
    PlayerSnapshot coordinatorTwo = coordinatorOne;
    coordinatorTwo.entity = { 202, 1 };
    coordinatorTwo.ownerPlayerId = 202;
    coordinatorTwo.position.x = 100.0f;
    const std::vector<PlayerSnapshot> coordinatorPlayers{ coordinatorOne, coordinatorTwo };
    const auto coordinatorPlayerTransitions =
        coordinator.ReconcilePlayers(coordinatorPlayers, { 201, 202 }, 1000.0f);
    if (coordinatorPlayerTransitions.size() != 2 ||
        !coordinator.PlayerVisible(201, 202) || !coordinator.PlayerVisible(202, 201) ||
        coordinator.PlayerObservers(201) != std::vector<int32_t>{ 202 } ||
        coordinator.PlayerObservers(202) != std::vector<int32_t>{ 201 } ||
        coordinator.TotalPlayerVisibilityCount() != 2) {
        return 129;
    }

    const ReplicatedOwnedEntity coordinatorArrow{
        { OwnedEntityKind::Arrow, 202, 77 }, { 301, 1 }, 118,
        { 100.0f, 0.0f, 0.0f }, false
    };
    const auto coordinatorOwnedTransitions =
        coordinator.ReconcileOwnedEntities(
            { coordinatorArrow }, coordinatorPlayers, { 201, 202 }, 1000.0f);
    const ReplicatedSpatialEntity coordinatorCorpse{
        { SpatialEntityKind::Corpse, 88 }, { 302, 1 }, 88, -1, 118,
        { 50.0f, 0.0f, 0.0f }
    };
    const auto coordinatorSpatialTransitions = coordinator.ReconcileSpatialEntities(
        { coordinatorCorpse }, coordinatorPlayers, { 201, 202 }, 1000.0f);
    if (coordinatorOwnedTransitions.size() != 1 ||
        !coordinator.OwnedEntityVisible(201, coordinatorArrow.key) ||
        coordinator.OwnedEntityVisible(202, coordinatorArrow.key) ||
        coordinatorSpatialTransitions.size() != 2 ||
        !coordinator.SpatialEntityVisible(201, coordinatorCorpse.key) ||
        !coordinator.SpatialEntityVisible(202, coordinatorCorpse.key) ||
        coordinator.SpatialEntityObservers(coordinatorCorpse.key) !=
            std::vector<int32_t>({ 201, 202 }) ||
        coordinator.NextOwnedEntityStateSequence(201, coordinatorArrow.key) != 1 ||
        coordinator.NextOwnedEntityStateSequence(201, coordinatorArrow.key) != 2 ||
        coordinator.NextOwnedEntityStateSequence(202, coordinatorArrow.key) != 1) {
        return 130;
    }

    // Scene changes must reconcile all three interest graphs in the same
    // server operation. No periodic player snapshot should be needed to retire
    // stale actors/entities or introduce them again after re-entry.
    std::vector<PlayerSnapshot> movedCoordinatorPlayers = coordinatorPlayers;
    movedCoordinatorPlayers[0].sceneId = 119;
    const auto scenePlayerLeaves = coordinator.ReconcilePlayers(
        movedCoordinatorPlayers, { 201, 202 }, 1000.0f);
    const auto sceneOwnedLeaves = coordinator.ReconcileOwnedEntities(
        { coordinatorArrow }, movedCoordinatorPlayers, { 201, 202 }, 1000.0f);
    const auto sceneSpatialLeaves = coordinator.ReconcileSpatialEntities(
        { coordinatorCorpse }, movedCoordinatorPlayers, { 201, 202 }, 1000.0f);
    if (scenePlayerLeaves.size() != 2 || sceneOwnedLeaves.size() != 1 ||
        sceneOwnedLeaves.front().action != OwnedEntityVisibilityAction::Leave ||
        sceneSpatialLeaves.size() != 1 ||
        sceneSpatialLeaves.front().action != SpatialEntityVisibilityAction::Leave ||
        coordinator.PlayerVisible(201, 202) || coordinator.PlayerVisible(202, 201) ||
        !coordinator.PlayerObservers(201).empty() ||
        !coordinator.PlayerObservers(202).empty() ||
        coordinator.OwnedEntityVisible(201, coordinatorArrow.key) ||
        coordinator.SpatialEntityVisible(201, coordinatorCorpse.key) ||
        coordinator.SpatialEntityObservers(coordinatorCorpse.key) !=
            std::vector<int32_t>{ 202 }) {
        return 170;
    }
    const auto scenePlayerEnters = coordinator.ReconcilePlayers(
        coordinatorPlayers, { 201, 202 }, 1000.0f);
    const auto sceneOwnedEnters = coordinator.ReconcileOwnedEntities(
        { coordinatorArrow }, coordinatorPlayers, { 201, 202 }, 1000.0f);
    const auto sceneSpatialEnters = coordinator.ReconcileSpatialEntities(
        { coordinatorCorpse }, coordinatorPlayers, { 201, 202 }, 1000.0f);
    if (scenePlayerEnters.size() != 2 || sceneOwnedEnters.size() != 1 ||
        sceneOwnedEnters.front().action != OwnedEntityVisibilityAction::Enter ||
        sceneSpatialEnters.size() != 1 ||
        sceneSpatialEnters.front().action != SpatialEntityVisibilityAction::Enter ||
        !coordinator.PlayerVisible(201, 202) || !coordinator.PlayerVisible(202, 201) ||
        coordinator.PlayerObservers(201) != std::vector<int32_t>{ 202 } ||
        coordinator.PlayerObservers(202) != std::vector<int32_t>{ 201 } ||
        !coordinator.OwnedEntityVisible(201, coordinatorArrow.key) ||
        !coordinator.SpatialEntityVisible(201, coordinatorCorpse.key) ||
        coordinator.SpatialEntityObservers(coordinatorCorpse.key) !=
            std::vector<int32_t>({ 201, 202 })) {
        return 171;
    }

    const ReplicationStreamKey coordinatorStream{ 5, 202, coordinatorArrow.entity, 77 };
    if (coordinator.Submit(201, coordinatorStream, ReplicationPriority::High,
                           "old-state", true, false, 64) != ReplicationSubmission::Queued ||
        coordinator.Submit(201, coordinatorStream, ReplicationPriority::High,
                           "new-state", true, false, 64) != ReplicationSubmission::Queued ||
        coordinator.PendingCount(201) != 1 ||
        coordinator.QueueStatsFor(201).coalesced != 1 ||
        coordinator.Submit(201, {}, ReplicationPriority::Critical,
                           "reliable", true, true, 64) != ReplicationSubmission::SendNow) {
        return 131;
    }
    std::vector<std::string> coordinatorPayloads;
    if (coordinator.Flush(
            { 201, 202 },
            [&coordinatorPayloads](int32_t observer, const std::string& payload, bool high) {
                if (observer != 201 || !high) return false;
                coordinatorPayloads.push_back(payload);
                return true;
            }) != 1 || coordinatorPayloads != std::vector<std::string>{ "new-state" }) {
        return 132;
    }

    coordinator.Submit(201, coordinatorStream, ReplicationPriority::High,
                       "pending", true, false, 64);
    const auto coordinatorDeparture = coordinator.RemovePlayer(201);
    if (coordinatorDeparture.playerLeaves.size() != 1 ||
        coordinatorDeparture.playerLeaves.front().observerPlayerId != 202 ||
        coordinatorDeparture.playerLeaves.front().subject.playerId != 201 ||
        coordinator.PendingCount(201) != 0 || coordinator.ObserverCount() != 1 ||
        coordinator.PlayerVisible(202, 201) ||
        !coordinator.PlayerObservers(201).empty() ||
        !coordinator.PlayerObservers(202).empty() ||
        coordinator.OwnedEntityVisible(201, coordinatorArrow.key) ||
        coordinator.SpatialEntityVisible(201, coordinatorCorpse.key) ||
        coordinator.SpatialEntityObservers(coordinatorCorpse.key) !=
            std::vector<int32_t>{ 202 }) {
        return 133;
    }
    coordinator.RemovePlayer(202);
    if (coordinator.ObserverCount() != 0 || coordinator.TotalPendingCount() != 0 ||
        coordinator.TotalPlayerVisibilityCount() != 0 ||
        coordinator.TotalOwnedVisibilityCount() != 0 ||
        coordinator.TotalSpatialVisibilityCount() != 0 ||
        !coordinator.SpatialEntityObservers(coordinatorCorpse.key).empty()) {
        return 134;
    }

    ServerReplicationCoordinator reverseFanout;
    PlayerSnapshot fanoutSubject{};
    fanoutSubject.ownerPlayerId = 300;
    fanoutSubject.entity = { 300, 1 };
    fanoutSubject.sceneId = 118;
    PlayerSnapshot fanoutNearOne = fanoutSubject;
    fanoutNearOne.ownerPlayerId = 301;
    fanoutNearOne.entity = { 301, 1 };
    fanoutNearOne.position.x = 10.0f;
    PlayerSnapshot fanoutNearTwo = fanoutSubject;
    fanoutNearTwo.ownerPlayerId = 302;
    fanoutNearTwo.entity = { 302, 1 };
    fanoutNearTwo.position.x = 20.0f;
    PlayerSnapshot fanoutOtherScene = fanoutSubject;
    fanoutOtherScene.ownerPlayerId = 303;
    fanoutOtherScene.entity = { 303, 1 };
    fanoutOtherScene.sceneId = 119;
    std::vector<PlayerSnapshot> fanoutPlayers{
        fanoutSubject, fanoutNearOne, fanoutNearTwo, fanoutOtherScene
    };
    reverseFanout.ReconcilePlayers(fanoutPlayers, { 300, 301, 302, 303 },
                                   1000.0f);
    if (reverseFanout.PlayerObservers(300) !=
            std::vector<int32_t>({ 301, 302 }) ||
        !reverseFanout.PlayerObservers(303).empty()) {
        return 389;
    }
    fanoutPlayers[2].sceneId = 119;
    reverseFanout.ReconcilePlayers(fanoutPlayers, { 300, 301, 302, 303 },
                                   1000.0f);
    if (reverseFanout.PlayerObservers(300) != std::vector<int32_t>({ 301 })) {
        return 390;
    }
    reverseFanout.RemovePlayer(300);
    if (!reverseFanout.PlayerObservers(300).empty()) return 391;

    ServerIntentAdmission intentAdmission;
    if (intentAdmission.Admit(301, 1, 1, ServerIntentKind::Projectile, 0xFFFFFFFEU) !=
            ServerIntentResult::Fresh ||
        intentAdmission.Admit(301, 1, 1, ServerIntentKind::Projectile, 0xFFFFFFFEU) !=
            ServerIntentResult::Duplicate ||
        intentAdmission.Admit(301, 1, 1, ServerIntentKind::Projectile, 0xFFFFFFFDU) !=
            ServerIntentResult::Stale ||
        intentAdmission.Admit(301, 1, 1, ServerIntentKind::Projectile, 0xFFFFFFFFU) !=
            ServerIntentResult::Fresh ||
        intentAdmission.Admit(301, 1, 1, ServerIntentKind::Projectile, 0U) !=
            ServerIntentResult::Fresh ||
        intentAdmission.Admit(301, 1, 1, ServerIntentKind::Fish, 0xFFFFFFFEU) !=
            ServerIntentResult::Fresh ||
        intentAdmission.Admit(-1, 1, 1, ServerIntentKind::Lure, 1) !=
            ServerIntentResult::Invalid ||
        intentAdmission.Admit(301, 1, 2, ServerIntentKind::WeaponSelection, 1) !=
            ServerIntentResult::Invalid) {
        return 135;
    }
    if (!intentAdmission.CooldownReady(301, ServerIntentKind::Projectile, 100) ||
        intentAdmission.CooldownReady(-1, ServerIntentKind::Projectile, 100)) {
        return 136;
    }
    intentAdmission.RecordAccepted(301, ServerIntentKind::Projectile, 100);
    if (intentAdmission.CooldownReady(301, ServerIntentKind::Projectile, 108) ||
        !intentAdmission.CooldownReady(301, ServerIntentKind::Projectile, 109) ||
        !intentAdmission.CooldownReady(301, ServerIntentKind::Structure, 100)) {
        return 137;
    }
    intentAdmission.RecordAccepted(301, ServerIntentKind::Structure, 100);
    if (intentAdmission.CooldownReady(301, ServerIntentKind::Structure, 114) ||
        !intentAdmission.CooldownReady(301, ServerIntentKind::Structure, 115) ||
        !intentAdmission.CooldownReady(301, ServerIntentKind::Fish, 100)) {
        return 137;
    }
    if (intentAdmission.Admit(301, 2, 1, ServerIntentKind::Projectile, 0x7FFFFFFFU) !=
            ServerIntentResult::Invalid ||
        intentAdmission.Admit(301, 2, 2, ServerIntentKind::Projectile, 1) !=
            ServerIntentResult::Fresh ||
        !intentAdmission.CooldownReady(301, ServerIntentKind::Projectile, 100) ||
        intentAdmission.Admit(301, 2, 1, ServerIntentKind::Projectile, 0x7FFFFFFFU) !=
            ServerIntentResult::Invalid ||
        intentAdmission.Admit(301, 2, 2, ServerIntentKind::Projectile, 2) !=
            ServerIntentResult::Fresh) {
        return 137;
    }
    intentAdmission.RemovePlayer(301);
    if (intentAdmission.Admit(301, 1, 1, ServerIntentKind::Projectile, 0U) !=
            ServerIntentResult::Fresh ||
        !intentAdmission.CooldownReady(301, ServerIntentKind::Projectile, 100)) {
        return 138;
    }
    intentAdmission.RecordAccepted(301, ServerIntentKind::Projectile, 100);
    intentAdmission.Reset();
    if (!intentAdmission.CooldownReady(301, ServerIntentKind::Projectile, 100) ||
        intentAdmission.Admit(301, 1, 1, ServerIntentKind::Projectile, 0U) !=
            ServerIntentResult::Fresh) {
        return 139;
    }

    ServerWorld deathWorld;
    const auto presentedPlayer = deathWorld.AdmitPlayer(
        401, { 118, { 10.0f, 20.0f, 30.0f }, 0.5f });
    if (!presentedPlayer) return 140;
    PlayerCommand presentedWeapon{};
    presentedWeapon.ownerPlayerId = 401;
    presentedWeapon.sequence = 1;
    presentedWeapon.sceneId = 118;
    if (!deathWorld.ExecuteWeaponSelection(
            { 401, 1, presentedPlayer->lifeEpoch, 2 }) ||
        !deathWorld.SubmitPlayerCommand(presentedWeapon)) return 143;
    ServerWorldTestAccess::Players(deathWorld).StepFixed();
    if (!ServerWorldTestAccess::Players(deathWorld).ApplyDamage(-1, 401, 48, 0)) return 143;
    const auto presentedDeath = deathWorld.DrainPlayerLifeEvents();
    const auto deathCorpses = deathWorld.CorpseSnapshots();
    if (presentedDeath.size() != 1 ||
        presentedDeath.front().kind != Game::Simulation::PlayerLifeEventKind::Died ||
        presentedDeath.front().entity != presentedPlayer->entity ||
        presentedDeath.front().lifeEpoch != presentedPlayer->lifeEpoch ||
        deathCorpses.size() != 1 ||
        deathCorpses.front().pose.sourcePlayerEntity != presentedPlayer->entity ||
        deathCorpses.front().pose.sourceLifeEpoch != presentedPlayer->lifeEpoch) {
        return 144;
    }
    for (int tick = 0; tick < 150; ++tick) {
        ServerWorldTestAccess::Players(deathWorld).StepFixed();
    }
    const auto presentedRespawn = deathWorld.DrainPlayerLifeEvents();
    const auto presentationCorpses = deathWorld.CorpseSnapshots();
    if (presentedRespawn.size() != 1 ||
        presentedRespawn.front().kind != Game::Simulation::PlayerLifeEventKind::Respawned ||
        presentedRespawn.front().lifeEpoch != 2 ||
        presentationCorpses.size() != 1 ||
        presentationCorpses.front().pose.sourcePlayerId != 401 ||
        presentationCorpses.front().pose.roomId != -1 ||
        presentationCorpses.front().pose.selectedWeapon != 2) {
        return 146;
    }
    deathWorld.RemovePlayer(401);
    if (deathWorld.PlayerFor(401) || deathWorld.CorpseSnapshots().size() != 1) return 147;

    const auto replacementPlayer = deathWorld.AdmitPlayer(
        401, { 118, { 40.0f, 20.0f, 30.0f }, 1.0f });
    if (!replacementPlayer || replacementPlayer->entity == presentedPlayer->entity ||
        !ServerWorldTestAccess::Players(deathWorld).ApplyDamage(-1, 401, 48, 0)) {
        return 147;
    }
    const auto replacementDeath = deathWorld.DrainPlayerLifeEvents();
    const auto reincarnationCorpses = deathWorld.CorpseSnapshots();
    if (replacementDeath.size() != 1 || reincarnationCorpses.size() != 2 ||
        reincarnationCorpses.back().pose.sourcePlayerEntity != replacementPlayer->entity ||
        reincarnationCorpses.back().pose.sourceLifeEpoch != replacementPlayer->lifeEpoch) {
        return 147;
    }

    ServerWorld fishingPresentationWorld;
    fishingPresentationWorld.SetFishingCollisionQuery(
        [](int32_t, const Game::Simulation::Vec3&,
           const Game::Simulation::Vec3& to,
           Game::Simulation::Vec3& impact) {
            impact = to;
            return true;
        });
    ServerReplicationCoordinator fishingPresentationRelay;
    const auto fishingPlayer = fishingPresentationWorld.AdmitPlayer(
        501, { 118, { 10.0f, 20.0f, 30.0f }, 0.0f });
    if (!fishingPlayer) return 148;
    PlayerCommand equipFishing{};
    equipFishing.ownerPlayerId = 501;
    equipFishing.sequence = 1;
    equipFishing.sceneId = 118;
    if (!fishingPresentationWorld.ExecuteWeaponSelection(
            { 501, 1, fishingPlayer->lifeEpoch, 4 }) ||
        !fishingPresentationWorld.SubmitPlayerCommand(equipFishing)) return 149;
    ServerWorldTestAccess::Players(fishingPresentationWorld).StepFixed();
    Game::Replication::FishingPresentationState fishingPose{};
    fishingPose.playerId = 501;
    fishingPose.entity = fishingPlayer->entity;
    fishingPose.sceneId = 118;
    fishingPose.sequence = 10;
    fishingPose.state = 2;
    fishingPose.rodTipOffset = { 1.0f, 2.0f, 3.0f };
    if (fishingPresentationRelay.UpdateFishingPresentation(
            fishingPose, *fishingPresentationWorld.PlayerFor(501)) !=
            Game::Replication::FishingPresentationUpdateResult::Accepted ||
        !fishingPresentationRelay.FishingPresentationFor(501)) {
        return 150;
    }
    fishingPose.rodTipOffset[0] = 4.0f;
    if (fishingPresentationRelay.UpdateFishingPresentation(
            fishingPose, *fishingPresentationWorld.PlayerFor(501)) !=
            Game::Replication::FishingPresentationUpdateResult::Accepted ||
        fishingPresentationRelay.FishingPresentationFor(501)->rodTipOffset[0] != 4.0f) {
        return 151;
    }
    fishingPose.sequence = 9;
    if (fishingPresentationRelay.UpdateFishingPresentation(
            fishingPose, *fishingPresentationWorld.PlayerFor(501)) !=
            Game::Replication::FishingPresentationUpdateResult::Stale) {
        return 152;
    }
    fishingPose.sequence = 11;
    ++fishingPose.entity.generation;
    if (fishingPresentationRelay.UpdateFishingPresentation(
            fishingPose, *fishingPresentationWorld.PlayerFor(501)) !=
        Game::Replication::FishingPresentationUpdateResult::Invalid) {
        return 153;
    }
    const Game::Simulation::FishIdentity sceneFish{
        118, Game::Simulation::MakeFishSpawnKey(118, 0, 10, 20, 30)
    };
    const uint32_t fishingLifeEpoch = fishingPresentationWorld.PlayerFor(501)->lifeEpoch;
    if (!fishingPresentationWorld.RegisterFish({
            sceneFish, { 10.0f, 20.0f, 30.0f },
            Game::Simulation::FishSpecies::HylianBass, 8.0f }) ||
        !fishingPresentationWorld.ExecuteLureControl(
            { 501, 1, fishingLifeEpoch, true, false })) {
        return 153;
    }
    ServerWorldTestAccess::Fishing(fishingPresentationWorld).StepFixed(
        ServerWorldTestAccess::Players(fishingPresentationWorld));
    if (!fishingPresentationWorld.ExecuteFishAction(
            { 501, 1, fishingLifeEpoch,
              Game::Simulation::FishActionKind::Hook })) return 153;
    PlayerCommand unequipFishing = equipFishing;
    unequipFishing.sequence = 2;
    if (!fishingPresentationWorld.ExecuteWeaponSelection(
            { 501, 2, fishingPresentationWorld.PlayerFor(501)->lifeEpoch, 1 }) ||
        !fishingPresentationWorld.SubmitPlayerCommand(unequipFishing)) return 154;
    const auto fishingClock = ServerWorld::Clock::now();
    fishingPresentationWorld.Advance(fishingClock);
    fishingPresentationWorld.Advance(fishingClock + std::chrono::milliseconds(34));
    fishingPresentationRelay.ReconcilePlayers(
        fishingPresentationWorld.PlayerSnapshots(), { 501 }, 5000.0f);
    if (fishingPresentationRelay.FishingPresentationFor(501) ||
        fishingPresentationWorld.LureForPlayer(501) ||
        fishingPresentationWorld.FishOwnedBy(501)) {
        return 155;
    }
    equipFishing.sequence = 3;
    if (!fishingPresentationWorld.ExecuteWeaponSelection(
            { 501, 3, fishingPresentationWorld.PlayerFor(501)->lifeEpoch, 4 }) ||
        !fishingPresentationWorld.SubmitPlayerCommand(equipFishing)) return 156;
    fishingPresentationWorld.Advance(fishingClock + std::chrono::milliseconds(68));
    fishingPose.entity = fishingPresentationWorld.PlayerFor(501)->entity;
    fishingPose.sequence = 12;
    if (fishingPresentationRelay.UpdateFishingPresentation(
            fishingPose, *fishingPresentationWorld.PlayerFor(501)) !=
            Game::Replication::FishingPresentationUpdateResult::Accepted) {
        return 157;
    }
    const auto sceneArrow =
        ServerWorldTestAccess::Projectiles(fishingPresentationWorld).SpawnArrow(
            { 501, 118, 0, { 10.0f, 50.0f, 30.0f }, {}, 0 });
    if (!fishingPresentationWorld.ConfigureSceneSpawn(
            { 119, { 40.0f, 50.0f, 60.0f }, 1.0f }) ||
        !fishingPresentationWorld.ExecuteLureControl(
            { 501, 2, fishingLifeEpoch, true, false }) || !sceneArrow) {
        return 158;
    }
    ServerWorldTestAccess::Fishing(fishingPresentationWorld).StepFixed(
        ServerWorldTestAccess::Players(fishingPresentationWorld));
    if (!fishingPresentationWorld.ExecuteFishAction(
            { 501, 2, fishingLifeEpoch,
              Game::Simulation::FishActionKind::Hook })) return 158;
    if (!fishingPresentationWorld.AuthorizeSceneTransition(501, 119)) return 158;
    const auto sceneOutcome = fishingPresentationWorld.ExecuteSceneEntry(
        { 501, 1, fishingLifeEpoch });
    const auto conflictingSceneReplay = fishingPresentationWorld.ExecuteSceneEntry(
        { 501, 1, fishingLifeEpoch });
    fishingPresentationRelay.ReconcilePlayers(
        fishingPresentationWorld.PlayerSnapshots(), { 501 }, 5000.0f);
    if (!sceneOutcome || !sceneOutcome->accepted || !sceneOutcome->changedScene ||
        sceneOutcome->admitted ||
        fishingPresentationRelay.FishingPresentationFor(501) ||
        fishingPresentationWorld.LureForPlayer(501) ||
        fishingPresentationWorld.FishOwnedBy(501) ||
        ServerWorldTestAccess::Projectiles(fishingPresentationWorld).HasArrow(
            501, sceneArrow->replicationId) ||
        !conflictingSceneReplay || !conflictingSceneReplay->accepted ||
        conflictingSceneReplay->changedScene) {
        return 158;
    }
    fishingPose.entity = fishingPresentationWorld.PlayerFor(501)->entity;
    fishingPose.sceneId = 119;
    fishingPose.sequence = 13;
    if (fishingPresentationRelay.UpdateFishingPresentation(
            fishingPose, *fishingPresentationWorld.PlayerFor(501)) !=
        Game::Replication::FishingPresentationUpdateResult::Accepted) {
        return 159;
    }
    const Game::Simulation::FishIdentity deathFish{
        119, Game::Simulation::MakeFishSpawnKey(119, 0, 40, 50, 60)
    };
    if (!fishingPresentationWorld.RegisterFish({
            deathFish, { 40.0f, 50.0f, 60.0f },
            Game::Simulation::FishSpecies::HylianLoach, 12.0f }) ||
        !fishingPresentationWorld.ExecuteLureControl(
            { 501, 3, fishingLifeEpoch, true, false })) {
        return 160;
    }
    ServerWorldTestAccess::Fishing(fishingPresentationWorld).StepFixed(
        ServerWorldTestAccess::Players(fishingPresentationWorld));
    if (!fishingPresentationWorld.ExecuteFishAction(
            { 501, 3, fishingLifeEpoch,
              Game::Simulation::FishActionKind::Hook }) ||
        !ServerWorldTestAccess::Players(fishingPresentationWorld).ApplyDamage(-1, 501, 48, 0)) {
        return 160;
    }
    fishingPresentationWorld.DrainPlayerLifeEvents();
    fishingPresentationRelay.ReconcilePlayers(
        fishingPresentationWorld.PlayerSnapshots(), { 501 }, 5000.0f);
    if (fishingPresentationRelay.FishingPresentationFor(501) ||
        fishingPresentationWorld.LureForPlayer(501) ||
        fishingPresentationWorld.FishOwnedBy(501)) {
        return 161;
    }

    PlayerSnapshot authorityPlayer{};
    authorityPlayer.entity = { 700, 3 };
    authorityPlayer.ownerPlayerId = 77;
    authorityPlayer.sceneId = 118;
    authorityPlayer.position = { 10.0f, 20.0f, 30.0f };
    authorityPlayer.health = 48;
    authorityPlayer.selectedWeapon = 4;
    Game::Replication::FishingPresentationState untrustedPose{};
    untrustedPose.sequence = 1;
    untrustedPose.state = 5;
    untrustedPose.lureDrawOffset = { 9000.0f, 9000.0f, 9000.0f };
    untrustedPose.fishRotation = { 1, 2, 3 };
    if (!Game::Replication::FishingPresentationAuthority::Constrain(
            untrustedPose, authorityPlayer, std::nullopt, std::nullopt) ||
        untrustedPose.playerId != 77 || untrustedPose.entity != authorityPlayer.entity ||
        untrustedPose.sceneId != 118 || untrustedPose.state != 0 ||
        untrustedPose.lureDrawOffset != std::array<float, 3>{} ||
        untrustedPose.fishRotation != std::array<int16_t, 3>{}) {
        return 335;
    }
    Game::Simulation::FishingLureSnapshot authorityLure{};
    authorityLure.entity = { 701, 1 };
    authorityLure.ownerPlayerId = 77;
    authorityLure.sceneId = 118;
    authorityLure.position = { 13.0f, 26.0f, 39.0f };
    authorityLure.phase = Game::Simulation::FishingLurePhase::Flying;
    authorityLure.lureType = 2;
    untrustedPose.state = 5;
    untrustedPose.lineScale = 0.0015f;
    untrustedPose.sinkingLureSegmentIndex = 255;
    if (!Game::Replication::FishingPresentationAuthority::Constrain(
            untrustedPose, authorityPlayer, authorityLure, std::nullopt) ||
        untrustedPose.state != 1 ||
        untrustedPose.lureDrawOffset != std::array<float, 3>{ 3.0f, 6.0f, 9.0f } ||
        untrustedPose.sinkingLureSegmentIndex != 19) {
        return 336;
    }
    authorityLure.phase = Game::Simulation::FishingLurePhase::Settled;
    if (!Game::Replication::FishingPresentationAuthority::Constrain(
            untrustedPose, authorityPlayer, authorityLure, std::nullopt) ||
        untrustedPose.state != 3) {
        return 337;
    }
    Game::Simulation::FishSnapshot authorityFish{};
    authorityFish.entity = { 702, 1 };
    authorityFish.ownerPlayerId = 77;
    authorityFish.identity.sceneId = 118;
    authorityLure.phase = Game::Simulation::FishingLurePhase::Hooked;
    if (!Game::Replication::FishingPresentationAuthority::Constrain(
            untrustedPose, authorityPlayer, authorityLure, authorityFish) ||
        untrustedPose.state != 4) {
        return 338;
    }
    Game::Replication::FishingPresentationState unsafeFishingPose = untrustedPose;
    unsafeFishingPose.rodBendY = 1000.0f;
    if (Game::Replication::FishingPresentationAuthority::Constrain(
            unsafeFishingPose, authorityPlayer, authorityLure, authorityFish)) {
        return 493;
    }
    unsafeFishingPose = untrustedPose;
    unsafeFishingPose.lureHookOffsets[0] = { 1000.0f, 1000.0f, 1000.0f };
    if (!Game::Replication::FishingPresentationAuthority::Constrain(
            unsafeFishingPose, authorityPlayer, authorityLure, authorityFish) ||
        unsafeFishingPose.lureHookOffsets[0] !=
            unsafeFishingPose.lureDrawOffset) {
        return 494;
    }
    authorityLure.ownerPlayerId = 78;
    if (Game::Replication::FishingPresentationAuthority::Constrain(
            untrustedPose, authorityPlayer, authorityLure, authorityFish)) {
        return 339;
    }

    ReplicationCadence cadence;
    const auto noProgressDue = cadence.Advance(0);
    const auto firstTickDue = cadence.Advance(1);
    const auto secondTickDue = cadence.Advance(1);
    const auto playerDue = cadence.Advance(1);
    if (noProgressDue.players || noProgressDue.objectives || noProgressDue.structures ||
        firstTickDue.players || firstTickDue.objectives || firstTickDue.structures ||
        secondTickDue.players || secondTickDue.objectives || secondTickDue.structures ||
        !playerDue.players || playerDue.objectives || playerDue.structures) {
        return 331;
    }
    const auto beforeWorldStateDue = cadence.Advance(2);
    const auto allDue = cadence.Advance(1);
    if (beforeWorldStateDue.players || beforeWorldStateDue.objectives ||
        beforeWorldStateDue.structures || !allDue.players || !allDue.objectives ||
        !allDue.structures) {
        return 332;
    }
    cadence.Reset();
    const auto catchupDue = cadence.Advance(15);
    const auto catchupRemainderDue = cadence.Advance(3);
    if (!catchupDue.players || !catchupDue.objectives || !catchupDue.structures ||
        !catchupRemainderDue.players || !catchupRemainderDue.objectives ||
        !catchupRemainderDue.structures) {
        return 333;
    }
    cadence.Reset();
    const auto resetDue = cadence.Advance(3);
    if (!resetDue.players || resetDue.objectives || resetDue.structures) return 334;

    Game::Simulation::AuthoritativePlayerSkeletonPose hitPose{};
    hitPose.joints.fill({ 1000.0f, 1000.0f, 1000.0f });
    hitPose[Game::Simulation::PlayerHitJoint::HeadBase] = { 0.0f, 60.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::HeadTop] = { 0.0f, 70.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::TorsoBottom] = { 0.0f, 35.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::TorsoTop] = { 0.0f, 55.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::WaistBottom] = { 0.0f, 25.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::LeftShoulder] = { -20.0f, 50.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::LeftElbow] = { -20.0f, 40.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::LeftWrist] = { -20.0f, 30.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::RightShoulder] = { 20.0f, 50.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::RightElbow] = { 20.0f, 40.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::RightWrist] = { 20.0f, 30.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::LeftHip] = { -8.0f, 25.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::LeftKnee] = { -8.0f, 13.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::LeftAnkle] = { -8.0f, 1.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::RightHip] = { 8.0f, 25.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::RightKnee] = { 8.0f, 13.0f, 0.0f };
    hitPose[Game::Simulation::PlayerHitJoint::RightAnkle] = { 8.0f, 1.0f, 0.0f };
    const Game::Simulation::PlayerHitRigDimensions hitDimensions{
        6.0f, 8.0f, 7.0f, 4.0f, 3.0f, 5.0f, 4.0f
    };
    auto articulatedHitRig =
        Game::Simulation::BuildArticulatedPlayerHitRig(hitPose, hitDimensions);
    Game::Simulation::PlayerRigHit rigHit{};
    if (!Game::Simulation::SegmentArticulatedPlayerHitRigFirstHit(
            { -20.0f, 65.0f, 0.0f }, { 20.0f, 65.0f, 0.0f },
            articulatedHitRig, rigHit) ||
        rigHit.region != Game::Simulation::PlayerHitRegion::Head) {
        return 487;
    }
    if (!Game::Simulation::SegmentArticulatedPlayerHitRigFirstHit(
            { 14.0f, 35.0f, 0.0f }, { 26.0f, 35.0f, 0.0f },
            articulatedHitRig, rigHit) ||
        rigHit.region != Game::Simulation::PlayerHitRegion::RightForearm) {
        return 488;
    }
    if (Game::Simulation::DamageForPlayerHitRegion(
            8, Game::Simulation::PlayerHitRegion::Head) != 16 ||
        Game::Simulation::DamageForPlayerHitRegion(
            8, Game::Simulation::PlayerHitRegion::Torso) != 8 ||
        Game::Simulation::DamageForPlayerHitRegion(
            8, Game::Simulation::PlayerHitRegion::Waist) != 8 ||
        Game::Simulation::DamageForPlayerHitRegion(
            8, Game::Simulation::PlayerHitRegion::RightForearm) != 4 ||
        Game::Simulation::DamageForPlayerHitRegion(
            16, Game::Simulation::PlayerHitRegion::LeftThigh) != 8) {
        return 539;
    }

    // A bent forearm follows the supplied authoritative skeleton instead of
    // remaining in an approximate upright-player volume.
    hitPose[Game::Simulation::PlayerHitJoint::RightWrist] = { 35.0f, 40.0f, 0.0f };
    articulatedHitRig =
        Game::Simulation::BuildArticulatedPlayerHitRig(hitPose, hitDimensions);
    if (Game::Simulation::SegmentArticulatedPlayerHitRigFirstHit(
            { 14.0f, 35.0f, 0.0f }, { 26.0f, 35.0f, 0.0f },
            articulatedHitRig, rigHit) ||
        !Game::Simulation::SegmentArticulatedPlayerHitRigFirstHit(
            { 27.0f, 40.0f, -10.0f }, { 27.0f, 40.0f, 10.0f },
            articulatedHitRig, rigHit) ||
        rigHit.region != Game::Simulation::PlayerHitRegion::RightForearm) {
        return 489;
    }

    Game::Simulation::PlayerSnapshot authoritativePoseState{};
    authoritativePoseState.ownerPlayerId = 7;
    authoritativePoseState.sceneId = 1;
    authoritativePoseState.serverTick = 24;
    authoritativePoseState.position = { 100.0f, 20.0f, -50.0f };
    authoritativePoseState.headingRadians = 0.0f;
    authoritativePoseState.actionState = Game::Simulation::PlayerActionState::Idle;
    auto standingPose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(authoritativePoseState);
    if (standingPose[Game::Simulation::PlayerHitJoint::HeadTop].x != 100.0f ||
        standingPose[Game::Simulation::PlayerHitJoint::HeadTop].y != 88.0f ||
        standingPose[Game::Simulation::PlayerHitJoint::HeadTop].z != -50.0f) {
        return 490;
    }
    authoritativePoseState.actionState = Game::Simulation::PlayerActionState::Aiming;
    authoritativePoseState.aimPitchRadians = 0.5f;
    const auto aimingPose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(authoritativePoseState);
    if (aimingPose[Game::Simulation::PlayerHitJoint::LeftWrist].z <=
            standingPose[Game::Simulation::PlayerHitJoint::LeftWrist].z ||
        aimingPose[Game::Simulation::PlayerHitJoint::LeftWrist].y <=
            standingPose[Game::Simulation::PlayerHitJoint::LeftWrist].y) {
        return 491;
    }
    authoritativePoseState.headingRadians = 1.5707963267948966f;
    const auto rotatedPose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(authoritativePoseState);
    if (rotatedPose[Game::Simulation::PlayerHitJoint::LeftWrist].x <= 100.0f) {
        return 492;
    }

    // The dedicated server and client presentation policy derive locomotion
    // direction and action time from the same authoritative snapshot.
    authoritativePoseState.headingRadians = 0.0f;
    authoritativePoseState.actionState = Game::Simulation::PlayerActionState::Idle;
    authoritativePoseState.serverTick = 30;
    authoritativePoseState.locomotionPhaseRadians = 1.5707963267948966f;
    authoritativePoseState.velocity = { 180.0f, 0.0f, 0.0f };
    const auto strafeState =
        Game::Simulation::SampleAuthoritativePlayerPoseState(authoritativePoseState);
    if (strafeState.direction != Game::Simulation::PlayerPoseDirection::Right ||
        strafeState.locomotionAmount != 1.0f) {
        return 493;
    }
    const auto strafePose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(authoritativePoseState);
    authoritativePoseState.velocity = { 0.0f, 0.0f, 0.0f };
    const auto idlePose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(authoritativePoseState);
    if (strafePose[Game::Simulation::PlayerHitJoint::LeftKnee].x ==
        idlePose[Game::Simulation::PlayerHitJoint::LeftKnee].x) {
        return 494;
    }

    // Render frames between authoritative snapshots advance the same semantic
    // locomotion pose. The server collision call remains deterministic because
    // its default advance is zero.
    authoritativePoseState.velocity = { 0.0f, 0.0f, 180.0f };
    authoritativePoseState.locomotionPhaseRadians = 0.0f;
    const auto snapshotPose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(
            authoritativePoseState);
    const auto advancedPose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(
            authoritativePoseState, 0.1f);
    if (snapshotPose[Game::Simulation::PlayerHitJoint::LeftKnee].z ==
            advancedPose[Game::Simulation::PlayerHitJoint::LeftKnee].z ||
        snapshotPose[Game::Simulation::PlayerHitJoint::RightKnee].z ==
            advancedPose[Game::Simulation::PlayerHitJoint::RightKnee].z) {
        return 504;
    }

    authoritativePoseState.selectedWeapon = 1;
    authoritativePoseState.actionState =
        Game::Simulation::PlayerActionState::PrimaryWindup;
    authoritativePoseState.actionStartTick = 30;
    authoritativePoseState.serverTick = 31;
    const auto windupState =
        Game::Simulation::SampleAuthoritativePlayerPoseState(authoritativePoseState);
    const auto windupPose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(authoritativePoseState);
    authoritativePoseState.actionState =
        Game::Simulation::PlayerActionState::PrimaryActive;
    authoritativePoseState.actionStartTick = 32;
    authoritativePoseState.serverTick = 35;
    const auto activeState =
        Game::Simulation::SampleAuthoritativePlayerPoseState(authoritativePoseState);
    const auto activePose =
        Game::Simulation::SampleAuthoritativePlayerSkeletonPose(authoritativePoseState);
    if (windupState.actionProgress >= activeState.actionProgress ||
        windupPose[Game::Simulation::PlayerHitJoint::RightWrist].x ==
            activePose[Game::Simulation::PlayerHitJoint::RightWrist].x ||
        windupPose[Game::Simulation::PlayerHitJoint::RightWrist].z ==
            activePose[Game::Simulation::PlayerHitJoint::RightWrist].z) {
        return 495;
    }

    Game::Simulation::AuthoritativeMeleeWeaponSegment weaponSegment{};
    if (!Game::Simulation::SampleAuthoritativeMeleeWeaponSegment(
            authoritativePoseState, weaponSegment)) {
        return 496;
    }
    const auto segmentLength = [](const auto& segment) {
        const float x = segment.tip.x - segment.base.x;
        const float y = segment.tip.y - segment.base.y;
        const float z = segment.tip.z - segment.base.z;
        return std::sqrt(x * x + y * y + z * z);
    };
    if (std::fabs(segmentLength(weaponSegment) -
                  Game::Simulation::kMasterSwordWorldLength) > 0.001f) {
        return 497;
    }
    authoritativePoseState.selectedWeapon = 2;
    if (!Game::Simulation::SampleAuthoritativeMeleeWeaponSegment(
            authoritativePoseState, weaponSegment) ||
        std::fabs(segmentLength(weaponSegment) -
                  Game::Simulation::kBiggoronSwordWorldLength) > 0.001f) {
        return 498;
    }
    authoritativePoseState.actionState = Game::Simulation::PlayerActionState::Idle;
    if (Game::Simulation::SampleAuthoritativeMeleeWeaponSegment(
            authoritativePoseState, weaponSegment)) {
        return 499;
    }

    // Locomotion animation is server state advanced by actual post-collision
    // travel. It must stop on the exact phase where movement stopped instead
    // of continuing from a global server tick clock.
    PlayerSimulation phaseSimulation;
    phaseSimulation.EnsurePlayer(77, { 1, {}, 0.0f });
    PlayerCommand phaseMove{};
    phaseMove.ownerPlayerId = 77;
    phaseMove.sequence = 1;
    phaseMove.lifeEpoch = 1;
    phaseMove.sceneId = 1;
    phaseMove.moveY = 1.0f;
    if (!phaseSimulation.SubmitCommand(phaseMove)) return 500;
    phaseSimulation.StepFixed();
    const auto movingPhase = phaseSimulation.SnapshotForPlayer(77);
    if (!movingPhase || movingPhase->locomotionPhaseRadians <= 0.0f ||
        movingPhase->locomotionPhaseRadians >= 6.28318530717958647692f) {
        return 501;
    }
    phaseMove.sequence = 2;
    phaseMove.moveY = 0.0f;
    if (!phaseSimulation.SubmitCommand(phaseMove)) return 502;
    phaseSimulation.StepFixed();
    const auto stoppedPhase = phaseSimulation.SnapshotForPlayer(77);
    if (!stoppedPhase ||
        stoppedPhase->locomotionPhaseRadians !=
            movingPhase->locomotionPhaseRadians) {
        return 503;
    }

    replication.Reset();
    return 0;
}
