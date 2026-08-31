#include "NetworkSessionLifecycleService.h"

#include "NetworkProtocol.h"
#include "ClientSessionIngress.h"
#include "CommunicationInbox.h"
#include "LocalClientAdmissionService.h"
#include "LocalVoiceSubmissionService.h"
#include "NetworkConnectionTransport.h"
#include "SecureTransportChannel.h"
#include "ServerSessionManager.h"
#include "../../platform/replication/ServerReplicationCoordinator.h"
#include "../../platform/server/ServerAuthorityScheduler.h"
#include "../../platform/simulation/ServerWorld.h"

namespace SoH::Network {

NetworkSessionLifecycleService::NetworkSessionLifecycleService(
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
    Game::Server::ServerAuthorityScheduler& authorityScheduler)
    : mConnection(connection),
      mServerSessions(serverSessions),
      mCommunication(communication),
      mClientCrypto(clientCrypto),
      mClientAdmission(clientAdmission),
      mClientIngress(clientIngress),
      mServerWorld(serverWorld),
      mReplication(replication),
      mSecureTransport(secureTransport),
      mLocalVoiceSubmission(localVoiceSubmission),
      mAuthorityScheduler(authorityScheduler) {
}

void NetworkSessionLifecycleService::Disconnect() {
    mConnection.Disconnect();
    mClientCrypto.clear();
    mServerSessions.Reset();
    mClientAdmission.Reset();
    mInboundBytesPerSecond = 0;
    mOutboundBytesPerSecond = 0;
    mSecureTransport.ResetCounters();
    mRateSampleTime = std::chrono::steady_clock::now();
    mClientIngress.Reset();
    mServerWorld.Reset();
    mReplication.Reset();
    mAuthorityScheduler.Reset();
    mLocalVoiceSubmission.Reset();

    ++mSessionGeneration;
    if (mSessionGeneration == 0) mSessionGeneration = 1;
}

void NetworkSessionLifecycleService::Update() {
    const NetworkConnectionPumpResult connection =
        mConnection.Pump(mServerSessions.Peers());
    if (connection.clientConnected) {
        mClientAdmission.BeginCryptoHandshake();
    } else if (connection.connectionFailed) {
        mCommunication.QueueChat("system: connection failed", CLKSystem);
    }

    if (mConnection.IsHost()) {
        const auto now = std::chrono::steady_clock::now();
        mReplication.UpdateObservers(mServerSessions.Peers(), now);
        mAuthorityScheduler.Advance(now);
        mReplication.Flush(
            mServerSessions.Peers(),
            [this](int32_t peer, const std::string& payload,
                   bool highPriority) {
                const NetMsgFlags flags =
                    highPriority ? NMFHighPriority : NMFNone;
                return mSecureTransport.SendEncryptedPayloadToPeer(
                    peer, payload, flags);
            });
    }

    if (connection.clientTerminated) {
        mCommunication.QueueChat(
            connection.terminationReason.empty()
                ? "system: disconnected"
                : "system: " + connection.terminationReason,
            CLKSystem);
        Disconnect();
        return;
    }

    SampleTraffic(std::chrono::steady_clock::now());
}

bool NetworkSessionLifecycleService::IsSecure() const {
    if (mConnection.ClientReady()) {
        return mClientCrypto.ready() && mClientAdmission.IdentitySent();
    }
    if (mConnection.IsHost()) return mServerSessions.AllPeersSecure();
    return false;
}

void NetworkSessionLifecycleService::SampleTraffic(
    std::chrono::steady_clock::time_point now) {
    const double elapsed =
        std::chrono::duration<double>(now - mRateSampleTime).count();
    if (elapsed < 1.0) return;

    mInboundBytesPerSecond =
        static_cast<int32_t>(mSecureTransport.InboundBytes() / elapsed);
    mOutboundBytesPerSecond =
        static_cast<int32_t>(mSecureTransport.OutboundBytes() / elapsed);
    mSecureTransport.ResetCounters();
    mRateSampleTime = now;
}

} // namespace SoH::Network
