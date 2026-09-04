#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Engine::Rendering {
class GameRenderer;
}

namespace Game::UI {

enum class MultiplayerHudLineKind : uint8_t {
    Player,
    System,
    Private,
};

struct MultiplayerHudLine {
    std::string text;
    MultiplayerHudLineKind kind = MultiplayerHudLineKind::System;
};

// Transport-neutral immutable data consumed by the native HUD renderer.
struct MultiplayerHudView {
    std::span<const MultiplayerHudLine> history;
    size_t historyScrollOffset = 0;
    size_t visibleHistoryRows = 0;
    std::string_view draft;
    size_t cursor = 0;
    bool inputActive = false;
    bool passiveChatEnabled = true;
    std::string_view statusNotice;
    std::string_view gameplayNotice;
    bool lifeVisible = false;
    int32_t health = 0;
    int32_t healthCapacity = 0;
    int32_t healthPerSegment = 1;
};

class MultiplayerHudRenderer final {
  public:
    explicit MultiplayerHudRenderer(Engine::Rendering::GameRenderer& renderer);

    void Draw(const MultiplayerHudView& view);
    void Reset();

  private:
    Engine::Rendering::GameRenderer& mRenderer;
    size_t mDraftViewStart = 0;

    void DrawChat(const MultiplayerHudView& view);
    void DrawLife(const MultiplayerHudView& view);
};

} // namespace Game::UI
