#pragma once

#include "NativeVoiceController.h"
#include "../../platform/client/MultiplayerInteractionPort.h"

namespace SoH::Network {

class NetworkRuntime;

// Native adapter for UI/application multiplayer interaction. It is the only
// object in this path that knows both the semantic port and NetworkRuntime.
class NativeMultiplayerInteractionController final
    : public Game::Client::MultiplayerInteractionPort {
  public:
    explicit NativeMultiplayerInteractionController(NetworkRuntime& runtime);
    ~NativeMultiplayerInteractionController() override;

    bool Host(uint16_t port) override;
    bool Connect(const std::string& address) override;
    void Disconnect() override;
    bool SendChat(const std::string& message) override;
    bool SendPrivateChat(int32_t targetPlayer,
                         const std::string& message) override;
    bool PollChat(Game::Client::MultiplayerChatMessage& message) override;
    std::vector<Game::Client::MultiplayerPeerIdentity> Players() const override;
    Game::Client::MultiplayerConnectionStatus Status() const override;
    void UpdateVoice(bool textInputActive) override;
    void Shutdown() override;

  private:
    NetworkRuntime& mRuntime;
    NativeVoiceController mVoice;
};

} // namespace SoH::Network
