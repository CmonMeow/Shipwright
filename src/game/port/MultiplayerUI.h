#pragma once

#include <memory>

namespace SoH::Network {
class NetworkRuntime;
}

class MultiplayerUI final {
  public:
    MultiplayerUI();
    ~MultiplayerUI();

    MultiplayerUI(const MultiplayerUI&) = delete;
    MultiplayerUI& operator=(const MultiplayerUI&) = delete;

    void Update(SoH::Network::NetworkRuntime& runtime);
    void ShowNotification(const char* text);
    void ClearNotification();
    void Shutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
