#include "NativePlayerNameplatePresenter.h"

#include "rendering/Overlay.h"
#include "global.h"

namespace Game::Multiplayer {

NativePlayerNameplatePresenter::NativePlayerNameplatePresenter(
    const NativeRemotePlayerRenderer& renderer)
    : mRenderer(renderer) {
}

void NativePlayerNameplatePresenter::Queue(
    PlayState* play, int32_t playerId, const char* name) const {
    if (!play || playerId <= 0 || !name || name[0] == '\0') return;

    const auto position = mRenderer.WorldPositionForPlayer(playerId);
    if (!position) return;

    Vec3f worldPosition = { position->x, position->y + 78.0f, position->z };
    Vec3f obstruction{};
    CollisionPoly* obstructionPoly = nullptr;
    int32_t obstructionBgId = BGCHECK_SCENE;
    Vec3f cameraPosition = play->view.eye;
    if (BgCheck_AnyLineTest3(&play->colCtx, &cameraPosition, &worldPosition,
                             &obstruction, &obstructionPoly, true, true, true,
                             true, &obstructionBgId)) {
        return;
    }

    Vec3f clipPosition{};
    float clipW = 0.0f;
    SkinMatrix_Vec3fMtxFMultXYZW(
        &play->viewProjectionMtxF, &worldPosition, &clipPosition, &clipW);
    if (clipW <= 0.01f) return;

    const float ndcX = clipPosition.x / clipW;
    const float ndcY = clipPosition.y / clipW;
    if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) {
        return;
    }

    const float screenX = (ndcX + 1.0f) * (SCREEN_WIDTH * 0.5f);
    const float screenY = (1.0f - ndcY) * (SCREEN_HEIGHT * 0.5f);
    Engine::Rendering::Overlay::QueueCenteredGameText(
        name, screenX, screenY, 0.92f, 0.96f, 1.0f, 0.95f);
}

} // namespace Game::Multiplayer
