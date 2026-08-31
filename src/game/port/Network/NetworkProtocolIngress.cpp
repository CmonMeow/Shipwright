#include "NetworkProtocolIngress.h"

#include "SecureTransportChannel.h"
#include "ServerSessionManager.h"

#include <utility>

namespace SoH::Network {

NetworkProtocolIngress::NetworkProtocolIngress(
    SecureTransportChannel& secureTransport,
    ServerSessionManager& serverSessions,
    ClientProtocolSink& clientEndpoint, ServerProtocolSink& serverEndpoint,
    NetworkProtocolIngressDelivery delivery)
    : mSecureTransport(secureTransport), mServerSessions(serverSessions),
      mClientEndpoint(clientEndpoint), mServerEndpoint(serverEndpoint),
      mDelivery(std::move(delivery)) {
}

ProtocolDispatchResult NetworkProtocolIngress::ReceiveClient(
    char* buffer, int32_t size) {
    if (size > 0) mSecureTransport.RecordInbound(static_cast<size_t>(size));
    std::string decrypted;
    const char* message = nullptr;
    int32_t messageSize = 0;
    if (!mSecureTransport.PrepareClientMessage(
            buffer, size, decrypted, message, messageSize)) {
        return ProtocolDispatchResult::Malformed;
    }
    return ProtocolDispatcher::DispatchClient(message, messageSize,
                                              mClientEndpoint);
}

ProtocolDispatchResult NetworkProtocolIngress::ReceiveServer(
    int32_t sender, char* buffer, int32_t size) {
    if (size > 0) mSecureTransport.RecordInbound(static_cast<size_t>(size));
    std::string decrypted;
    const char* message = nullptr;
    int32_t messageSize = 0;
    if (!mSecureTransport.PrepareServerMessage(
            sender, buffer, size, decrypted, message, messageSize)) {
        return ProtocolDispatchResult::Malformed;
    }

    const bool awaitingIdentity = !mServerSessions.HasIdentity(sender);
    const ProtocolDispatchResult result = ProtocolDispatcher::DispatchServer(
        sender, message, messageSize, awaitingIdentity, mServerEndpoint);
    if (awaitingIdentity && result != ProtocolDispatchResult::Dispatched &&
        mDelivery.kick) {
        mDelivery.kick(sender, "invalid or incompatible identity");
    }
    return result;
}

} // namespace SoH::Network
