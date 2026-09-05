#include "FishingPresentationAuthority.h"
#include "FishingPresentationValidation.h"
#include "../simulation/PlayerLoadoutPolicy.h"

#include <algorithm>
#include <cmath>

namespace Game::Replication {
namespace {

void ClearLureVisuals(FishingPresentationState& presentation) {
    presentation.state = 0;
    presentation.lureDrawOffset = {};
    presentation.lureRotation = {};
    presentation.lureSpin = 0.0f;
    presentation.lureZOffset = 0.0f;
    presentation.lureHookOffsets = {};
    presentation.lureHookRotations = {};
    presentation.lineScale = 0.0f;
    presentation.lineGravity = 0.0f;
    presentation.lineSpooled = 0;
    presentation.sinkingLureSegmentIndex = 0;
    presentation.sinkingLureUnderwater = 0;
}

void ClearFishVisuals(FishingPresentationState& presentation) {
    presentation.fishRotation = {};
    presentation.fishLimbRotation = {};
}

void AnchorHooksToAuthoritativeLure(FishingPresentationState& presentation) {
    const float limitSquared = kFishingHookOffsetFromLureLimit *
                               kFishingHookOffsetFromLureLimit;
    for (auto& hook : presentation.lureHookOffsets) {
        const float x = hook[0] - presentation.lureDrawOffset[0];
        const float y = hook[1] - presentation.lureDrawOffset[1];
        const float z = hook[2] - presentation.lureDrawOffset[2];
        const float distanceSquared = x * x + y * y + z * z;
        if (!std::isfinite(distanceSquared) || distanceSquared > limitSquared) {
            hook = presentation.lureDrawOffset;
        }
    }
}

} // namespace

bool FishingPresentationAuthority::Constrain(
    FishingPresentationState& presentation,
    const Simulation::PlayerSnapshot& player,
    const std::optional<Simulation::FishingLureSnapshot>& lure,
    const std::optional<Simulation::FishSnapshot>& fish) {
    if (player.ownerPlayerId < 0 || !player.entity.Valid() || player.sceneId < 0 ||
        player.health == 0 ||
        player.selectedWeapon !=
            static_cast<uint8_t>(Simulation::PlayerWeaponSlot::FishingPole)) {
        return false;
    }

    presentation.playerId = player.ownerPlayerId;
    presentation.entity = player.entity;
    presentation.sceneId = player.sceneId;
    presentation.lifeEpoch = player.lifeEpoch;

    if (fish && (fish->ownerPlayerId != player.ownerPlayerId ||
                 fish->ownerLifeEpoch != player.lifeEpoch ||
                 fish->identity.sceneId != player.sceneId || !fish->entity.Valid())) {
        return false;
    }
    if (!fish) ClearFishVisuals(presentation);

    if (!lure) {
        ClearLureVisuals(presentation);
        return FishingPresentationPoseIsBounded(presentation);
    }
    if (lure->ownerPlayerId != player.ownerPlayerId ||
        lure->ownerLifeEpoch != player.lifeEpoch ||
        lure->sceneId != player.sceneId || !lure->entity.Valid()) {
        return false;
    }

    presentation.lureDrawOffset = {
        lure->position.x - player.position.x,
        lure->position.y - player.position.y,
        lure->position.z - player.position.z,
    };
    AnchorHooksToAuthoritativeLure(presentation);
    switch (lure->phase) {
        case Simulation::FishingLurePhase::Flying:
            presentation.state = 1;
            break;
        case Simulation::FishingLurePhase::Settled:
            presentation.state = 3;
            break;
        case Simulation::FishingLurePhase::Hooked:
            presentation.state = fish ? 4 : 3;
            break;
    }

    if (lure->lureType != 2) {
        presentation.sinkingLureSegmentIndex = 0;
        presentation.sinkingLureUnderwater = 0;
    } else {
        presentation.sinkingLureSegmentIndex =
            std::min<uint8_t>(presentation.sinkingLureSegmentIndex, 19);
        presentation.sinkingLureUnderwater =
            presentation.sinkingLureUnderwater != 0;
    }
    return FishingPresentationPoseIsBounded(presentation);
}

} // namespace Game::Replication
