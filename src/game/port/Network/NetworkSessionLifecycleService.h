#pragma once

#include <chrono>
#include <cstdint>

class cCryptoSession;

namespace Game::Replication {
class ServerReplicationCoordinator;
}

namespace Game::Server {
class ServerAuthorityScheduler;
}

namespace Game::Simulation {
class ServerWorld;
}

namespace SoH::Network {

class ClientSessionIngress;
class CommunicationInbox;
class LocalClientAdmissionService;
class LocalVoiceSubmissionService;
class NetworkConnectionTransport;
class SecureTransportChannel;
class ServerSessionManager;

// Owns state that spans one transport session. This is the single boundary for
// disconnect cleanup, host authority advancement, replication flushing,
// connection lifecycle notifications, security readiness, traffic telemetry,
// and the generation used by native presentation to reject stale state.
class NetworkSessionLifecycleService final {
  public:
    NetworkSessionLifecycleService(
        NetworkConnectionTransport& connection,
        ServerSessionManager& serverSessions,
        CommunicationInbox& communication,
        cCryptoSession& clientCrypto,
        LocalClientAdmissionService& clientAdmission,
        ClientSessionIngress& clientIngress,
        Game::Simulation::ServerWorld& serverWorld,
        Game::Replication::ServerReplicationCoordinator& replication,
        SecureTransportChannel& secureTransport,
        LocalVoiceSubmissionService& localVoiceSubmission,
        Game::Server::ServerAuthorityScheduler& authorityScheduler);

    void Disconnect();
    void Update();

    bool IsSecure() const;
    uint64_t SessionGeneration() const { return mSessionGeneration; }
    int32_t InboundBytesPerSecond() const { return mInboundBytesPerSecond; }
    int32_t OutboundBytesPerSecond() const { return mOutboundBytesPerSecond; }

  private:
    void SampleTraffic(std::chrono::steady_clock::time_point now);

    NetworkConnectionTransport& mConnection;
    ServerSessionManager& mServerSessions;
    CommunicationInbox& mCommunication;
    cCryptoSession& mClientCrypto;
    LocalClientAdmissionService& mClientAdmission;
    ClientSessionIngress& mClientIngress;
    Game::Simulation::ServerWorld& mServerWorld;
    Game::Replication::ServerReplicationCoordinator& mReplication;
    SecureTransportChannel& mSecureTransport;
    LocalVoiceSubmissionService& mLocalVoiceSubmission;
    Game::Server::ServerAuthorityScheduler& mAuthorityScheduler;
    uint64_t mSessionGeneration = 1;
    int32_t mInboundBytesPerSecond = 0;
    int32_t mOutboundBytesPerSecond = 0;
    std::chrono::steady_clock::time_point mRateSampleTime =
        std::chrono::steady_clock::now();
};

} // namespace SoH::Network
