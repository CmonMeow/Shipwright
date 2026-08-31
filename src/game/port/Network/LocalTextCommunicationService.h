#pragma once

#include "NetworkProtocol.h"

#include <cstdint>
#include <functional>
#include <string>

namespace SoH::Network {

class CommunicationInbox;
class PrivateChatService;

enum class LocalTextCommunicationRole : uint8_t {
    Inactive,
    Client,
    Host,
};

struct LocalTextCommunicationDelivery {
    std::function<LocalTextCommunicationRole()> role;
    std::function<bool(NetAppMessageType, const NetworkMessageRaw&, NetMsgFlags)> sendToServer;
    std::function<bool(const std::string&)> sendHostChat;
    std::function<bool(int32_t, const std::string&)> sendHostPrivateChat;
    std::function<std::string(int32_t)> playerName;
};

// Owns local text validation, private-message encryption, wire encoding, and
// role-dependent routing. The runtime supplies transport endpoints only.
class LocalTextCommunicationService final {
  public:
    LocalTextCommunicationService(PrivateChatService& privateChat,
                                  CommunicationInbox& communication,
                                  LocalTextCommunicationDelivery delivery);

    bool SendChat(const std::string& message);
    bool SendPrivateChat(int32_t targetPlayer, const std::string& message);

  private:
    std::string PlayerName(int32_t player) const;

    PrivateChatService& mPrivateChat;
    CommunicationInbox& mCommunication;
    LocalTextCommunicationDelivery mDelivery;
};

} // namespace SoH::Network
