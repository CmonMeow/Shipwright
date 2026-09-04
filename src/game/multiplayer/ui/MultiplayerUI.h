#pragma once

#include <memory>

namespace Game::Client {
class MultiplayerInteractionPort;
}

namespace Engine {
class ConsoleVariable;
namespace Rendering {
class GameRenderer;
}
}

struct Input;

class MultiplayerUI final {
  public:
    MultiplayerUI(Game::Client::MultiplayerInteractionPort& interaction, Engine::ConsoleVariable& variables,
                  Engine::Rendering::GameRenderer& renderer, Input& input);
    ~MultiplayerUI();

    MultiplayerUI(const MultiplayerUI&) = delete;
    MultiplayerUI& operator=(const MultiplayerUI&) = delete;

    void Update();
    void ShowNotification(const char* text);
    void ClearNotification();
    void Shutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
