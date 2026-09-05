#pragma once

#include "platform/client/LocalProjectileIntentStream.h"
#include "platform/client/NativePresentationBindingRegistry.h"
#include "platform/client/RemoteProjectileReplicaStore.h"

#include <cstdint>

struct Actor;

namespace Game::Multiplayer {

// Owns the native presentation side of locally fired projectiles. Transport
// and simulation retain semantic IDs only; native Actor pointers never cross
// this boundary.
class NativeLocalProjectileController final {
  public:
    explicit NativeLocalProjectileController(
        Game::Client::LocalProjectileIntentStream& intents);

    void ResetBindings();
    void BindPredictedArrow(Actor* actor, int32_t sceneId);
    bool CommitArrowFire(Actor* actor, int32_t sceneId, uint32_t clientTick,
                         int16_t heading, int16_t aimPitch);
    void UnbindPredictedArrow(Actor* actor);

    void ApplyAuthorityResult(
        const Game::Client::LocalProjectileIntentDecision& decision);
    void ApplyAuthoritativeState(
        const Game::Client::RemoteProjectileReplicaState& state,
        int32_t localPlayerId);

  private:
    void RetirePresentation(
        Game::Client::LocalProjectilePresentationId presentationId,
        bool killActor);

    Game::Client::LocalProjectileIntentStream& mIntents;
    Game::Client::NativePresentationBindingRegistry<Actor> mBindings;
};

} // namespace Game::Multiplayer
