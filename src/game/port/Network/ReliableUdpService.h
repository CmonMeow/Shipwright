#pragma once

#include "Network/netTransport.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace SoH::Network {

struct ReliableUdpPacket {
    int32_t sender = -1;
    std::vector<uint8_t> payload;
};

class ReliableUdpService final {
  public:
    ReliableUdpService() = default;
    ~ReliableUdpService();

    ReliableUdpService(const ReliableUdpService&) = delete;
    ReliableUdpService& operator=(const ReliableUdpService&) = delete;

    static std::string DefaultPlayerName();

    bool Host(uint16_t port = 777, const std::string& sessionName = "Ocarina of Time");
    bool Join(const std::string& address, const std::string& playerName = DefaultPlayerName());
    void Shutdown();

    bool IsHosting() const;
    bool IsConnected() const;
    bool IsActive() const;

    bool SendToServer(const std::vector<uint8_t>& payload, bool guaranteed = true, bool highPriority = false);
    bool SendToPeer(int32_t peer, const std::vector<uint8_t>& payload, bool guaranteed = true,
                    bool highPriority = false);
    void Broadcast(const std::vector<uint8_t>& payload, bool guaranteed = true, bool highPriority = false);

    void Update();
    bool Poll(ReliableUdpPacket& packet);

  private:
    static void OnClientMessage(char* buffer, __int32 size, void* context);
    static void OnServerMessage(__int32 sender, char* buffer, __int32 size, void* context);
    static void OnPeerCreated(__int32 peer, bool botClient, const char* name, unsigned long address, void* context);
    static void OnPeerDeleted(__int32 peer, void* context);

    void QueuePacket(int32_t sender, const char* buffer, __int32 size);
    static NetMsgFlags MakeFlags(bool guaranteed, bool highPriority);

    std::unique_ptr<NetTranspClient> mClient;
    std::unique_ptr<NetTranspServer> mServer;
    std::unordered_set<int32_t> mPeers;
    std::deque<ReliableUdpPacket> mReceived;
    mutable std::mutex mMutex;
};

} // namespace SoH::Network
