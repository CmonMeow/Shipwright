#pragma once

#include "NetworkProtocol.h"

#include <functional>

namespace Game::Multiplayer {

class PrivateChatService;

struct LocalClientAdmissionDelivery {
    std::function<bool()> clientActive;
    std::function<bool(NetAppMessageType, const NetworkMessageRaw&)> sendPlain;
    std::function<bool(NetAppMessageType, const NetworkMessageRaw&, NetMsgFlags)> sendSecure;
};

// Owns the local client's admission sequence: key exchange, signed persistent
// identity proof, private-chat key registration, and identity submission state.
class LocalClientAdmissionService final {
  public:
    LocalClientAdmissionService(cCryptoSession& crypto,
                                PrivateChatService& privateChat,
                                LocalClientAdmissionDelivery delivery);

    bool BeginCryptoHandshake();
    bool SubmitIdentity();
    bool SubmitPrivateChatKey();
    void Reset();

    bool IdentitySent() const { return mIdentitySent; }

  private:
    cCryptoSession& mCrypto;
    PrivateChatService& mPrivateChat;
    LocalClientAdmissionDelivery mDelivery;
    bool mIdentitySent = false;
};

} // namespace Game::Multiplayer
