#include "ServerGameplayIngress.h"
#include "../replication/FishingPresentationAuthority.h"

namespace Game::Simulation {

bool ServerGameplayIngress::SubmitPlayerCommand(
    int32_t authenticatedPlayerId, PlayerCommand command) {
    if (authenticatedPlayerId < 0) return false;
    const auto player = mWorld.PlayerFor(authenticatedPlayerId);
    if (!player) return false;
    command.ownerPlayerId = authenticatedPlayerId;
    command.sceneId = player->sceneId;
    return mWorld.SubmitPlayerCommand(command);
}

bool ServerGameplayIngress::ExecuteWeaponSelection(
    int32_t authenticatedPlayerId, WeaponSelectionCommand command) {
    if (authenticatedPlayerId < 0) return false;
    command.playerId = authenticatedPlayerId;
    return mWorld.ExecuteWeaponSelection(command);
}

std::optional<ServerSceneEntryOutcome> ServerGameplayIngress::ExecuteSceneEntry(
    int32_t authenticatedPlayerId, SceneEntryCommand command) {
    if (authenticatedPlayerId < 0) return std::nullopt;
    command.playerId = authenticatedPlayerId;
    return mWorld.ExecuteSceneEntry(command);
}

ArrowFireDecision ServerGameplayIngress::ExecuteArrowFire(
    int32_t authenticatedPlayerId, ArrowFireCommand command) {
    if (authenticatedPlayerId < 0) return {};
    command.playerId = authenticatedPlayerId;
    return mWorld.ExecuteArrowFire(command);
}

bool ServerGameplayIngress::ExecuteLureControl(
    int32_t authenticatedPlayerId, LureControlCommand command) {
    if (authenticatedPlayerId < 0) return false;
    command.playerId = authenticatedPlayerId;
    return mWorld.ExecuteLureControl(command);
}

bool ServerGameplayIngress::ExecuteFishAction(
    int32_t authenticatedPlayerId, FishActionCommand command) {
    if (authenticatedPlayerId < 0) return false;
    command.playerId = authenticatedPlayerId;
    return mWorld.ExecuteFishAction(command);
}

std::optional<AdmittedFishingPresentation>
ServerGameplayIngress::AdmitFishingPresentation(
    int32_t authenticatedPlayerId,
    Replication::FishingPresentationIntent intent) const {
    if (authenticatedPlayerId < 0 || intent.lifeEpoch == 0) {
        return std::nullopt;
    }
    const auto player = mWorld.PlayerFor(authenticatedPlayerId);
    if (!player || player->lifeEpoch != intent.lifeEpoch) {
        return std::nullopt;
    }
    if (!Replication::FishingPresentationAuthority::Constrain(
            intent.presentation, *player,
            mWorld.LureForPlayer(authenticatedPlayerId),
            mWorld.FishOwnedBy(authenticatedPlayerId))) {
        return std::nullopt;
    }
    return AdmittedFishingPresentation{ intent.presentation, *player };
}

StructureActionDecision ServerGameplayIngress::ExecuteStructureAction(
    int32_t authenticatedPlayerId, StructureActionCommand command) {
    if (authenticatedPlayerId < 0) return {};
    command.playerId = authenticatedPlayerId;
    return mWorld.ExecuteStructureAction(command);
}

} // namespace Game::Simulation
