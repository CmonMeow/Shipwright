#pragma once

#include <memory>

namespace Game::Client {
class MultiplayerInteractionPort;
}

class MultiplayerUI final {
  public:
    explicit MultiplayerUI(Game::Client::MultiplayerInteractionPort& interaction);
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
