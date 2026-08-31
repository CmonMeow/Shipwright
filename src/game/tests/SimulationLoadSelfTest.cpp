#include "../platform/replication/PlayerReplicationSystem.h"
#include "../platform/replication/OwnedEntityReplicationSystem.h"
#include "../platform/replication/ReplicationBudgetSystem.h"
#include "../platform/replication/SpatialEntityReplicationSystem.h"
#include "../platform/simulation/ServerWorld.h"
#include "ServerWorldTestAccess.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

constexpr int32_t kScene = 118;
constexpr int32_t kPlayers = 128;
constexpr int32_t kSimulationSeconds = 120;
constexpr int32_t kServerSteps = kSimulationSeconds * 60;

using namespace Game::Simulation;

Vec3 SpawnPosition(int32_t player) {
    return { static_cast<float>((player % 16) * 250), 0.0f,
             static_cast<float>((player / 16) * 250) };
}

} // namespace

int main() {
    ServerWorld serverWorld;
    PlayerSimulation& players = ServerWorldTestAccess::Players(serverWorld);
    ProjectileSimulation& projectiles = ServerWorldTestAccess::Projectiles(serverWorld);
    Game::Replication::PlayerReplicationSystem replication;
    Game::Replication::OwnedEntityReplicationSystem ownedReplication;
    Game::Replication::ReplicationBudgetSystem replicationBudgets;
    Game::Replication::SpatialEntityReplicationSystem spatialReplication;
    ObjectiveSimulation& objectives = ServerWorldTestAccess::Objectives(serverWorld);
    StructureSimulation& structures = ServerWorldTestAccess::Structures(serverWorld);
    CorpseSimulation& corpses = ServerWorldTestAccess::Corpses(serverWorld);

    projectiles.SetCollisionQuery([](int32_t sceneId, const Vec3&, const Vec3& end, Vec3& impact) {
        if (sceneId != kScene || end.z < 2250.0f) return false;
        impact = end;
        impact.z = 2250.0f;
        return true;
    });

    std::vector<uint32_t> commandSequences(kPlayers, 0);
    std::vector<int32_t> connectedPlayers;
    connectedPlayers.reserve(kPlayers);
    for (int32_t player = 0; player < kPlayers; ++player) {
        const TeamId team = (player & 1) == 0 ? TeamId::Red : TeamId::Blue;
        if (!players.EnsurePlayer(player, { kScene, SpawnPosition(player), 0.0f, team }).Valid() ||
            !players.SelectWeapon(player, static_cast<uint8_t>((player % 3) + 1))) {
            std::fprintf(stderr, "failed to create player %d\n", player);
            return 1;
        }
        connectedPlayers.push_back(player);
    }
    for (int32_t index = 0; index < 8; ++index) {
        const TeamId owner = (index & 1) == 0 ? TeamId::Red : TeamId::Blue;
        const int32_t objectiveKey = 1000 + index;
        if (!objectives.EnsureObjective({ objectiveKey, kScene,
                                          { static_cast<float>(index * 500), 0.0f, 875.0f }, 325.0f,
                                          owner }).Valid() ||
            !structures.EnsureStructure({ 2000 + index, objectiveKey, kScene,
                                          { static_cast<float>(index * 500), 0.0f, 1050.0f }, 1000, 100 }).Valid()) {
            return 2;
        }
        for (int contribution = 0; contribution < 4; ++contribution) {
            if (!structures.ContributeBuild(2000 + index, owner, owner, 25)) return 3;
        }
    }
    for (int32_t index = 0; index < 99; ++index) {
        CorpsePose pose{};
        pose.sourcePlayerId = index % kPlayers;
        pose.sourcePlayerEntity = { static_cast<uint32_t>(pose.sourcePlayerId), 1 };
        pose.sourceLifeEpoch = 1;
        pose.sceneId = kScene;
        pose.position = { static_cast<float>((index % 16) * 250), 0.0f,
                          static_cast<float>((index / 16) * 250) };
        if (!corpses.Create(pose).Valid()) return 14;
    }

    const auto simulationStart = ServerWorld::Clock::now();
    serverWorld.Advance(simulationStart);
    size_t peakArrows = 0;
    size_t peakObservers = 0;
    size_t peakOwnedEntities = 0;
    size_t peakSpatialEntities = 0;
    size_t peakSpatialCandidates = 0;
    uint64_t lifecycleTransitions = 0;
    uint64_t ownedLifecycleTransitions = 0;
    uint64_t spatialLifecycleTransitions = 0;
    uint64_t acceptedCommands = 0;
    uint64_t rejectedDeadCommands = 0;
    uint64_t spawnedArrows = 0;
    const auto wallStart = std::chrono::steady_clock::now();

    for (int32_t step = 1; step <= kServerSteps; ++step) {
        const auto now = simulationStart + std::chrono::nanoseconds(16666667LL * step);
        if ((step & 1) == 0) {
            const uint32_t playerTick = static_cast<uint32_t>(step / 2);
            for (int32_t player = 0; player < kPlayers; ++player) {
                const auto playerState = players.SnapshotForPlayer(player);
                if (!playerState) return 5;
                PlayerCommand command{};
                command.ownerPlayerId = player;
                command.sequence = ++commandSequences[player];
                command.lifeEpoch = playerState->lifeEpoch;
                command.sceneId = kScene;
                command.moveX = static_cast<float>((player % 3) - 1) * 0.45f;
                command.moveY = (player & 1) == 0 ? 0.65f : -0.65f;
                command.headingRadians = std::fmod(static_cast<float>(playerTick) * 0.005f +
                                                       static_cast<float>(player) * 0.1f,
                                                   6.28318530718f);
                if (playerTick % 180 == 1 && player % 8 == 0) {
                    command.actionSequence = command.sequence;
                    command.pressedActions = PLAYER_ACTION_PRIMARY;
                }
                if (player % 16 == 1) command.heldActions = PLAYER_ACTION_BLOCK;
                if (playerState->health == 0) {
                    if (players.SubmitCommand(command)) return 5;
                    ++rejectedDeadCommands;
                    continue;
                }
                if (!players.SubmitCommand(command)) return 5;
                ++acceptedCommands;
            }
        }

        if (step % 30 == 0) {
            for (int32_t owner = 0; owner < kPlayers; owner += 16) {
                const auto player = players.SnapshotForPlayer(owner);
                if (!player || player->health == 0) continue;
                ArrowSpawn spawn{};
                spawn.ownerPlayerId = owner;
                spawn.sceneId = kScene;
                spawn.position = { player->position.x, player->position.y + 40.0f, player->position.z };
                spawn.velocity = { 0.0f, 0.0f, 900.0f };
                if (!projectiles.SpawnArrow(spawn)) return 6;
                ++spawnedArrows;
            }
        }
        if (step % 60 == 1) {
            CorpsePose pose{};
            pose.sourcePlayerId = (step / 60) % kPlayers;
            pose.sourcePlayerEntity = { static_cast<uint32_t>(pose.sourcePlayerId), 1 };
            pose.sourceLifeEpoch = 1;
            pose.sceneId = kScene;
            pose.position = SpawnPosition(pose.sourcePlayerId);
            if (!corpses.Create(pose).Valid()) return 15;
        }

        serverWorld.Advance(now);
        replicationBudgets.UpdateObservers(connectedPlayers, now);
        const auto snapshots = players.Snapshots();
        if (step % 60 == 0) {
            lifecycleTransitions += replication.Reconcile(snapshots, connectedPlayers, 1200.0f).size();
            peakObservers = (std::max)(peakObservers, replication.TotalVisiblePairCount());
            std::vector<Game::Replication::ReplicatedOwnedEntity> replicatedEntities;
            const auto arrowSnapshots = projectiles.Snapshots();
            replicatedEntities.reserve(arrowSnapshots.size());
            for (const auto& arrow : arrowSnapshots) {
                replicatedEntities.push_back({
                    { Game::Replication::OwnedEntityKind::Arrow, arrow.ownerPlayerId,
                      arrow.replicationId },
                    arrow.entity, arrow.sceneId, arrow.position, false
                });
            }
            ownedLifecycleTransitions +=
                ownedReplication.Reconcile(replicatedEntities, connectedPlayers,
                                           snapshots, 1200.0f).size();
            peakOwnedEntities = (std::max)(peakOwnedEntities,
                                           ownedReplication.TotalVisiblePairCount());
            std::vector<Game::Replication::ReplicatedSpatialEntity> spatialEntities;
            const auto corpseSnapshots = corpses.Snapshots();
            spatialEntities.reserve(corpseSnapshots.size());
            for (const auto& corpse : corpseSnapshots) {
                spatialEntities.push_back({
                    { Game::Replication::SpatialEntityKind::Corpse,
                      static_cast<int32_t>(corpse.entity.index) },
                    corpse.entity, corpse.pose.sourcePlayerId, -1, corpse.pose.sceneId,
                    corpse.pose.position
                });
            }
            spatialLifecycleTransitions += spatialReplication.Reconcile(
                spatialEntities, snapshots, connectedPlayers, 1200.0f).size();
            peakSpatialEntities = (std::max)(peakSpatialEntities,
                                             spatialReplication.TotalVisiblePairCount());
            peakSpatialCandidates = (std::max)(peakSpatialCandidates,
                                               spatialReplication.LastCandidateCount());
        }
        for (const int32_t observer : connectedPlayers) {
            const size_t visiblePlayers = replication.VisibleCount(observer);
            if (visiblePlayers != 0) {
                replicationBudgets.TryConsume(observer, Game::Replication::ReplicationPriority::High,
                                              visiblePlayers * 96);
            }
            const size_t visibleOwned = ownedReplication.VisibleCount(observer);
            if (visibleOwned != 0) {
                replicationBudgets.TryConsume(observer, Game::Replication::ReplicationPriority::Normal,
                                              visibleOwned * 48);
            }
            replicationBudgets.TryConsume(observer, Game::Replication::ReplicationPriority::Low, 128);
            if (step % 300 == 0) {
                replicationBudgets.TryConsume(observer, Game::Replication::ReplicationPriority::Critical,
                                              4096);
            }
        }
        if (step % 600 == 0) {
            for (int32_t player = (step / 600) % 8; player < kPlayers; player += 32) {
                players.ApplyDamage(-1, player, 48, 0);
            }
        } else if (step % 600 == 60) {
            for (int32_t player = ((step - 60) / 600) % 8; player < kPlayers; player += 32) {
                players.RespawnPlayer(player);
            }
        }

        peakArrows = (std::max)(peakArrows, projectiles.Snapshots().size());
        players.DrainCombatResults();
        projectiles.DrainEvents();
        objectives.DrainCapturedEvents();
        structures.DrainEvents();
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - wallStart);
    const auto finalPlayers = players.Snapshots();
    const auto finalArrows = projectiles.Snapshots();
    Game::Replication::ReplicationBudgetStats budgetTotals{};
    for (const int32_t observer : connectedPlayers) {
        const auto stats = replicationBudgets.StatsFor(observer);
        budgetTotals.acceptedPackets += stats.acceptedPackets;
        budgetTotals.acceptedBytes += stats.acceptedBytes;
        budgetTotals.deferredPackets += stats.deferredPackets;
        budgetTotals.deferredBytes += stats.deferredBytes;
    }
    std::printf(
                 "simulation_soak seconds=%d players=%zu player_ticks=%u projectile_ticks=%u commands=%llu rejected_dead=%llu "
                 "spawned_arrows=%llu objectives=%zu structures=%zu "
                 "peak_arrows=%zu peak_interest=%zu lifecycle=%llu "
                 "peak_owned=%zu owned_lifecycle=%llu "
                 "peak_spatial=%zu spatial_candidates=%zu spatial_lifecycle=%llu "
                 "budget_sent=%llu budget_deferred=%llu "
                 "final_arrows=%zu elapsed_ms=%lld\n",
                 kSimulationSeconds,
                 finalPlayers.size(), players.CurrentTick(), projectiles.CurrentTick(),
                 static_cast<unsigned long long>(acceptedCommands),
                 static_cast<unsigned long long>(rejectedDeadCommands),
                 static_cast<unsigned long long>(spawnedArrows),
                 objectives.Snapshots().size(),
                 structures.Snapshots().size(), peakArrows,
                 peakObservers, static_cast<unsigned long long>(lifecycleTransitions),
                 peakOwnedEntities, static_cast<unsigned long long>(ownedLifecycleTransitions),
                 peakSpatialEntities, peakSpatialCandidates,
                 static_cast<unsigned long long>(spatialLifecycleTransitions),
                 static_cast<unsigned long long>(budgetTotals.acceptedPackets),
                 static_cast<unsigned long long>(budgetTotals.deferredPackets),
                 finalArrows.size(), static_cast<long long>(elapsed.count()));
    if (finalPlayers.size() != kPlayers || players.CurrentTick() < kSimulationSeconds * 29 ||
        projectiles.CurrentTick() < kSimulationSeconds * 59 ||
        acceptedCommands + rejectedDeadCommands != 460800 || rejectedDeadCommands == 0 ||
        objectives.Snapshots().size() != 8 || structures.Snapshots().size() != 8 ||
        peakObservers == 0 || lifecycleTransitions == 0 || peakOwnedEntities == 0 || ownedLifecycleTransitions == 0 ||
        corpses.Snapshots().size() != 99 || peakSpatialEntities == 0 ||
        peakSpatialCandidates == 0 || spatialLifecycleTransitions == 0 ||
        budgetTotals.acceptedPackets == 0 || budgetTotals.deferredPackets == 0 ||
        finalArrows.size() > static_cast<size_t>(kPlayers) * 99 ||
        elapsed > std::chrono::seconds(20)) {
        return 8;
    }

    for (int32_t player = 0; player < kPlayers / 2; ++player) {
        const ServerPlayerDeparture departure = serverWorld.RemovePlayer(player);
        if (!departure.player) return 9;
        lifecycleTransitions += replication.RemovePlayer(player).size();
        replicationBudgets.RemoveObserver(player);
        spatialReplication.RemoveObserver(player);
        connectedPlayers.erase(connectedPlayers.begin());
    }
    for (const auto& arrow : projectiles.Snapshots()) {
        if (arrow.ownerPlayerId < kPlayers / 2) return 9;
    }
    if (players.Snapshots().size() != kPlayers / 2 ||
        replicationBudgets.ObserverCount() != kPlayers / 2) return 10;

    replication.Reconcile(players.Snapshots(), connectedPlayers, 1200.0f);
    std::vector<Game::Replication::ReplicatedOwnedEntity> remainingEntities;
    for (const auto& arrow : projectiles.Snapshots()) {
        remainingEntities.push_back({
            { Game::Replication::OwnedEntityKind::Arrow, arrow.ownerPlayerId,
              arrow.replicationId },
            arrow.entity, arrow.sceneId, arrow.position, false
        });
    }
    ownedReplication.Reconcile(remainingEntities, connectedPlayers,
                               players.Snapshots(), 1200.0f);
    for (int32_t removed = 0; removed < kPlayers / 2; ++removed) {
        for (const int32_t observer : connectedPlayers) {
            if (replication.IsVisible(observer, removed)) return 12;
        }
    }
    for (const int32_t observer : connectedPlayers) {
        for (const auto& entity : remainingEntities) {
            if (entity.key.ownerPlayerId < kPlayers / 2 &&
                ownedReplication.IsVisible(observer, entity.key)) return 13;
        }
    }

    if (peakArrows < finalArrows.size()) return 11;
    return 0;
}
