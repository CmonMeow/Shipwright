#pragma once

#include "NetworkProtocol.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Game::Multiplayer {

struct ServerSessionDeparture {
    int32_t peer = -1;
    std::optional<NetworkIdentity> identity;
    std::string moderationReason;
};

class ServerSessionManager final {
  public:
    ~ServerSessionManager();

    bool ConnectPeer(int32_t peer);
    std::optional<ServerSessionDeparture> DisconnectPeer(int32_t peer);
    void Reset();

    bool AdmitIdentity(int32_t peer, NetworkIdentity identity);
    bool HasPeer(int32_t peer) const;
    bool HasIdentity(int32_t peer) const;
    const NetworkIdentity* IdentityFor(int32_t peer) const;
    cCryptoSession* CryptoFor(int32_t peer);
    const cCryptoSession* CryptoFor(int32_t peer) const;

    const std::vector<int32_t>& Peers() const;
    const std::vector<int32_t>& AdmittedPeers() const;
    bool AllPeersSecure() const;
    size_t PeerCount() const;

    bool SetModerationReason(int32_t peer, std::string reason);

  private:
    struct Session {
        cCryptoSession crypto;
        std::optional<NetworkIdentity> identity;
        std::string moderationReason;
    };

    std::vector<int32_t> mPeerOrder;
    std::vector<int32_t> mAdmittedPeerOrder;
    std::map<int32_t, Session> mSessions;
};

} // namespace Game::Multiplayer
