#pragma once

#include "NetworkProtocol.h"

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace Game::Multiplayer {

struct NetworkConnectionEvents {
    std::function<void(char*, int32_t)> clientMessage;
    std::function<void(int32_t, char*, int32_t)> serverMessage;
    std::function<void(int32_t)> peerCreated;
    std::function<void(int32_t)> peerDeleted;
};

struct NetworkConnectionPumpResult {
    bool clientConnected = false;
    bool connectionFailed = false;
    bool clientTerminated = false;
    std::string terminationReason;
};

// Sole owner of the legacy reliable-UDP transport objects and their asynchronous
// connection lifecycle. Higher layers receive messages/events and submit bytes;
// they never own or dereference NetTranspClient/NetTranspServer.
class NetworkConnectionTransport final {
  public:
    explicit NetworkConnectionTransport(NetworkConnectionEvents events);
    ~NetworkConnectionTransport();

    NetworkConnectionTransport(const NetworkConnectionTransport&) = delete;
    NetworkConnectionTransport& operator=(const NetworkConnectionTransport&) = delete;

    bool Host(uint16_t port, const std::string& sessionName);
    bool Connect(const std::string& address, uint16_t port, const std::string& playerName);
    void Disconnect();
    NetworkConnectionPumpResult Pump(const std::vector<int32_t>& serverPeers);

    bool IsHost() const { return mServer != nullptr; }
    bool IsClient() const { return mClient != nullptr || mConnectFuture.valid(); }
    bool IsActive() const { return IsHost() || IsClient(); }
    bool ClientReady() const { return mClient != nullptr; }

    bool SendToServer(const std::string& payload, NetMsgFlags flags);
    bool SendToPeer(int32_t peer, const std::string& payload, NetMsgFlags flags);
    bool Kick(int32_t peer, NetTerminationReason reason,
              const std::string& explanation);
    bool GetPeerConnectionInfo(int32_t peer, int32_t& latencyMilliseconds,
                               int32_t& throughputBytesPerSecond) const;

    int32_t LatencyMilliseconds() const { return mLatencyMilliseconds; }
    int32_t ThroughputBytesPerSecond() const {
        return mThroughputBytesPerSecond;
    }

  private:
    struct ConnectAttempt {
        std::unique_ptr<NetTranspClient> client;
        ConnectResult result = CRError;
    };

    static void OnClientMessage(char* buffer, int32_t size, void* context);
    static void OnServerMessage(int32_t sender, char* buffer, int32_t size,
                                void* context);
    static void OnPeerCreated(int32_t peer, bool botClient, const char* name,
                              unsigned long address, void* context);
    static void OnPeerDeleted(int32_t peer, void* context);

    NetworkConnectionEvents mEvents;
    std::unique_ptr<NetTranspServer> mServer;
    std::unique_ptr<NetTranspClient> mClient;
    std::future<ConnectAttempt> mConnectFuture;
    int32_t mLatencyMilliseconds = 0;
    int32_t mThroughputBytesPerSecond = 0;
};

} // namespace Game::Multiplayer
