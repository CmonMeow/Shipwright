#include "Network/ShipwrightNetworkRuntime.h"

#include <libultraship/log/PathEngineLog.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

std::atomic_bool gRunning{ true };

BOOL WINAPI ConsoleHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT ||
        event == CTRL_LOGOFF_EVENT || event == CTRL_SHUTDOWN_EVENT) {
        gRunning = false;
        return TRUE;
    }
    return FALSE;
}

void DrainRelayQueues(SoH::Network::ShipwrightNetworkRuntime& network) {
    NetworkChatLine chat;
    while (network.PollChat(chat)) {
    }
    NetworkPlayerStatePacket player;
    while (network.PollPlayerState(player)) {
    }
    NetworkPlayerRemovePacket removal;
    while (network.PollPlayerRemove(removal)) {
    }
    NetworkDynamicObjectStatePacket objectState;
    while (network.PollDynamicObjectState(objectState)) {
    }
    NetworkProjectileStatePacket projectile;
    while (network.PollProjectileState(projectile)) {
    }
    NetworkPlayerRespawnPacket respawn;
    while (network.PollPlayerRespawn(respawn)) {
    }
    NetworkVoicePacket voice;
    while (network.PollVoice(voice)) {
    }
}

} // namespace

int main(int argc, char** argv) {
    ClearLog();
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    uint16_t port = DEFAULT_NETWORK_PORT;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] ? argv[index] : "";
        if ((argument == "--port" || argument == "-p") && index + 1 < argc) {
            const long parsed = std::strtol(argv[++index], nullptr, 10);
            if (parsed > 0 && parsed <= 49151) {
                port = static_cast<uint16_t>(parsed);
            }
        }
    }

    SoH::Network::ShipwrightNetworkRuntime network;
    if (!network.Host(port, "Shipwright Dedicated Server")) {
        Error("Dedicated server failed to host on port %u", static_cast<unsigned>(port));
        std::fprintf(stderr, "Unable to host on UDP port %u.\n", static_cast<unsigned>(port));
        return EXIT_FAILURE;
    }

    std::printf("Shipwright dedicated server listening on UDP port %u.\n", static_cast<unsigned>(port));
    Error("Dedicated server hosting secure session on port %u", static_cast<unsigned>(port));

    while (gRunning) {
        network.Update();
        DrainRelayQueues(network);
        Sleep(10);
    }

    network.Disconnect();
    Error("Dedicated server stopped");
    return EXIT_SUCCESS;
}
