#include "NativeCombatPresentationController.h"

#include "ClientCombatPresentationPolicy.h"
#include "global.h"
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"

namespace SoH::Network {

void NativeCombatPresentationController::Apply(
    PlayState* play, const Game::Simulation::CombatResultEvent& event,
    int32_t localPlayerId) const {
    if (!play) return;

    const auto action = ClientCombatPresentationPolicy::Evaluate(
        event, localPlayerId, play->sceneNum);
    if (action == ClientCombatPresentationAction::Ignore) return;

    Vec3f impact{ event.impactPosition.x, event.impactPosition.y,
                  event.impactPosition.z };
    if (action == ClientCombatPresentationAction::BlockedImpact) {
        EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_METAL, &impact);
        CollisionCheck_PlayMetalSoundAt(&impact);
        return;
    }
    if (action == ClientCombatPresentationAction::ObservedDamageImpact) {
        EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_WHITE, &impact);
        return;
    }

    Player* player = GET_PLAYER(play);
    if (!player) return;

    // PlayerSimulation has already decided health and replicated it through
    // ordered snapshots. Native hit handling contributes reaction, voice,
    // knockback, and flicker only.
    player->actor.colChkInfo.damage = 0;
    player->actor.colChkInfo.acHitEffect = 0;
    func_80838280(player);
    func_80837C0C(play, player, PLAYER_HIT_RESPONSE_NONE, 4.0f, 5.0f,
                  event.impactHeading, 20);
}

} // namespace SoH::Network
