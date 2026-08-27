#include "ReliableUdpService.h"

#include <Windows.h>

namespace SoH::Network {

ReliableUdpService::~ReliableUdpService() {
    Shutdown();
}

std::string ReliableUdpService::DefaultPlayerName() {
    char name[256]{};
    DWORD size = static_cast<DWORD>(sizeof(name));
    if (GetUserNameA(name, &size) && name[0] != '\0') {
        return name;
    }
    return "Player";
}

bool ReliableUdpService::Host(uint16_t port, const std::string& sessionName) {
    if (port == 0 || IsActive()) {
        return false;
    }

    SetNetworkPort(port);
    mServer.reset(CreateNetServer());
    if (!mServer || !mServer->Init(sessionName, "", port)) {
        mServer.reset();
        destroyPool();
        return false;
    }
    return true;
}

bool ReliableUdpService::Join(const std::string& address, const std::string& playerName) {
    if (address.empty() || IsActive()) {
        return false;
    }

    unsigned short port = 777;
    mClient.reset(CreateNetClient());
    if (!mClient || mClient->Init(address, "", false, port, playerName, nullptr) != CROK) {
        mClient.reset();
        destroyPool();
        return false;
    }
    return true;
}

void ReliableUdpService::Shutdown() {
    mClient.reset();
    mServer.reset();
    stopUdpListenSend();
    destroyPool();

    std::lock_guard lock(mMutex);
    mPeers.clear();
    mReceived.clear();
}

bool ReliableUdpService::IsHosting() const {
    return mServer != nullptr;
}

bool ReliableUdpService::IsConnected() const {
    return mClient != nullptr && !mClient->IsSessionTerminated();
}

bool ReliableUdpService::IsActive() const {
    return mClient != nullptr || mServer != nullptr;
}

NetMsgFlags ReliableUdpService::MakeFlags(bool guaranteed, bool highPriority) {
    NetMsgFlags flags = NMFNone;
    if (guaranteed) {
        flags = flags | NMFGuaranteed;
    }
    if (highPriority) {
        flags = flags | NMFHighPriority;
    }
    return flags;
}

bool ReliableUdpService::SendToServer(const std::vector<uint8_t>& payload, bool guaranteed, bool highPriority) {
    if (!IsConnected() || payload.empty()) {
        return false;
    }

    DWORD messageId = 0;
    return static_cast<bool>(mClient->SendMsg(const_cast<BYTE*>(payload.data()), static_cast<__int32>(payload.size()),
                                              messageId, MakeFlags(guaranteed, highPriority), nullptr));
}

bool ReliableUdpService::SendToPeer(int32_t peer, const std::vector<uint8_t>& payload, bool guaranteed,
                                    bool highPriority) {
    if (!mServer || payload.empty()) {
        return false;
    }

    DWORD messageId = 0;
    return static_cast<bool>(mServer->SendMsg(peer, const_cast<BYTE*>(payload.data()),
                                              static_cast<__int32>(payload.size()), messageId,
                                              MakeFlags(guaranteed, highPriority), nullptr));
}

void ReliableUdpService::Broadcast(const std::vector<uint8_t>& payload, bool guaranteed, bool highPriority) {
    std::vector<int32_t> peers;
    {
        std::lock_guard lock(mMutex);
        peers.assign(mPeers.begin(), mPeers.end());
    }
    for (const int32_t peer : peers) {
        SendToPeer(peer, payload, guaranteed, highPriority);
    }
}

void ReliableUdpService::Update() {
    if (mServer) {
        mServer->ProcessPlayers(OnPeerCreated, OnPeerDeleted, this);
        mServer->ProcessUserMessages(OnServerMessage, this);
    }
    if (mClient) {
        mClient->ProcessUserMessages(OnClientMessage, this);
        if (mClient->IsSessionTerminated()) {
            mClient.reset();
            destroyPool();
        }
    }
}

bool ReliableUdpService::Poll(ReliableUdpPacket& packet) {
    std::lock_guard lock(mMutex);
    if (mReceived.empty()) {
        return false;
    }
    packet = std::move(mReceived.front());
    mReceived.pop_front();
    return true;
}

void ReliableUdpService::OnClientMessage(char* buffer, __int32 size, void* context) {
    static_cast<ReliableUdpService*>(context)->QueuePacket(0, buffer, size);
}

void ReliableUdpService::OnServerMessage(__int32 sender, char* buffer, __int32 size, void* context) {
    static_cast<ReliableUdpService*>(context)->QueuePacket(sender, buffer, size);
}

void ReliableUdpService::OnPeerCreated(__int32 peer, bool, const char*, unsigned long, void* context) {
    auto* service = static_cast<ReliableUdpService*>(context);
    std::lock_guard lock(service->mMutex);
    service->mPeers.insert(peer);
}

void ReliableUdpService::OnPeerDeleted(__int32 peer, void* context) {
    auto* service = static_cast<ReliableUdpService*>(context);
    std::lock_guard lock(service->mMutex);
    service->mPeers.erase(peer);
}

void ReliableUdpService::QueuePacket(int32_t sender, const char* buffer, __int32 size) {
    if (!buffer || size <= 0) {
        return;
    }

    ReliableUdpPacket packet;
    packet.sender = sender;
    packet.payload.assign(reinterpret_cast<const uint8_t*>(buffer), reinterpret_cast<const uint8_t*>(buffer) + size);
    std::lock_guard lock(mMutex);
    mReceived.push_back(std::move(packet));
}

} // namespace SoH::Network
