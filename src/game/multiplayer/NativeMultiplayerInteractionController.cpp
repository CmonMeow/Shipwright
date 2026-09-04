#include "NativeMultiplayerInteractionController.h"

#include "NetworkRuntime.h"

namespace Game::Multiplayer {

NativeMultiplayerInteractionController::NativeMultiplayerInteractionController(
    NetworkRuntime& runtime, Engine::ConsoleVariable& variables, Input& input)
    : mRuntime(runtime), mVoice(variables, input) {
}

NativeMultiplayerInteractionController::~NativeMultiplayerInteractionController() {
    Shutdown();
}

bool NativeMultiplayerInteractionController::Host(uint16_t port) {
    mRuntime.Disconnect();
    return mRuntime.Host(port);
}

bool NativeMultiplayerInteractionController::Connect(
    const std::string& address, uint16_t port) {
    mRuntime.Disconnect();
    return mRuntime.Connect(address, port);
}

void NativeMultiplayerInteractionController::Disconnect() {
    mRuntime.Disconnect();
}

bool NativeMultiplayerInteractionController::SendChat(
    const std::string& message) {
    return mRuntime.SendChat(message);
}

bool NativeMultiplayerInteractionController::SendPrivateChat(
    int32_t targetPlayer, const std::string& message) {
    return mRuntime.SendPrivateChat(targetPlayer, message);
}

bool NativeMultiplayerInteractionController::PollChat(
    Game::Client::MultiplayerChatMessage& message) {
    NetworkChatLine line;
    if (!mRuntime.PollChat(line)) return false;
    message.text = std::move(line.text);
    switch (line.kind) {
        case CLKPrivate:
            message.kind = Game::Client::MultiplayerChatKind::Private;
            break;
        case CLKSystem:
            message.kind = Game::Client::MultiplayerChatKind::System;
            break;
        case CLKNormal:
        default:
            message.kind = Game::Client::MultiplayerChatKind::Player;
            break;
    }
    return true;
}

std::vector<Game::Client::MultiplayerPeerIdentity>
NativeMultiplayerInteractionController::Players() const {
    const auto players = mRuntime.Players();
    std::vector<Game::Client::MultiplayerPeerIdentity> result;
    result.reserve(players.size());
    for (const auto& player : players) {
        result.push_back(
            { player.playerId, player.identity, player.name });
    }
    return result;
}

Game::Client::MultiplayerConnectionStatus
NativeMultiplayerInteractionController::Status() const {
    Game::Client::MultiplayerConnectionStatus status{};
    if (mRuntime.IsHost()) {
        status.phase = Game::Client::MultiplayerConnectionPhase::Hosting;
        status.secure = true;
    } else if (mRuntime.IsClient()) {
        status.phase = mRuntime.IsSecure()
                           ? Game::Client::MultiplayerConnectionPhase::Connected
                           : Game::Client::MultiplayerConnectionPhase::Connecting;
        status.secure = mRuntime.IsSecure();
    }
    status.latencyMilliseconds = mRuntime.LatencyMilliseconds();
    status.inboundBytesPerSecond = mRuntime.InboundBytesPerSecond();
    status.outboundBytesPerSecond = mRuntime.OutboundBytesPerSecond();
    status.playerCount = mRuntime.Players().size();
    return status;
}

void NativeMultiplayerInteractionController::UpdateVoice(
    bool textInputActive) {
    mVoice.Update(mRuntime, textInputActive);
}

void NativeMultiplayerInteractionController::Shutdown() {
    mVoice.Shutdown();
}

} // namespace Game::Multiplayer
