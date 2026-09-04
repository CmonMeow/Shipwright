#include "NativeLocalRespawnController.h"

#include "platform/win32/PCInput.h"
#include "global.h"
#include "variables.h"

#include <runtime/log/Log.h>

#include <cmath>

namespace Game::Multiplayer {

NativeLocalRespawnController::NativeLocalRespawnController(
    Game::Client::ClientGameplaySession& gameplay,
    NativeLocalProjectileController& projectiles)
    : mGameplay(gameplay), mProjectiles(projectiles) {
}

bool NativeLocalRespawnController::Apply(
    PlayState* play, const Game::Simulation::PlayerRespawnEvent& event,
    int32_t localPlayerId) {
    if (!play || event.playerId != localPlayerId) return false;
    const auto authorizedEntity = mGameplay.Scene().AuthorizedEntity();
    if (!authorizedEntity || *authorizedEntity != event.entity ||
        !mGameplay.Scene().IsAuthorized(event.sceneId) ||
        play->sceneNum != event.sceneId || event.serverTick == 0) {
        return false;
    }

    gSaveContext.healthCapacity = STARTING_HEALTH;
    gSaveContext.health = STARTING_HEALTH;
    gSaveContext.healthAccumulator = 0;
    play->gameOverCtx.state = GAMEOVER_INACTIVE;
    if (Player* player = GET_PLAYER(play)) {
        player->authoritativeBodyHidden = false;
    }

    // The server advances lifeEpoch and resets admission floors at respawn.
    // Rebase every local action producer at the same boundary so no held edge,
    // pending acknowledgement, or native actor binding from the dead lifetime
    // can execute in the new one.
    mGameplay.BeginLife(event.lifeEpoch);
    mGameplay.Commands().ObserveAuthoritativeWeapon(
        event.selectedWeapon, event.serverTick);
    Game::Simulation::PlayerSnapshot baseline{};
    baseline.entity = event.entity;
    baseline.ownerPlayerId = event.playerId;
    baseline.sceneId = event.sceneId;
    baseline.serverTick = event.serverTick;
    baseline.lifeEpoch = event.lifeEpoch;
    baseline.position = event.position;
    baseline.headingRadians = event.headingRadians;
    baseline.selectedWeapon = event.selectedWeapon;
    baseline.health = STARTING_HEALTH;
    mGameplay.Vitals().Apply(baseline, localPlayerId);
    mGameplay.Prediction().SeedAuthoritative(baseline);
    mProjectiles.ResetBindings();
    PCInput_DiscardActionIntents();

    constexpr float kRadiansToBinaryAngle = 32768.0f / 3.14159265358979323846f;
    const int16_t heading = static_cast<int16_t>(
        std::lround(event.headingRadians * kRadiansToBinaryAngle));

    // Recreate Link through the native scene loader, but seed that loader from
    // the reliable authoritative baseline. Negative respawn flags skip the
    // stored position in Player_Init, so use DOWN mode (1).
    Play_TriggerRespawn(play);
    gSaveContext.respawn[RESPAWN_MODE_DOWN].pos = {
        event.position.x, event.position.y, event.position.z
    };
    gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw = heading;
    gSaveContext.respawnFlag = 1;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    play->gameplayFrames = 0;
    if (Player* player = GET_PLAYER(play)) {
        player->actor.world.pos = gSaveContext.respawn[RESPAWN_MODE_DOWN].pos;
        player->actor.home.pos = player->actor.world.pos;
        player->actor.prevPos = player->actor.world.pos;
        player->actor.velocity = {};
        player->actor.speedXZ = 0.0f;
        player->actor.shape.rot.y = heading;
        player->actor.world.rot.y = heading;
        player->yaw = heading;
    }
    Error("Network game: respawning local player %d at authoritative tick %u",
          localPlayerId, event.serverTick);
    return true;
}

} // namespace Game::Multiplayer
