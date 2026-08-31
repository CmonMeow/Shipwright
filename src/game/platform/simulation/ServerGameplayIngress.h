#pragma once

#include "ServerWorld.h"
#include "../replication/FishingPresentationState.h"

namespace Game::Simulation {

struct AdmittedFishingPresentation {
    Replication::FishingPresentationState presentation{};
    PlayerSnapshot authoritativePlayer{};
};

// Authenticated boundary between transport and authoritative simulation.
// The caller supplies the authenticated session player separately; identity
// and scene carried in a domain command are never trusted across this edge.
class ServerGameplayIngress final {
  public:
    explicit ServerGameplayIngress(ServerWorld& world) : mWorld(world) {}

    bool SubmitPlayerCommand(int32_t authenticatedPlayerId, PlayerCommand command);
    bool ExecuteWeaponSelection(int32_t authenticatedPlayerId,
                                WeaponSelectionCommand command);
    std::optional<ServerSceneEntryOutcome> ExecuteSceneEntry(
        int32_t authenticatedPlayerId, SceneEntryCommand command);
    ArrowFireDecision ExecuteArrowFire(int32_t authenticatedPlayerId,
                                       ArrowFireCommand command);
    bool ExecuteLureControl(int32_t authenticatedPlayerId,
                            LureControlCommand command);
    bool ExecuteFishAction(int32_t authenticatedPlayerId,
                           FishActionCommand command);
    std::optional<AdmittedFishingPresentation> AdmitFishingPresentation(
        int32_t authenticatedPlayerId,
        Replication::FishingPresentationIntent intent) const;
    StructureActionDecision ExecuteStructureAction(
        int32_t authenticatedPlayerId, StructureActionCommand command);

  private:
    ServerWorld& mWorld;
};

} // namespace Game::Simulation
