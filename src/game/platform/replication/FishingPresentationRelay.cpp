#include "FishingPresentationRelay.h"
#include "../SequenceNumber.h"

#include <map>

namespace Game::Replication {

namespace {

bool Eligible(const FishingPresentationState& presentation,
              const Simulation::PlayerSnapshot& player) {
    return presentation.playerId == player.ownerPlayerId &&
           presentation.entity == player.entity &&
           presentation.sceneId == player.sceneId && presentation.sequence != 0 &&
           player.health != 0 &&
           player.selectedWeapon ==
               static_cast<uint8_t>(Simulation::PlayerWeaponSlot::FishingPole);
}

} // namespace

FishingPresentationUpdateResult FishingPresentationRelay::Update(
    const FishingPresentationState& presentation,
    const Simulation::PlayerSnapshot& authoritativePlayer) {
    if (!Eligible(presentation, authoritativePlayer)) {
        return FishingPresentationUpdateResult::Invalid;
    }
    const auto previous = mPresentations.find(presentation.playerId);
    if (previous != mPresentations.end() &&
        presentation.sequence != previous->second.sequence &&
        !Sequence::IsNewer(presentation.sequence, previous->second.sequence)) {
        return FishingPresentationUpdateResult::Stale;
    }
    mPresentations[presentation.playerId] = presentation;
    return FishingPresentationUpdateResult::Accepted;
}

void FishingPresentationRelay::Reconcile(
    const std::vector<Simulation::PlayerSnapshot>& players) {
    std::map<int32_t, const Simulation::PlayerSnapshot*> authoritative;
    for (const Simulation::PlayerSnapshot& player : players) {
        authoritative.emplace(player.ownerPlayerId, &player);
    }
    for (auto presentation = mPresentations.begin();
         presentation != mPresentations.end();) {
        const auto player = authoritative.find(presentation->first);
        if (player == authoritative.end() ||
            !Eligible(presentation->second, *player->second)) {
            presentation = mPresentations.erase(presentation);
        } else {
            ++presentation;
        }
    }
}

void FishingPresentationRelay::RemovePlayer(int32_t playerId) {
    mPresentations.erase(playerId);
}

std::optional<FishingPresentationState>
FishingPresentationRelay::ForPlayer(int32_t playerId) const {
    const auto presentation = mPresentations.find(playerId);
    if (presentation == mPresentations.end()) return std::nullopt;
    return presentation->second;
}

size_t FishingPresentationRelay::Count() const {
    return mPresentations.size();
}

void FishingPresentationRelay::Reset() {
    mPresentations.clear();
}

} // namespace Game::Replication
