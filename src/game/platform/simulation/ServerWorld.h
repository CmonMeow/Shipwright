#pragma once

#include "CorpseSimulation.h"
#include "FishingSimulation.h"
#include "ObjectiveSimulation.h"
#include "PlayerLoadoutPolicy.h"
#include "PlayerSimulation.h"
#include "ProjectileSimulation.h"
#include "SceneTransitionAuthority.h"
#include "ServerIntentAdmission.h"
#include "StrategicWorldTopology.h"
#include "StructureSimulation.h"
#include "StructureActionAuthority.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Game::Simulation {

struct ServerWorldUpdate {
    uint32_t worldSteps = 0;
    uint32_t playerSteps = 0;
};

struct ServerPlayerDeparture {
    std::optional<PlayerSnapshot> player;
};

struct SceneEntryCommand {
    int32_t playerId = -1;
    uint32_t sequence = 0;
    uint32_t lifeEpoch = 0;
};

struct WeaponSelectionCommand {
    int32_t playerId = -1;
    uint32_t sequence = 0;
    uint32_t lifeEpoch = 0;
    uint8_t selectedWeapon = 0;
};

struct ServerSceneEntryOutcome {
    bool accepted = false;
    bool admitted = false;
    bool changedScene = false;
    std::optional<PlayerSnapshot> player;
};

enum class FishActionKind : uint8_t { Hook, Release };

struct LureControlCommand {
    int32_t playerId = -1;
    uint32_t sequence = 0;
    uint32_t lifeEpoch = 0;
    bool deployed = false;
    bool reelHeld = false;
};

struct FishActionCommand {
    int32_t playerId = -1;
    uint32_t sequence = 0;
    uint32_t lifeEpoch = 0;
    FishActionKind action = FishActionKind::Hook;
};

struct ArrowFireCommand {
    int32_t playerId = -1;
    uint32_t sequence = 0;
    uint32_t lifeEpoch = 0;
    uint32_t clientTick = 0;
    int16_t heading = 0;
    int16_t aimPitch = 0;
};

struct ArrowFireDecision {
    uint32_t sequence = 0;
    uint32_t lifeEpoch = 0;
    int32_t projectileId = 0;
    bool accepted = false;
};

class ServerWorld final {
  public:
    using Clock = std::chrono::steady_clock;

    explicit ServerWorld(int32_t defaultSceneId = 110);

    ServerWorldUpdate Advance(Clock::time_point now = Clock::now());
    void Reset();

    std::optional<PlayerSnapshot> AdmitPlayer(int32_t playerId, const PlayerSpawn& spawn);
    std::optional<PlayerSnapshot> AdmitPlayerAtDefaultSpawn(int32_t playerId);
    ServerPlayerDeparture RemovePlayer(int32_t playerId);
    bool ConfigureSceneSpawn(const PlayerSpawn& spawn);
    bool AuthorizeSceneTransition(int32_t playerId, int32_t destinationSceneId);
    std::optional<ServerSceneEntryOutcome> ExecuteSceneEntry(const SceneEntryCommand& command);
    void SetPlayerCollisionQuery(SegmentCast segmentCast);
    void SetPlayerCollisionSceneQuery(CollisionSceneQuery collisionSceneQuery);
    void SetPlayerWaterSurfaceQuery(WaterSurfaceQuery waterSurfaceQuery);
    void SetPlayerClimbSurfaceQuery(ClimbSurfaceQuery climbSurfaceQuery);
    bool SubmitPlayerCommand(const PlayerCommand& command);
    bool ExecuteWeaponSelection(const WeaponSelectionCommand& command);
    bool ConfigurePlayerLoadout(int32_t playerId, const PlayerLoadout& loadout);
    bool SetPlayerTeam(int32_t playerId, TeamId team);
    std::optional<PlayerSnapshot> PlayerFor(int32_t playerId) const;
    std::vector<PlayerSnapshot> PlayerSnapshots() const;
    std::vector<CombatResultEvent> DrainCombatResults();
    std::vector<PlayerLifeEvent> DrainPlayerLifeEvents();

    void SetProjectileCollisionQuery(SegmentCast segmentCast);
    ArrowFireDecision ExecuteArrowFire(const ArrowFireCommand& command);
    std::vector<ArrowSnapshot> ArrowSnapshots() const;
    std::vector<ArrowEvent> DrainArrowEvents();

    void SetFishingCollisionQuery(SegmentCast segmentCast);
    void SetFishingWaterSurfaceQuery(FishingSimulation::WaterSurfaceQuery waterSurfaceQuery);
    bool ExecuteLureControl(const LureControlCommand& command);
    std::optional<FishingLureSnapshot> LureForPlayer(int32_t playerId) const;
    std::vector<FishingLureSnapshot> LureSnapshots() const;
    bool RegisterFish(const FishDefinition& definition);
    size_t RegisteredFishCount() const;
    bool ExecuteFishAction(const FishActionCommand& command);
    std::optional<FishSnapshot> FishOwnedBy(int32_t playerId) const;
    std::vector<FishSnapshot> FishSnapshots() const;
    std::vector<FishingLureEvent> DrainFishingLureEvents();

    EntityId EnsureObjective(const ObjectiveDefinition& definition);
    bool RemoveObjective(int32_t objectiveKey);
    std::vector<ObjectiveSnapshot> ObjectiveSnapshots() const;
    std::vector<ObjectiveCapturedEvent> DrainObjectiveCapturedEvents();
    EntityId EnsureStrategicSite(const ObjectiveDefinition& objective,
                                 StrategicSiteKind kind, int32_t influenceRegionKey);
    bool EnsureSupplyRoute(const SupplyRouteDefinition& definition);
    bool RemoveSupplyRoute(int32_t routeKey);
    bool EnsureInfluenceAdjacency(
        const InfluenceRegionAdjacencyDefinition& definition);
    bool RemoveInfluenceAdjacency(int32_t adjacencyKey);
    std::vector<StrategicSiteDefinition> StrategicSites() const;
    std::vector<SupplyRouteDefinition> SupplyRoutes() const;
    std::vector<InfluenceRegionAdjacencyDefinition> InfluenceAdjacencies() const;
    bool HasFriendlySupply(int32_t destinationObjectiveKey) const;
    EntityId EnsureStructure(const StructureDefinition& definition);
    bool RemoveStructure(int32_t structureKey);
    std::vector<StructureSnapshot> StructureSnapshots() const;
    std::vector<StructureEvent> DrainStructureEvents();
    StructureActionDecision ExecuteStructureAction(const StructureActionCommand& command);
    EntityId CreateCorpse(const CorpsePose& pose);
    std::vector<CorpseSnapshot> CorpseSnapshots() const;

    uint64_t CurrentTick() const { return mCurrentTick; }

  private:
    friend class ServerWorldTestAccess;

    static constexpr double kTickSeconds = 1.0 / 60.0;
    static constexpr uint32_t kMaximumCatchupSteps = 15;

    void CleanupIneligibleFishingOwners();
    void ProcessObjectiveCapturedEvents();

    PlayerSimulation mPlayers;
    PlayerLoadoutPolicy mPlayerLoadouts;
    SceneTransitionAuthority mSceneTransitions;
    struct SceneEntryReceipt {
        uint32_t sequence = 0;
        uint32_t lifeEpoch = 0;
        int32_t destinationSceneId = -1;
        bool accepted = false;
    };
    std::map<int32_t, SceneEntryReceipt> mLastSceneEntryReceipts;
    ProjectileSimulation mProjectiles;
    std::map<int32_t, ArrowFireDecision> mLastArrowFireDecisions;
    FishingSimulation mFishing;
    ObjectiveSimulation mObjectives;
    std::vector<ObjectiveCapturedEvent> mPendingObjectiveCapturedEvents;
    StrategicWorldTopology mStrategicTopology;
    StructureSimulation mStructures;
    StructureActionAuthority mStructureActions;
    ServerIntentAdmission mIntentAdmission;
    CorpseSimulation mCorpses;
    Clock::time_point mLastUpdate{};
    double mAccumulatorSeconds = 0.0;
    uint64_t mCurrentTick = 0;
};

} // namespace Game::Simulation
