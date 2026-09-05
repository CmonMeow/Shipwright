#include "ServerWorld.h"
#include "../SequenceNumber.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Game::Simulation {

namespace {

constexpr float kRadiansToBinaryAngle = 32768.0f / 3.14159265358979323846f;
constexpr float kBinaryAngleToRadians = 3.14159265358979323846f / 32768.0f;
constexpr float kArrowSpeed = 3000.0f;
constexpr uint8_t kNormalArrowType = 2;
constexpr uint8_t kSinkingLureType = 2;
// A client may identify a previously admitted aim sample, but cannot select
// an arbitrary old direction. Eight native 20 Hz samples bound this metadata
// to the most recent 400 ms while leaving room for ordinary packet reordering.
constexpr uint32_t kMaximumArrowFireSampleAge = 8;

CorpsePose BuildCorpsePose(const PlayerLifeEvent& death) {
    constexpr float radiansToBinaryAngle = 32768.0f / 3.14159265358979323846f;
    CorpsePose pose{};
    pose.sourcePlayerId = death.playerId;
    pose.sourcePlayerEntity = death.entity;
    pose.sourceLifeEpoch = death.lifeEpoch;
    pose.sceneId = death.sceneId;
    pose.roomId = -1;
    pose.position = death.position;
    pose.rotation = {
        0,
        static_cast<int16_t>(std::lround(death.headingRadians * radiansToBinaryAngle)),
        0
    };
    pose.selectedWeapon = death.selectedWeapon;
    return pose;
}

} // namespace

ServerWorld::ServerWorld(int32_t defaultSceneId)
    : mSceneTransitions(4096, defaultSceneId) {
}

ServerWorldUpdate ServerWorld::Advance(Clock::time_point now) {
    ServerWorldUpdate result{};
    if (mLastUpdate == Clock::time_point{}) {
        mLastUpdate = now;
        return result;
    }

    const double elapsed = std::clamp(std::chrono::duration<double>(now - mLastUpdate).count(), 0.0, 0.25);
    mLastUpdate = now;
    mAccumulatorSeconds += elapsed;

    while (mAccumulatorSeconds >= kTickSeconds && result.worldSteps < kMaximumCatchupSteps) {
        ++mCurrentTick;
        if ((mCurrentTick & 1U) == 0) {
            mPlayers.StepFixed();
            mObjectives.Update(mPlayers, static_cast<float>(kTickSeconds * 2.0));
            ProcessObjectiveCapturedEvents();
            ++result.playerSteps;
        }
        mProjectiles.StepFixed(mPlayers, mStructures);
        // Combat and projectiles may make an owner ineligible during this
        // same tick. Retire dependent entities before fishing advances.
        CleanupIneligibleFishingOwners();
        mFishing.StepFixed(mPlayers);
        mAccumulatorSeconds -= kTickSeconds;
        ++result.worldSteps;
    }
    if (result.worldSteps == kMaximumCatchupSteps) {
        mAccumulatorSeconds = std::min(mAccumulatorSeconds, kTickSeconds);
    }
    return result;
}

void ServerWorld::Reset() {
    mPlayers.Reset();
    mSceneTransitions.Reset();
    mLastSceneEntryReceipts.clear();
    mProjectiles.Reset();
    mLastArrowFireDecisions.clear();
    mFishing.Reset();
    mObjectives.Reset();
    mPendingObjectiveCapturedEvents.clear();
    mStrategicTopology.Reset();
    mStructures.Reset();
    mIntentAdmission.Reset();
    mCorpses.Reset();
    mPlayerLoadouts.Reset();
    mLastUpdate = {};
    mAccumulatorSeconds = 0.0;
    mCurrentTick = 0;
}

std::optional<PlayerSnapshot> ServerWorld::AdmitPlayer(int32_t playerId, const PlayerSpawn& spawn) {
    if (playerId < 0 || spawn.sceneId < 0 || !mPlayers.EnsurePlayer(playerId, spawn).Valid() ||
        !mPlayerLoadouts.EnsurePlayer(playerId)) {
        mPlayers.RemovePlayer(playerId);
        return std::nullopt;
    }
    const std::optional<PlayerSnapshot> admitted = mPlayers.SnapshotForPlayer(playerId);
    if (!admitted) {
        mPlayers.RemovePlayer(playerId);
    }
    return admitted;
}

std::optional<PlayerSnapshot> ServerWorld::AdmitPlayerAtDefaultSpawn(int32_t playerId) {
    const auto spawn = mSceneTransitions.DefaultSpawn();
    return spawn ? AdmitPlayer(playerId, *spawn) : std::nullopt;
}

ServerPlayerDeparture ServerWorld::RemovePlayer(int32_t playerId) {
    ServerPlayerDeparture departure{};
    departure.player = mPlayers.SnapshotForPlayer(playerId);
    mFishing.ReleaseOwnedBy(playerId);
    mFishing.RemoveLure(playerId);
    mProjectiles.RemoveOwnedBy(playerId);
    mIntentAdmission.RemovePlayer(playerId);
    mSceneTransitions.RevokePlayer(playerId);
    mLastSceneEntryReceipts.erase(playerId);
    mLastArrowFireDecisions.erase(playerId);
    mPlayerLoadouts.RemovePlayer(playerId);
    mPlayers.RemovePlayer(playerId);
    return departure;
}

bool ServerWorld::ConfigureSceneSpawn(const PlayerSpawn& spawn) {
    return mSceneTransitions.ConfigureSpawn(spawn);
}

bool ServerWorld::AuthorizeSceneTransition(int32_t playerId,
                                           int32_t destinationSceneId) {
    const auto player = mPlayers.SnapshotForPlayer(playerId);
    return player && player->health != 0 &&
           mSceneTransitions.Grant(playerId, player->lifeEpoch,
                                   destinationSceneId);
}

std::optional<ServerSceneEntryOutcome> ServerWorld::ExecuteSceneEntry(
    const SceneEntryCommand& command) {
    if (command.playerId < 0 || command.sequence == 0 || command.lifeEpoch == 0) {
        return std::nullopt;
    }
    auto current = mPlayers.SnapshotForPlayer(command.playerId);
    if ((current && current->lifeEpoch != command.lifeEpoch) ||
        (!current && command.lifeEpoch != 1)) {
        // Reject an old incarnation before scene replay admission so a valid
        // request from the current life may reuse the same sequence.
        return std::nullopt;
    }
    const uint32_t authoritativeLifeEpoch = current ? current->lifeEpoch : 1;
    const ServerIntentResult admission = mIntentAdmission.Admit(
        command.playerId, authoritativeLifeEpoch, command.lifeEpoch,
        ServerIntentKind::SceneEntry, command.sequence);
    if (admission == ServerIntentResult::Invalid ||
        admission == ServerIntentResult::Stale) {
        return std::nullopt;
    }
    ServerSceneEntryOutcome outcome{};
    if (admission == ServerIntentResult::Duplicate) {
        const auto receipt = mLastSceneEntryReceipts.find(command.playerId);
        if (receipt == mLastSceneEntryReceipts.end() ||
            receipt->second.sequence != command.sequence ||
            receipt->second.lifeEpoch != command.lifeEpoch) {
            return std::nullopt;
        }
        outcome.player = current;
        outcome.accepted = receipt->second.accepted && current &&
                           current->sceneId == receipt->second.destinationSceneId;
        return outcome;
    }
    const auto decision = mSceneTransitions.Evaluate(
        command.playerId, command.sequence, authoritativeLifeEpoch, !current);
    if (!decision) return std::nullopt;
    const auto record = [&](bool accepted, int32_t destinationSceneId) {
        mLastSceneEntryReceipts.insert_or_assign(
            command.playerId,
            SceneEntryReceipt{ command.sequence, command.lifeEpoch,
                               destinationSceneId, accepted });
    };
    if (decision->result == SceneEntryResult::Rejected) {
        outcome.player = current;
        record(false, current ? current->sceneId : -1);
        return outcome;
    }
    if (!decision->spawn) return std::nullopt;

    if (!current) {
        outcome.player = AdmitPlayer(command.playerId, *decision->spawn);
        outcome.admitted = outcome.player.has_value();
        outcome.accepted = outcome.admitted;
        record(outcome.accepted, decision->spawn->sceneId);
        return outcome;
    }
    if (current->sceneId == decision->spawn->sceneId) {
        outcome.accepted = true;
        outcome.player = current;
        record(true, current->sceneId);
        return outcome;
    }
    if (!mPlayers.ChangeScene(command.playerId, *decision->spawn)) {
        return std::nullopt;
    }
    mFishing.ReleaseOwnedBy(command.playerId);
    mFishing.RemoveLure(command.playerId);
    mProjectiles.RemoveOwnedBy(command.playerId);
    outcome.player = mPlayers.SnapshotForPlayer(command.playerId);
    outcome.changedScene = outcome.player.has_value();
    outcome.accepted = outcome.changedScene;
    record(outcome.accepted, decision->spawn->sceneId);
    return outcome;
}

void ServerWorld::SetPlayerCollisionQuery(SegmentCast segmentCast) {
    mPlayers.SetCollisionQuery(std::move(segmentCast));
}

void ServerWorld::SetPlayerCollisionSceneQuery(
    CollisionSceneQuery collisionSceneQuery) {
    mPlayers.SetCollisionSceneQuery(std::move(collisionSceneQuery));
}

void ServerWorld::SetPlayerWaterSurfaceQuery(
    WaterSurfaceQuery waterSurfaceQuery) {
    mPlayers.SetWaterSurfaceQuery(std::move(waterSurfaceQuery));
}

void ServerWorld::SetPlayerClimbSurfaceQuery(
    ClimbSurfaceQuery climbSurfaceQuery) {
    mPlayers.SetClimbSurfaceQuery(std::move(climbSurfaceQuery));
}

bool ServerWorld::SubmitPlayerCommand(const PlayerCommand& command) {
    const auto player = mPlayers.SnapshotForPlayer(command.ownerPlayerId);
    if (!player) return false;
    const auto decision = mPlayerLoadouts.Evaluate(*player, command);
    return decision && mPlayers.SubmitCommand(decision->command);
}

bool ServerWorld::ExecuteWeaponSelection(const WeaponSelectionCommand& command) {
    if (command.sequence == 0 || command.selectedWeapon > 4) return false;
    const auto player = mPlayers.SnapshotForPlayer(command.playerId);
    if (!player ||
        mIntentAdmission.Admit(command.playerId, player->lifeEpoch, command.lifeEpoch,
                              ServerIntentKind::WeaponSelection, command.sequence) !=
            ServerIntentResult::Fresh ||
        player->health == 0 ||
        !mPlayerLoadouts.AllowsSelection(command.playerId, command.selectedWeapon)) {
        return false;
    }

    const uint8_t previousWeapon = player->selectedWeapon;
    if (!mPlayers.SelectWeapon(command.playerId, command.selectedWeapon)) return false;
    if (previousWeapon == static_cast<uint8_t>(PlayerWeaponSlot::FishingPole) &&
        command.selectedWeapon != static_cast<uint8_t>(PlayerWeaponSlot::FishingPole)) {
        mFishing.ReleaseOwnedBy(command.playerId);
        mFishing.RemoveLure(command.playerId);
    }
    return true;
}

bool ServerWorld::ConfigurePlayerLoadout(int32_t playerId,
                                         const PlayerLoadout& loadout) {
    const auto player = mPlayers.SnapshotForPlayer(playerId);
    if (!player || !mPlayerLoadouts.ConfigurePlayer(playerId, loadout)) return false;
    const uint8_t selectedWeapon = loadout.FallbackWeapon(player->selectedWeapon);
    if (!mPlayers.SelectWeapon(playerId, selectedWeapon)) return false;
    if (player->selectedWeapon == static_cast<uint8_t>(PlayerWeaponSlot::FishingPole) &&
        selectedWeapon != static_cast<uint8_t>(PlayerWeaponSlot::FishingPole)) {
        mFishing.ReleaseOwnedBy(playerId);
        mFishing.RemoveLure(playerId);
    }
    return true;
}

bool ServerWorld::SetPlayerTeam(int32_t playerId, TeamId team) {
    return mPlayers.SetPlayerTeam(playerId, team);
}

std::optional<PlayerSnapshot> ServerWorld::PlayerFor(int32_t playerId) const {
    return mPlayers.SnapshotForPlayer(playerId);
}

std::vector<PlayerSnapshot> ServerWorld::PlayerSnapshots() const {
    return mPlayers.Snapshots();
}

std::vector<CombatResultEvent> ServerWorld::DrainCombatResults() {
    return mPlayers.DrainCombatResults();
}

std::vector<PlayerLifeEvent> ServerWorld::DrainPlayerLifeEvents() {
    // Enforce the invariant for administrative/test damage performed outside
    // Advance(). Normal gameplay has already cleaned up in the fixed tick.
    CleanupIneligibleFishingOwners();
    std::vector<PlayerLifeEvent> events = mPlayers.DrainLifeEvents();
    for (const PlayerLifeEvent& event : events) {
        if (event.kind == PlayerLifeEventKind::Died) {
            mProjectiles.DetachFromPlayerLife(event.playerId,
                                              event.lifeEpoch);
            mCorpses.Create(BuildCorpsePose(event));
        }
    }
    return events;
}

void ServerWorld::CleanupIneligibleFishingOwners() {
    const std::vector<PlayerSnapshot> players = mPlayers.Snapshots();
    mFishing.RemoveIneligibleOwners(players);
}

void ServerWorld::ProcessObjectiveCapturedEvents() {
    std::vector<ObjectiveCapturedEvent> captured =
        mObjectives.DrainCapturedEvents();
    for (const ObjectiveCapturedEvent& event : captured) {
        for (const StructureSnapshot& structure : mStructures.Snapshots()) {
            if (structure.objectiveKey == event.objectiveKey) {
                mStructures.ResetStructure(structure.structureKey);
            }
        }
        mPendingObjectiveCapturedEvents.push_back(event);
    }
}

void ServerWorld::SetProjectileCollisionQuery(SegmentCast segmentCast) {
    mProjectiles.SetCollisionQuery(std::move(segmentCast));
}

ArrowFireDecision ServerWorld::ExecuteArrowFire(const ArrowFireCommand& command) {
    ArrowFireDecision decision{ command.sequence, command.lifeEpoch, 0, false };
    if (command.playerId < 0 || command.sequence == 0 || command.lifeEpoch == 0) return decision;
    const auto shooter = mPlayers.SnapshotForPlayer(command.playerId);
    if (!shooter || shooter->lifeEpoch != command.lifeEpoch) return decision;
    const ServerIntentResult admission =
        mIntentAdmission.Admit(command.playerId, shooter->lifeEpoch, command.lifeEpoch,
                               ServerIntentKind::Projectile, command.sequence);
    if (admission == ServerIntentResult::Duplicate) {
        const auto previous = mLastArrowFireDecisions.find(command.playerId);
        if (previous != mLastArrowFireDecisions.end() &&
            previous->second.sequence == command.sequence &&
            previous->second.lifeEpoch == command.lifeEpoch) {
            return previous->second;
        }
        return decision;
    }
    if (admission != ServerIntentResult::Fresh) return decision;
    if (shooter->health == 0 || shooter->selectedWeapon != 3 ||
        !mPlayers.BowShotReady(command.playerId) ||
        !mIntentAdmission.CooldownReady(command.playerId, ServerIntentKind::Projectile,
                                        mCurrentTick)) {
        mLastArrowFireDecisions.insert_or_assign(command.playerId, decision);
        return decision;
    }
    // A reliable fire intent may arrive after newer disposable movement.
    // Resolve aim from the exact admitted native input sample that fired it,
    // rather than from whichever orientation is current at packet arrival.
    // Origin, collision, and damage remain server-owned.
    const auto fireSample =
        mPlayers.SubmittedCommandForPlayerAtClientTick(command.playerId,
                                                       command.clientTick);
    const auto latestSample =
        mPlayers.SubmittedCommandForPlayer(command.playerId);
    const bool fireNewerThanLatest = latestSample &&
        Sequence::IsNewer(command.clientTick, latestSample->clientTick);
    const uint32_t sampleDistance = !latestSample
        ? std::numeric_limits<uint32_t>::max()
        : fireNewerThanLatest
            ? command.clientTick - latestSample->clientTick
            : latestSample->clientTick - command.clientTick;
    if (!latestSample || sampleDistance > kMaximumArrowFireSampleAge) {
        mLastArrowFireDecisions.insert_or_assign(command.playerId, decision);
        return decision;
    }
    // Prefer the server-admitted command. If that disposable movement packet
    // was lost, the reliable action's compact aim input preserves the shot.
    // Its timestamp is still bounded against the latest admitted input and it
    // cannot provide an origin, target, collision, or damage result.
    const float fireHeading = fireSample
        ? fireSample->headingRadians
        : static_cast<float>(command.heading) * kBinaryAngleToRadians;
    const float firePitch = fireSample
        ? fireSample->aimPitchRadians
        : static_cast<float>(command.aimPitch) * kBinaryAngleToRadians;
    const float horizontal = std::cos(firePitch) * kArrowSpeed;
    const ArrowSpawn spawn{
        command.playerId, shooter->sceneId, kNormalArrowType,
        { shooter->position.x + std::sin(fireHeading) * 14.0f,
          shooter->position.y + 42.0f,
          shooter->position.z + std::cos(fireHeading) * 14.0f },
        { std::sin(fireHeading) * horizontal,
          -std::sin(firePitch) * kArrowSpeed,
          std::cos(fireHeading) * horizontal },
        static_cast<int16_t>(std::lround(fireHeading * kRadiansToBinaryAngle))
    };
    const auto spawned = mProjectiles.SpawnArrow(spawn);
    if (!spawned) {
        mLastArrowFireDecisions.insert_or_assign(command.playerId, decision);
        return decision;
    }
    if (!mPlayers.CommitBowShot(command.playerId)) {
        // ServerWorld is single-threaded, so readiness cannot normally change
        // between validation and creation. Keep the invariant explicit if
        // simulation ownership changes in the future.
        mProjectiles.RemoveArrow(command.playerId, spawned->replicationId);
        mLastArrowFireDecisions.insert_or_assign(command.playerId, decision);
        return decision;
    }
    mIntentAdmission.RecordAccepted(command.playerId, ServerIntentKind::Projectile,
                                    mCurrentTick);
    decision.projectileId = spawned->replicationId;
    decision.accepted = true;
    mLastArrowFireDecisions.insert_or_assign(command.playerId, decision);
    return decision;
}

std::vector<ArrowSnapshot> ServerWorld::ArrowSnapshots() const {
    return mProjectiles.Snapshots();
}

std::vector<ArrowEvent> ServerWorld::DrainArrowEvents() {
    return mProjectiles.DrainEvents();
}

void ServerWorld::SetFishingCollisionQuery(SegmentCast segmentCast) {
    mFishing.SetCollisionQuery(std::move(segmentCast));
}

void ServerWorld::SetFishingWaterSurfaceQuery(FishingSimulation::WaterSurfaceQuery waterSurfaceQuery) {
    mFishing.SetWaterSurfaceQuery(std::move(waterSurfaceQuery));
}

bool ServerWorld::ExecuteLureControl(const LureControlCommand& command) {
    if (command.playerId < 0 || command.sequence == 0 || command.lifeEpoch == 0) {
        return false;
    }
    const auto owner = mPlayers.SnapshotForPlayer(command.playerId);
    if (!owner || owner->lifeEpoch != command.lifeEpoch) return false;
    const ServerIntentResult admission =
        mIntentAdmission.Admit(command.playerId, owner->lifeEpoch, command.lifeEpoch,
                               ServerIntentKind::Lure, command.sequence);
    if (admission != ServerIntentResult::Fresh) {
        return admission == ServerIntentResult::Duplicate;
    }
    if (owner->health == 0 || owner->selectedWeapon != 4) {
        return false;
    }
    if (command.deployed && !CanPerformFishingAction(*owner)) return false;
    PlayerSnapshot castOwner = *owner;
    if (const auto submitted = mPlayers.SubmittedCommandForPlayer(command.playerId)) {
        // Lure deployment can arrive after its command but before the next
        // fixed step. Use that admitted orientation for this dependent spawn;
        // position, life, scene, equipment, and all movement remain sourced
        // from the authoritative player snapshot.
        castOwner.headingRadians = submitted->headingRadians;
        castOwner.aimPitchRadians = submitted->aimPitchRadians;
    }
    return mFishing.ApplyLureControl(command.playerId, owner->sceneId,
                                     command.deployed, command.reelHeld,
                                     kSinkingLureType, castOwner);
}

std::optional<FishingLureSnapshot> ServerWorld::LureForPlayer(int32_t playerId) const {
    return mFishing.LureForPlayer(playerId);
}

std::vector<FishingLureSnapshot> ServerWorld::LureSnapshots() const {
    return mFishing.LureSnapshots();
}

bool ServerWorld::RegisterFish(const FishDefinition& definition) {
    return mFishing.RegisterFish(definition);
}

size_t ServerWorld::RegisteredFishCount() const {
    return mFishing.RegisteredFishCount();
}

bool ServerWorld::ExecuteFishAction(const FishActionCommand& command) {
    if (command.playerId < 0 || command.sequence == 0 || command.lifeEpoch == 0) return false;
    const auto player = mPlayers.SnapshotForPlayer(command.playerId);
    if (!player || player->lifeEpoch != command.lifeEpoch) return false;
    const ServerIntentResult admission =
        mIntentAdmission.Admit(command.playerId, player->lifeEpoch, command.lifeEpoch,
                               ServerIntentKind::Fish, command.sequence);
    if (admission != ServerIntentResult::Fresh) {
        return admission == ServerIntentResult::Duplicate;
    }
    if (player->health == 0 || player->selectedWeapon != 4) {
        return false;
    }
    if (command.action == FishActionKind::Release) {
        const auto owned = mFishing.FishOwnedBy(command.playerId);
        return owned && mFishing.Release(owned->identity, command.playerId);
    }
    if (!CanPerformFishingAction(*player)) return false;
    return mFishing.HookNearestRegistered(command.playerId);
}

std::optional<FishSnapshot> ServerWorld::FishOwnedBy(int32_t playerId) const {
    return mFishing.FishOwnedBy(playerId);
}

std::vector<FishSnapshot> ServerWorld::FishSnapshots() const {
    return mFishing.Snapshots();
}

std::vector<FishingLureEvent> ServerWorld::DrainFishingLureEvents() {
    return mFishing.DrainLureEvents();
}

EntityId ServerWorld::EnsureObjective(const ObjectiveDefinition& definition) {
    if (definition.objectiveKey < 0 || definition.sceneId < 0) return {};
    return mObjectives.EnsureObjective(definition);
}

bool ServerWorld::RemoveObjective(int32_t objectiveKey) {
    if (!mObjectives.SnapshotForObjective(objectiveKey)) return false;
    std::vector<int32_t> dependentStructures;
    for (const StructureSnapshot& structure : mStructures.Snapshots()) {
        if (structure.objectiveKey == objectiveKey) dependentStructures.push_back(structure.structureKey);
    }
    for (const int32_t structureKey : dependentStructures) {
        mStructures.RemoveStructure(structureKey);
    }
    std::erase_if(mPendingObjectiveCapturedEvents,
                  [objectiveKey](const ObjectiveCapturedEvent& event) {
                      return event.objectiveKey == objectiveKey;
                  });
    mStrategicTopology.RemoveSite(objectiveKey);
    return mObjectives.RemoveObjective(objectiveKey);
}

std::vector<ObjectiveSnapshot> ServerWorld::ObjectiveSnapshots() const {
    return mObjectives.Snapshots();
}

std::vector<ObjectiveCapturedEvent> ServerWorld::DrainObjectiveCapturedEvents() {
    std::vector<ObjectiveCapturedEvent> events;
    events.swap(mPendingObjectiveCapturedEvents);
    return events;
}

EntityId ServerWorld::EnsureStrategicSite(const ObjectiveDefinition& objective,
                                          StrategicSiteKind kind,
                                          int32_t influenceRegionKey) {
    const bool existed = mObjectives.SnapshotForObjective(objective.objectiveKey).has_value();
    const EntityId entity = EnsureObjective(objective);
    if (!entity.Valid()) return {};
    if (mStrategicTopology.EnsureSite({ objective.objectiveKey, kind, influenceRegionKey })) {
        return entity;
    }
    if (!existed) mObjectives.RemoveObjective(objective.objectiveKey);
    return {};
}

bool ServerWorld::EnsureSupplyRoute(const SupplyRouteDefinition& definition) {
    const auto source = mObjectives.SnapshotForObjective(definition.sourceObjectiveKey);
    const auto destination = mObjectives.SnapshotForObjective(definition.destinationObjectiveKey);
    return source && destination && source->sceneId == destination->sceneId &&
           mStrategicTopology.EnsureSupplyRoute(definition);
}

bool ServerWorld::RemoveSupplyRoute(int32_t routeKey) {
    return mStrategicTopology.RemoveSupplyRoute(routeKey);
}

bool ServerWorld::EnsureInfluenceAdjacency(
    const InfluenceRegionAdjacencyDefinition& definition) {
    const auto lowerSite =
        mStrategicTopology.SiteForInfluenceRegion(definition.lowerRegionKey);
    const auto upperSite =
        mStrategicTopology.SiteForInfluenceRegion(definition.upperRegionKey);
    if (!lowerSite || !upperSite) return false;
    const auto lowerObjective =
        mObjectives.SnapshotForObjective(lowerSite->objectiveKey);
    const auto upperObjective =
        mObjectives.SnapshotForObjective(upperSite->objectiveKey);
    return lowerObjective && upperObjective &&
           lowerObjective->sceneId == upperObjective->sceneId &&
           mStrategicTopology.EnsureInfluenceAdjacency(definition);
}

bool ServerWorld::RemoveInfluenceAdjacency(int32_t adjacencyKey) {
    return mStrategicTopology.RemoveInfluenceAdjacency(adjacencyKey);
}

std::vector<StrategicSiteDefinition> ServerWorld::StrategicSites() const {
    return mStrategicTopology.Sites();
}

std::vector<SupplyRouteDefinition> ServerWorld::SupplyRoutes() const {
    return mStrategicTopology.SupplyRoutes();
}

std::vector<InfluenceRegionAdjacencyDefinition>
ServerWorld::InfluenceAdjacencies() const {
    return mStrategicTopology.InfluenceAdjacencies();
}

bool ServerWorld::HasFriendlySupply(int32_t destinationObjectiveKey) const {
    const auto destinationSite = mStrategicTopology.SiteForObjective(destinationObjectiveKey);
    const auto destination = mObjectives.SnapshotForObjective(destinationObjectiveKey);
    if (!destinationSite || !destination || destination->owner == TeamId::Neutral ||
        (destinationSite->kind != StrategicSiteKind::Tower &&
         destinationSite->kind != StrategicSiteKind::Keep)) {
        return false;
    }
    for (const int32_t sourceKey : mStrategicTopology.SupplySourcesFor(destinationObjectiveKey)) {
        const auto source = mObjectives.SnapshotForObjective(sourceKey);
        if (source && source->owner == destination->owner) return true;
    }
    return false;
}

EntityId ServerWorld::EnsureStructure(const StructureDefinition& definition) {
    if (definition.structureKey < 0 || definition.objectiveKey < 0 || definition.sceneId < 0 ||
        !mObjectives.SnapshotForObjective(definition.objectiveKey)) {
        return {};
    }
    return mStructures.EnsureStructure(definition);
}

bool ServerWorld::RemoveStructure(int32_t structureKey) {
    return mStructures.SnapshotForStructure(structureKey).has_value() &&
           mStructures.RemoveStructure(structureKey);
}

std::vector<StructureSnapshot> ServerWorld::StructureSnapshots() const {
    return mStructures.Snapshots();
}

std::vector<StructureEvent> ServerWorld::DrainStructureEvents() {
    return mStructures.DrainEvents();
}

StructureActionDecision ServerWorld::ExecuteStructureAction(const StructureActionCommand& command) {
    StructureActionDecision rejected{};
    rejected.command = command;
    if (command.playerId < 0 || command.sequence == 0 || command.lifeEpoch == 0) {
        return rejected;
    }
    const auto player = mPlayers.SnapshotForPlayer(command.playerId);
    if (!player || player->lifeEpoch != command.lifeEpoch) {
        rejected.result = StructureActionResult::StaleLife;
        return rejected;
    }
    const ServerIntentResult admission =
        mIntentAdmission.Admit(command.playerId, player->lifeEpoch, command.lifeEpoch,
                               ServerIntentKind::Structure, command.sequence);
    if (admission != ServerIntentResult::Fresh) {
        rejected.result = StructureActionResult::Replayed;
        return rejected;
    }
    if (!mIntentAdmission.CooldownReady(command.playerId, ServerIntentKind::Structure,
                                        mCurrentTick)) {
        StructureActionDecision decision{};
        decision.command = command;
        decision.result = StructureActionResult::RateLimited;
        return decision;
    }

    bool hasRequiredSupply = true;
    if (const auto structure = mStructures.SnapshotForStructure(command.structureKey)) {
        if (const auto site =
                mStrategicTopology.SiteForObjective(structure->objectiveKey)) {
            if (site->kind == StrategicSiteKind::Tower ||
                site->kind == StrategicSiteKind::Keep) {
                hasRequiredSupply = HasFriendlySupply(structure->objectiveKey);
            }
        }
    }
    StructureActionDecision decision = mStructureActions.Execute(
        command, mPlayers, mObjectives, mStructures, hasRequiredSupply);
    if (decision.Accepted()) {
        mIntentAdmission.RecordAccepted(command.playerId, ServerIntentKind::Structure,
                                        mCurrentTick);
    }
    return decision;
}

EntityId ServerWorld::CreateCorpse(const CorpsePose& pose) {
    return mCorpses.Create(pose);
}

std::vector<CorpseSnapshot> ServerWorld::CorpseSnapshots() const {
    return mCorpses.Snapshots();
}

} // namespace Game::Simulation
