#pragma once

#include "NativeRemotePlayerRenderer.h"

#include <cstdint>

struct PlayState;

namespace Game::Multiplayer {

// Projects an admitted remote player's identity into the HUD while respecting
// native world occlusion. Transport and replica lifecycle remain outside this
// presentation-only boundary.
class NativePlayerNameplatePresenter final {
  public:
    explicit NativePlayerNameplatePresenter(
        const NativeRemotePlayerRenderer& renderer);

    void Queue(PlayState* play, int32_t playerId, const char* name) const;

  private:
    const NativeRemotePlayerRenderer& mRenderer;
};

} // namespace Game::Multiplayer
