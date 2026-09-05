#include "multiplayer/FishingNetworkAdapter.h"
#include "multiplayer/PlayerLifecycleNetworkAdapter.h"
#include "multiplayer/PlayerSimulationNetworkAdapter.h"
#include "multiplayer/ProjectileNetworkAdapter.h"
#include "../platform/simulation/StructureActionAuthority.h"
#include "../platform/simulation/ServerIntentAdmission.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

struct ScheduledPacket {
    uint32_t deliveryTick = 0;
    NetworkPlayerCommandPacket packet{};
};

uint32_t NextRandom(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

NetworkPlayerCommandPacket MovementPacket(uint32_t sequence) {
    NetworkPlayerCommandPacket packet{};
    packet.sequence = sequence;
    packet.lifeEpoch = 1;
    packet.clientTick = sequence;
    packet.moveX = static_cast<int8_t>((sequence / 45) % 2 == 0 ? 50 : -50);
    packet.moveY = 85;
    packet.heading = static_cast<int16_t>(sequence * 73U);
    return packet;
}

} // namespace

int main() {
    namespace Adapter = Game::Multiplayer::PlayerSimulationNetworkAdapter;
    using Game::Simulation::PlayerActionState;

    const auto bindTestCommand = [](const NetworkPlayerCommandPacket& packet) {
        Game::Simulation::PlayerCommand command = Adapter::ToCommand(packet);
        command.ownerPlayerId = 42;
        command.sceneId = 118;
        return command;
    };

    Game::Simulation::PlayerSimulation simulation;
    const Game::Simulation::EntityId initialEntity = simulation.EnsurePlayer(
        42, { 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    if (!initialEntity.Valid() || !simulation.SelectWeapon(42, 1)) return 1;

    constexpr uint32_t kSimulationTicks = 900;
    std::vector<ScheduledPacket> network;
    network.reserve(kSimulationTicks * 2);
    uint32_t random = 0xC0FFEE21U;
    uint32_t droppedMovement = 0;
    uint32_t duplicatedMovement = 0;
    uint32_t reliableActions = 0;
    uint32_t reorderedActions = 0;
    uint32_t currentWindowActions = 0;

    for (uint32_t tick = 1; tick <= kSimulationTicks; ++tick) {
        NetworkPlayerCommandPacket movement = MovementPacket(tick);
        const bool forceFinal = tick == kSimulationTicks;
        if (forceFinal || NextRandom(random) % 100 >= 25) {
            const uint32_t jitter = forceFinal ? 0 : NextRandom(random) % 8;
            network.push_back({ tick + jitter, movement });
            if (!forceFinal && NextRandom(random) % 100 < 12) {
                network.push_back({ tick + jitter + 1 + NextRandom(random) % 5, movement });
                ++duplicatedMovement;
            }
        } else {
            ++droppedMovement;
        }

        if (tick % 60 == 0) {
            NetworkPlayerCommandPacket action = movement;
            action.actionSequence = ++reliableActions;
            action.pressedActions = NETWORK_ACTION_PRIMARY;
            // Two copies model reliable retransmission. The first is delayed
            // beyond several newer disposable movement samples.
            const uint32_t reliableDelay = 5 + NextRandom(random) % 6;
            network.push_back({ tick + reliableDelay, action });
            network.push_back({ tick + reliableDelay + 3, action });
            ++reorderedActions;
        } else if (tick % 60 == 30) {
            NetworkPlayerCommandPacket action = movement;
            action.actionSequence = ++reliableActions;
            action.pressedActions = NETWORK_ACTION_PRIMARY;
            // The first reliable copy belongs to the current command window
            // and must execute exactly once. Its delayed retransmission must
            // neither repeat nor resurrect the edge.
            network.push_back({ tick, action });
            network.push_back({ tick + 3, action });
            ++currentWindowActions;
        }
    }

    std::sort(network.begin(), network.end(), [](const ScheduledPacket& left,
                                                 const ScheduledPacket& right) {
        if (left.deliveryTick != right.deliveryTick) {
            return left.deliveryTick < right.deliveryTick;
        }
        // Force newer sequence numbers ahead of older packets sharing a tick.
        return left.packet.sequence > right.packet.sequence;
    });

    size_t nextPacket = 0;
    uint32_t acceptedPackets = 0;
    uint32_t rejectedPackets = 0;
    uint32_t observedActions = 0;
    uint32_t expectedActionExecutions = 0;
    uint32_t currentCommandSequence = 0;
    uint32_t previousProcessed = 0;
    uint32_t previousMeleeAttackId = 0;
    constexpr uint32_t kDrainTicks = 20;
    for (uint32_t tick = 1; tick <= kSimulationTicks + kDrainTicks; ++tick) {
        while (nextPacket < network.size() && network[nextPacket].deliveryTick == tick) {
            const auto& packet = network[nextPacket++].packet;
            if (!Adapter::IsSane(packet)) return 2;
            const bool newerMovement =
                static_cast<int32_t>(packet.sequence - currentCommandSequence) > 0;
            if (simulation.SubmitCommand(bindTestCommand(packet))) {
                ++acceptedPackets;
                if (packet.pressedActions != 0) {
                    ++expectedActionExecutions;
                }
                if (newerMovement) currentCommandSequence = packet.sequence;
            } else {
                ++rejectedPackets;
            }
        }
        simulation.StepFixed();
        const auto snapshot = simulation.SnapshotForPlayer(42);
        if (!snapshot || snapshot->lastProcessedCommand < previousProcessed ||
            !std::isfinite(snapshot->position.x) || !std::isfinite(snapshot->position.z)) {
            return 3;
        }
        if (snapshot->meleeAttackId != 0 &&
            snapshot->meleeAttackId != previousMeleeAttackId) {
            ++observedActions;
        }
        previousProcessed = snapshot->lastProcessedCommand;
        previousMeleeAttackId = snapshot->meleeAttackId;
    }

    const auto beforeDeath = simulation.SnapshotForPlayer(42);
    if (!beforeDeath || beforeDeath->lastProcessedCommand != kSimulationTicks ||
        reorderedActions == 0 || currentWindowActions == 0 ||
        expectedActionExecutions < currentWindowActions ||
        expectedActionExecutions != reliableActions ||
        observedActions != expectedActionExecutions || droppedMovement == 0 ||
        duplicatedMovement == 0 || rejectedPackets == 0) {
        std::fprintf(stderr,
                     "fault baseline failed: processed=%u stale=%u current=%u expected=%u observed=%u "
                     "dropped=%u duplicated=%u rejected=%u\n",
                     beforeDeath ? beforeDeath->lastProcessedCommand : 0,
                     reorderedActions, currentWindowActions, expectedActionExecutions,
                     observedActions,
                     droppedMovement, duplicatedMovement, rejectedPackets);
        return 4;
    }

    NetworkPlayerCommandPacket staleAction = MovementPacket(kSimulationTicks + 100);
    staleAction.actionSequence = reliableActions + 1;
    staleAction.pressedActions = NETWORK_ACTION_PRIMARY;
    if (!simulation.ApplyDamage(-1, 42, 48, 0) ||
        simulation.SubmitCommand(bindTestCommand(staleAction))) {
        return 5;
    }
    for (uint32_t tick = 0; tick < 150; ++tick) simulation.StepFixed();
    const auto respawned = simulation.SnapshotForPlayer(42);
    if (!respawned || respawned->health != 48 || respawned->lifeEpoch != 2 ||
        simulation.SubmitCommand(bindTestCommand(staleAction))) {
        return 6;
    }

    NetworkPlayerCommandPacket newLifeAction = staleAction;
    newLifeAction.sequence += 1;
    newLifeAction.actionSequence += 1;
    newLifeAction.lifeEpoch = respawned->lifeEpoch;
    if (!simulation.SubmitCommand(bindTestCommand(newLifeAction))) return 7;
    simulation.StepFixed();
    const auto afterNewLifeAction = simulation.SnapshotForPlayer(42);
    if (!afterNewLifeAction ||
        afterNewLifeAction->actionState != PlayerActionState::PrimaryWindup) {
        return 8;
    }

    Game::Replication::EntityLifetimeRegistry lifetimes;
    const Game::Replication::ReplicatedPlayer firstLifetime{
        42, { 10, 1 }, 1, 118, {}
    };
    const Game::Replication::ReplicatedPlayer replacementLifetime{
        42, { 10, 2 }, 1, 118, {}
    };
    const auto establish = Game::Multiplayer::PlayerLifecycleNetworkAdapter::ToPacket(
        firstLifetime, true);
    const auto replace = Game::Multiplayer::PlayerLifecycleNetworkAdapter::ToPacket(
        replacementLifetime, true);
    const auto staleRetire = Game::Multiplayer::PlayerLifecycleNetworkAdapter::ToPacket(
        firstLifetime, false);
    if (!Game::Multiplayer::PlayerLifecycleNetworkAdapter::Apply(establish, lifetimes) ||
        !Game::Multiplayer::PlayerLifecycleNetworkAdapter::Apply(replace, lifetimes) ||
        Game::Multiplayer::PlayerLifecycleNetworkAdapter::Apply(staleRetire, lifetimes) ||
        !lifetimes.Matches(42, replacementLifetime.entity)) {
        return 9;
    }

    Game::Replication::ProjectileLifetimeRegistry projectileLifetimes;
    const Game::Replication::ReplicatedOwnedEntity firstArrow{
        { Game::Replication::OwnedEntityKind::Arrow, 42, 700 },
        { 20, 1 }, 118, {}, false
    };
    const Game::Replication::ReplicatedOwnedEntity replacementArrow{
        firstArrow.key, { 20, 2 }, 118, {}, false
    };
    auto arrowCreate = Game::Multiplayer::ProjectileNetworkAdapter::ToLifecyclePacket(
        firstArrow, true);
    auto arrowReplace = Game::Multiplayer::ProjectileNetworkAdapter::ToLifecyclePacket(
        replacementArrow, true);
    auto arrowStaleRetire = Game::Multiplayer::ProjectileNetworkAdapter::ToLifecyclePacket(
        firstArrow, false);
    if (!Game::Multiplayer::ProjectileNetworkAdapter::ApplyLifecycle(
            arrowCreate, projectileLifetimes).Accepted() ||
        Game::Multiplayer::ProjectileNetworkAdapter::ApplyLifecycle(
            arrowReplace, projectileLifetimes).kind !=
            Game::Multiplayer::ProjectileNetworkAdapter::LifecycleApplyKind::Replaced ||
        Game::Multiplayer::ProjectileNetworkAdapter::ApplyLifecycle(
            arrowStaleRetire, projectileLifetimes).Accepted()) {
        return 10;
    }
    Game::Simulation::ArrowSnapshot oldArrowState{};
    oldArrowState.entity = firstArrow.entity;
    oldArrowState.replicationId = 700;
    oldArrowState.ownerPlayerId = 42;
    oldArrowState.sceneId = 118;
    oldArrowState.sequence = 20;
    oldArrowState.active = true;
    Game::Simulation::ArrowSnapshot newArrowState = oldArrowState;
    newArrowState.entity = replacementArrow.entity;
    if (Game::Multiplayer::ProjectileNetworkAdapter::MatchesActiveLifetime(
            Game::Multiplayer::ProjectileNetworkAdapter::ToPacket(oldArrowState),
            projectileLifetimes) ||
        !Game::Multiplayer::ProjectileNetworkAdapter::MatchesActiveLifetime(
            Game::Multiplayer::ProjectileNetworkAdapter::ToPacket(newArrowState),
            projectileLifetimes)) {
        return 11;
    }

    Game::Replication::EntityLifetimeRegistry fishLifetimes;
    Game::Simulation::FishSnapshot firstFish{};
    firstFish.entity = { 30, 1 };
    firstFish.identity = {
        118, Game::Simulation::MakeFishSpawnKey(118, 3, 666, -45, 354)
    };
    firstFish.ownerPlayerId = 42;
    firstFish.ownerLifeEpoch = 1;
    Game::Simulation::FishSnapshot replacementFish = firstFish;
    replacementFish.entity = { 30, 2 };
    const auto firstFishState = Game::Multiplayer::FishingNetworkAdapter::ToPacket(
        firstFish, 40, true);
    const auto replacementFishState = Game::Multiplayer::FishingNetworkAdapter::ToPacket(
        replacementFish, 41, true);
    const auto staleFishRetire = Game::Multiplayer::FishingNetworkAdapter::ToPacket(
        firstFish, 42, false);
    if (!Game::Multiplayer::FishingNetworkAdapter::ApplyLifetime(
            firstFishState, fishLifetimes).Accepted() ||
        Game::Multiplayer::FishingNetworkAdapter::ApplyLifetime(
            replacementFishState, fishLifetimes).kind !=
            Game::Multiplayer::FishingNetworkAdapter::LifetimeApplyKind::Replaced ||
        Game::Multiplayer::FishingNetworkAdapter::ApplyLifetime(
            staleFishRetire, fishLifetimes).Accepted() ||
        !fishLifetimes.Matches(42, replacementFish.entity)) {
        return 12;
    }

    Game::Simulation::PlayerSimulation towerPlayers;
    Game::Simulation::ObjectiveSimulation objectives;
    Game::Simulation::StructureSimulation structures;
    Game::Simulation::StructureActionAuthority structureActions;
    Game::Simulation::ServerIntentAdmission intentAdmission;
    towerPlayers.EnsurePlayer(
        77, { 118, {}, 0.0f, Game::Simulation::TeamId::Red });
    objectives.EnsureObjective(
        { 5, 118, {}, 300.0f, Game::Simulation::TeamId::Red });
    structures.EnsureStructure({ 6, 5, 118, { 100.0f, 0.0f, 0.0f }, 500, 100 });
    const uint32_t towerLifeEpoch = towerPlayers.SnapshotForPlayer(77)->lifeEpoch;
    Game::Simulation::StructureActionCommand build{
        77, 1, towerLifeEpoch, 6, Game::Simulation::StructureActionKind::Build
    };
    const auto unsuppliedTowerBuild =
        structureActions.Execute(build, towerPlayers, objectives, structures, false);
    if (unsuppliedTowerBuild.result !=
            Game::Simulation::StructureActionResult::SupplyUnavailable ||
        structures.SnapshotForStructure(6)->buildProgress != 0) {
        return 13;
    }
    if (intentAdmission.Admit(77, towerLifeEpoch, towerLifeEpoch,
                              Game::Simulation::ServerIntentKind::Structure, build.sequence) !=
            Game::Simulation::ServerIntentResult::Fresh ||
        !intentAdmission.CooldownReady(77, Game::Simulation::ServerIntentKind::Structure, 15) ||
        !structureActions.Execute(build, towerPlayers, objectives, structures,
                                  true).Accepted()) {
        return 13;
    }
    intentAdmission.RecordAccepted(77, Game::Simulation::ServerIntentKind::Structure, 15);
    if (intentAdmission.Admit(77, towerLifeEpoch, towerLifeEpoch,
                              Game::Simulation::ServerIntentKind::Structure, build.sequence) !=
        Game::Simulation::ServerIntentResult::Duplicate) {
        return 14;
    }
    build.sequence = 3;
    if (intentAdmission.Admit(77, towerLifeEpoch, towerLifeEpoch,
                              Game::Simulation::ServerIntentKind::Structure, build.sequence) !=
            Game::Simulation::ServerIntentResult::Fresh ||
        !intentAdmission.CooldownReady(77, Game::Simulation::ServerIntentKind::Structure, 30) ||
        !structureActions.Execute(build, towerPlayers, objectives, structures,
                                  true).Accepted()) {
        return 15;
    }
    intentAdmission.RecordAccepted(77, Game::Simulation::ServerIntentKind::Structure, 30);
    build.sequence = 2;
    if (intentAdmission.Admit(77, towerLifeEpoch, towerLifeEpoch,
                              Game::Simulation::ServerIntentKind::Structure, build.sequence) !=
        Game::Simulation::ServerIntentResult::Stale) {
        return 16;
    }
    const auto tower = structures.SnapshotForStructure(6);
    if (!tower || tower->buildProgress != 50) return 17;

    // A reliable copy may be delivered after the disposable movement stream
    // has stalled entirely. Matching the last sequence is not sufficient once
    // that command window has expired: consume the edge sequence without
    // resurrecting the action, then prove a fresh window still executes.
    Game::Simulation::PlayerSimulation stalledSimulation;
    stalledSimulation.EnsurePlayer(90, { 118, {}, 0.0f });
    if (!stalledSimulation.SelectWeapon(90, 1)) return 18;
    NetworkPlayerCommandPacket stalledMovement = MovementPacket(1);
    stalledMovement.lifeEpoch = 1;
    const auto bindStalled = [](const NetworkPlayerCommandPacket& packet) {
        Game::Simulation::PlayerCommand command = Adapter::ToCommand(packet);
        command.ownerPlayerId = 90;
        command.sceneId = 118;
        return command;
    };
    if (!stalledSimulation.SubmitCommand(bindStalled(stalledMovement))) return 19;
    for (uint32_t tick = 0; tick < 8; ++tick) stalledSimulation.StepFixed();
    NetworkPlayerCommandPacket expiredEdge = stalledMovement;
    expiredEdge.actionSequence = 1;
    expiredEdge.pressedActions = NETWORK_ACTION_PRIMARY;
    if (!stalledSimulation.SubmitCommand(bindStalled(expiredEdge))) return 20;
    stalledSimulation.StepFixed();
    const auto afterExpiredEdge = stalledSimulation.SnapshotForPlayer(90);
    if (!afterExpiredEdge || afterExpiredEdge->actionState != PlayerActionState::Idle ||
        stalledSimulation.SubmitCommand(bindStalled(expiredEdge))) {
        return 21;
    }
    NetworkPlayerCommandPacket freshEdge = expiredEdge;
    freshEdge.sequence = 2;
    freshEdge.actionSequence = 2;
    if (!stalledSimulation.SubmitCommand(bindStalled(freshEdge))) return 22;
    stalledSimulation.StepFixed();
    const auto afterFreshEdge = stalledSimulation.SnapshotForPlayer(90);
    if (!afterFreshEdge ||
        afterFreshEdge->actionState != PlayerActionState::PrimaryWindup) {
        return 23;
    }

    std::printf("network_fault_injection ticks=%u dropped_movement=%u duplicated_movement=%u "
                "accepted=%u rejected=%u reliable_actions=%u reordered_actions=%u "
                "current_actions=%u eligible_actions=%u observed_actions=%u life_epoch=%u "
                "tower_build=%u\n",
                kSimulationTicks, droppedMovement, duplicatedMovement, acceptedPackets,
                rejectedPackets, reliableActions, reorderedActions, currentWindowActions,
                expectedActionExecutions, observedActions, afterNewLifeAction->lifeEpoch,
                tower->buildProgress);
    return 0;
}
