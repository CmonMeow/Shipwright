#pragma once

#include "EntityRegistry.h"
#include "EntityId.h"
#include "CombatHitRegion.h"
#include "SpatialGridIndex.h"
#include "Vec3.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace Game::Simulation {

enum PlayerAction : uint16_t {
    PLAYER_ACTION_PRIMARY = 1 << 0,
    PLAYER_ACTION_BLOCK = 1 << 1,
    PLAYER_ACTION_AIM = 1 << 2,
    PLAYER_ACTION_EVADE = 1 << 3,
};

enum class PlayerActionState : uint8_t {
    Idle,
    PrimaryWindup,
    PrimaryActive,
    PrimaryRecovery,
    Blocking,
    Aiming,
    Evading,
    JumpSlashing,
    SpinAttacking,
};

// Directional sword move selected by authority from the same movement sample
// that caused the attack. This is semantic gameplay state, not a client-owned
// animation id; rendering and melee collision both consume it.
enum class MeleeAttackVariant : uint8_t {
    ForwardSlash,
    ForwardCombo,
    RightSlash,
    RightCombo,
    LeftSlash,
    LeftCombo,
};

struct MeleeAttackTiming {
    uint32_t windupTicks = 0;
    uint32_t activeTicks = 1;
    uint32_t recoveryTicks = 1;
};

struct NativeMeleeAnimationTiming {
    uint8_t frameCount = 1;
    uint8_t activeStartFrame = 0;
    uint8_t activeEndFrame = 0;
};

// Native sword animations advance 2/3 frame per 20 Hz gameplay update.
// Authority runs at 30 Hz, making one native animation frame 9/4 server ticks.
inline constexpr uint32_t NativeMeleeFrameToServerTick(uint32_t frame) {
    return (frame * 9 + 3) / 4;
}

// These values are the frame counts and weapon-active ranges from the native
// D_80854190 combat table. They are semantic combat data, not client animation
// choices, and are therefore shared by headless authority and presentation.
inline constexpr NativeMeleeAnimationTiming NativeMeleeTimingFor(
    MeleeAttackVariant variant, bool twoHanded) {
    switch (variant) {
        case MeleeAttackVariant::ForwardSlash:
            return { 5, 1, 4 };
        case MeleeAttackVariant::ForwardCombo:
            return twoHanded ? NativeMeleeAnimationTiming{ 8, 1, 7 }
                             : NativeMeleeAnimationTiming{ 6, 0, 5 };
        case MeleeAttackVariant::RightSlash:
            return twoHanded ? NativeMeleeAnimationTiming{ 6, 0, 5 }
                             : NativeMeleeAnimationTiming{ 5, 1, 4 };
        case MeleeAttackVariant::RightCombo:
            return twoHanded ? NativeMeleeAnimationTiming{ 9, 3, 8 }
                             : NativeMeleeAnimationTiming{ 9, 2, 8 };
        case MeleeAttackVariant::LeftSlash:
            return twoHanded ? NativeMeleeAnimationTiming{ 6, 0, 5 }
                             : NativeMeleeAnimationTiming{ 5, 0, 4 };
        case MeleeAttackVariant::LeftCombo:
            return twoHanded ? NativeMeleeAnimationTiming{ 6, 1, 5 }
                             : NativeMeleeAnimationTiming{ 7, 0, 6 };
    }
    return {};
}

inline constexpr MeleeAttackTiming MeleeAttackTimingFor(
    MeleeAttackVariant variant, uint8_t selectedWeapon) {
    const NativeMeleeAnimationTiming native =
        NativeMeleeTimingFor(variant, selectedWeapon == 2);
    const uint32_t windup =
        NativeMeleeFrameToServerTick(native.activeStartFrame);
    const uint32_t activeEnd =
        NativeMeleeFrameToServerTick(native.activeEndFrame);
    return { windup, activeEnd > windup ? activeEnd - windup : 1, 1 };
}

enum class TeamId : uint8_t {
    Neutral,
    Red,
    Blue,
    Green,
};

enum class PlayerLocomotionMode : uint8_t {
    Grounded,
    Airborne,
    Swimming,
    Climbing,
};

struct PlayerCommand {
    // Input intent only. Equipment is authoritative PlayerEntity state and is
    // changed exclusively through WeaponSelectionCommand.
    int32_t ownerPlayerId = -1;
    uint32_t sequence = 0;
    uint32_t actionSequence = 0;
    uint32_t lifeEpoch = 1;
    // Native simulation sample time. This is ordering/latency metadata only;
    // the server still chooses when and whether the command executes.
    uint32_t clientTick = 0;
    int32_t sceneId = -1;
    // Keyboard axis convention: negative is left, positive is right.
    float moveX = 0.0f;
    float moveY = 0.0f;
    float headingRadians = 0.0f;
    float aimPitchRadians = 0.0f;
    uint16_t heldActions = 0;
    uint16_t pressedActions = 0;
    MeleeAttackVariant reportedMeleeAttackVariant =
        MeleeAttackVariant::RightSlash;
    bool hasReportedMeleeAttackVariant = false;
    Vec3 reportedPosition{};
    PlayerLocomotionMode reportedLocomotionMode =
        PlayerLocomotionMode::Grounded;
    bool hasReportedPose = false;
};

inline constexpr float kPlayerSimulationTickSeconds = 1.0f / 30.0f;
// Twelve native 20 Hz animation frames occupy eighteen 30 Hz server ticks.
inline constexpr uint32_t kEvadeDurationTicks = 18;
inline constexpr uint32_t kSpinAttackDurationTicks = 18;
// Shortest native re-notch interval represented by authority. This is based
// on simulation time, never packet-arrival timing or client wall-clock time.
inline constexpr uint32_t kBowRefireDurationTicks = 2;
// Native parallel locomotion advances the run animation by approximately one
// animation frame per world unit (playSpeed = linearVelocity * MREG(95)/100).
// The 29-frame run therefore completes after about 29 world units at the
// default movement tuning.
inline constexpr float kPlayerLocomotionCycleDistance = 29.0f;

// Pure horizontal locomotion shared by authoritative simulation and local
// prediction. World and player collision are applied by their respective
// callers after this unconstrained command velocity is calculated.
Vec3 CalculatePlayerVelocity(const PlayerCommand& command);
Vec3 CalculatePlayerEvadeVelocity(const PlayerCommand& command);
MeleeAttackVariant BaseMeleeAttackVariantForCommand(
    const PlayerCommand& command);
MeleeAttackVariant MeleeAttackVariantForCommand(const PlayerCommand& command);
Vec3 AdvancePlayerPosition(const Vec3& position, const PlayerCommand& command,
                           float deltaSeconds);

struct PlayerSpawn {
    int32_t sceneId = -1;
    Vec3 position{};
    float headingRadians = 0.0f;
    TeamId team = TeamId::Neutral;
};

struct PlayerSnapshot {
    EntityId entity{};
    int32_t ownerPlayerId = -1;
    int32_t sceneId = -1;
    uint32_t serverTick = 0;
    uint32_t lastProcessedCommand = 0;
    uint32_t lifeEpoch = 1;
    Vec3 position{};
    Vec3 velocity{};
    float headingRadians = 0.0f;
    float aimPitchRadians = 0.0f;
    uint16_t heldActions = 0;
    uint8_t selectedWeapon = 0;
    PlayerLocomotionMode locomotionMode = PlayerLocomotionMode::Grounded;
    PlayerActionState actionState = PlayerActionState::Idle;
    MeleeAttackVariant meleeAttackVariant = MeleeAttackVariant::RightSlash;
    uint32_t meleeAttackId = 0;
    uint32_t actionStartTick = 0;
    uint8_t health = 48;
    TeamId team = TeamId::Neutral;
    float locomotionPhaseRadians = 0.0f;
};

// Shared server policy for gameplay that requires stable footing. Callers may
// still allow explicit cleanup/retire commands while non-grounded.
bool CanPerformGroundedAction(const PlayerSnapshot& player);
bool CanPerformFishingAction(const PlayerSnapshot& player);

enum class CombatAttackKind : uint8_t {
    Melee,
    Arrow,
    Explosion,
    Environment,
};

enum class CombatResultKind : uint8_t {
    Damaged,
    Blocked,
};

struct CombatResultEvent {
    uint32_t eventId = 0;
    int32_t sourcePlayerId = -1;
    int32_t targetPlayerId = -1;
    EntityId sourceEntity{};
    EntityId targetEntity{};
    uint32_t sourceLifeEpoch = 0;
    uint32_t targetLifeEpoch = 0;
    uint32_t meleeAttackId = 0;
    int32_t sceneId = -1;
    CombatAttackKind attackKind = CombatAttackKind::Environment;
    CombatResultKind result = CombatResultKind::Damaged;
    uint8_t damage = 0;
    int16_t impactHeading = 0;
    Vec3 impactPosition{};
    PlayerHitRegion hitRegion = PlayerHitRegion::None;
};

enum class PlayerLifeEventKind : uint8_t {
    Died,
    Respawned,
};

struct PlayerLifeEvent {
    PlayerLifeEventKind kind = PlayerLifeEventKind::Died;
    int32_t playerId = -1;
    EntityId entity{};
    uint32_t lifeEpoch = 0;
    int32_t sceneId = -1;
    Vec3 position{};
    float headingRadians = 0.0f;
    uint8_t selectedWeapon = 0;
    uint32_t serverTick = 0;
};

// Client-facing authoritative incarnation change. Unlike the server's internal
// PlayerLifeEvent, this is bound to the exact replicated entity generation so a
// delayed respawn cannot be applied to a replacement player entity.
struct PlayerRespawnEvent {
    int32_t playerId = -1;
    EntityId entity{};
    uint32_t lifeEpoch = 0;
    int32_t sceneId = -1;
    uint32_t serverTick = 0;
    Vec3 position{};
    float headingRadians = 0.0f;
    uint8_t selectedWeapon = 0;
};

using SegmentCast = std::function<bool(int32_t sceneId, const Vec3& start, const Vec3& end, Vec3& impact)>;
using CollisionSceneQuery = std::function<bool(int32_t sceneId)>;
using WaterSurfaceQuery =
    std::function<bool(int32_t sceneId, const Vec3& position, float& surfaceY)>;
// A client may identify the native traversal animation it entered, but only
// server collision can authorize movement through a wall during that climb.
using ClimbSurfaceQuery =
    std::function<bool(int32_t sceneId, const Vec3& currentPosition,
                       const Vec3& requestedPosition, float headingRadians)>;

class PlayerSimulation final {
  public:
    static constexpr size_t kMaximumPlayers = 150;
    static constexpr size_t kMaximumPlayersPerTeam = 50;

    void SetCollisionQuery(SegmentCast segmentCast);
    void SetCollisionSceneQuery(CollisionSceneQuery collisionSceneQuery);
    void SetWaterSurfaceQuery(WaterSurfaceQuery waterSurfaceQuery);
    void SetClimbSurfaceQuery(ClimbSurfaceQuery climbSurfaceQuery);
    EntityId EnsurePlayer(int32_t ownerPlayerId, const PlayerSpawn& spawn);
    bool ChangeScene(int32_t ownerPlayerId, const PlayerSpawn& spawn);
    bool RespawnPlayer(int32_t ownerPlayerId);
    bool SelectWeapon(int32_t ownerPlayerId, uint8_t selectedWeapon);
    bool SetPlayerTeam(int32_t ownerPlayerId, TeamId team);
    std::optional<TeamId> TeamForPlayer(int32_t ownerPlayerId) const;
    void RemovePlayer(int32_t ownerPlayerId);
    void Reset();

    bool SubmitCommand(const PlayerCommand& command);
    bool ApplyDamage(int32_t sourcePlayerId, int32_t targetPlayerId, uint8_t damage,
                     int16_t impactHeading,
                     CombatAttackKind attackKind = CombatAttackKind::Environment,
                     const Vec3& impactPosition = {},
                     PlayerHitRegion hitRegion = PlayerHitRegion::None,
                     uint32_t meleeAttackId = 0);
    bool ReportBlockedHit(int32_t sourcePlayerId, int32_t targetPlayerId,
                          CombatAttackKind attackKind, int16_t impactHeading,
                          const Vec3& impactPosition,
                          uint32_t meleeAttackId = 0);
    bool BowShotReady(int32_t ownerPlayerId) const;
    bool CommitBowShot(int32_t ownerPlayerId);
    void StepFixed();
    // Latest server-admitted input, before the next fixed tick commits it to
    // the authoritative snapshot. Causally dependent actions may read its
    // orientation, but movement remains fixed-step only.
    std::optional<PlayerCommand> SubmittedCommandForPlayer(
        int32_t ownerPlayerId) const;
    std::optional<PlayerCommand> SubmittedCommandForPlayerAtClientTick(
        int32_t ownerPlayerId, uint32_t clientTick) const;
    std::optional<PlayerSnapshot> SnapshotForPlayer(int32_t ownerPlayerId) const;
    // Coarse scene/XZ candidates around a world-space point. Callers retain
    // ownership of exact 3D radius and gameplay eligibility checks.
    std::vector<PlayerSnapshot> CandidateSnapshotsNear(
        int32_t sceneId, const Vec3& position, float radius) const;
    // Coarse authoritative combat candidates for a swept segment. The caller
    // must still apply articulated body/shield narrow-phase collision.
    std::vector<PlayerSnapshot> SnapshotsNearSegment(
        int32_t sceneId, const Vec3& start, const Vec3& end,
        float paddingRadius) const;
    std::vector<PlayerSnapshot> Snapshots() const;
    std::vector<CombatResultEvent> DrainCombatResults();
    std::vector<PlayerLifeEvent> DrainLifeEvents();
    uint32_t CurrentTick() const;

  private:
    struct PlayerEntity {
        EntityId id{};
        int32_t ownerPlayerId = -1;
        int32_t sceneId = -1;
        Vec3 position{};
        Vec3 spawnPosition{};
        Vec3 velocity{};
        Vec3 evadeVelocity{};
        float headingRadians = 0.0f;
        float spawnHeadingRadians = 0.0f;
        float aimPitchRadians = 0.0f;
        uint16_t heldActions = 0;
        uint8_t selectedWeapon = 0;
        PlayerLocomotionMode locomotionMode = PlayerLocomotionMode::Grounded;
        PlayerActionState actionState = PlayerActionState::Idle;
        MeleeAttackVariant meleeAttackVariant = MeleeAttackVariant::RightSlash;
        uint8_t repeatedMeleeAttackCount = 0;
        uint32_t meleeAttackId = 0;
        uint32_t actionStartTick = 0;
        uint8_t health = 48;
        uint32_t lifeEpoch = 1;
        TeamId team = TeamId::Neutral;
        uint32_t lastProcessedCommand = 0;
        uint32_t respawnTick = 0;
        PlayerCommand command{};
        PlayerCommand pressedActionSample{};
        uint16_t pressedActionsForNextTick = 0;
        bool hasPressedActionSample = false;
        uint32_t lastReceivedActionSequence = 0;
        uint32_t commandReceivedTick = 0;
        // A native pose belongs to one input sample. The fixed 30 Hz server
        // may simulate the same latest command more than once, but it must
        // never apply that sample's root displacement more than once.
        uint32_t lastAppliedPoseSequence = 0;
        // Consumed and admitted are intentionally distinct. A rejected pose
        // must not drive locomotion presentation merely because its command
        // remains the latest input sample during subsequent fixed ticks.
        uint32_t lastAdmittedPoseSequence = 0;
        uint32_t lastPoseAdmissionServerTick = 0;
        bool nativeClimbAuthorized = false;
        Vec3 nativeClimbLastSurfacePosition{};
        uint32_t nativeClimbLastSurfaceTick = 0;
        uint32_t nativeClimbReentryTick = 0;
        std::deque<PlayerCommand> recentCommands;
        std::set<int32_t> hitPlayers;
        bool hasCommand = false;
        bool bowPrimaryHeld = false;
        bool bowShotArmed = false;
        bool swordPrimaryHeld = false;
        bool swordSpinCharged = false;
        uint32_t bowDrawStartTick = 0;
        uint32_t nextBowShotTick = 0;
        float actionHeadingRadians = 0.0f;
        float locomotionPhaseRadians = 0.0f;
    };

    PlayerEntity* FindPlayer(int32_t ownerPlayerId);
    const PlayerEntity* FindPlayer(int32_t ownerPlayerId) const;
    size_t TeamPopulation(TeamId team) const;
    void SimulateTick();
    void EnterDeadState(PlayerEntity& player);
    void Respawn(PlayerEntity& player, bool emitEvent);
    void CancelGroundedAction(PlayerEntity& player) const;
    void UpdateAction(PlayerEntity& player);
    void UpdateBowDrawState(PlayerEntity& player);
    void EvaluateCombat(PlayerEntity& attacker);
    uint32_t NextCombatEventId();
    void MovePlayer(PlayerEntity& player, float deltaSeconds);
    void ApplyNativeTraversalRequest(PlayerEntity& player);
    static void ClearNativeClimbAuthorization(PlayerEntity& player);
    bool HasAuthoritativeCollisionScene(int32_t sceneId) const;
    bool GroundedPoseSupported(int32_t sceneId, const Vec3& position) const;
    bool SwimmingPoseSupported(int32_t sceneId, const Vec3& position) const;
    bool LocomotionModeSupported(const PlayerEntity& player,
                                 PlayerLocomotionMode requestedMode,
                                 const Vec3& requestedPosition) const;
    void ResolvePlayerCollisions(const std::vector<std::pair<int32_t, Vec3>>& startPositions,
                                 float deltaSeconds);
    void RefreshSpatialIndex();
    PlayerLifeEvent BuildLifeEvent(const PlayerEntity& player,
                                   PlayerLifeEventKind kind) const;
    bool MovementBlocked(const PlayerEntity& player, const Vec3& start, const Vec3& end) const;
    void UpdateLocomotionSurface(PlayerEntity& player, float deltaSeconds) const;
    void SnapToFloor(PlayerEntity& player) const;
    PlayerSnapshot BuildSnapshot(const PlayerEntity& player) const;
    std::vector<PlayerSnapshot> SnapshotsForSpatialCandidates(
        int32_t sceneId, const std::vector<SpatialIndexId>& candidates) const;

    static constexpr float kBodyRadius = 12.0f;
    static constexpr float kBodyHeight = 60.0f;
    static constexpr int kBodySolverPasses = 4;
    static constexpr uint32_t kRespawnDelayTicks = 5 * 30;
    static constexpr uint32_t kCommandTimeoutTicks = 6;

    EntityRegistry<PlayerEntity> mPlayers;
    // Session identity is the stable lookup key, while EntityId protects the
    // slot against reuse. All player lookup paths resolve through this index
    // instead of rescanning the authoritative registry.
    std::unordered_map<int32_t, EntityId> mPlayerByOwner;
    SpatialGridIndex mPlayerSpatialIndex;
    std::vector<CombatResultEvent> mCombatResults;
    std::vector<PlayerLifeEvent> mLifeEvents;
    SegmentCast mSegmentCast;
    CollisionSceneQuery mCollisionSceneQuery;
    WaterSurfaceQuery mWaterSurfaceQuery;
    ClimbSurfaceQuery mClimbSurfaceQuery;
    uint32_t mCurrentTick = 0;
    uint32_t mNextCombatEventId = 1;
};

} // namespace Game::Simulation
