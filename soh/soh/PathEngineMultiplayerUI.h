#pragma once

#include <memory>

namespace SoH::Network {
class ShipwrightNetworkRuntime;
}

class PathEngineMultiplayerUI final {
  public:
    PathEngineMultiplayerUI();
    ~PathEngineMultiplayerUI();

    PathEngineMultiplayerUI(const PathEngineMultiplayerUI&) = delete;
    PathEngineMultiplayerUI& operator=(const PathEngineMultiplayerUI&) = delete;

    void Update(SoH::Network::ShipwrightNetworkRuntime& runtime);
    void ShowNotification(const char* text);
    void ClearNotification();
    void Shutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
