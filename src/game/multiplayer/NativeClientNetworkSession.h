#pragma once

#include <memory>

struct PlayState;

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

namespace Game::Multiplayer {

class NativeClientNetworkSession final {
  public:
    NativeClientNetworkSession(Engine::ConsoleVariable& variables, Engine::Rendering::GameRenderer& renderer,
                               Input& input);
    ~NativeClientNetworkSession();

    NativeClientNetworkSession(const NativeClientNetworkSession&) = delete;
    NativeClientNetworkSession& operator=(const NativeClientNetworkSession&) = delete;

    void RegisterActors();
    void Initialize();
    void Shutdown();
    void UpdateTransport();
    void UpdateGameplay(PlayState* play);
    void PumpMoveLoop();
    Game::Client::MultiplayerInteractionPort& Interaction();

  private:
    struct State;
    std::unique_ptr<State> mState;
    Engine::ConsoleVariable& mVariables;
    Engine::Rendering::GameRenderer& mRenderer;
    Input& mInput;
};

} // namespace Game::Multiplayer
