#include "NativeRemoteFishPresentationController.h"

#include "NativePlayerPresentationState.h"
#include "NativeRemotePlayerRenderer.h"
#include "platform/client/RemotePlayerReplicaStore.h"

#include <cstddef>

namespace Game::Multiplayer {

NativeRemoteFishPresentationController::NativeRemoteFishPresentationController(
    Game::Client::RemoteFishingEntityState& fishing,
    Game::Client::RemotePlayerReplicaStore& players,
    NativeRemotePlayerRenderer& renderer)
    : mFishing(fishing), mPlayers(players), mRenderer(renderer) {
}

std::optional<NativeRemoteFishPresentation>
NativeRemoteFishPresentationController::Read(
    const Game::Client::RemoteFishIdentity& identity,
    double nowSeconds) const {
    const auto owner = mFishing.OwnerForFish(identity);
    if (!owner || !mRenderer.IsPlayerReady(*owner)) return std::nullopt;

    const auto* fish = mFishing.FishForOwner(*owner);
    const auto* base = mRenderer.FindPlayer(*owner);
    if (!fish || !base || fish->identity != identity ||
        base->sceneId != identity.sceneId) {
        return std::nullopt;
    }

    NativePlayerPresentationState player = *base;
    if (auto* replica = mPlayers.FindPlayerMutable(*owner)) {
        if (const auto fishing = replica->fishing.Evaluate(nowSeconds)) {
            NativePlayerPresentationComposer::ApplyFishingPresentation(
                player, *fishing);
        }
    }

    NativeRemoteFishPresentation result{};
    result.position = { fish->x, fish->y, fish->z };
    for (size_t axis = 0; axis < result.rotation.size(); ++axis) {
        result.rotation[axis] = player.fishingFishRot[axis];
    }
    for (size_t limb = 0; limb < result.limbRotation.size(); ++limb) {
        result.limbRotation[limb] = player.fishingFishLimbRot[limb];
    }
    result.length = fish->length;
    result.isLoach =
        fish->species == Game::Simulation::FishSpecies::HylianLoach;
    return result;
}

} // namespace Game::Multiplayer
