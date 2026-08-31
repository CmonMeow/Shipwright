#pragma once

#include "NetworkProtocol.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SoH::Network {

struct PrivateChatPeer {
    int32_t playerId = -1;
    std::string playerName;
    std::string publicKey;
};

class PrivateChatService final {
  public:
    PrivateChatService();
    ~PrivateChatService();

    PrivateChatService(const PrivateChatService&) = delete;
    PrivateChatService& operator=(const PrivateChatService&) = delete;

    bool Initialize();
    bool Ready() const { return mKeyReady; }
    void ResetPeers();

    bool SetPeer(int32_t playerId, const std::string& playerName, const std::string& publicKey);
    void RemovePeer(int32_t playerId);
    std::string PeerName(int32_t playerId) const;
    std::vector<PrivateChatPeer> Peers() const;

    std::string PublicKey() const;
    bool EncryptFor(int32_t targetPlayerId, const std::string& message, std::string& cipher) const;
    bool Decrypt(const std::string& cipher, std::string& message) const;

  private:
    std::map<int32_t, PrivateChatPeer> mPeers;
    unsigned char mPublicKey[crypto_box_PUBLICKEYBYTES]{};
    unsigned char mSecretKey[crypto_box_SECRETKEYBYTES]{};
    bool mKeyReady = false;
};

} // namespace SoH::Network
