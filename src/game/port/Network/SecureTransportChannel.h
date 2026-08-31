#pragma once

#include "NetworkProtocol.h"
#include "ServerSessionManager.h"
#include "../../platform/replication/ServerReplicationCoordinator.h"

#include <cstdint>
#include <functional>
#include <string>

namespace SoH::Network {

struct RawTransportDelivery {
    std::function<bool(const std::string&, NetMsgFlags)> sendToServer;
    std::function<bool(int32_t, const std::string&, NetMsgFlags)> sendToPeer;
};

class SecureTransportChannel final {
  public:
    SecureTransportChannel(
        cCryptoSession& clientCrypto,
        ServerSessionManager& serverSessions,
        Game::Replication::ServerReplicationCoordinator& replication);

    void SetDelivery(RawTransportDelivery delivery);
    void ResetCounters();
    void RecordInbound(size_t bytes);
    uint64_t InboundBytes() const { return mInboundBytes; }
    uint64_t OutboundBytes() const { return mOutboundBytes; }

    bool PrepareClientMessage(char* buffer, int32_t size,
                              std::string& decrypted, const char*& message,
                              int32_t& messageSize);
    bool PrepareServerMessage(int32_t sender, char* buffer, int32_t size,
                              std::string& decrypted, const char*& message,
                              int32_t& messageSize);

    bool SendPlainToServer(NetAppMessageType type, const NetworkMessageRaw& raw);
    bool SendPlainToPeer(int32_t peer, NetAppMessageType type,
                         const NetworkMessageRaw& raw);
    bool SendToServer(NetAppMessageType type, const NetworkMessageRaw& raw,
                      NetMsgFlags flags);
    bool SendToPeer(int32_t peer, NetAppMessageType type,
                    const NetworkMessageRaw& raw, NetMsgFlags flags,
                    Game::Replication::ReplicationStreamKey streamKey = {});
    bool SendEncryptedPayloadToServer(const std::string& payload,
                                      NetMsgFlags flags);
    bool SendEncryptedPayloadToPeer(int32_t peer, const std::string& payload,
                                    NetMsgFlags flags);

  private:
    bool SendRawToServer(const std::string& payload, NetMsgFlags flags);
    bool SendRawToPeer(int32_t peer, const std::string& payload,
                       NetMsgFlags flags);

    cCryptoSession& mClientCrypto;
    ServerSessionManager& mServerSessions;
    Game::Replication::ServerReplicationCoordinator& mReplication;
    RawTransportDelivery mDelivery;
    uint64_t mInboundBytes = 0;
    uint64_t mOutboundBytes = 0;
};

} // namespace SoH::Network
