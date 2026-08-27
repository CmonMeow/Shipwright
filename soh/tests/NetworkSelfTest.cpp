#include <sysdef.h>

#include "Network/netTransport.hpp"

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <memory>

namespace {

constexpr unsigned short kTestPort = 47777;
constexpr char kPing[] = "pathengine-ping";
constexpr char kPong[] = "pathengine-pong";

struct TestState {
    bool peerConnected = false;
    bool pingReceived = false;
    bool pongReceived = false;
    __int32 peer = -1;
};

void OnPeerCreated(__int32 peer, bool, const char*, unsigned long, void* context) {
    auto& state = *static_cast<TestState*>(context);
    state.peerConnected = true;
    state.peer = peer;
}

void OnPeerDeleted(__int32, void*) {
}

void OnServerMessage(__int32 peer, char* buffer, __int32 size, void* context) {
    auto& state = *static_cast<TestState*>(context);
    if (size == static_cast<__int32>(sizeof(kPing)) && std::memcmp(buffer, kPing, sizeof(kPing)) == 0) {
        state.pingReceived = true;
        state.peer = peer;
    }
}

void OnClientMessage(char* buffer, __int32 size, void* context) {
    auto& state = *static_cast<TestState*>(context);
    if (size == static_cast<__int32>(sizeof(kPong)) && std::memcmp(buffer, kPong, sizeof(kPong)) == 0) {
        state.pongReceived = true;
    }
}

} // namespace

int main() {
    ClearLog();
    SetNetworkPort(kTestPort);

    std::unique_ptr<NetTranspServer> server(CreateNetServer());
    if (!server || !server->Init("Shipwright transport test", "", kTestPort)) {
        Error("Network self-test: server initialization failed on port %u", kTestPort);
        server.reset();
        stopUdpListenSend();
        destroyPool();
        return 1;
    }

    unsigned short clientPort = kTestPort;
    std::unique_ptr<NetTranspClient> client(CreateNetClient());
    if (!client || client->Init("127.0.0.1:47777", "", false, clientPort, "ShipwrightTest", nullptr) != CROK) {
        Error("Network self-test: client connection failed");
        client.reset();
        server.reset();
        stopUdpListenSend();
        destroyPool();
        return 2;
    }

    TestState state;
    bool pingSent = false;
    bool pongSent = false;

    for (int attempt = 0; attempt < 1000 && !state.pongReceived; ++attempt) {
        server->ProcessPlayers(OnPeerCreated, OnPeerDeleted, &state);
        server->ProcessUserMessages(OnServerMessage, &state);
        client->ProcessUserMessages(OnClientMessage, &state);

        if (state.peerConnected && !pingSent) {
            DWORD messageId = 0;
            pingSent = static_cast<bool>(client->SendMsg(reinterpret_cast<BYTE*>(const_cast<char*>(kPing)),
                                                         static_cast<__int32>(sizeof(kPing)), messageId,
                                                         NMFGuaranteed | NMFHighPriority, nullptr));
        }

        if (state.pingReceived && !pongSent) {
            DWORD messageId = 0;
            pongSent = static_cast<bool>(server->SendMsg(state.peer, reinterpret_cast<BYTE*>(const_cast<char*>(kPong)),
                                                         static_cast<__int32>(sizeof(kPong)), messageId,
                                                         NMFGuaranteed | NMFHighPriority, nullptr));
        }

        Sleep(10);
    }

    client.reset();
    server.reset();
    stopUdpListenSend();
    destroyPool();

    if (!state.peerConnected || !pingSent || !state.pingReceived || !pongSent || !state.pongReceived) {
        Error("Network self-test failed: connected=%d pingSent=%d pingReceived=%d pongSent=%d pongReceived=%d",
              state.peerConnected, pingSent, state.pingReceived, pongSent, state.pongReceived);
        return 3;
    }

    Error("Network self-test passed: guaranteed high-priority localhost round trip");
    return 0;
}
