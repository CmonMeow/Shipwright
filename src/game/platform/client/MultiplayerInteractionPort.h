#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Game::Client {

inline constexpr const char* kDefaultMultiplayerAddress = "127.0.0.1";
inline constexpr uint16_t kDefaultMultiplayerPort = 777;
inline constexpr size_t kMultiplayerChatMessageCharacters = 140;
inline constexpr size_t kMultiplayerChatLineCharacters = 192;
inline constexpr size_t kMultiplayerChatVisibleRows = 9;
inline constexpr size_t kMultiplayerChatHistoryLines = 500;

inline std::string SanitiseMultiplayerText(const std::string& text,
                                           size_t maxCharacters) {
    std::string result;
    result.reserve(text.size() < maxCharacters ? text.size()
                                                : maxCharacters);
    for (unsigned char character : text) {
        if (result.size() >= maxCharacters) break;
        if (character >= 32 && character != 127) {
            result.push_back(static_cast<char>(character));
        }
    }
    return result;
}

enum class MultiplayerConnectionPhase : uint8_t {
    Offline,
    Hosting,
    Connecting,
    Connected,
};

enum class MultiplayerChatKind : uint8_t {
    Player,
    System,
    Private,
};

struct MultiplayerChatMessage {
    std::string text;
    MultiplayerChatKind kind = MultiplayerChatKind::Player;
};

struct MultiplayerPeerIdentity {
    int32_t playerId = -1;
    std::string identity;
    std::string name;
};

struct MultiplayerConnectionStatus {
    MultiplayerConnectionPhase phase = MultiplayerConnectionPhase::Offline;
    bool secure = false;
    int32_t latencyMilliseconds = 0;
    int32_t inboundBytesPerSecond = 0;
    int32_t outboundBytesPerSecond = 0;
    size_t playerCount = 0;

    bool Active() const {
        return phase != MultiplayerConnectionPhase::Offline;
    }
};

// Application-facing multiplayer interaction port. HUD/input code consumes
// semantic state and requests; transport, packets, crypto and audio devices
// remain behind the native adapter.
class MultiplayerInteractionPort {
  public:
    virtual ~MultiplayerInteractionPort() = default;

    virtual bool Host(uint16_t port) = 0;
    virtual bool Connect(const std::string& address) = 0;
    virtual void Disconnect() = 0;
    virtual bool SendChat(const std::string& message) = 0;
    virtual bool SendPrivateChat(int32_t targetPlayer,
                                 const std::string& message) = 0;
    virtual bool PollChat(MultiplayerChatMessage& message) = 0;
    virtual std::vector<MultiplayerPeerIdentity> Players() const = 0;
    virtual MultiplayerConnectionStatus Status() const = 0;
    virtual void UpdateVoice(bool textInputActive) = 0;
    virtual void Shutdown() = 0;
};

} // namespace Game::Client
