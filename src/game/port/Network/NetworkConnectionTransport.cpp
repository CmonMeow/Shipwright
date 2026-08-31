#include "NetworkConnectionTransport.h"

#include <atomic>
#include <chrono>
#include <utility>

namespace SoH::Network {

namespace {

std::atomic_bool gCancelConnection = false;

bool CancelPendingConnection() {
    return gCancelConnection.load(std::memory_order_relaxed);
}

} // namespace

NetworkConnectionTransport::NetworkConnectionTransport(
    NetworkConnectionEvents events)
    : mEvents(std::move(events)) {
}

NetworkConnectionTransport::~NetworkConnectionTransport() {
    Disconnect();
}

bool NetworkConnectionTransport::Host(uint16_t port,
                                      const std::string& sessionName) {
    if (IsActive() || port == 0 || port > 49151) return false;
    SetNetworkPort(port);
    mServer.reset(CreateNetServer());
    if (!mServer || !mServer->Init(sessionName, "", port)) {
        mServer.reset();
        stopUdpListenSend();
        destroyPool();
        return false;
    }
    return true;
}

bool NetworkConnectionTransport::Connect(const std::string& address,
                                         const std::string& playerName) {
    if (IsActive() || address.empty() || playerName.empty()) return false;
    gCancelConnection.store(false, std::memory_order_relaxed);
    mConnectFuture = std::async(
        std::launch::async, [address, playerName]() {
            ConnectAttempt attempt;
            unsigned short port = DEFAULT_NETWORK_PORT;
            attempt.client.reset(CreateNetClient());
            attempt.result = attempt.client
                ? attempt.client->Init(address, "", false, port, playerName,
                                       CancelPendingConnection)
                : CRError;
            if (attempt.result != CROK) {
                attempt.client.reset();
                stopUdpListenSend();
                destroyPool();
            }
            return attempt;
        });
    return true;
}

void NetworkConnectionTransport::Disconnect() {
    if (mConnectFuture.valid()) {
        gCancelConnection.store(true, std::memory_order_relaxed);
        ConnectAttempt attempt = mConnectFuture.get();
        attempt.client.reset();
        gCancelConnection.store(false, std::memory_order_relaxed);
        stopUdpListenSend();
        destroyPool();
    }
    if (mClient || mServer) {
        sendDisconnectMessages();
        stopUdpListenSend();
    }
    mClient.reset();
    mServer.reset();
    destroyPool();
    mLatencyMilliseconds = 0;
    mThroughputBytesPerSecond = 0;
}

NetworkConnectionPumpResult NetworkConnectionTransport::Pump(
    const std::vector<int32_t>& serverPeers) {
    NetworkConnectionPumpResult result{};
    if (mConnectFuture.valid() &&
        mConnectFuture.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::ready) {
        ConnectAttempt attempt = mConnectFuture.get();
        if (attempt.result == CROK && attempt.client) {
            mClient = std::move(attempt.client);
            result.clientConnected = true;
        } else {
            result.connectionFailed = true;
        }
    }

    if (mServer) {
        mServer->ProcessPlayers(OnPeerCreated, OnPeerDeleted, this);
        mServer->ProcessUserMessages(OnServerMessage, this);
        int64_t totalLatency = 0;
        int64_t totalThroughput = 0;
        int32_t connectedPeers = 0;
        for (const int32_t peer : serverPeers) {
            int32_t latency = 0;
            int32_t throughput = 0;
            if (mServer->GetConnectionInfo(peer, latency, throughput)) {
                totalLatency += latency;
                totalThroughput += throughput;
                ++connectedPeers;
            }
        }
        mLatencyMilliseconds = connectedPeers > 0
            ? static_cast<int32_t>(totalLatency / connectedPeers)
            : 0;
        mThroughputBytesPerSecond = connectedPeers > 0
            ? static_cast<int32_t>(totalThroughput / connectedPeers)
            : 0;
    }

    if (mClient) {
        mClient->ProcessUserMessages(OnClientMessage, this);
        mClient->GetConnectionInfo(mLatencyMilliseconds,
                                   mThroughputBytesPerSecond);
        if (mClient->IsSessionTerminated()) {
            result.clientTerminated = true;
            result.terminationReason = mClient->GetWhySessionTerminatedStr();
        }
    }
    return result;
}

bool NetworkConnectionTransport::SendToServer(const std::string& payload,
                                              NetMsgFlags flags) {
    if (!mClient || payload.empty()) return false;
    DWORD messageId = 0;
    return static_cast<bool>(mClient->SendMsg(
        reinterpret_cast<BYTE*>(const_cast<char*>(payload.data())),
        static_cast<int32_t>(payload.size()), messageId, flags, nullptr));
}

bool NetworkConnectionTransport::SendToPeer(int32_t peer,
                                            const std::string& payload,
                                            NetMsgFlags flags) {
    if (!mServer || payload.empty()) return false;
    DWORD messageId = 0;
    return static_cast<bool>(mServer->SendMsg(
        peer, reinterpret_cast<BYTE*>(const_cast<char*>(payload.data())),
        static_cast<int32_t>(payload.size()), messageId, flags, nullptr));
}

bool NetworkConnectionTransport::Kick(
    int32_t peer, NetTerminationReason reason,
    const std::string& explanation) {
    if (!mServer) return false;
    mServer->KickOff(peer, reason, explanation.c_str());
    return true;
}

bool NetworkConnectionTransport::GetPeerConnectionInfo(
    int32_t peer, int32_t& latencyMilliseconds,
    int32_t& throughputBytesPerSecond) const {
    return mServer && mServer->GetConnectionInfo(
                          peer, latencyMilliseconds,
                          throughputBytesPerSecond);
}

void NetworkConnectionTransport::OnClientMessage(
    char* buffer, int32_t size, void* context) {
    auto& self = *static_cast<NetworkConnectionTransport*>(context);
    if (self.mEvents.clientMessage) self.mEvents.clientMessage(buffer, size);
}

void NetworkConnectionTransport::OnServerMessage(
    int32_t sender, char* buffer, int32_t size, void* context) {
    auto& self = *static_cast<NetworkConnectionTransport*>(context);
    if (self.mEvents.serverMessage) {
        self.mEvents.serverMessage(sender, buffer, size);
    }
}

void NetworkConnectionTransport::OnPeerCreated(
    int32_t peer, bool, const char*, unsigned long, void* context) {
    auto& self = *static_cast<NetworkConnectionTransport*>(context);
    if (self.mEvents.peerCreated) self.mEvents.peerCreated(peer);
}

void NetworkConnectionTransport::OnPeerDeleted(int32_t peer, void* context) {
    auto& self = *static_cast<NetworkConnectionTransport*>(context);
    if (self.mEvents.peerDeleted) self.mEvents.peerDeleted(peer);
}

} // namespace SoH::Network
