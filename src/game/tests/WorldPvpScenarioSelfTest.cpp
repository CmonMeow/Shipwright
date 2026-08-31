#include "../platform/simulation/ServerWorld.h"
#include "ServerWorldTestAccess.h"

#include <chrono>
#include <cstdio>

namespace {

using namespace Game::Simulation;

void AdvanceWorld(ServerWorld& world, ServerWorld::Clock::time_point& now, uint32_t worldTicks) {
    for (uint32_t tick = 0; tick < worldTicks; ++tick) {
        now += std::chrono::microseconds(16667);
        world.Advance(now);
    }
}

bool ContainsLifeEvent(const std::vector<PlayerLifeEvent>& events, PlayerLifeEventKind kind,
                       int32_t playerId) {
    for (const PlayerLifeEvent& event : events) {
        if (event.kind == kind && event.playerId == playerId) return true;
    }
    return false;
}

bool ContainsStructureEvent(const std::vector<StructureEvent>& events,
                            StructureEventKind kind, int32_t structureKey) {
    for (const StructureEvent& event : events) {
        if (event.kind == kind && event.structureKey == structureKey) return true;
    }
    return false;
}

} // namespace

int main() {
    constexpr int32_t scene = 118;
    constexpr int32_t campObjectiveKey = 7000;
    constexpr int32_t objectiveKey = 7001;
    constexpr int32_t towerObjectiveKey = 7003;
    constexpr int32_t structureKey = 8001;
    constexpr int32_t campToKeepRouteKey = 9001;
    constexpr int32_t campToTowerRouteKey = 9002;
    constexpr int32_t redPlayer = 10;
    constexpr int32_t bluePlayer = 20;
    constexpr int32_t redAlly = 11;

    ServerWorld world;
    auto& players = ServerWorldTestAccess::Players(world);
    auto& projectiles = ServerWorldTestAccess::Projectiles(world);
    auto& objectives = ServerWorldTestAccess::Objectives(world);
    auto& structures = ServerWorldTestAccess::Structures(world);
    auto& corpses = ServerWorldTestAccess::Corpses(world);

    if (!world.AdmitPlayer(redPlayer, { scene, { 0.0f, 0.0f, 0.0f }, 0.0f, TeamId::Red }) ||
        !world.AdmitPlayer(bluePlayer, { scene, { 0.0f, 0.0f, 80.0f }, 3.14159265f, TeamId::Blue }) ||
        !world.AdmitPlayer(redAlly, { scene, { 500.0f, 0.0f, 0.0f }, 0.0f, TeamId::Red })) {
        return 1;
    }

    constexpr int32_t transientPlayer = 99;
    const auto transient = world.AdmitPlayer(
        transientPlayer, { scene, { 1000.0f, 0.0f, 1000.0f }, 0.0f, TeamId::Neutral });
    world.SetFishingCollisionQuery(
        [](int32_t, const Vec3&, const Vec3& to, Vec3& impact) {
            impact = to;
            return true;
        });
    const FishIdentity transientFish{
        scene, MakeFishSpawnKey(scene, 0, 1000, 0, 1000)
    };
    PlayerCommand fishingEquipment{};
    fishingEquipment.ownerPlayerId = transientPlayer;
    fishingEquipment.sequence = 1;
    fishingEquipment.actionSequence = 1;
    fishingEquipment.lifeEpoch = 1;
    fishingEquipment.sceneId = scene;
    if (!transient ||
        !world.ExecuteWeaponSelection(
            { transientPlayer, 1, transient->lifeEpoch, 4 }) ||
        !world.SubmitPlayerCommand(fishingEquipment)) {
        return 1;
    }
    players.StepFixed();
    const auto transientArrow = projectiles.SpawnArrow({ transientPlayer, scene, 0,
                                  { 1000.0f, 30.0f, 1000.0f }, {}, 0 });
    if (
        !world.RegisterFish({ transientFish, { 1000.0f, 0.0f, 1000.0f },
                              FishSpecies::HylianBass, 8.0f }) ||
        !transientArrow ||
        !world.ExecuteLureControl(
            { transientPlayer, 1, transient->lifeEpoch, true, false })) {
        return 1;
    }
    ServerWorldTestAccess::Fishing(world).StepFixed(players);
    if (!ServerWorldTestAccess::Fishing(world).HookNearestRegistered(transientPlayer)) return 1;
    const ServerPlayerDeparture transientDeparture = world.RemovePlayer(transientPlayer);
    if (!transientDeparture.player || players.SnapshotForPlayer(transientPlayer) ||
        projectiles.HasArrow(transientPlayer, transientArrow->replicationId) ||
        world.LureForPlayer(transientPlayer) || world.FishOwnedBy(transientPlayer) ||
        !world.FishSnapshots().empty()) {
        return 1;
    }
    world.DrainArrowEvents();
    world.DrainFishingLureEvents();
    if (!world.EnsureStrategicSite(
            { campObjectiveKey, scene, { 400.0f, 0.0f, 400.0f }, 80.0f, TeamId::Red },
            StrategicSiteKind::Camp, 1).Valid() ||
        !world.EnsureStrategicSite(
            { objectiveKey, scene, {}, 120.0f, TeamId::Neutral },
            StrategicSiteKind::Keep, 2).Valid() ||
        !world.EnsureStrategicSite(
            { towerObjectiveKey, scene, { -400.0f, 0.0f, 400.0f }, 80.0f, TeamId::Red },
            StrategicSiteKind::Tower, 3).Valid() ||
        !world.EnsureSupplyRoute(
            { campToKeepRouteKey, campObjectiveKey, objectiveKey }) ||
        !world.EnsureSupplyRoute(
            { campToTowerRouteKey, campObjectiveKey, towerObjectiveKey }) ||
        !world.EnsureInfluenceAdjacency({ 9005, 1, 2 }) ||
        !world.EnsureInfluenceAdjacency({ 9006, 2, 3 }) ||
        world.EnsureSupplyRoute({ 9003, objectiveKey, towerObjectiveKey }) ||
        world.EnsureSupplyRoute({ 9004, campObjectiveKey, objectiveKey }) ||
        !world.EnsureStructure({ structureKey, objectiveKey, scene, { 30.0f, 0.0f, 0.0f }, 500, 100 }).Valid()) {
        return 2;
    }

    // The blue player begins outside capture range while red establishes the objective.
    if (!players.ChangeScene(bluePlayer, { scene, { 0.0f, 0.0f, 300.0f }, 3.14159265f, TeamId::Blue })) return 3;
    auto now = ServerWorld::Clock::time_point{} + std::chrono::seconds(1);
    world.Advance(now);
    AdvanceWorld(world, now, 310);
    const auto redObjective = objectives.SnapshotForObjective(objectiveKey);
    const auto captureEvents = world.DrainObjectiveCapturedEvents();
    if (!redObjective || redObjective->owner != TeamId::Red || captureEvents.size() != 1 ||
        captureEvents.front().newOwner != TeamId::Red || !world.HasFriendlySupply(objectiveKey)) {
        return 4;
    }

    const uint32_t redLifeEpoch = world.PlayerFor(redPlayer)->lifeEpoch;
    const uint32_t initialBlueLifeEpoch = world.PlayerFor(bluePlayer)->lifeEpoch;
    const auto staleLifeBuild = world.ExecuteStructureAction(
        { redPlayer, 1, redLifeEpoch + 1, structureKey,
          StructureActionKind::Build });
    const auto firstBuild = world.ExecuteStructureAction(
        { redPlayer, 1, redLifeEpoch, structureKey, StructureActionKind::Build });
    const auto replayedBuild = world.ExecuteStructureAction(
        { redPlayer, 1, redLifeEpoch, structureKey, StructureActionKind::Build });
    const auto limitedBuild = world.ExecuteStructureAction(
        { redPlayer, 2, redLifeEpoch, structureKey, StructureActionKind::Build });
    if (staleLifeBuild.result != StructureActionResult::StaleLife ||
        !firstBuild.Accepted() || replayedBuild.result != StructureActionResult::Replayed ||
        limitedBuild.result != StructureActionResult::RateLimited) {
        return 5;
    }
    for (uint32_t sequence = 3; sequence <= 5; ++sequence) {
        AdvanceWorld(world, now, static_cast<uint32_t>(
                                     ServerIntentAdmission::CooldownTicks(
                                         ServerIntentKind::Structure)));
        if (!world.ExecuteStructureAction(
                 { redPlayer, sequence, redLifeEpoch, structureKey,
                   StructureActionKind::Build }).Accepted()) {
            return 5;
        }
    }
    const auto redFortification = structures.SnapshotForStructure(structureKey);
    if (!redFortification || redFortification->phase != StructurePhase::Active ||
        redFortification->team != TeamId::Red || redFortification->health != 500 ||
        structures.ApplyDamage(structureKey, TeamId::Red, 100)) {
        return 6;
    }
    projectiles.DrainEvents();
    const auto hostileStructureSpawn = projectiles.SpawnArrow(
        { bluePlayer, scene, 0, { 30.0f, 30.0f, -100.0f },
          { 0.0f, 0.0f, 12000.0f }, 0 });
    if (!hostileStructureSpawn) {
        return 6;
    }
    projectiles.StepFixed(players, structures);
    const auto hostileStructureArrow = projectiles.DrainEvents();
    const auto arrowDamagedFortification = structures.SnapshotForStructure(structureKey);
    if (hostileStructureArrow.empty() ||
        hostileStructureArrow.back().kind != ArrowEventKind::HitStructure ||
        hostileStructureArrow.back().hitStructureKey != structureKey ||
        !arrowDamagedFortification || arrowDamagedFortification->health != 492) {
        return 6;
    }
    const auto friendlyStructureSpawn = projectiles.SpawnArrow(
        { redPlayer, scene, 0, { 30.0f, 30.0f, -100.0f },
          { 0.0f, 0.0f, 12000.0f }, 0 });
    if (!friendlyStructureSpawn) {
        return 6;
    }
    projectiles.StepFixed(players, structures);
    const auto friendlyStructureArrow = projectiles.DrainEvents();
    const auto friendlyFireFortification = structures.SnapshotForStructure(structureKey);
    if (friendlyStructureArrow.empty() ||
        friendlyStructureArrow.back().kind != ArrowEventKind::HitStructure ||
        !friendlyFireFortification || friendlyFireFortification->health != 492 ||
        !structures.ApplyDamage(structureKey, TeamId::Blue, 492)) {
        return 6;
    }
    const auto destroyedFortification = structures.SnapshotForStructure(structureKey);
    if (!destroyedFortification || destroyedFortification->phase != StructurePhase::Destroyed ||
        destroyedFortification->health != 0) {
        return 7;
    }
    projectiles.StepFixed(players, structures);
    if (projectiles.HasArrow(bluePlayer, hostileStructureSpawn->replicationId) ||
        projectiles.HasArrow(redPlayer, friendlyStructureSpawn->replicationId)) {
        return 7;
    }

    // Restore the blue player to the arena and prove server projectile damage, friendly-fire rejection,
    // death scheduling, and exact one-shot projectile ownership without client hit reports.
    if (!players.ChangeScene(bluePlayer, { scene, { 0.0f, 0.0f, 80.0f }, 3.14159265f, TeamId::Blue }) ||
        players.ApplyDamage(redPlayer, redAlly, 8, 0, CombatAttackKind::Arrow)) {
        return 8;
    }
    players.DrainCombatResults();
    players.DrainLifeEvents();
    for (int32_t arrowId = 1; arrowId <= 6; ++arrowId) {
        const auto arrow = projectiles.SpawnArrow({ redPlayer, scene, 0,
                                                    { 0.0f, 30.0f, 0.0f },
                                                    { 0.0f, 0.0f, 6000.0f }, 0 });
        if (!arrow || !projectiles.HasArrow(redPlayer, arrow->replicationId)) return 9;
        projectiles.StepFixed(players);
        if (projectiles.HasArrow(redPlayer, arrow->replicationId)) return 10;
    }
    const auto deadBlue = players.SnapshotForPlayer(bluePlayer);
    const auto arrowCombat = players.DrainCombatResults();
    const auto deathEvents = players.DrainLifeEvents();
    if (!deadBlue || deadBlue->health != 0 || arrowCombat.size() != 6 ||
        !ContainsLifeEvent(deathEvents, PlayerLifeEventKind::Died, bluePlayer)) {
        return 11;
    }
    const auto deadBuild = world.ExecuteStructureAction(
        { bluePlayer, 1, initialBlueLifeEpoch, structureKey,
          StructureActionKind::Build });
    if (deadBuild.result != StructureActionResult::PlayerUnavailable) return 11;

    CorpsePose deathPose{};
    deathPose.sourcePlayerId = bluePlayer;
    deathPose.sourcePlayerEntity = deadBlue->entity;
    deathPose.sourceLifeEpoch = deadBlue->lifeEpoch;
    deathPose.sceneId = deadBlue->sceneId;
    deathPose.position = deadBlue->position;
    if (!world.CreateCorpse(deathPose).Valid() || corpses.Snapshots().size() != 1) return 12;

    // Dead players neither move nor capture, and queued action edges cannot execute after respawn.
    PlayerCommand staleAction{};
    staleAction.ownerPlayerId = bluePlayer;
    staleAction.sequence = 1;
    staleAction.actionSequence = 1;
    staleAction.sceneId = scene;
    staleAction.pressedActions = PLAYER_ACTION_PRIMARY;
    if (players.SubmitCommand(staleAction)) return 13;
    for (uint32_t tick = 0; tick < 150; ++tick) players.StepFixed();
    const auto respawnEvents = players.DrainLifeEvents();
    const auto respawnedBlue = players.SnapshotForPlayer(bluePlayer);
    if (!respawnedBlue || respawnedBlue->health != 48 ||
        respawnedBlue->lifeEpoch != 2 || players.SubmitCommand(staleAction) ||
        respawnedBlue->actionState != PlayerActionState::Idle ||
        !ContainsLifeEvent(respawnEvents, PlayerLifeEventKind::Respawned, bluePlayer)) {
        return 14;
    }
    const auto delayedOldLifeBuild = world.ExecuteStructureAction(
        { bluePlayer, 0x7FFFFFFFU, initialBlueLifeEpoch, structureKey,
          StructureActionKind::Build });
    const auto firstNewLifeBuild = world.ExecuteStructureAction(
        { bluePlayer, 1, respawnedBlue->lifeEpoch, structureKey,
          StructureActionKind::Build });
    if (delayedOldLifeBuild.result != StructureActionResult::StaleLife ||
        firstNewLifeBuild.result != StructureActionResult::ObjectiveNotOwned) {
        return 14;
    }

    // Blue takes the keep after red leaves, but cannot rebuild while the routed
    // camp still belongs to red. Capture automatically resets every dependent
    // fortification before the ownership event is exposed for replication.
    world.DrainStructureEvents();
    if (!players.ChangeScene(redPlayer, { scene, { 500.0f, 0.0f, 0.0f }, 0.0f, TeamId::Red }) ||
        !players.ChangeScene(bluePlayer, { scene, {}, 3.14159265f, TeamId::Blue })) {
        return 15;
    }
    AdvanceWorld(world, now, 310);
    const auto blueObjective = objectives.SnapshotForObjective(objectiveKey);
    const auto blueCaptureEvents = world.DrainObjectiveCapturedEvents();
    const auto captureStructureEvents = world.DrainStructureEvents();
    const auto resetFortification = structures.SnapshotForStructure(structureKey);
    if (!blueObjective || blueObjective->owner != TeamId::Blue ||
        world.HasFriendlySupply(objectiveKey) || blueCaptureEvents.size() != 1 ||
        blueCaptureEvents.front().objectiveKey != objectiveKey ||
        blueCaptureEvents.front().newOwner != TeamId::Blue ||
        !resetFortification || resetFortification->phase != StructurePhase::Planned ||
        resetFortification->team != TeamId::Neutral || resetFortification->health != 0 ||
        resetFortification->buildProgress != 0 ||
        !ContainsStructureEvent(captureStructureEvents, StructureEventKind::Reset,
                                structureKey)) {
        return 16;
    }
    const auto unsuppliedBuild = world.ExecuteStructureAction(
        { bluePlayer, 2, respawnedBlue->lifeEpoch, structureKey,
          StructureActionKind::Build });
    if (unsuppliedBuild.result != StructureActionResult::SupplyUnavailable) {
        return 16;
    }

    // Capturing the authored source camp enables its route. Returning to the
    // keep then permits exactly the same authoritative build command.
    if (!players.ChangeScene(
            bluePlayer,
            { scene, { 400.0f, 0.0f, 400.0f }, 3.14159265f,
              TeamId::Blue })) {
        return 16;
    }
    AdvanceWorld(world, now, 310);
    const auto campCaptureEvents = world.DrainObjectiveCapturedEvents();
    if (!world.HasFriendlySupply(objectiveKey) || campCaptureEvents.size() != 1 ||
        campCaptureEvents.front().objectiveKey != campObjectiveKey ||
        campCaptureEvents.front().newOwner != TeamId::Blue ||
        !players.ChangeScene(bluePlayer,
                             { scene, {}, 3.14159265f, TeamId::Blue })) {
        return 16;
    }
    for (uint32_t sequence = 3; sequence <= 6; ++sequence) {
        if (sequence != 3) {
            AdvanceWorld(world, now, static_cast<uint32_t>(
                                         ServerIntentAdmission::CooldownTicks(
                                             ServerIntentKind::Structure)));
        }
        if (!world.ExecuteStructureAction(
                 { bluePlayer, sequence, respawnedBlue->lifeEpoch, structureKey,
                   StructureActionKind::Build }).Accepted()) {
            return 16;
        }
    }
    const auto blueFortification = structures.SnapshotForStructure(structureKey);
    if (!blueFortification || blueFortification->phase != StructurePhase::Active ||
        blueFortification->team != TeamId::Blue || blueFortification->health != 500) {
        return 17;
    }
    // Repair obeys the same route authority. Removing the route disables it;
    // recreating the route does not replay the rejected command and requires a
    // newly sequenced intent.
    if (!structures.ApplyDamage(structureKey, TeamId::Red, 10)) return 17;
    AdvanceWorld(world, now, static_cast<uint32_t>(
                                 ServerIntentAdmission::CooldownTicks(
                                     ServerIntentKind::Structure)));
    if (!world.RemoveSupplyRoute(campToKeepRouteKey) ||
        world.HasFriendlySupply(objectiveKey)) {
        return 17;
    }
    const auto unsuppliedRepair = world.ExecuteStructureAction(
        { bluePlayer, 7, respawnedBlue->lifeEpoch, structureKey,
          StructureActionKind::Repair });
    if (unsuppliedRepair.result != StructureActionResult::SupplyUnavailable ||
        !world.EnsureSupplyRoute(
            { campToKeepRouteKey, campObjectiveKey, objectiveKey }) ||
        !world.HasFriendlySupply(objectiveKey)) {
        return 17;
    }
    const auto suppliedRepair = world.ExecuteStructureAction(
        { bluePlayer, 8, respawnedBlue->lifeEpoch, structureKey,
          StructureActionKind::Repair });
    if (!suppliedRepair.Accepted() || !suppliedRepair.structure ||
        suppliedRepair.structure->health != 500) {
        return 17;
    }
    // Corpse retention is deterministic and bounded per scene for long-running battles.
    for (int32_t playerId = 100; playerId < 200; ++playerId) {
        CorpsePose pose{};
        pose.sourcePlayerId = playerId;
        pose.sourcePlayerEntity = { static_cast<uint32_t>(playerId), 1 };
        pose.sourceLifeEpoch = 1;
        pose.sceneId = scene;
        pose.position.x = static_cast<float>(playerId);
        if (!world.CreateCorpse(pose).Valid()) return 18;
    }
    const auto retainedCorpses = corpses.Snapshots();
    if (retainedCorpses.size() != 99) return 19;

    constexpr int32_t temporaryObjective = 7002;
    constexpr int32_t temporaryStructure = 8002;
    if (!world.EnsureObjective({ temporaryObjective, scene, {}, 50.0f, TeamId::Neutral }).Valid() ||
        !world.EnsureStructure(
            { temporaryStructure, temporaryObjective, scene, {}, 100, 25 }).Valid() ||
        !world.RemoveObjective(temporaryObjective) ||
        ServerWorldTestAccess::Objectives(world).SnapshotForObjective(temporaryObjective) ||
        ServerWorldTestAccess::Structures(world).SnapshotForStructure(temporaryStructure)) {
        return 21;
    }

    std::printf("world_pvp_scenario ticks=%llu objective=blue structure=blue sites=%zu routes=%zu adjacencies=%zu corpses=%zu combat_hits=%zu\n",
                static_cast<unsigned long long>(world.CurrentTick()), world.StrategicSites().size(),
                world.SupplyRoutes().size(), world.InfluenceAdjacencies().size(),
                retainedCorpses.size(), arrowCombat.size());
    return 0;
}
