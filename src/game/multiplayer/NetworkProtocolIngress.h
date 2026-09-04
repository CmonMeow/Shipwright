#pragma once

#include "ProtocolDispatcher.h"

#include <cstdint>
#include <functional>
#include <string>

namespace Game::Multiplayer {

class SecureTransportChannel;
class ServerSessionManager;

struct NetworkProtocolIngressDelivery {
    std::function<void(int32_t, const std::string&)> kick;
};

// Sole inbound wire boundary. It accounts raw bytes, authenticates/decrypts
// envelopes, dispatches typed protocol messages, and enforces the server's
// pre-identity packet restriction before semantic endpoints see traffic.
class NetworkProtocolIngress final {
  public:
    NetworkProtocolIngress(SecureTransportChannel& secureTransport,
                           ServerSessionManager& serverSessions,
                           ClientProtocolSink& clientEndpoint,
                           ServerProtocolSink& serverEndpoint,
                           NetworkProtocolIngressDelivery delivery);

    ProtocolDispatchResult ReceiveClient(char* buffer, int32_t size);
    ProtocolDispatchResult ReceiveServer(int32_t sender, char* buffer,
                                         int32_t size);

  private:
    SecureTransportChannel& mSecureTransport;
    ServerSessionManager& mServerSessions;
    ClientProtocolSink& mClientEndpoint;
    ServerProtocolSink& mServerEndpoint;
    NetworkProtocolIngressDelivery mDelivery;
};

} // namespace Game::Multiplayer
